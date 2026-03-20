import { assert } from "../util.js";
/**
 * A codec for bit-rounding.
 *
 * Reduces floating-point precision by truncating mantissa bits during encoding.
 * Decoding is a no-op as the process is lossy and precision cannot be restored.
 *
 * Note: {@link BitroundCodec.encode} is not yet implemented since Zarrita is
 * primarily used in read-only contexts (web browser). If you need encoding support,
 * please open an issue at {@link https://github.com/manzt/zarrita.js/issues}.
 *
 * @see {@link https://github.com/zarr-developers/numcodecs/blob/main/numcodecs/bitround.py}
 * for the original Python implementation.
 *
 * @remarks
 * Data types are not validated, and `float16` arrays are not supported (reflecting browser support).
 */
export class BitroundCodec {
    kind = "array_to_array";
    constructor(configuration, _meta) {
        assert(configuration.keepbits >= 0, "keepbits must be zero or positive");
    }
    static fromConfig(configuration, meta) {
        return new BitroundCodec(configuration, meta);
    }
    /**
     * Encode a chunk of data with bit-rounding.
     * @param _arr - The chunk to encode
     */
    encode(_arr) {
        throw new Error("`BitroundCodec.encode` is not implemented. Please open an issue at https://github.com/manzt/zarrita.js/issues.");
    }
    /**
     * Decode a chunk of data (no-op).
     * @param arr - The chunk to decode
     * @returns The decoded chunk
     */
    decode(arr) {
        return arr; // No-op as bit-rounding is lossy
    }
}
//# sourceMappingURL=bitround.js.map