import { get_strides } from "../util.js";
export class VLenUTF8 {
    kind = "array_to_bytes";
    #shape;
    #strides;
    constructor(shape) {
        this.#shape = shape;
        this.#strides = get_strides(shape, "C");
    }
    static fromConfig(_, meta) {
        return new VLenUTF8(meta.shape);
    }
    encode(_chunk) {
        throw new Error("Method not implemented.");
    }
    decode(bytes) {
        let decoder = new TextDecoder();
        let view = new DataView(bytes.buffer);
        let data = Array(view.getUint32(0, true));
        let pos = 4;
        for (let i = 0; i < data.length; i++) {
            let item_length = view.getUint32(pos, true);
            pos += 4;
            data[i] = decoder.decode(bytes.buffer.slice(pos, pos + item_length));
            pos += item_length;
        }
        return { data, shape: this.#shape, stride: this.#strides };
    }
}
//# sourceMappingURL=vlen-utf8.js.map