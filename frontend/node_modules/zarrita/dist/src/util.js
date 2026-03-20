import { BoolArray, ByteStringArray, UnicodeStringArray, } from "./typedarray.js";
export function json_encode_object(o) {
    const str = JSON.stringify(o, null, 2);
    return new TextEncoder().encode(str);
}
export function json_decode_object(bytes) {
    const str = new TextDecoder().decode(bytes);
    return JSON.parse(str);
}
export function byteswap_inplace(view, bytes_per_element) {
    const numFlips = bytes_per_element / 2;
    const endByteIndex = bytes_per_element - 1;
    let t = 0;
    for (let i = 0; i < view.length; i += bytes_per_element) {
        for (let j = 0; j < numFlips; j += 1) {
            t = view[i + j];
            view[i + j] = view[i + endByteIndex - j];
            view[i + endByteIndex - j] = t;
        }
    }
}
export function get_ctr(data_type) {
    if (data_type === "v2:object") {
        return globalThis.Array;
    }
    let match = data_type.match(/v2:([US])(\d+)/);
    if (match) {
        let [, kind, chars] = match;
        // @ts-expect-error
        return (kind === "U" ? UnicodeStringArray : ByteStringArray).bind(null, Number(chars));
    }
    // Handle v3 variable-length string type
    if (data_type === "string") {
        return globalThis.Array;
    }
    // @ts-expect-error - We've checked that the key exists
    let ctr = {
        int8: Int8Array,
        int16: Int16Array,
        int32: Int32Array,
        int64: globalThis.BigInt64Array,
        uint8: Uint8Array,
        uint16: Uint16Array,
        uint32: Uint32Array,
        uint64: globalThis.BigUint64Array,
        float16: globalThis.Float16Array,
        float32: Float32Array,
        float64: Float64Array,
        bool: BoolArray,
    }[data_type];
    assert(ctr, `Unknown or unsupported data_type: ${data_type}`);
    return ctr;
}
/** Compute strides for 'C' or 'F' ordered array from shape */
export function get_strides(shape, order) {
    const rank = shape.length;
    if (typeof order === "string") {
        order =
            order === "C"
                ? Array.from({ length: rank }, (_, i) => i) // Row-major (identity order)
                : Array.from({ length: rank }, (_, i) => rank - 1 - i); // Column-major (reverse order)
    }
    assert(rank === order.length, "Order length must match the number of dimensions.");
    let step = 1;
    let stride = new Array(rank);
    for (let i = order.length - 1; i >= 0; i--) {
        stride[order[i]] = step;
        step *= shape[order[i]];
    }
    return stride;
}
// https://zarr-specs.readthedocs.io/en/latest/v3/core/v3.0.html#chunk-key-encoding
export function create_chunk_key_encoder({ name, configuration, }) {
    if (name === "default") {
        const separator = configuration?.separator ?? "/";
        return (chunk_coords) => ["c", ...chunk_coords].join(separator);
    }
    if (name === "v2") {
        const separator = configuration?.separator ?? ".";
        return (chunk_coords) => chunk_coords.join(separator) || "0";
    }
    throw new Error(`Unknown chunk key encoding: ${name}`);
}
function coerce_dtype(dtype) {
    if (dtype === "|O") {
        return { data_type: "v2:object" };
    }
    let match = dtype.match(/^([<|>])(.*)$/);
    assert(match, `Invalid dtype: ${dtype}`);
    let [, endian, rest] = match;
    let data_type = {
        b1: "bool",
        i1: "int8",
        u1: "uint8",
        i2: "int16",
        u2: "uint16",
        i4: "int32",
        u4: "uint32",
        i8: "int64",
        u8: "uint64",
        f2: "float16",
        f4: "float32",
        f8: "float64",
    }[rest] ??
        (rest.startsWith("S") || rest.startsWith("U") ? `v2:${rest}` : undefined);
    assert(data_type, `Unsupported or unknown dtype: ${dtype}`);
    if (endian === "|") {
        return { data_type };
    }
    return { data_type, endian: endian === "<" ? "little" : "big" };
}
export function v2_to_v3_array_metadata(meta, attributes = {}) {
    let codecs = [];
    let dtype = coerce_dtype(meta.dtype);
    if (meta.order === "F") {
        codecs.push({ name: "transpose", configuration: { order: "F" } });
    }
    if ("endian" in dtype && dtype.endian === "big") {
        codecs.push({ name: "bytes", configuration: { endian: "big" } });
    }
    for (let { id, ...configuration } of meta.filters ?? []) {
        codecs.push({ name: id, configuration });
    }
    if (meta.compressor) {
        let { id, ...configuration } = meta.compressor;
        codecs.push({ name: id, configuration });
    }
    return {
        zarr_format: 3,
        node_type: "array",
        shape: meta.shape,
        data_type: dtype.data_type,
        chunk_grid: {
            name: "regular",
            configuration: {
                chunk_shape: meta.chunks,
            },
        },
        chunk_key_encoding: {
            name: "v2",
            configuration: {
                separator: meta.dimension_separator ?? ".",
            },
        },
        codecs,
        fill_value: meta.fill_value,
        attributes,
    };
}
export function v2_to_v3_group_metadata(_meta, attributes = {}) {
    return {
        zarr_format: 3,
        node_type: "group",
        attributes,
    };
}
export function is_dtype(dtype, query) {
    if (query !== "number" &&
        query !== "bigint" &&
        query !== "boolean" &&
        query !== "object" &&
        query !== "string") {
        return dtype === query;
    }
    let is_boolean = dtype === "bool";
    if (query === "boolean")
        return is_boolean;
    let is_string = dtype.startsWith("v2:U") || dtype.startsWith("v2:S") || dtype === "string";
    if (query === "string")
        return is_string;
    let is_bigint = dtype === "int64" || dtype === "uint64";
    if (query === "bigint")
        return is_bigint;
    let is_object = dtype === "v2:object";
    if (query === "object")
        return is_object;
    return !is_string && !is_bigint && !is_boolean && !is_object;
}
export function is_sharding_codec(codec) {
    return codec?.name === "sharding_indexed";
}
export function ensure_correct_scalar(metadata) {
    if ((metadata.data_type === "uint64" || metadata.data_type === "int64") &&
        metadata.fill_value != null) {
        // @ts-expect-error - We've narrowed the type of fill_value correctly
        return BigInt(metadata.fill_value);
    }
    return metadata.fill_value;
}
/**
 * Ensures an error matches expected type(s), otherwise rethrows.
 *
 * Unmatched errors bubble up, like Python's `except`. Narrows error types for
 * type-safe property access.
 *
 * @see {@link https://gist.github.com/manzt/3702f19abb714e21c22ce48851c75abf}
 *
 * @example
 * ```ts
 * class DatabaseError extends Error { }
 * class NetworkError extends Error { }
 *
 * try {
 *   await db.query();
 * } catch (err) {
 *   rethrow_unless(err, DatabaseError, NetworkError);
 *   err // DatabaseError | NetworkError
 * }
 * ```
 *
 * @param error - The error to check
 * @param errors - Expected error type(s)
 * @throws The original error if it doesn't match expected type(s)
 */
