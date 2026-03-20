import { byteswap_inplace, get_ctr, get_strides } from "../util.js";
const LITTLE_ENDIAN_OS = system_is_little_endian();
function system_is_little_endian() {
    const a = new Uint32Array([0x12345678]);
    const b = new Uint8Array(a.buffer, a.byteOffset, a.byteLength);
    return !(b[0] === 0x12);
}
function bytes_per_element(TypedArray) {
    if ("BYTES_PER_ELEMENT" in TypedArray) {
        return TypedArray.BYTES_PER_ELEMENT;
    }
    // Unicode string array is backed by a Int32Array.
    return 4;
}
export class BytesCodec {
    kind = "array_to_bytes";
    #stride;
    #TypedArray;
    #BYTES_PER_ELEMENT;
    #shape;
    #endian;
    constructor(configuration, meta) {
        this.#endian = configuration?.endian;
        this.#TypedArray = get_ctr(meta.data_type);
        this.#shape = meta.shape;
        this.#stride = get_strides(meta.shape, "C");
        // TODO: fix me.
        // hack to get bytes per element since it's dynamic for string types.
        const sample = new this.#TypedArray(0);
        this.#BYTES_PER_ELEMENT = sample.BYTES_PER_ELEMENT;
    }
    static fromConfig(configuration, meta) {
        return new BytesCodec(configuration, meta);
    }
    encode(arr) {
        let bytes = new Uint8Array(arr.data.buffer);
        if (LITTLE_ENDIAN_OS && this.#endian === "big") {
            byteswap_inplace(bytes, bytes_per_element(this.#TypedArray));
        }
        return bytes;
    }
    decode(bytes) {
        if (LITTLE_ENDIAN_OS && this.#endian === "big") {
            byteswap_inplace(bytes, bytes_per_element(this.#TypedArray));
        }
        return {
            data: new this.#TypedArray(bytes.buffer, bytes.byteOffset, bytes.byteLength / this.#BYTES_PER_ELEMENT),
            shape: this.#shape,
            stride: this.#stride,
        };
    }
}
//# sourceMappingURL=bytes.js.map