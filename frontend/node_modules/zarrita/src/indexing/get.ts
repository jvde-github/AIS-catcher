import type { Readable } from "@zarrita/storage";

import { type Array, get_context } from "../hierarchy.js";
import type { Chunk, DataType, Scalar, TypedArray } from "../metadata.js";
import { BasicIndexer } from "./indexer.js";
import type {
	GetOptions,
	Prepare,
	SetFromChunk,
	SetScalar,
	Slice,
} from "./types.js";
import { create_queue } from "./util.js";

function unwrap<D extends DataType>(
	arr: TypedArray<D>,
	idx: number,
): Scalar<D> {
	return ("get" in arr ? arr.get(idx) : arr[idx]) as Scalar<D>;
}

export async function get<
	D extends DataType,
	Store extends Readable,
	Arr extends Chunk<D>,
	Sel extends (null | Slice | number)[],
>(
	arr: Array<D, Store>,
	selection: null | Sel,
	opts: GetOptions<Parameters<Store["get"]>[1]>,
	setter: {
		prepare: Prepare<D, Arr>;
		set_scalar: SetScalar<D, Arr>;
		set_from_chunk: SetFromChunk<D, Arr>;
	},
): Promise<
	null extends Sel[number] ? Arr : Slice extends Sel[number] ? Arr : Scalar<D>
> {
	let context = get_context(arr);
	let indexer = new BasicIndexer({
		selection,
		shape: arr.shape,
		chunk_shape: arr.chunks,
	});

	let out = setter.prepare(
		new context.TypedArray(indexer.shape.reduce((a, b) => a * b, 1)),
		indexer.shape,
		context.get_strides(indexer.shape),
	);

	let queue = opts.create_queue?.() ?? create_queue();
	for (const { chunk_coords, mapping } of indexer) {
		queue.add(async () => {
			let { data, shape, stride } = await arr.getChunk(chunk_coords, opts.opts);
			let chunk = setter.prepare(data, shape, stride);
			setter.set_from_chunk(out, chunk, mapping);
		});
	}

	await queue.onIdle();

	// If the final out shape is empty, we just return a scalar.
	// @ts-expect-error - TS can't narrow this conditional type
	return indexer.shape.length === 0 ? unwrap(out.data, 0) : out;
}
