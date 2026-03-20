import { BoolArray, ByteStringArray, UnicodeStringArray, } from "../typedarray.js";
import { assert, get_strides } from "../util.js";
function proxy(arr) {
    if (arr instanceof BoolArray ||
        arr instanceof ByteStringArray ||
        arr instanceof UnicodeStringArray) {
        // @ts-expect-error - TS cannot infer arr is a TypedArrayProxy<D>
        const arrp = new Proxy(arr, {
            get(target, prop) {
                return target.get(Number(prop));
            },
            set(target, prop, value) {
                // @ts-expect-error - value is OK
                target.set(Number(prop), value);
                return true;
            },
        });
        return arrp;
    }
    // @ts-expect-error - TS cannot infer arr is a TypedArrayProxy<D>
    return arr;
}
function empty_like(chunk, order) {
    let data;
    if (chunk.data instanceof ByteStringArray ||
        chunk.data instanceof UnicodeStringArray) {
        data = new chunk.constructor(
        // @ts-expect-error
        chunk.data.length, chunk.data.chars);
    }
    else {
        data = new chunk.constructor(chunk.data.length);
    }
    return {
        data,
        shape: chunk.shape,
        stride: get_strides(chunk.shape, order),
    };
}
function convert_array_order(src, target) {
    let out = empty_like(src, target);
    let n_dims = src.shape.length;
    let size = src.data.length;
    let index = Array(n_dims).fill(0);
    let src_data = proxy(src.data);
    let out_data = proxy(out.data);
    for (let src_idx = 0; src_idx < size; src_idx++) {
        let out_idx = 0;
        for (let dim = 0; dim < n_dims; dim++) {
            out_idx += index[dim] * out.stride[dim];
        }
        out_data[out_idx] = src_data[src_idx];
        index[0] += 1;
        for (let dim = 0; dim < n_dims; dim++) {
            if (index[dim] === src.shape[dim]) {
                if (dim + 1 === n_dims) {
                    break;
                }
                index[dim] = 0;
                index[dim + 1] += 1;
            }
        }
    }
    return out;
}
/** Determine the memory order (axis permutation) for a chunk */
function get_order(chunk) {
    let rank = chunk.shape.length;
    assert(rank === chunk.stride.length, "Shape and stride must have the same length.");
    return chunk.stride
        .map((s, i) => ({ stride: s, index: i }))
        .sort((a, b) => b.stride - a.stride)
        .map((entry) => entry.index);
}
function matches_order(chunk, target) {
    let source = get_order(chunk);
    assert(source.length === target.length, "Orders must match");
    return source.every((dim, i) => dim === target[i]);
}
export class TransposeCodec {
    kind = "array_to_array";
    #order;
    #inverseOrder;
    constructor(configuration, meta) {
        let value = configuration.order ?? "C";
        let rank = meta.shape.length;
        let order = new Array(rank);
        let inverseOrder = new Array(rank);
        if (value === "C") {
            for (let i = 0; i < rank; ++i) {
                order[i] = i;
                inverseOrder[i] = i;
            }
        }
        else if (value === "F") {
            for (let i = 0; i < rank; ++i) {
                order[i] = rank - i - 1;
                inverseOrder[i] = rank - i - 1;
            }
        }
        else {
            order = value;
            order.forEach((x, i) => {
                assert(inverseOrder[x] === undefined, `Invalid permutation: ${JSON.stringify(value)}`);
                inverseOrder[x] = i;
            });
        }
        this.#order = order;
        this.#inverseOrder = inverseOrder;
    }
    static fromConfig(configuration, meta) {
        return new TransposeCodec(configuration, meta);
    }
    encode(arr) {
        if (matches_order(arr, this.#inverseOrder)) {
            // can skip making a copy
            return arr;
        }
        return convert_array_order(arr, this.#inverseOrder);
    }
    decode(arr) {
        return {
            data: arr.data,
            shape: arr.shape,
            stride: get_strides(arr.shape, this.#order),
        };
    }
}
//# sourceMappingURL=transpose.js.map