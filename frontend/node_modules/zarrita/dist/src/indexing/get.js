import { get_context } from "../hierarchy.js";
import { BasicIndexer } from "./indexer.js";
import { create_queue } from "./util.js";
function unwrap(arr, idx) {
    return ("get" in arr ? arr.get(idx) : arr[idx]);
}
export async function get(arr, selection, opts, setter) {
    let context = get_context(arr);
    let indexer = new BasicIndexer({
        selection,
        shape: arr.shape,
        chunk_shape: arr.chunks,
    });
    let out = setter.prepare(new context.TypedArray(indexer.shape.reduce((a, b) => a * b, 1)), indexer.shape, context.get_strides(indexer.shape));
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
//# sourceMappingURL=get.js.map