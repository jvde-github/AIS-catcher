import type { ChunkQueue, Indices, Slice } from "./types.js";
/** Similar to python's `range` function. Supports positive ranges only. */
export declare function range(start: number, stop?: number, step?: number): Iterable<number>;
/**
 * python-like itertools.product generator
 * https://gist.github.com/cybercase/db7dde901d7070c98c48
 */
export declare function product<T extends Array<Iterable<unknown>>>(...iterables: T): IterableIterator<{
    [K in keyof T]: T[K] extends Iterable<infer U> ? U : never;
}>;
export declare function slice_indices({ start, stop, step }: Slice, length: number): Indices;
/** @category Utilty */
export declare function slice(stop: number | null): Slice;
export declare function slice(start: number | null, stop?: number | null, step?: number | null): Slice;
/** Built-in "queue" for awaiting promises. */
export declare function create_queue(): ChunkQueue;
