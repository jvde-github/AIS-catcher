import type { AbsolutePath, Readable } from "@zarrita/storage";
import { KeyError, NodeNotFoundError } from "./errors.js";
import type {
	ArrayMetadata,
	ArrayMetadataV2,
	Attributes,
	GroupMetadata,
	GroupMetadataV2,
} from "./metadata.js";
import {
	assert,
	json_decode_object,
	json_encode_object,
	rethrow_unless,
} from "./util.js";

type ConsolidatedMetadata = {
	metadata: Record<string, ArrayMetadataV2 | GroupMetadataV2>;
	zarr_consolidated_format: 1;
};

/**
 * Represents a read-only store that can list its contents.
 */
export interface Listable<Store extends Readable> {
	/** Get the bytes at a given path. */
	get: (...args: Parameters<Store["get"]>) => Promise<Uint8Array | undefined>;
	/** Get a byte range at a given path. */
	getRange: Store["getRange"];
	/** List the contents of the store. */
	contents(): { path: AbsolutePath; kind: "array" | "group" }[];
}

async function get_consolidated_metadata(
	store: Readable,
	metadataKeyOption: string | undefined,
): Promise<ConsolidatedMetadata> {
	const metadataKey = metadataKeyOption ?? ".zmetadata";
	let bytes = await store.get(`/${metadataKey}`);
	if (!bytes) {
		throw new NodeNotFoundError("v2 consolidated metadata", {
			cause: new KeyError(`/${metadataKey}`),
		});
	}
	let meta: ConsolidatedMetadata = json_decode_object(bytes);
	assert(
		meta.zarr_consolidated_format === 1,
		"Unsupported consolidated format.",
	);
	return meta;
}

type Metadata =
	| ArrayMetadataV2
	| GroupMetadataV2
	| ArrayMetadata
	| GroupMetadata
	| Attributes;

function is_meta_key(key: string): boolean {
	return (
		key.endsWith(".zarray") ||
		key.endsWith(".zgroup") ||
		key.endsWith(".zattrs") ||
		key.endsWith("zarr.json")
	);
}

function is_v3(meta: Metadata): meta is ArrayMetadata | GroupMetadata {
	return "zarr_format" in meta && meta.zarr_format === 3;
}

/** Options for {@linkcode withConsolidated} and {@linkcode tryWithConsolidated}. */
export interface WithConsolidatedOptions {
	/**
	 * Key to read consolidated metadata from.
	 *
	 * @default {".zmetadata"}
	 */
	readonly metadataKey?: string;
}

/**
 * Open a consolidated store.
 *
 * This will open a store with Zarr v2 consolidated metadata (`.zmetadata`).
 * @see {@link https://zarr.readthedocs.io/en/stable/spec/v2.html#consolidated-metadata}
 *
 * @param store The store to open.
 * @param opts Options object.
 * @returns A listable store.
 *
 * @example
 * ```js
 * let store = await withConsolidated(
 *   new zarr.FetchStore("https://my-bucket.s3.amazonaws.com");
 * );
 * store.contents(); // [{ path: "/", kind: "group" }, { path: "/foo", kind: "array" }, ...]
 * let grp = zarr.open(store); // Open the root group.
 * let foo = zarr.open(grp.resolve(contents[1].path)); // Open the foo array
 * ```
 */
export async function withConsolidated<Store extends Readable>(
	store: Store,
	opts: WithConsolidatedOptions = {},
): Promise<Listable<Store>> {
	let v2_meta = await get_consolidated_metadata(store, opts.metadataKey);
	let known_meta: Record<AbsolutePath, Metadata> = {};
	for (let [key, value] of Object.entries(v2_meta.metadata)) {
		known_meta[`/${key}`] = value;
	}

	return {
		async get(
			...args: Parameters<Store["get"]>
		): Promise<Uint8Array | undefined> {
			let [key, opts] = args;
			if (known_meta[key]) {
				return json_encode_object(known_meta[key]);
			}
			let maybe_bytes = await store.get(key, opts);
			if (is_meta_key(key) && maybe_bytes) {
				let meta = json_decode_object(maybe_bytes);
				known_meta[key] = meta;
			}
			return maybe_bytes;
		},
		// Delegate range requests to the underlying store.
		// Note: Supporting range requests for consolidated metadata is possible
		// but unlikely to be useful enough to justify the effort.
		getRange: store.getRange?.bind(store),
		contents(): { path: AbsolutePath; kind: "array" | "group" }[] {
			let contents: { path: AbsolutePath; kind: "array" | "group" }[] = [];
			for (let [key, value] of Object.entries(known_meta)) {
				let parts = key.split("/");
				let filename = parts.pop();
				let path = (parts.join("/") || "/") as AbsolutePath;
				if (filename === ".zarray") contents.push({ path, kind: "array" });
				if (filename === ".zgroup") contents.push({ path, kind: "group" });
				if (is_v3(value)) {
					contents.push({ path, kind: value.node_type });
				}
			}
			return contents;
		},
	};
}

/**
 * Try to open a consolidated store, but fall back to the original store if the
 * consolidated metadata is missing.
 *
 * Provides a convenient way to open a store that may or may not have consolidated,
 * returning a consistent interface for both cases. Ideal for usage senarios with
 * known access paths, since store with consolidated metadata do not incur
 * additional network requests when accessing underlying groups and arrays.
 *
 * @param store The store to open.
 * @param opts Options to pass to withConsolidated.
 * @returns A listable store.
 */
export async function tryWithConsolidated<Store extends Readable>(
	store: Store,
	opts: WithConsolidatedOptions = {},
): Promise<Listable<Store> | Store> {
	return withConsolidated(store, opts).catch((error: unknown) => {
		rethrow_unless(error, NodeNotFoundError);
		return store;
	});
}
