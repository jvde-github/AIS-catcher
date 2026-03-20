/**
 * Custom array-like views (i.e., TypedArrays) for Zarr binary data buffers.
 *
 * @module
 */
/**
 * An array-like view of a fixed-length boolean buffer.
 *
 * Encoded as 1 byte per value.
 */
export declare class BoolArray {
    #private;
    constructor(size: number);
    constructor(arr: Iterable<boolean>);
    constructor(buffer: ArrayBuffer, byteOffset?: number, length?: number);
    get BYTES_PER_ELEMENT(): 1;
    get byteOffset(): number;
    get byteLength(): number;
    get buffer(): ArrayBuffer;
    get length(): number;
    get(idx: number): boolean;
    set(idx: number, value: boolean): void;
    fill(value: boolean): void;
    [Symbol.iterator](): IterableIterator<boolean>;
}
/**
 * An array-like view of a fixed-length byte buffer.
 *
 * Encodes a raw byte sequences without enforced encoding.
 */
export declare class ByteStringArray {
    #private;
    _data: Uint8Array;
    chars: number;
    constructor(chars: number, size: number);
    constructor(chars: number, buffer: ArrayBuffer, byteOffset?: number, length?: number);
    constructor(chars: number, arr: Iterable<string>);
    get BYTES_PER_ELEMENT(): number;
    get byteOffset(): number;
    get byteLength(): number;
    get buffer(): ArrayBuffer;
    get length(): number;
    get(idx: number): string;
    set(idx: number, value: string): void;
    fill(value: string): void;
    [Symbol.iterator](): IterableIterator<string>;
}
/**
 * An array-like view of a fixed-length Unicode string buffer.
 *
 * Encoded as UTF-32 code points.
 */
export declare class UnicodeStringArray {
    #private;
    chars: number;
    constructor(chars: number, size: number);
    constructor(chars: number, buffer: ArrayBuffer, byteOffset?: number, length?: number);
    constructor(chars: number, arr: Iterable<string>);
    get BYTES_PER_ELEMENT(): number;
    get byteLength(): number;
    get byteOffset(): number;
    get buffer(): ArrayBuffer;
    get length(): number;
    get(idx: number): string;
    set(idx: number, value: string): void;
    fill(value: string): void;
    [Symbol.iterator](): IterableIterator<string>;
}
