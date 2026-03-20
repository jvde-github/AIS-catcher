import ndarray from "ndarray";
import { assign } from "ndarray-ops";
import { describe, expect, it } from "vitest";
import { setter } from "../../src/indexing/ops.js";
import { slice, slice_indices } from "../../src/indexing/util.js";
/** Compute strides for 'C' or 'F' ordered array from shape */
function get_strides(shape, order) {
    return (order === "C" ? row_major_stride : col_major_stride)(shape);
}
function row_major_stride(shape) {
    const ndim = shape.length;
    const stride = globalThis.Array(ndim);
    for (let i = ndim - 1, step = 1; i >= 0; i--) {
        stride[i] = step;
        step *= shape[i];
    }
    return stride;
}
function col_major_stride(shape) {
    const ndim = shape.length;
    const stride = globalThis.Array(ndim);
    for (let i = 0, step = 1; i < ndim; i++) {
        stride[i] = step;
        step *= shape[i];
    }
    return stride;
}
function to_c({ data, shape, stride, }) {
    let size = shape.reduce((a, b) => a * b, 1);
    // @ts-expect-error - We know constructor exists on TypedArray
    let out = ndarray(new data.constructor(size), shape);
    // @ts-expect-error - ndarray types are a mismatch with ours but this operation is safe
    assign(out, ndarray(data, shape, stride));
    return out;
}
function indices(size) {
    return slice_indices(slice(null), size);
}
describe("setter", () => {
    it("setter.set_scalar - fill", async () => {
        let a = setter.prepare(new Float32Array(2 * 3 * 4), [2, 3, 4], get_strides([2, 3, 4], "C"));
        let sel = [2, 3, 4].map(indices);
        setter.set_scalar(a, sel, 1);
        // biome-ignore format: the array should not be formatted
        expect(a.data).toStrictEqual(new Float32Array([
            1, 1, 1, 1,
            1, 1, 1, 1,
            1, 1, 1, 1,
            1, 1, 1, 1,
            1, 1, 1, 1,
            1, 1, 1, 1,
        ]));
    });
    it("setter.set_scalar - point", async () => {
        let a = setter.prepare(new Float32Array(2 * 3 * 4), [2, 3, 4], get_strides([2, 3, 4], "C"));
        setter.set_scalar(a, [0, 0, 0], 1);
        // biome-ignore format: the array should not be formatted
        expect(a.data).toStrictEqual(new Float32Array([
            1, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
        ]));
        setter.set_scalar(a, [1, 1, 1], 2);
        // biome-ignore format: the array should not be formatted
        expect(a.data).toStrictEqual(new Float32Array([
            1, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 2, 0, 0,
            0, 0, 0, 0,
        ]));
        setter.set_scalar(a, [1, 2, 3], 3);
        // biome-ignore format: the array should not be formatted
        expect(a.data).toStrictEqual(new Float32Array([
            1, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 2, 0, 0,
            0, 0, 0, 3,
        ]));
        setter.set_scalar(a, [1, 2, 2], 4);
        // biome-ignore format: the array should not be formatted
        expect(a.data).toStrictEqual(new Float32Array([
            1, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 2, 0, 0,
            0, 0, 4, 3,
        ]));
    });
    it("setter.set_scalar - mixed", async () => {
        let a = setter.prepare(new Float32Array(2 * 3 * 4), [2, 3, 4], get_strides([2, 3, 4], "C"));
        let sel = [indices(2), slice_indices(slice(2), 3), 0];
        setter.set_scalar(a, sel, 1);
        // biome-ignore format: the array should not be formatted
        expect(a.data).toStrictEqual(new Float32Array([
            1, 0, 0, 0,
            1, 0, 0, 0,
            0, 0, 0, 0,
            1, 0, 0, 0,
            1, 0, 0, 0,
            0, 0, 0, 0,
        ]));
        sel = [0, indices(3), indices(4)];
        setter.set_scalar(a, sel, 2);
        // biome-ignore format: the array should not be formatted
        expect(a.data).toStrictEqual(new Float32Array([
            2, 2, 2, 2,
            2, 2, 2, 2,
            2, 2, 2, 2,
            1, 0, 0, 0,
            1, 0, 0, 0,
            0, 0, 0, 0,
        ]));
    });
    it("setter.set_scalar - mixed F order", async () => {
        let f = setter.prepare(new Float32Array(2 * 3 * 4), [2, 3, 4], get_strides([2, 3, 4], "F"));
        let sel = [slice_indices(slice(null), 2), slice_indices(slice(2), 3), 0];
        setter.set_scalar(f, sel, 1);
        // biome-ignore format: the array should not be formatted
        expect(f.data).toStrictEqual(new Float32Array([
            1, 1, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
        ]));
        sel = [0, slice_indices(slice(null), 3), slice_indices(slice(null), 4)];
        setter.set_scalar(f, sel, 2);
        // biome-ignore format: the array should not be formatted
        expect(f.data).toStrictEqual(new Float32Array([
            2, 1, 2, 1, 2, 0,
            2, 0, 2, 0, 2, 0,
            2, 0, 2, 0, 2, 0,
            2, 0, 2, 0, 2, 0,
        ]));
        // biome-ignore format: the array should not be formatted
        expect(to_c(f).data).toStrictEqual(new Float32Array([
            2, 2, 2, 2,
            2, 2, 2, 2,
            2, 2, 2, 2,
            1, 0, 0, 0,
            1, 0, 0, 0,
            0, 0, 0, 0,
        ]));
    });
    it("set_from_chunk - complete", async () => {
        let dest = setter.prepare(new Float32Array(2 * 3 * 4), [2, 3, 4], get_strides([2, 3, 4], "C"));
        let src = setter.prepare(new Float32Array(2 * 2 * 2).fill(1), [2, 2, 2], get_strides([2, 2, 2], "C"));
        let mapping = [
            { from: [0, 2, 1], to: [0, 2, 1] },
            { from: [0, 2, 1], to: [0, 2, 1] },
            { from: [0, 2, 1], to: [0, 2, 1] },
        ];
        setter.set_from_chunk(dest, src, mapping);
        // biome-ignore format: the array should not be formatted
        expect(dest.data).toStrictEqual(new Float32Array([
            1, 1, 0, 0,
            1, 1, 0, 0,
            0, 0, 0, 0,
            1, 1, 0, 0,
            1, 1, 0, 0,
            0, 0, 0, 0,
        ]));
    });
    it("set_from_chunk - from complete to strided", async () => {
        let dest = setter.prepare(new Float32Array(2 * 3 * 4), [2, 3, 4], get_strides([2, 3, 4], "C"));
        let src = setter.prepare(new Float32Array(2 * 2 * 2).fill(2), [2, 2, 2], get_strides([2, 2, 2], "C"));
        let mapping = [
            { from: [0, 2, 1], to: [0, 2, 1] },
            { from: [0, 2, 1], to: [0, 3, 2] },
            { from: [0, 2, 1], to: [0, 4, 2] },
        ];
        setter.set_from_chunk(dest, src, mapping);
        // biome-ignore format: the array should not be formatted
        expect(dest.data).toStrictEqual(new Float32Array([
            2, 0, 2, 0,
            0, 0, 0, 0,
            2, 0, 2, 0,
            2, 0, 2, 0,
            0, 0, 0, 0,
            2, 0, 2, 0,
        ]));
    });
    it("set_from_chunk - from strided to complete", async () => {
        let dest = setter.prepare(new Float32Array(2 * 2 * 2), [2, 2, 2], get_strides([2, 2, 2], "C"));
        let src = setter.prepare(
        // biome-ignore format: the array should not be formatted
        new Float32Array([
            2, 0, 2, 0,
            0, 0, 0, 0,
            2, 0, 2, 0,
            2, 0, 2, 0,
            0, 0, 0, 0,
            2, 0, 2, 0,
        ]), [2, 3, 4], get_strides([2, 3, 4], "C"));
        let mapping = [
            { to: [0, 2, 1], from: [0, 2, 1] },
            { to: [0, 2, 1], from: [0, 3, 2] },
            { to: [0, 2, 1], from: [0, 4, 2] },
        ];
        setter.set_from_chunk(dest, src, mapping);
        expect(dest.data).toStrictEqual(new Float32Array(2 * 2 * 2).fill(2));
    });
    it("set_from_chunk - src squeezed", async () => {
        let dest = setter.prepare(new Float32Array(2 * 3 * 4), [2, 3, 4], get_strides([2, 3, 4], "C"));
        let src = setter.prepare(new Float32Array([2, 0, 0, 2]), [4], get_strides([4], "C"));
        let mapping = [
            { to: 0, from: null },
            { to: [0, 3, 2], from: [0, 4, 3] },
            { to: 1, from: null },
        ];
        setter.set_from_chunk(dest, src, mapping);
        // biome-ignore format: the array should not be formatted
        expect(dest.data).toStrictEqual(new Float32Array([
            0, 2, 0, 0,
            0, 0, 0, 0,
            0, 2, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
        ]));
    });
    it("set_from_chunk - dest squeezed", async () => {
        let dest = setter.prepare(new Float32Array(4), [4], get_strides([4], "C"));
        let src = setter.prepare(
        // biome-ignore format: the array should not be formatted
        new Float32Array([
            0, 2, 0, 0,
            0, 0, 0, 0,
            0, 2, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
        ]), [2, 3, 4], get_strides([2, 3, 4], "C"));
        let mapping = [
            { from: 0, to: null },
            { from: [0, 3, 2], to: [0, 4, 3] },
            { from: 1, to: null },
        ];
        setter.set_from_chunk(dest, src, mapping);
        expect(dest.data).toStrictEqual(new Float32Array([2, 0, 0, 2]));
    });
    it("set_from_chunk - complete F order", async () => {
        let dest = setter.prepare(new Float32Array(2 * 3 * 4), [2, 3, 4], get_strides([2, 3, 4], "F"));
        let src = setter.prepare(new Float32Array(2 * 2 * 2).fill(1), [2, 2, 2], get_strides([2, 2, 2], "F"));
        let mapping = [
            { from: [0, 2, 1], to: [0, 2, 1] },
            { from: [0, 2, 1], to: [0, 2, 1] },
            { from: [0, 2, 1], to: [0, 2, 1] },
        ];
        setter.set_from_chunk(dest, src, mapping);
        // biome-ignore format: the array should not be formatted
        expect(to_c(dest).data).toStrictEqual(new Float32Array([
            1, 1, 0, 0,
            1, 1, 0, 0,
            0, 0, 0, 0,
            1, 1, 0, 0,
            1, 1, 0, 0,
            0, 0, 0, 0,
        ]));
    });
    it.skip("set_from_chunk - F order", async () => {
        let dest = setter.prepare(new Float32Array(2 * 3 * 4), [2, 3, 4], get_strides([2, 3, 4], "F"));
        let src = setter.prepare(new Float32Array([2, 0, 0, 2]), [4], get_strides([4], "F"));
        let mapping = [
            { to: 0, from: null },
            { to: [0, 3, 2], from: [0, 4, 3] },
            { to: 1, from: null },
        ];
        setter.set_from_chunk(dest, src, mapping);
        // biome-ignore format: the array should not be formatted
        expect(to_c(dest).data).toStrictEqual(new Float32Array([
            0, 2, 0, 0,
            0, 0, 0, 0,
            0, 2, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
        ]));
    });
    it.skip("set_from_chunk - dest=F order, src=C order", async () => {
        let dest = setter.prepare(new Float32Array(2 * 3 * 4), [2, 3, 4], get_strides([2, 3, 4], "F"));
        let src = setter.prepare(new Float32Array([2, 0, 0, 2]), [4], get_strides([4], "C"));
        let mapping = [
            { to: 0, from: null },
            { to: [0, 3, 2], from: [0, 4, 3] },
            { to: 1, from: null },
        ];
        setter.set_from_chunk(dest, src, mapping);
        // biome-ignore format: the array should not be formatted
        expect(to_c(dest).data).toStrictEqual(new Float32Array([
            0, 2, 0, 0,
            0, 0, 0, 0,
            0, 2, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
        ]));
    });
    it("set_from_chunk - dest=C order, src=F order", async () => {
        let dest = setter.prepare(new Float32Array(2 * 3 * 4), [2, 3, 4], get_strides([2, 3, 4], "C"));
        let src = setter.prepare(new Float32Array([2, 0, 0, 2]), [4], get_strides([4], "F"));
        let mapping = [
            { to: 0, from: null },
            { to: [0, 3, 2], from: [0, 4, 3] },
            { to: 1, from: null },
        ];
        setter.set_from_chunk(dest, src, mapping);
        // biome-ignore format: the array should not be formatted
        expect(dest.data).toStrictEqual(new Float32Array([
            0, 2, 0, 0,
            0, 0, 0, 0,
            0, 2, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
        ]));
    });
});
//# sourceMappingURL=setter.test.js.map