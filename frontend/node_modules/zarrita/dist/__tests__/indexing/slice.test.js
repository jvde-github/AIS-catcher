import { describe, expect, it } from "vitest";
import * as zarr from "../../src/index.js";
import { get, set, slice } from "../../src/index.js";
import { IndexError } from "../../src/indexing/indexer.js";
const DATA = {
    // biome-ignore format: the array should not be formatted
    data: new Int32Array([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23]),
    shape: [2, 3, 4],
    stride: [12, 4, 1],
};
describe("slice", async () => {
    let arr = await zarr.create(new Map(), {
        shape: [2, 3, 4],
        data_type: "int32",
        chunk_shape: [1, 2, 2],
    });
    await set(arr, null, DATA);
    it.each([
        undefined,
        null,
        [null, null, null],
        [slice(null), slice(null), slice(null)],
        [null, slice(null), slice(null)],
    ])("Reads entire array: selection = %j", async (sel) => {
        let chunk = await get(arr, sel);
        expect(chunk).toStrictEqual(DATA);
    });
    it.each([
        [
            [1, slice(1, 3), null],
            {
                data: new Int32Array([16, 17, 18, 19, 20, 21, 22, 23]),
                shape: [2, 4],
                stride: [4, 1],
            },
        ],
        [
            [null, slice(1, 3), slice(2)],
            {
                data: new Int32Array([4, 5, 8, 9, 16, 17, 20, 21]),
                shape: [2, 2, 2],
                stride: [4, 2, 1],
            },
        ],
        [
            [null, null, 0],
            {
                data: new Int32Array([0, 4, 8, 12, 16, 20]),
                shape: [2, 3],
                stride: [3, 1],
            },
        ],
        [
            [slice(3), slice(2), slice(2)],
            {
                data: new Int32Array([0, 1, 4, 5, 12, 13, 16, 17]),
                shape: [2, 2, 2],
                stride: [4, 2, 1],
            },
        ],
        [
            [1, null, null],
            {
                // biome-ignore format: the array should not be formatted
                data: new Int32Array([12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23]),
                shape: [3, 4],
                stride: [4, 1],
            },
        ],
        [
            [0, slice(null, null, 2), slice(null, null, 3)],
            {
                data: new Int32Array([0, 3, 8, 11]),
                shape: [2, 2],
                stride: [2, 1],
            },
        ],
        [
            [null, null, slice(null, null, 4)],
            {
                data: new Int32Array([0, 4, 8, 12, 16, 20]),
                shape: [2, 3, 1],
                stride: [3, 1, 1],
            },
        ],
        [
            [1, null, slice(null, 3, 2)],
            {
                data: new Int32Array([12, 14, 16, 18, 20, 22]),
                shape: [3, 2],
                stride: [2, 1],
            },
        ],
        [
            [null, 1, null],
            {
                data: new Int32Array([4, 5, 6, 7, 16, 17, 18, 19]),
                shape: [2, 4],
                stride: [4, 1],
            },
        ],
        [
            [1, 2, null],
            {
                data: new Int32Array([20, 21, 22, 23]),
                shape: [4],
                stride: [1],
            },
        ],
    ])("Reads fancy slices: selection - %j", async (sel, expected) => {
        let { data, shape, stride } = await get(arr, sel);
        expect({ data, shape, stride }).toStrictEqual(expected);
    });
    it("Reads a scalar", async () => {
        let sel = [1, 1, 1];
        let val = await get(arr, sel);
        expect(val).toBe(17);
    });
    it("Does not support negative indices", async () => {
        let sel = [0, slice(null, null, -2), slice(null, null, 3)];
        await expect(get(arr, sel)).rejects.toThrowError(IndexError);
    });
});
//# sourceMappingURL=slice.test.js.map