/**
 * @typedef {Object} Slice
 * @property {number} offset
 * @property {number} length
 */
/** @typedef {Slice & {data: ArrayBufferLike}} SliceWithData */
export class BaseSource {
    /**
     * @param {Array<Slice>} slices
     * @param {AbortSignal} [signal]
     * @returns {Promise<ArrayBufferLike[]>}
     */
    fetch(slices: Array<Slice>, signal?: AbortSignal): Promise<ArrayBufferLike[]>;
    /**
     * @param {Slice} slice
     * @param {AbortSignal} [_signal]
     * @returns {Promise<SliceWithData>}
     */
    fetchSlice(slice: Slice, _signal?: AbortSignal): Promise<SliceWithData>;
    /**
     * Returns the filesize if already determined and null otherwise
     * @returns {number|null}
     */
    get fileSize(): number | null;
    close(): Promise<void>;
}
export type Slice = {
    offset: number;
    length: number;
};
export type SliceWithData = Slice & {
    data: ArrayBufferLike;
};
//# sourceMappingURL=basesource.d.ts.map