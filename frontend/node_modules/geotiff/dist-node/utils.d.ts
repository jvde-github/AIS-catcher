export function assign(target: any, source: any): any;
export function chunk(iterable: any, length: any): any[][];
export function endsWith(string: any, expectedEnding: any): boolean;
export function forEach(iterable: any, func: any): void;
export function invert(oldObj: any): {};
export function range(n: any): number[];
export function times(numTimes: any, func: any): any[];
export function toArray(iterable: any): any[];
export function toArrayRecursively(input: any): any;
/** Copied from https://github.com/academia-de-codigo/parse-content-range-header/blob/master/index.js */
export function parseContentRange(headerValue: any): {
    unit: string | null;
    first: number;
    last: number;
    length: number | null;
} | {
    unit: string | null;
    first: null;
    last: null;
    length: number | null;
} | null;
/**
 * Promisified wrapper around 'setTimeout' to allow 'await'
 * @param {number} [milliseconds]
 * @returns {Promise<void>}
 */
export function wait(milliseconds?: number): Promise<void>;
export function zip(a: any, b: any): any[][];
export function isTypedFloatArray(input: any): boolean;
export function isTypedIntArray(input: any): boolean;
export function isTypedUintArray(input: any): boolean;
export class AbortError extends Error {
    constructor(...args: any[]);
    signal: any;
}
export class CustomAggregateError extends Error {
    constructor(errors: any, message: any);
    errors: any;
    message: any;
}
export const AggregateError: typeof CustomAggregateError;
export namespace typeMap {
    let Float64Array: Float64ArrayConstructor;
    let Float32Array: Float32ArrayConstructor;
    let Uint32Array: Uint32ArrayConstructor;
    let Uint16Array: Uint16ArrayConstructor;
    let Uint8Array: Uint8ArrayConstructor;
}
//# sourceMappingURL=utils.d.ts.map