export function rethrow_unless(error, ...errors) {
    if (!errors.some((ErrorClass) => error instanceof ErrorClass)) {
        throw error;
    }
}
/**
 * Make an assertion.
 *
 * Usage
 * @example
 * ```ts
 * const value: boolean = Math.random() <= 0.5;
 * assert(value, "value is greater than than 0.5!");
 * value // true
 * ```
 *
 * @param expression - The expression to test.
 * @param msg - The optional message to display if the assertion fails.
 * @throws an {@link Error} if `expression` is not truthy.
 */
export function assert(expression, msg = "") {
    if (!expression) {
        throw new Error(msg);
    }
}
/**
 * @param {ArrayBuffer |ArrayBufferView | Response} data
 * @param {Object} options
 * @param {CompressionFormat} options.format
 * @param {AbortSignal} [options.signal]
 *
 * @returns {Promise<ArrayBuffer>}
 */
export async function decompress(data, { format, signal }) {
    const response = data instanceof Response ? data : new Response(data);
    assert(response.body, "Response does not contain body.");
    try {
        const decompressedResponse = new Response(response.body.pipeThrough(new DecompressionStream(format), { signal }));
        const buffer = await decompressedResponse.arrayBuffer();
        return buffer;
    }
    catch {
        signal?.throwIfAborted();
        throw new Error(`Failed to decode ${format}`);
    }
}
//# sourceMappingURL=util.js.map