/**
 * Create a new source from a given file/blob.
 * @param {Blob} file The file or blob to read from.
 * @returns {FileReaderSource} The constructed source
 */
export function makeFileReaderSource(file: Blob): FileReaderSource;
declare class FileReaderSource extends BaseSource {
    /**
     * @param {Blob} file
     */
    constructor(file: Blob);
    file: Blob;
    /**
     * @param {import('./basesource.js').Slice} slice
     * @param {AbortSignal} signal
     * @returns {Promise<import('./basesource.js').SliceWithData>}
     */
    fetchSlice(slice: import("./basesource.js").Slice, signal: AbortSignal): Promise<import("./basesource.js").SliceWithData>;
}
import { BaseSource } from './basesource.js';
export {};
//# sourceMappingURL=filereader.d.ts.map