import { get as get_with_setter } from "./get.js";
import { set as set_with_setter } from "./set.js";
/** A 1D "view" of an array that can be used to set values in the array. */
function object_array_view(arr, offset = 0, size) {
    let length = size ?? arr.length - offset;
    return {
        length,
        subarray(from, to = length) {
            return object_array_view(arr, offset + from, to - from);
        },
        set(data, start = 0) {
            for (let i = 0; i < data.length; i++) {
                arr[offset + start + i] = data.get(i);
            }
        },
        get(index) {
            return arr[offset + index];
        },
    };
}
/**
 * Convert a chunk to a Uint8Array that can be used with the binary
 * set functions. This is necessary because the binary set functions
 * require a contiguous block of memory, and allows us to support more than
 * just the browser's TypedArray objects.
 *
 * WARNING: This function is not meant to be used directly and is NOT type-safe.
 * In the case of `Array` instances, it will return a `object_array_view` of
 * the underlying, which is supported by our binary set functions.
 */
function compat_chunk(arr) {
    if (globalThis.Array.isArray(arr.data)) {
        return {
            // @ts-expect-error
            data: object_array_view(arr.data),
            stride: arr.stride,
            bytes_per_element: 1,
        };
    }
    return {
        data: new Uint8Array(arr.data.buffer, arr.data.byteOffset, arr.data.byteLength),
        stride: arr.stride,
        bytes_per_element: arr.data.BYTES_PER_ELEMENT,
    };
}
/** Hack to get the constructor of a typed array constructor from an existing TypedArray. */
function get_typed_array_constructor(arr) {
    if ("chars" in arr) {
        // our custom TypedArray needs to bind the number of characters per
        // element to the constructor.
        return arr.constructor.bind(null, arr.chars);
    }
    return arr.constructor;
}
/**
 * Convert a scalar to a Uint8Array that can be used with the binary
 * set functions. This is necessary because the binary set functions
 * require a contiguous block of memory, and allows us to support more
 * than just the browser's TypedArray objects.
 *
 * WARNING: This function is not meant to be used directly and is NOT type-safe.
 * In the case of `Array` instances, it will return a `object_array_view` of
 * the scalar, which is supported by our binary set functions.
 */
function compat_scalar(arr, value) {
    if (globalThis.Array.isArray(arr.data)) {
        // @ts-expect-error
        return object_array_view([value]);
    }
    let TypedArray = get_typed_array_constructor(arr.data);
    // @ts-expect-error - value is a scalar and matches
    let data = new TypedArray([value]);
    return new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
}
export const setter = {
    prepare(data, shape, stride) {
        return { data, shape, stride };
    },
    set_scalar(dest, sel, value) {
        let view = compat_chunk(dest);
        set_scalar_binary(view, sel, compat_scalar(dest, value), view.bytes_per_element);
    },
    set_from_chunk(dest, src, projections) {
        let view = compat_chunk(dest);
        set_from_chunk_binary(view, compat_chunk(src), view.bytes_per_element, projections);
    },
};
/** @category Utility */
export async function get(arr, selection = null, opts = {}) {
    return get_with_setter(arr, selection, opts, setter);
}
/** @category Utility */
export async function set(arr, selection, value, opts = {}) {
    return set_with_setter(arr, selection, value, opts, setter);
}
function indices_len(start, stop, step) {
    if (step < 0 && stop < start) {
        return Math.floor((start - stop - 1) / -step) + 1;
    }
    if (start < stop)
        return Math.floor((stop - start - 1) / step) + 1;
    return 0;
}
function set_scalar_binary(out, out_selection, value, bytes_per_element) {
    if (out_selection.length === 0) {
        out.data.set(value, 0);
        return;
    }
    const [slice, ...slices] = out_selection;
    const [curr_stride, ...stride] = out.stride;
    if (typeof slice === "number") {
        const data = out.data.subarray(curr_stride * slice * bytes_per_element);
        set_scalar_binary({ data, stride }, slices, value, bytes_per_element);
        return;
    }
    const [from, to, step] = slice;
    const len = indices_len(from, to, step);
    if (slices.length === 0) {
        for (let i = 0; i < len; i++) {
            out.data.set(value, curr_stride * (from + step * i) * bytes_per_element);
        }
        return;
    }
    for (let i = 0; i < len; i++) {
        const data = out.data.subarray(curr_stride * (from + step * i) * bytes_per_element);
        set_scalar_binary({ data, stride }, slices, value, bytes_per_element);
    }
}
function set_from_chunk_binary(dest, src, bytes_per_element, projections) {
    const [proj, ...projs] = projections;
    const [dstride, ...dstrides] = dest.stride;
    const [sstride, ...sstrides] = src.stride;
    if (proj.from === null) {
        if (projs.length === 0) {
            dest.data.set(src.data.subarray(0, bytes_per_element), proj.to * bytes_per_element);
            return;
        }
        set_from_chunk_binary({
            data: dest.data.subarray(dstride * proj.to * bytes_per_element),
            stride: dstrides,
        }, src, bytes_per_element, projs);
        return;
    }
    if (proj.to === null) {
        if (projs.length === 0) {
            let offset = proj.from * bytes_per_element;
            dest.data.set(src.data.subarray(offset, offset + bytes_per_element), 0);
            return;
        }
        set_from_chunk_binary(dest, {
            data: src.data.subarray(sstride * proj.from * bytes_per_element),
            stride: sstrides,
        }, bytes_per_element, projs);
        return;
    }
    const [from, to, step] = proj.to;
    const [sfrom, _, sstep] = proj.from;
    const len = indices_len(from, to, step);
    if (projs.length === 0) {
        // NB: we have a contiguous block of memory
        // so we can just copy over all the data at once.
        if (step === 1 && sstep === 1 && dstride === 1 && sstride === 1) {
            let offset = sfrom * bytes_per_element;
            let size = len * bytes_per_element;
            dest.data.set(src.data.subarray(offset, offset + size), from * bytes_per_element);
            return;
        }
        // Otherwise, we have to copy over each element individually.
        for (let i = 0; i < len; i++) {
            let offset = sstride * (sfrom + sstep * i) * bytes_per_element;
            dest.data.set(src.data.subarray(offset, offset + bytes_per_element), dstride * (from + step * i) * bytes_per_element);
        }
        return;
    }
    for (let i = 0; i < len; i++) {
        set_from_chunk_binary({
            data: dest.data.subarray(dstride * (from + i * step) * bytes_per_element),
            stride: dstrides,
        }, {
            data: src.data.subarray(sstride * (sfrom + i * sstep) * bytes_per_element),
            stride: sstrides,
        }, bytes_per_element, projs);
    }
}
//# sourceMappingURL=ops.js.map