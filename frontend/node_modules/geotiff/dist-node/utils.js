"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.typeMap = exports.AggregateError = exports.CustomAggregateError = exports.AbortError = void 0;
exports.assign = assign;
exports.chunk = chunk;
exports.endsWith = endsWith;
exports.forEach = forEach;
exports.invert = invert;
exports.range = range;
exports.times = times;
exports.toArray = toArray;
exports.toArrayRecursively = toArrayRecursively;
exports.parseContentRange = parseContentRange;
exports.wait = wait;
exports.zip = zip;
exports.isTypedFloatArray = isTypedFloatArray;
exports.isTypedIntArray = isTypedIntArray;
exports.isTypedUintArray = isTypedUintArray;
function assign(target, source) {
    for (const key in source) {
        if (source.hasOwnProperty(key)) {
            target[key] = source[key];
        }
    }
    return target;
}
function chunk(iterable, length) {
    const results = [];
    const lengthOfIterable = iterable.length;
    for (let i = 0; i < lengthOfIterable; i += length) {
        const chunked = [];
        for (let ci = i; ci < i + length; ci++) {
            chunked.push(iterable[ci]);
        }
        results.push(chunked);
    }
    return results;
}
function endsWith(string, expectedEnding) {
    if (string.length < expectedEnding.length) {
        return false;
    }
    const actualEnding = string.substr(string.length - expectedEnding.length);
    return actualEnding === expectedEnding;
}
function forEach(iterable, func) {
    const { length } = iterable;
    for (let i = 0; i < length; i++) {
        func(iterable[i], i);
    }
}
function invert(oldObj) {
    const newObj = {};
    for (const key in oldObj) {
        if (oldObj.hasOwnProperty(key)) {
            const value = oldObj[key];
            newObj[value] = key;
        }
    }
    return newObj;
}
function range(n) {
    const results = [];
    for (let i = 0; i < n; i++) {
        results.push(i);
    }
    return results;
}
function times(numTimes, func) {
    const results = [];
    for (let i = 0; i < numTimes; i++) {
        results.push(func(i));
    }
    return results;
}
function toArray(iterable) {
    const results = [];
    const { length } = iterable;
    for (let i = 0; i < length; i++) {
        results.push(iterable[i]);
    }
    return results;
}
function toArrayRecursively(input) {
    if (isArrayLike(input)) {
        return toArray(input).map(toArrayRecursively);
    }
    return input;
}
function isArrayLike(v) {
    return Boolean(typeof v === 'object' && v !== null && 'length' in v);
}
/** Copied from https://github.com/academia-de-codigo/parse-content-range-header/blob/master/index.js */
function parseContentRange(headerValue) {
    if (!headerValue) {
        return null;
    }
    if (typeof headerValue !== 'string') {
        throw new Error('invalid argument');
    }
    const parseInt = (number) => Number.parseInt(number, 10);
    // Check for presence of unit
    let matches = headerValue.match(/^(\w*) /);
    const unit = matches && matches[1];
    // check for start-end/size header format
    matches = headerValue.match(/(\d+)-(\d+)\/(\d+|\*)/);
    if (matches) {
        return {
            unit,
            first: parseInt(matches[1]),
            last: parseInt(matches[2]),
            length: matches[3] === '*' ? null : parseInt(matches[3]),
        };
    }
    // check for size header format
    matches = headerValue.match(/(\d+|\*)/);
    if (matches) {
        return {
            unit,
            first: null,
            last: null,
            length: matches[1] === '*' ? null : parseInt(matches[1]),
        };
    }
    return null;
}
/**
 * Promisified wrapper around 'setTimeout' to allow 'await'
 * @param {number} [milliseconds]
 * @returns {Promise<void>}
 */
async function wait(milliseconds) {
    return new Promise((resolve) => setTimeout(resolve, milliseconds));
}
function zip(a, b) {
    const A = Array.isArray(a) ? a : Array.from(a);
    const B = Array.isArray(b) ? b : Array.from(b);
    return A.map((k, i) => [k, B[i]]);
}
// Based on https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Error
class AbortError extends Error {
    constructor(...args) {
        // Pass remaining arguments (including vendor specific ones) to parent constructor
        super(...args);
        // Maintains proper stack trace for where our error was thrown (only available on V8)
        if (Error.captureStackTrace) {
            Error.captureStackTrace(this, AbortError);
        }
        this.name = 'AbortError';
        this.signal = undefined;
    }
}
exports.AbortError = AbortError;
class CustomAggregateError extends Error {
    constructor(errors, message) {
        super(message);
        this.errors = errors;
        this.message = message;
        this.name = 'AggregateError';
    }
}
exports.CustomAggregateError = CustomAggregateError;
exports.AggregateError = CustomAggregateError;
function isTypedFloatArray(input) {
    if (ArrayBuffer.isView(input)) {
        const ctr = input.constructor;
        if (ctr === Float32Array || ctr === Float64Array) {
            return true;
        }
    }
    return false;
}
function isTypedIntArray(input) {
    if (ArrayBuffer.isView(input)) {
        const ctr = input.constructor;
        if (ctr === Int8Array || ctr === Int16Array || ctr === Int32Array) {
            return true;
        }
    }
    return false;
}
function isTypedUintArray(input) {
    if (ArrayBuffer.isView(input)) {
        const ctr = input.constructor;
        if (ctr === Uint8Array ||
            ctr === Uint16Array ||
            ctr === Uint32Array ||
            ctr === Uint8ClampedArray) {
            return true;
        }
    }
    return false;
}
exports.typeMap = {
    Float64Array,
    Float32Array,
    Uint32Array,
    Uint16Array,
    Uint8Array,
};
//# sourceMappingURL=utils.js.map
//# sourceMappingURL=utils.js.map