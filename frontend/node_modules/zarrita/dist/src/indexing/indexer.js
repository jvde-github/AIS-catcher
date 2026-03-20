import { product, range, slice, slice_indices } from "./util.js";
export class IndexError extends Error {
    constructor(msg) {
        super(msg);
        this.name = "IndexError";
    }
}
function err_too_many_indices(selection, shape) {
    throw new IndexError(`too many indicies for array; expected ${shape.length}, got ${selection.length}`);
}
function err_boundscheck(dim_len) {
    throw new IndexError(`index out of bounds for dimension with length ${dim_len}`);
}
function err_negative_step() {
    throw new IndexError("only slices with step >= 1 are supported");
}
function check_selection_length(selection, shape) {
    if (selection.length > shape.length) {
        err_too_many_indices(selection, shape);
    }
}
export function normalize_integer_selection(dim_sel, dim_len) {
    // normalize type to int
    dim_sel = Math.trunc(dim_sel);
    // handle wraparound
    if (dim_sel < 0) {
        dim_sel = dim_len + dim_sel;
    }
    // handle out of bounds
    if (dim_sel >= dim_len || dim_sel < 0) {
        err_boundscheck(dim_len);
    }
    return dim_sel;
}
class IntDimIndexer {
    dim_sel;
    dim_len;
    dim_chunk_len;
    nitems;
    constructor({ dim_sel, dim_len, dim_chunk_len }) {
        // normalize
        dim_sel = normalize_integer_selection(dim_sel, dim_len);
        // store properties
        this.dim_sel = dim_sel;
        this.dim_len = dim_len;
        this.dim_chunk_len = dim_chunk_len;
        this.nitems = 1;
    }
    *[Symbol.iterator]() {
        const dim_chunk_ix = Math.floor(this.dim_sel / this.dim_chunk_len);
        const dim_offset = dim_chunk_ix * this.dim_chunk_len;
        const dim_chunk_sel = this.dim_sel - dim_offset;
        yield { dim_chunk_ix, dim_chunk_sel };
    }
}
class SliceDimIndexer {
    start;
    stop;
    step;
    dim_len;
    dim_chunk_len;
    nitems;
    nchunks;
    constructor({ dim_sel, dim_len, dim_chunk_len }) {
        // normalize
        const [start, stop, step] = slice_indices(dim_sel, dim_len);
        this.start = start;
        this.stop = stop;
        this.step = step;
        if (this.step < 1)
            err_negative_step();
        // store properties
        this.dim_len = dim_len;
        this.dim_chunk_len = dim_chunk_len;
        this.nitems = Math.max(0, Math.ceil((this.stop - this.start) / this.step));
        this.nchunks = Math.ceil(this.dim_len / this.dim_chunk_len);
    }
    *[Symbol.iterator]() {
        // figure out the range of chunks we need to visit
        const dim_chunk_ix_from = Math.floor(this.start / this.dim_chunk_len);
        const dim_chunk_ix_to = Math.ceil(this.stop / this.dim_chunk_len);
        for (const dim_chunk_ix of range(dim_chunk_ix_from, dim_chunk_ix_to)) {
            // compute offsets for chunk within overall array
            const dim_offset = dim_chunk_ix * this.dim_chunk_len;
            const dim_limit = Math.min(this.dim_len, (dim_chunk_ix + 1) * this.dim_chunk_len);
            // determine chunk length, accounting for trailing chunk
            const dim_chunk_len = dim_limit - dim_offset;
            let dim_out_offset = 0;
            let dim_chunk_sel_start = 0;
            if (this.start < dim_offset) {
                // selection start before current chunk
                const remainder = (dim_offset - this.start) % this.step;
                if (remainder)
                    dim_chunk_sel_start += this.step - remainder;
                // compute number of previous items, provides offset into output array
                dim_out_offset = Math.ceil((dim_offset - this.start) / this.step);
            }
            else {
                // selection starts within current chunk
                dim_chunk_sel_start = this.start - dim_offset;
            }
            // selection starts within current chunk if true,
            // otherwise selection ends after current chunk.
            const dim_chunk_sel_stop = this.stop > dim_limit ? dim_chunk_len : this.stop - dim_offset;
            const dim_chunk_sel = [
                dim_chunk_sel_start,
                dim_chunk_sel_stop,
                this.step,
            ];
            const dim_chunk_nitems = Math.ceil((dim_chunk_sel_stop - dim_chunk_sel_start) / this.step);
            const dim_out_sel = [
                dim_out_offset,
                dim_out_offset + dim_chunk_nitems,
                1,
            ];
            yield { dim_chunk_ix, dim_chunk_sel, dim_out_sel };
        }
    }
}
export function normalize_selection(selection, shape) {
    let normalized = [];
    if (selection === null) {
        normalized = shape.map((_) => slice(null));
    }
    else if (Array.isArray(selection)) {
        normalized = selection.map((s) => s ?? slice(null));
    }
    check_selection_length(normalized, shape);
    return normalized;
}
export class BasicIndexer {
    dim_indexers;
    shape;
    constructor({ selection, shape, chunk_shape }) {
        // setup per-dimension indexers
        this.dim_indexers = normalize_selection(selection, shape).map((dim_sel, i) => {
            return new (typeof dim_sel === "number" ? IntDimIndexer : SliceDimIndexer)({
                // @ts-expect-error ts inference not strong enough to know correct chunk
                dim_sel: dim_sel,
                dim_len: shape[i],
                dim_chunk_len: chunk_shape[i],
            });
        });
        this.shape = this.dim_indexers
            .filter((ixr) => ixr instanceof SliceDimIndexer)
            .map((sixr) => sixr.nitems);
    }
    *[Symbol.iterator]() {
        for (const dim_projections of product(...this.dim_indexers)) {
            const chunk_coords = dim_projections.map((p) => p.dim_chunk_ix);
            const mapping = dim_projections.map((p) => {
                if ("dim_out_sel" in p) {
                    return { from: p.dim_chunk_sel, to: p.dim_out_sel };
                }
                return { from: p.dim_chunk_sel, to: null };
            });
            yield { chunk_coords, mapping };
        }
    }
}
//# sourceMappingURL=indexer.js.map