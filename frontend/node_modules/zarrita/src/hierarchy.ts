import type { AbsolutePath, Readable } from "@zarrita/storage";
import { create_sharded_chunk_getter } from "./codecs/sharding.js";
import { create_codec_pipeline } from "./codecs.js";
import type {
	ArrayMetadata,
	Attributes,
	Chunk,
	CodecMetadata,
	DataType,
	GroupMetadata,
	Scalar,
	TypedArrayConstructor,
} from "./metadata.js";
import {
	create_chunk_key_encoder,
	type DataTypeQuery,
	ensure_correct_scalar,
	get_ctr,
	get_strides,
	is_dtype,
	is_sharding_codec,
	type NarrowDataType,
} from "./util.js";

export class Location<Store> {
	constructor(
		public readonly store: Store,
		public readonly path: AbsolutePath = "/",
	) {}

	resolve(path: string): Location<Store> {
		// reuse URL resolution logic built into the browser
		// handles relative paths, absolute paths, etc.
		let root = new URL(
			`file://${this.path.endsWith("/") ? this.path : `${this.path}/`}`,
		);
		return new Location(
			this.store,
			decodeURIComponent(new URL(path, root).pathname) as AbsolutePath,
		);
	}
}

export function root<Store>(store: Store): Location<Store>;
export function root(): Location<Map<string, Uint8Array>>;
export function root<Store>(
	store?: Store,
): Location<Store | Map<string, Uint8Array>> {
	return new Location(store ?? new Map());
}

export class Group<Store extends Readable> extends Location<Store> {
	readonly kind = "group";
	#metadata: GroupMetadata;
	constructor(store: Store, path: AbsolutePath, metadata: GroupMetadata) {
		super(store, path);
		this.#metadata = metadata;
	}
	get attrs(): Attributes {
		return this.#metadata.attributes;
	}
}

function get_array_order(
	codecs: CodecMetadata[],
): "C" | "F" | globalThis.Array<number> {
	const maybe_transpose_codec = codecs.find((c) => c.name === "transpose");
	// @ts-expect-error - TODO: Should validate?
	return maybe_transpose_codec?.configuration?.order ?? "C";
}

const CONTEXT_MARKER = Symbol("zarrita.context");

export function get_context<T>(obj: { [CONTEXT_MARKER]: T }): T {
	return obj[CONTEXT_MARKER];
}

function create_context<Store extends Readable, D extends DataType>(
	location: Location<Readable>,
	metadata: ArrayMetadata<D>,
): ArrayContext<Store, D> {
	let { configuration } = metadata.codecs.find(is_sharding_codec) ?? {};
	let shared_context = {
		encode_chunk_key: create_chunk_key_encoder(metadata.chunk_key_encoding),
		TypedArray: get_ctr(metadata.data_type),
		fill_value: metadata.fill_value,
	};

	if (configuration) {
		let native_order = get_array_order(configuration.codecs);
		return {
			...shared_context,
			kind: "sharded",
			chunk_shape: configuration.chunk_shape,
			codec: create_codec_pipeline({
				data_type: metadata.data_type,
				shape: configuration.chunk_shape,
				codecs: configuration.codecs,
			}),
			get_strides(shape: number[]) {
				return get_strides(shape, native_order);
			},
			get_chunk_bytes: create_sharded_chunk_getter(
				location,
				metadata.chunk_grid.configuration.chunk_shape,
				shared_context.encode_chunk_key,
				configuration,
			),
		};
	}

	let native_order = get_array_order(metadata.codecs);
	return {
		...shared_context,
		kind: "regular",
		chunk_shape: metadata.chunk_grid.configuration.chunk_shape,
		codec: create_codec_pipeline({
			data_type: metadata.data_type,
			shape: metadata.chunk_grid.configuration.chunk_shape,
			codecs: metadata.codecs,
		}),
		get_strides(shape: number[]) {
			return get_strides(shape, native_order);
		},
		async get_chunk_bytes(chunk_coords, options) {
			let chunk_key = shared_context.encode_chunk_key(chunk_coords);
			let chunk_path = location.resolve(chunk_key).path;
			return location.store.get(chunk_path, options);
		},
	};
}

/** For internal use only, and is subject to change. */
interface ArrayContext<Store extends Readable, D extends DataType> {
	kind: "sharded" | "regular";
	/** The codec pipeline for this array. */
	codec: ReturnType<typeof create_codec_pipeline<D>>;
	/** Encode a chunk key from chunk coordinates. */
	encode_chunk_key(chunk_coords: number[]): string;
	/** The TypedArray constructor for this array chunks. */
	TypedArray: TypedArrayConstructor<D>;
	/** A function to get the strides for a given shape, using the array order */
	get_strides(shape: number[]): number[];
	/** The fill value for this array. */
	fill_value: Scalar<D> | null;
	/** A function to get the bytes for a given chunk. */
	get_chunk_bytes(
		chunk_coords: number[],
		options?: Parameters<Store["get"]>[1],
	): Promise<Uint8Array | undefined>;
	/** The chunk shape for this array. */
	chunk_shape: number[];
}

export class Array<
	Dtype extends DataType,
	Store extends Readable = Readable,
> extends Location<Store> {
	readonly kind = "array";
	#metadata: ArrayMetadata<Dtype>;
	[CONTEXT_MARKER]: ArrayContext<Store, Dtype>;

	constructor(
		store: Store,
		path: AbsolutePath,
		metadata: ArrayMetadata<Dtype>,
	) {
		super(store, path);
		this.#metadata = {
			...metadata,
			fill_value: ensure_correct_scalar(metadata),
		};
		this[CONTEXT_MARKER] = create_context(this, metadata);
	}

	get attrs(): Attributes {
		return this.#metadata.attributes;
	}

	get shape(): number[] {
		return this.#metadata.shape;
	}

	get chunks(): number[] {
		return this[CONTEXT_MARKER].chunk_shape;
	}

	get dtype(): Dtype {
		return this.#metadata.data_type;
	}

	async getChunk(
		chunk_coords: number[],
		options?: Parameters<Store["get"]>[1],
	): Promise<Chunk<Dtype>> {
		let context = this[CONTEXT_MARKER];
		let maybe_bytes = await context.get_chunk_bytes(chunk_coords, options);
		if (!maybe_bytes) {
			let size = context.chunk_shape.reduce((a, b) => a * b, 1);
			let data = new context.TypedArray(size);
			// @ts-expect-error: TS can't infer that `fill_value` is union (assumes never) but this is ok
			data.fill(context.fill_value);
			return {
				data,
				shape: context.chunk_shape,
				stride: context.get_strides(context.chunk_shape),
			};
		}
		return context.codec.decode(maybe_bytes);
	}

	/**
	 * A helper method to narrow `zarr.Array` Dtype.
	 *
	 * ```typescript
	 * let arr: zarr.Array<DataType, FetchStore> = zarr.open(store, { kind: "array" });
	 *
	 * // Option 1: narrow by scalar type (e.g. "bool", "raw", "bigint", "number")
	 * if (arr.is("bigint")) {
	 *   // zarr.Array<"int64" | "uint64", FetchStore>
	 * }
	 *
	 * // Option 3: exact match
	 * if (arr.is("float32")) {
	 *   // zarr.Array<"float32", FetchStore, "/">
	 * }
	 * ```
	 */
	is<Query extends DataTypeQuery>(
		query: Query,
	): this is Array<NarrowDataType<Dtype, Query>, Store> {
		return is_dtype(this.dtype, query);
	}
}
