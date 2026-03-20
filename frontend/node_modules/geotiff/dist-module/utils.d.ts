export declare function assign<T extends Record<string, unknown>, S extends Record<string, unknown>>(target: T, source: S): T & S;
export declare function chunk<T>(iterable: ArrayLike<T>, length: number): T[][];
export declare function endsWith(string: string, expectedEnding: string): boolean;
export declare function forEach<T>(iterable: ArrayLike<T>, func: (item: T, index: number) => void): void;
export declare function invert<K extends string | number, V extends string | number>(oldObj: Record<K, V>): Record<V, K>;
export declare function range(n: number): number[];
export declare function times<T>(numTimes: number, func: (index: number) => T): T[];
export declare function toArray<T>(iterable: ArrayLike<T>): T[];
export declare function toArrayRecursively(input: unknown): unknown;
/** Copied from https://github.com/academia-de-codigo/parse-content-range-header/blob/master/index.js */
export declare function parseContentRange(headerValue: string): null | {
    unit: string | null;
    first: number | null;
    last: number | null;
    length: number | null;
};
/**
 * Promisified wrapper around 'setTimeout' to allow 'await'
 * @param {number} [milliseconds]
 * @returns {Promise<void>}
 */
export declare function wait(milliseconds?: number): Promise<void>;
export declare function zip<T, U>(a: Iterable<T>, b: Iterable<U>): [T, U][];
export declare class AbortError extends Error {
    signal?: AbortSignal;
    constructor(...args: any[]);
}
export declare class CustomAggregateError extends Error {
    errors: Error[];
    constructor(errors: Error[], message: string);
}
export declare const AggregateError: typeof CustomAggregateError;
export declare function isTypedFloatArray(input: unknown): input is Float32Array | Float64Array;
export declare function isTypedIntArray(input: unknown): input is Int8Array | Int16Array | Int32Array;
export declare function isTypedUintArray(input: unknown): input is Uint8Array | Uint16Array | Uint32Array | Uint8ClampedArray;
export declare const typeMap: {
    Float64Array: Float64ArrayConstructor;
    Float32Array: Float32ArrayConstructor;
    Uint32Array: Uint32ArrayConstructor;
    Uint16Array: Uint16ArrayConstructor;
    Uint8Array: Uint8ArrayConstructor;
};
//# sourceMappingURL=utils.d.ts.map