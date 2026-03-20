/**
 * @param {Array<number>|Array<Array<Array<number>>>|import('./geotiff.js').TypedArray} data
 * @param {GeotiffWriterMetadata} metadata
 * @returns {ArrayBuffer}
 */
export function writeGeotiff(data: Array<number> | Array<Array<Array<number>>> | import("./geotiff.js").TypedArray, metadata: GeotiffWriterMetadata): ArrayBuffer;
export type Read = (buff: Uint8Array, p: number) => number;
export type BinBE = {
    nextZero: (data: Uint8Array, o: number) => number;
    readUshort: Read;
    readShort: Read;
    readInt: Read;
    readUint: Read;
    readASCII: (buff: Uint8Array, p: number, l: Array<number>) => string;
    readFloat: Read;
    readDouble: Read;
    writeUshort: (buff: Uint8Array, p: number, n: number) => void;
    writeUint: (buff: Uint8Array, p: number, n: number) => void;
    writeASCII: (buff: Uint8Array, p: number, s: string) => void;
    ui8: Uint8Array;
    fl64: Float64Array;
    fl32: Float32Array;
    ui32: Uint32Array;
    i32: Int32Array;
    i16: Int16Array;
    writeDouble: (buff: Uint8Array, p: number, n: number) => void;
};
import type { GeotiffWriterMetadata } from './geotiff.js';
//# sourceMappingURL=geotiffwriter.d.ts.map