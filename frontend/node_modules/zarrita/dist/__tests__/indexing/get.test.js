import * as path from "node:path";
import * as url from "node:url";
import FileSystemStore from "@zarrita/storage/fs";
import { describe, expect, it } from "vitest";
import * as zarr from "../../src/index.js";
import { get } from "../../src/indexing/ops.js";
import { range } from "../../src/indexing/util.js";
let __dirname = path.dirname(url.fileURLToPath(import.meta.url));
async function get_v2(abs_path, ...args) {
    let root = path.resolve(__dirname, "../../../../fixtures/v2/data.zarr");
    let store = zarr.root(new FileSystemStore(root));
    let arr = await zarr.open.v2(store.resolve(abs_path), { kind: "array" });
    // @ts-expect-error - TS not happy about spreading these args and its fine for this func
    return get(arr, ...args);
}
describe("get v2", () => {
    it("1d.contiguous.zlib.i2", async () => {
        expect(await get_v2("/1d.contiguous.zlib.i2")).toMatchInlineSnapshot(`
			{
			  "data": Int16Array [
			    1,
			    2,
			    3,
			    4,
			  ],
			  "shape": [
			    4,
			  ],
			  "stride": [
			    1,
			  ],
			}
		`);
    });
    it("1d.contiguous.blosc.i2", async () => {
        expect(await get_v2("/1d.contiguous.blosc.i2")).toMatchInlineSnapshot(`
			{
			  "data": Int16Array [
			    1,
			    2,
			    3,
			    4,
			  ],
			  "shape": [
			    4,
			  ],
			  "stride": [
			    1,
			  ],
			}
		`);
    });
    it("1d.contiguous.lz4.i2", async () => {
        expect(await get_v2("/1d.contiguous.lz4.i2")).toMatchInlineSnapshot(`
			{
			  "data": Int16Array [
			    1,
			    2,
			    3,
			    4,
			  ],
			  "shape": [
			    4,
			  ],
			  "stride": [
			    1,
			  ],
			}
		`);
    });
    it("1d.contiguous.zstd.i2", async () => {
        expect(await get_v2("/1d.contiguous.zstd.i2")).toMatchInlineSnapshot(`
			{
			  "data": Int16Array [
			    1,
			    2,
			    3,
			    4,
			  ],
			  "shape": [
			    4,
			  ],
			  "stride": [
			    1,
			  ],
			}
		`);
    });
    it("1d.contiguous.raw.i2", async () => {
        expect(await get_v2("/1d.contiguous.raw.i2")).toMatchInlineSnapshot(`
			{
			  "data": Int16Array [
			    1,
			    2,
			    3,
			    4,
			  ],
			  "shape": [
			    4,
			  ],
			  "stride": [
			    1,
			  ],
			}
		`);
    });
    it("1d.contiguous.i4", async () => {
        expect(await get_v2("/1d.contiguous.i4")).toMatchInlineSnapshot(`
			{
			  "data": Int32Array [
			    1,
			    2,
			    3,
			    4,
			  ],
			  "shape": [
			    4,
			  ],
			  "stride": [
			    1,
			  ],
			}
		`);
    });
    it("1d.contiguous.u1", async () => {
        expect(await get_v2("/1d.contiguous.u1")).toMatchInlineSnapshot(`
			{
			  "data": Uint8Array [
			    255,
			    0,
			    255,
			    0,
			  ],
			  "shape": [
			    4,
			  ],
			  "stride": [
			    1,
			  ],
			}
		`);
    });
    it("1d.contiguous.f4.le", async () => {
        expect(await get_v2("/1d.contiguous.f4.le")).toMatchInlineSnapshot(`
			{
			  "data": Float32Array [
			    -1000.5,
			    0,
			    1000.5,
			    0,
			  ],
			  "shape": [
			    4,
			  ],
			  "stride": [
			    1,
			  ],
			}
		`);
    });
    it("1d.contiguous.f4.be", async () => {
        expect(await get_v2("/1d.contiguous.f4.be")).toMatchInlineSnapshot(`
			{
			  "data": Float32Array [
			    -1000.5,
			    0,
			    1000.5,
			    0,
			  ],
			  "shape": [
			    4,
			  ],
			  "stride": [
			    1,
			  ],
			}
		`);
    });
    it("1d.contiguous.f8", async () => {
        expect(await get_v2("/1d.contiguous.f8")).toMatchInlineSnapshot(`
			{
			  "data": Float64Array [
			    1.5,
			    2.5,
			    3.5,
			    4.5,
			  ],
			  "shape": [
			    4,
			  ],
			  "stride": [
			    1,
			  ],
			}
		`);
    });
    it("1d.contiguous.U13.le", async () => {
        let res = await get_v2("/1d.contiguous.U13.le");
        expect(res.data).toBeInstanceOf(zarr.UnicodeStringArray);
        expect(Array.from(res.data)).toStrictEqual(["a", "b", "cc", "d"]);
        expect(res.shape).toStrictEqual([4]);
    });
    it("1d.contiguous.U13.be", async () => {
        let res = await get_v2("/1d.contiguous.U13.be");
        expect(res.data).toBeInstanceOf(zarr.UnicodeStringArray);
        expect(Array.from(res.data)).toStrictEqual(["a", "b", "cc", "d"]);
        expect(res.shape).toStrictEqual([4]);
    });
    it("1d.contiguous.U7", async () => {
        let res = await get_v2("/1d.contiguous.U7");
        expect(res.data).toBeInstanceOf(zarr.UnicodeStringArray);
        expect(Array.from(res.data)).toStrictEqual(["a", "b", "cc", "d"]);
        expect(res.shape).toStrictEqual([4]);
    });
    it("1d.contiguous.S7", async () => {
        let res = await get_v2("/1d.contiguous.S7");
        expect(res.data).toBeInstanceOf(zarr.ByteStringArray);
        expect(Array.from(res.data)).toStrictEqual(["a", "b", "cc", "d"]);
        expect(res.shape).toStrictEqual([4]);
    });
    it("1d.contiguous.b1", async () => {
        let res = await get_v2("/1d.contiguous.b1");
        expect(res).toMatchInlineSnapshot(`
			{
			  "data": BoolArray {},
			  "shape": [
			    4,
			  ],
			  "stride": [
			    1,
			  ],
			}
		`);
        expect(Array.from(res.data)).toStrictEqual([true, false, true, false]);
    });
    it("2d.contiguous.i2", async () => {
        expect(await get_v2("/2d.contiguous.i2")).toMatchInlineSnapshot(`
			{
			  "data": Int16Array [
			    1,
			    2,
			    3,
			    4,
			  ],
			  "shape": [
			    2,
			    2,
			  ],
			  "stride": [
			    2,
			    1,
			  ],
			}
		`);
    });
    it("3d.contiguous.i2", async () => {
        expect(await get_v2("/3d.contiguous.i2")).toMatchInlineSnapshot(`
			{
			  "data": Int16Array [
			    0,
			    1,
			    2,
			    3,
			    4,
			    5,
			    6,
			    7,
			    8,
			    9,
			    10,
			    11,
			    12,
			    13,
			    14,
			    15,
			    16,
			    17,
			    18,
			    19,
			    20,
			    21,
			    22,
			    23,
			    24,
			    25,
			    26,
			  ],
			  "shape": [
			    3,
			    3,
			    3,
			  ],
			  "stride": [
			    9,
			    3,
			    1,
			  ],
			}
		`);
    });
    it("1d.chunked.i2", async () => {
        expect(await get_v2("/1d.chunked.i2")).toMatchInlineSnapshot(`
			{
			  "data": Int16Array [
			    1,
			    2,
			    3,
			    4,
			  ],
			  "shape": [
			    4,
			  ],
			  "stride": [
			    1,
			  ],
			}
		`);
    });
    it("1d.chunked.ragged.i2", async () => {
        expect(await get_v2("/1d.chunked.ragged.i2")).toMatchInlineSnapshot(`
			{
			  "data": Int16Array [
			    1,
			    2,
			    3,
			    4,
			    5,
			  ],
			  "shape": [
			    5,
			  ],
			  "stride": [
			    1,
			  ],
			}
		`);
    });
    it("2d.chunked.i2", async () => {
        expect(await get_v2("/2d.chunked.i2")).toMatchInlineSnapshot(`
			{
			  "data": Int16Array [
			    1,
			    2,
			    3,
			    4,
			  ],
			  "shape": [
			    2,
			    2,
			  ],
			  "stride": [
			    2,
			    1,
			  ],
			}
		`);
    });
    it("2d.chunked.U7", async () => {
        let res = await get_v2("/2d.chunked.U7");
        expect(Array.from(res.data)).toStrictEqual(["a", "b", "cc", "d"]);
        expect(res.shape).toStrictEqual([2, 2]);
    });
    it("2d.chunked.ragged.i2", async () => {
        expect(await get_v2("/2d.chunked.ragged.i2")).toMatchInlineSnapshot(`
			{
			  "data": Int16Array [
			    1,
			    2,
			    3,
			    4,
			    5,
			    6,
			    7,
			    8,
			    9,
			  ],
			  "shape": [
			    3,
			    3,
			  ],
			  "stride": [
			    3,
			    1,
			  ],
			}
		`);
    });
    it("3d.chunked.i2", async () => {
        expect(await get_v2("/3d.chunked.i2")).toMatchInlineSnapshot(`
			{
			  "data": Int16Array [
			    0,
			    1,
			    2,
			    3,
			    4,
			    5,
			    6,
			    7,
			    8,
			    9,
			    10,
			    11,
			    12,
			    13,
			    14,
			    15,
			    16,
			    17,
			    18,
			    19,
			    20,
			    21,
			    22,
			    23,
			    24,
			    25,
			    26,
			  ],
			  "shape": [
			    3,
			    3,
			    3,
			  ],
			  "stride": [
			    9,
			    3,
			    1,
			  ],
			}
		`);
    });
    it("3d.chunked.mixed.i2.C", async () => {
        expect(await get_v2("/3d.chunked.mixed.i2.C")).toMatchInlineSnapshot(`
			{
			  "data": Int16Array [
			    0,
			    1,
			    2,
			    3,
			    4,
			    5,
			    6,
			    7,
			    8,
			    9,
			    10,
			    11,
			    12,
			    13,
			    14,
			    15,
			    16,
			    17,
			    18,
			    19,
			    20,
			    21,
			    22,
			    23,
			    24,
			    25,
			    26,
			  ],
			  "shape": [
			    3,
			    3,
			    3,
			  ],
			  "stride": [
			    9,
			    3,
			    1,
			  ],
			}
		`);
    });
    it("3d.chunked.mixed.i2.F", async () => {
        let res = await get_v2("/3d.chunked.mixed.i2.F");
        // biome-ignore format: the array should not be formatted
        expect(res.data).toStrictEqual(new Int16Array([
            0, 9, 18, 3, 12, 21, 6, 15, 24,
            1, 10, 19, 4, 13, 22, 7, 16, 25,
            2, 11, 20, 5, 14, 23, 8, 17, 26,
        ]));
        expect(res.shape).toStrictEqual([3, 3, 3]);
        expect(res.stride).toStrictEqual([1, 3, 9]);
    });
    it("3d.chunked.O", async () => {
        let arr = await get_v2("/3d.chunked.O");
        expect(arr).toStrictEqual({
            data: ["a", "aa", "aaa", "aaaa", "b", "bb", "bbb", "bbbb"],
            shape: [2, 2, 2],
            stride: [4, 2, 1],
        });
    });
});
async function get_v3(abs_path, ...args) {
    let root = path.resolve(__dirname, "../../../../fixtures/v3/data.zarr");
    let store = zarr.root(new FileSystemStore(root));
    let arr = await zarr.open.v3(store.resolve(abs_path), { kind: "array" });
    // @ts-expect-error - TS not happy about spreading these args
    return get(arr, ...args);
}
describe("get v3", () => {
    it("1d.contiguous.gzip.i2", async () => {
        let res = await get_v3("/1d.contiguous.gzip.i2");
        expect(res.data).toStrictEqual(new Int16Array([1, 2, 3, 4]));
        expect(res.shape).toStrictEqual([4]);
    });
    it("1d.contiguous.blosc.i2", async () => {
        let res = await get_v3("/1d.contiguous.blosc.i2");
        expect(res.data).toStrictEqual(new Int16Array([1, 2, 3, 4]));
        expect(res.shape).toStrictEqual([4]);
    });
    it("1d.contiguous.raw.i2", async () => {
        let res = await get_v3("/1d.contiguous.raw.i2");
        expect(res.data).toStrictEqual(new Int16Array([1, 2, 3, 4]));
        expect(res.shape).toStrictEqual([4]);
    });
    it("1d.contiguous.i4", async () => {
        let res = await get_v3("/1d.contiguous.i4");
        expect(res.data).toStrictEqual(new Int32Array([1, 2, 3, 4]));
        expect(res.shape).toStrictEqual([4]);
    });
    it("1d.contiguous.u1", async () => {
        let res = await get_v3("/1d.contiguous.u1");
        expect(res.data).toStrictEqual(new Uint8Array([255, 0, 255, 0]));
        expect(res.shape).toStrictEqual([4]);
    });
    it("1d.contiguous.f4.le", async () => {
        let res = await get_v3("/1d.contiguous.f4.le");
        expect(res.data).toStrictEqual(new Float32Array([-1000.5, 0, 1000.5, 0]));
        expect(res.shape).toStrictEqual([4]);
    });
    it("1d.contiguous.f4.be", async () => {
        let res = await get_v3("/1d.contiguous.f4.be");
        expect(res.data).toStrictEqual(new Float32Array([-1000.5, 0, 1000.5, 0]));
        expect(res.shape).toStrictEqual([4]);
    });
    it("1d.contiguous.f8", async () => {
        let res = await get_v3("/1d.contiguous.f8");
        expect(res.data).toStrictEqual(new Float64Array([1.5, 2.5, 3.5, 4.5]));
        expect(res.shape).toStrictEqual([4]);
    });
    it("1d.contiguous.b1", async () => {
        let res = await get_v3("/1d.contiguous.b1");
        expect(res.data).toBeInstanceOf(zarr.BoolArray);
        expect(Array.from(res.data)).toStrictEqual([
            true,
            false,
            true,
            false,
        ]);
        expect(res.shape).toStrictEqual([4]);
    });
    it("2d.contiguous.i2", async () => {
        let res = await get_v3("/2d.contiguous.i2");
        expect(res.data).toStrictEqual(new Int16Array([1, 2, 3, 4]));
        expect(res.shape).toStrictEqual([2, 2]);
    });
    it("3d.contiguous.i2", async () => {
        let res = await get_v3("/3d.contiguous.i2");
        expect(res.data).toStrictEqual(new Int16Array(range(27)));
        expect(res.shape).toStrictEqual([3, 3, 3]);
    });
    it("1d.chunked.i2", async () => {
        let res = await get_v3("/1d.chunked.i2");
        expect(res.data).toStrictEqual(new Int16Array([1, 2, 3, 4]));
        expect(res.shape).toStrictEqual([4]);
    });
    it("1d.chunked.ragged.i2", async () => {
        let res = await get_v3("/1d.chunked.ragged.i2");
        expect(res.data).toStrictEqual(new Int16Array([1, 2, 3, 4, 5]));
        expect(res.shape).toStrictEqual([5]);
    });
    it("2d.chunked.i2", async () => {
        let res = await get_v3("/2d.chunked.i2");
        expect(res.data).toStrictEqual(new Int16Array([1, 2, 3, 4]));
        expect(res.shape).toStrictEqual([2, 2]);
    });
    it("2d.chunked.ragged.i2", async () => {
        let res = await get_v3("/2d.chunked.ragged.i2");
        expect(res.data).toStrictEqual(new Int16Array(range(1, 10)));
        expect(res.shape).toStrictEqual([3, 3]);
    });
    it("3d.chunked.i2", async () => {
        let res = await get_v3("/3d.chunked.i2");
        expect(res.data).toStrictEqual(new Int16Array(range(27)));
        expect(res.shape).toStrictEqual([3, 3, 3]);
    });
    it("3d.chunked.mixed.i2.C", async () => {
        let res = await get_v3("/3d.chunked.mixed.i2.C");
        expect(res.data).toStrictEqual(new Int16Array(range(27)));
        expect(res.shape).toStrictEqual([3, 3, 3]);
        expect(res.stride).toStrictEqual([9, 3, 1]);
    });
    it("3d.chunked.mixed.i2.F", async () => {
        let res = await get_v3("/3d.chunked.mixed.i2.F");
        // biome-ignore format: the array should not be formatted
        expect(res.data).toStrictEqual(new Int16Array([
            0, 9, 18, 3, 12, 21, 6, 15, 24,
            1, 10, 19, 4, 13, 22, 7, 16, 25,
            2, 11, 20, 5, 14, 23, 8, 17, 26,
        ]));
        expect(res.shape).toStrictEqual([3, 3, 3]);
        expect(res.stride).toStrictEqual([1, 3, 9]);
    });
});
//# sourceMappingURL=get.test.js.map