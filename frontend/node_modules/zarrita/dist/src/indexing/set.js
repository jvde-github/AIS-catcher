import { get_context } from "../hierarchy.js";
import { BasicIndexer } from "./indexer.js";
import { create_queue } from "./util.js";
function flip_indexer_projection(m) {
    if (m.to == null)
        return { from: m.to, to: m.from };
    return { from: m.to, to: m.from };
}
export async function set(arr, selection, value, opts, setter) {
    const context = get_context(arr);
    if (context.kind === "sharded") {
        throw new Error("Set not supported for sharded arrays.");
    }
    const indexer = new BasicIndexer({
        selection,
        shape: arr.shape,
        chunk_shape: arr.chunks,
    });
    // We iterate over all chunks which overlap the selection and thus contain data
    // that needs to be replaced. Each chunk is processed in turn, extracting the
    // necessary data from the value array and storing into the chunk array.
    const chunk_size = arr.chunks.reduce((a, b) => a * b, 1);
    const queue = opts.create_queue ? opts.create_queue() : create_queue();
    // N.B., it is an important optimisation that we only visit chunks which overlap
    // the selection. This minimises the number of iterations in the main for loop.
    for (const { chunk_coords, mapping } of indexer) {
        const chunk_selection = mapping.map((i) => i.from);
        const flipped = mapping.map(flip_indexer_projection);
        queue.add(async () => {
            // obtain key for chunk storage
            const chunk_path = arr.resolve(context.encode_chunk_key(chunk_coords)).path;
            let chunk_data;
            const chunk_shape = arr.chunks.slice();
            const chunk_stride = context.get_strides(chunk_shape);
            if (is_total_slice(chunk_selection, chunk_shape)) {
                // totally replace
                chunk_data = new context.TypedArray(chunk_size);
                // optimization: we are completely replacing the chunk, so no need
                // to access the exisiting chunk data
                if (typeof value === "object") {
                    // Otherwise data just contiguous TypedArray
                    const chunk = setter.prepare(chunk_data, chunk_shape.slice(), chunk_stride.slice());
                    // @ts-expect-error - Value is not a scalar
                    setter.set_from_chunk(chunk, value, flipped);
                }
                else {
                    // @ts-expect-error - Value is a scalar
                    chunk_data.fill(value);
                }
            }
            else {
                // partially replace the contents of this chunk
                chunk_data = await arr.getChunk(chunk_coords).then(({ data }) => data);
                const chunk = setter.prepare(chunk_data, chunk_shape.slice(), chunk_stride.slice());
                // Modify chunk data
                if (typeof value === "object") {
                    // @ts-expect-error - Value is not a scalar
                    setter.set_from_chunk(chunk, value, flipped);
                }
                else {
                    setter.set_scalar(chunk, chunk_selection, value);
                }
            }
            await arr.store.set(chunk_path, await context.codec.encode({
                data: chunk_data,
                shape: chunk_shape,
                stride: chunk_stride,
            }));
        });
    }
    await queue.onIdle();
}
function is_total_slice(selection, shape) {
    // all items are Indices and every slice is complete
    return selection.every((s, i) => {
        // can't be a full selection
        if (typeof s === "number")
            return false;
        // explicit complete slice
        const [start, stop, step] = s;
        return stop - start === shape[i] && step === 1;
    });
}
//# sourceMappingURL=set.js.map