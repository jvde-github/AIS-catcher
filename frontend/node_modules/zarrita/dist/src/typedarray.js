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
export class BoolArray {
    #bytes;
    constructor(x, byteOffset, length) {
        if (typeof x === "number") {
            this.#bytes = new Uint8Array(x);
        }
        else if (x instanceof ArrayBuffer) {
            this.#bytes = new Uint8Array(x, byteOffset, length);
        }
        else {
            this.#bytes = new Uint8Array(Array.from(x, (v) => (v ? 1 : 0)));
        }
    }
    get BYTES_PER_ELEMENT() {
        return 1;
    }
    get byteOffset() {
        return this.#bytes.byteOffset;
    }
    get byteLength() {
        return this.#bytes.byteLength;
    }
    get buffer() {
        return this.#bytes.buffer;
    }
    get length() {
        return this.#bytes.length;
    }
    get(idx) {
        let value = this.#bytes[idx];
        return typeof value === "number" ? value !== 0 : value;
    }
    set(idx, value) {
        this.#bytes[idx] = value ? 1 : 0;
    }
    fill(value) {
        this.#bytes.fill(value ? 1 : 0);
    }
    *[Symbol.iterator]() {
        for (let i = 0; i < this.length; i++) {
            yield this.get(i);
        }
    }
}
/**
 * An array-like view of a fixed-length byte buffer.
 *
 * Encodes a raw byte sequences without enforced encoding.
 */
export class ByteStringArray {
    _data;
    chars;
    #encoder;
    constructor(chars, x, byteOffset, length) {
        this.chars = chars;
        this.#encoder = new TextEncoder();
        if (typeof x === "number") {
            this._data = new Uint8Array(x * chars);
        }
        else if (x instanceof ArrayBuffer) {
            if (length)
                length = length * chars;
            this._data = new Uint8Array(x, byteOffset, length);
        }
        else {
            let values = Array.from(x);
            this._data = new Uint8Array(values.length * chars);
            for (let i = 0; i < values.length; i++) {
                this.set(i, values[i]);
            }
        }
    }
    get BYTES_PER_ELEMENT() {
        return this.chars;
    }
    get byteOffset() {
        return this._data.byteOffset;
    }
    get byteLength() {
        return this._data.byteLength;
    }
    get buffer() {
        return this._data.buffer;
    }
    get length() {
        return this.byteLength / this.BYTES_PER_ELEMENT;
    }
    get(idx) {
        const view = new Uint8Array(this.buffer, this.byteOffset + this.chars * idx, this.chars);
        // biome-ignore lint/suspicious/noControlCharactersInRegex: necessary for null byte removal
        return new TextDecoder().decode(view).replace(/\x00/g, "");
    }
    set(idx, value) {
        const view = new Uint8Array(this.buffer, this.byteOffset + this.chars * idx, this.chars);
        view.fill(0); // clear current
        view.set(this.#encoder.encode(value));
    }
    fill(value) {
        const encoded = this.#encoder.encode(value);
        for (let i = 0; i < this.length; i++) {
            this._data.set(encoded, i * this.chars);
        }
    }
    *[Symbol.iterator]() {
        for (let i = 0; i < this.length; i++) {
            yield this.get(i);
        }
    }
}
/**
 * An array-like view of a fixed-length Unicode string buffer.
 *
 * Encoded as UTF-32 code points.
 */
export class UnicodeStringArray {
    #data;
    chars;
    constructor(chars, x, byteOffset, length) {
        this.chars = chars;
        if (typeof x === "number") {
            this.#data = new Int32Array(x * chars);
        }
        else if (x instanceof ArrayBuffer) {
            if (length)
                length *= chars;
            this.#data = new Int32Array(x, byteOffset, length);
        }
        else {
            const values = x;
            const d = new UnicodeStringArray(chars, 1);
            this.#data = new Int32Array((function* () {
                for (let str of values) {
                    d.set(0, str);
                    yield* d.#data;
                }
            })());
        }
    }
    get BYTES_PER_ELEMENT() {
        return this.#data.BYTES_PER_ELEMENT * this.chars;
    }
    get byteLength() {
        return this.#data.byteLength;
    }
    get byteOffset() {
        return this.#data.byteOffset;
    }
    get buffer() {
        return this.#data.buffer;
    }
    get length() {
        return this.#data.length / this.chars;
    }
    get(idx) {
        const offset = this.chars * idx;
        let result = "";
        for (let i = 0; i < this.chars; i++) {
            result += String.fromCodePoint(this.#data[offset + i]);
        }
        // biome-ignore lint/suspicious/noControlCharactersInRegex: necessary for null byte removal
        return result.replace(/\u0000/g, "");
    }
    set(idx, value) {
        const offset = this.chars * idx;
        const view = this.#data.subarray(offset, offset + this.chars);
        view.fill(0); // clear current
        for (let i = 0; i < this.chars; i++) {
            view[i] = value.codePointAt(i) ?? 0;
        }
    }
    fill(value) {
        // encode once
        this.set(0, value);
        // copy the encoded values to all other elements
        let encoded = this.#data.subarray(0, this.chars);
        for (let i = 1; i < this.length; i++) {
            this.#data.set(encoded, i * this.chars);
        }
    }
    *[Symbol.iterator]() {
        for (let i = 0; i < this.length; i++) {
            yield this.get(i);
        }
    }
}
//# sourceMappingURL=typedarray.js.map