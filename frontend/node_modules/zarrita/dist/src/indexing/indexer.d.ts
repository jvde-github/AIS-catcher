import type { Indices, Slice } from "./types.js";
export declare class IndexError extends Error {
    constructor(msg: string);
}
export declare function normalize_integer_selection(dim_sel: number, dim_len: number): number;
interface IntChunkDimProjection {
    dim_chunk_ix: number;
    dim_chunk_sel: number;
}
interface IntDimIndexerProps {
    dim_sel: number;
    dim_len: number;
    dim_chunk_len: number;
}
declare class IntDimIndexer {
    dim_sel: number;
    dim_len: number;
    dim_chunk_len: number;
    nitems: 1;
    constructor({ dim_sel, dim_len, dim_chunk_len }: IntDimIndexerProps);
    [Symbol.iterator](): IterableIterator<IntChunkDimProjection>;
}
interface SliceChunkDimProjection {
    dim_chunk_ix: number;
    dim_chunk_sel: Indices;
    dim_out_sel: Indices;
}
interface SliceDimIndexerProps {
    dim_sel: Slice;
    dim_len: number;
    dim_chunk_len: number;
}
declare class SliceDimIndexer {
    start: number;
    stop: number;
    step: number;
    dim_len: number;
    dim_chunk_len: number;
    nitems: number;
    nchunks: number;
    constructor({ dim_sel, dim_len, dim_chunk_len }: SliceDimIndexerProps);
    [Symbol.iterator](): IterableIterator<SliceChunkDimProjection>;
}
export declare function normalize_selection(selection: null | (Slice | null | number)[], shape: readonly number[]): (number | Slice)[];
interface BasicIndexerProps {
    selection: null | (null | number | Slice)[];
    shape: readonly number[];
    chunk_shape: readonly number[];
}
export type IndexerProjection = {
    from: number;
    to: null;
} | {
    from: Indices;
    to: Indices;
};
interface ChunkProjection {
    chunk_coords: number[];
    mapping: IndexerProjection[];
}
export declare class BasicIndexer {
    dim_indexers: (SliceDimIndexer | IntDimIndexer)[];
    shape: number[];
    constructor({ selection, shape, chunk_shape }: BasicIndexerProps);
    [Symbol.iterator](): IterableIterator<ChunkProjection>;
}
export {};
