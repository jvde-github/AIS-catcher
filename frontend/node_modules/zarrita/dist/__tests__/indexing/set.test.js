import ndarray from "ndarray";
import { expect, test } from "vitest";
import * as zarr from "../../src/index.js";
import { get, set } from "../../src/indexing/ops.js";
import { range, slice } from "../../src/indexing/util.js";
test("Read and write array data - builtin", async () => {
    let h = zarr.root(new Map());
    let a = await zarr.create(h.resolve("/arthur/dent"), {
        shape: [5, 10],
        data_type: "int32",
        chunk_shape: [2, 5],
    });
    let res = await get(a, [null, null]);
    expect(res.shape).toStrictEqual([5, 10]);
    expect(res.data).toStrictEqual(new Int32Array(50));
    res = await get(a);
    expect(res.shape).toStrictEqual([5, 10]);
    expect(res.data).toStrictEqual(new Int32Array(50));
    await set(a, [0, null], 42);
    expect((await get(a, null)).data).toStrictEqual(new Int32Array(50).fill(42, 0, 10));
    const expected = new Int32Array(50).fill(42, 0, 10);
    for (let i of [10, 20, 30, 40]) {
        expected[i] = 42;
    }
    await set(a, [null, 0], 42);
    expect((await get(a, null)).data).toStrictEqual(expected);
    await set(a, null, 42);
    expected.fill(42);
    expect((await get(a, null)).data).toStrictEqual(expected);
    let arr = ndarray(new Int32Array(Array(10).keys()), [10]);
    expected.set(arr.data);
    await set(a, [0, null], arr);
    expect((await get(a, null)).data).toStrictEqual(expected);
    arr = ndarray(new Int32Array(range(50)), [5, 10]);
    await set(a, null, arr);
    expect((await get(a, null)).data).toStrictEqual(arr.data);
    // Read array slices
    res = await get(a, [null, 0]);
    expect(res.shape).toStrictEqual([5]);
    expect(res.data).toStrictEqual(new Int32Array([0, 10, 20, 30, 40]));
    res = await get(a, [null, 1]);
    expect(res.shape).toStrictEqual([5]);
    expect(res.data).toStrictEqual(new Int32Array([1, 11, 21, 31, 41]));
    res = await get(a, [0, null]);
    expect(res.shape).toStrictEqual([10]);
    expect(res.data).toStrictEqual(new Int32Array([0, 1, 2, 3, 4, 5, 6, 7, 8, 9]));
    res = await get(a, [1, null]);
    expect(res.shape).toStrictEqual([10]);
    expect(res.data).toStrictEqual(new Int32Array([10, 11, 12, 13, 14, 15, 16, 17, 18, 19]));
    res = await get(a, [null, slice(0, 7)]);
    expect(res.shape).toStrictEqual([5, 7]);
    // biome-ignore format: the array should not be formatted
    expect(res.data).toStrictEqual(new Int32Array([
        0, 1, 2, 3, 4,
        5, 6, 10, 11, 12,
        13, 14, 15, 16, 20,
        21, 22, 23, 24, 25,
        26, 30, 31, 32, 33,
        34, 35, 36, 40, 41,
        42, 43, 44, 45, 46,
    ]));
    res = await get(a, [slice(0, 3), null]);
    expect(res.shape).toStrictEqual([3, 10]);
    // biome-ignore format: the array should not be formatted
    expect(res.data).toStrictEqual(new Int32Array([
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
        20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    ]));
    res = await get(a, [slice(0, 3), slice(0, 7)]);
    expect(res.shape).toStrictEqual([3, 7]);
    // biome-ignore format: the array should not be formatted
    expect(res.data).toStrictEqual(new Int32Array([
        0, 1, 2, 3, 4, 5, 6,
        10, 11, 12, 13, 14, 15, 16,
        20, 21, 22, 23, 24, 25, 26,
    ]));
    res = await get(a, [slice(1, 4), slice(2, 7)]);
    expect(res.shape).toStrictEqual([3, 5]);
    // biome-ignore format: the array should not be formatted
    expect(res.data).toStrictEqual(new Int32Array([
        12, 13, 14, 15, 16,
        22, 23, 24, 25, 26,
        32, 33, 34, 35, 36,
    ]));
    let b = await zarr.create(h.resolve("/deep/thought"), {
        shape: [7500000],
        data_type: "float64",
        chunk_shape: [42],
    });
    let resb = await get(b, [slice(10)]);
    expect(resb.shape).toStrictEqual([10]);
    expect(resb.data).toStrictEqual(new Float64Array(10));
    expected.fill(1, 0, 5);
    await set(b, [slice(5)], 1);
    expect((await get(b, [slice(10)])).data).toStrictEqual(new Float64Array(10).fill(1, 0, 5));
});
//# sourceMappingURL=set.test.js.map