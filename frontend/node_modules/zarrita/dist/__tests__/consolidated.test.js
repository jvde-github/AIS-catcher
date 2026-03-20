import * as path from "node:path";
import * as url from "node:url";
import { FileSystemStore } from "@zarrita/storage";
import { assert, describe, expect, it } from "vitest";
import { tryWithConsolidated, withConsolidated } from "../src/consolidated.js";
import { NodeNotFoundError } from "../src/errors.js";
import { Array as ZarrArray } from "../src/hierarchy.js";
import { open } from "../src/open.js";
let __dirname = path.dirname(url.fileURLToPath(import.meta.url));
describe("withConsolidated", () => {
    it("loads consolidated metadata", async () => {
        let root = path.join(__dirname, "../../../fixtures/v2/data.zarr");
        let store = await withConsolidated(new FileSystemStore(root));
        let map = new Map(store.contents().map((x) => [x.path, x.kind]));
        expect(map).toMatchInlineSnapshot(`
			Map {
			  "/" => "group",
			  "/1d.chunked.i2" => "array",
			  "/1d.chunked.ragged.i2" => "array",
			  "/1d.contiguous.S7" => "array",
			  "/1d.contiguous.U13.be" => "array",
			  "/1d.contiguous.U13.le" => "array",
			  "/1d.contiguous.U7" => "array",
			  "/1d.contiguous.b1" => "array",
			  "/1d.contiguous.blosc.i2" => "array",
			  "/1d.contiguous.f4.be" => "array",
			  "/1d.contiguous.f4.le" => "array",
			  "/1d.contiguous.f8" => "array",
			  "/1d.contiguous.i4" => "array",
			  "/1d.contiguous.lz4.i2" => "array",
			  "/1d.contiguous.raw.i2" => "array",
			  "/1d.contiguous.u1" => "array",
			  "/1d.contiguous.zlib.i2" => "array",
			  "/1d.contiguous.zstd.i2" => "array",
			  "/2d.chunked.U7" => "array",
			  "/2d.chunked.i2" => "array",
			  "/2d.chunked.ragged.i2" => "array",
			  "/2d.contiguous.i2" => "array",
			  "/3d.chunked.O" => "array",
			  "/3d.chunked.i2" => "array",
			  "/3d.chunked.mixed.i2.C" => "array",
			  "/3d.chunked.mixed.i2.F" => "array",
			  "/3d.contiguous.i2" => "array",
			  "/my group with spaces" => "group",
			}
		`);
    });
    it("loads chunk data from underlying store", async () => {
        let root = path.join(__dirname, "../../../fixtures/v2/data.zarr");
        let store = await withConsolidated(new FileSystemStore(root));
        // biome-ignore lint/style/noNonNullAssertion: Fine for a test
        let entry = store
            .contents()
            .find((x) => x.path === "/3d.chunked.mixed.i2.C");
        let grp = await open(store, { kind: "group" });
        let arr = await open(grp.resolve(entry.path), { kind: entry.kind });
        expect(arr).toBeInstanceOf(ZarrArray);
        // @ts-expect-error - we know this is an array
        expect(await arr.getChunk([0, 0, 0])).toMatchInlineSnapshot(`
			{
			  "data": Int16Array [
			    0,
			    3,
			    6,
			    9,
			    12,
			    15,
			    18,
			    21,
			    24,
			  ],
			  "shape": [
			    3,
			    3,
			    1,
			  ],
			  "stride": [
			    3,
			    1,
			    1,
			  ],
			}
		`);
    });
    it("loads and navigates from root", async () => {
        let path_root = path.join(__dirname, "../../../fixtures/v2/data.zarr");
        let store = await withConsolidated(new FileSystemStore(path_root));
        let grp = await open(store, { kind: "group" });
        expect(grp.kind).toBe("group");
        let arr = await open(grp.resolve("1d.chunked.i2"), { kind: "array" });
        expect(arr.kind).toBe("array");
    });
    it("throws if consolidated metadata is missing", async () => {
        let root = path.join(__dirname, "../../../fixtures/v2/data.zarr/3d.contiguous.i2");
        let try_open = () => withConsolidated(new FileSystemStore(root));
        await expect(try_open).rejects.toThrowError(NodeNotFoundError);
        await expect(try_open).rejects.toThrowErrorMatchingInlineSnapshot("[NodeNotFoundError: Node not found: v2 consolidated metadata]");
    });
});
describe("tryWithConsolidated", () => {
    it("creates Listable from consolidated store", async () => {
        let root = path.join(__dirname, "../../../fixtures/v2/data.zarr");
        let store = await tryWithConsolidated(new FileSystemStore(root));
        expect(store).toHaveProperty("contents");
    });
    it("falls back to original store if missing consolidated metadata", async () => {
        let root = path.join(__dirname, "../../../fixtures/v2/data.zarr/3d.contiguous.i2");
        let store = await tryWithConsolidated(new FileSystemStore(root));
        expect(store).toBeInstanceOf(FileSystemStore);
    });
    it("supports a zmetadataKey option", async () => {
        let root = path.join(__dirname, "../../../fixtures/v2/data.zarr");
        let store = await tryWithConsolidated(new FileSystemStore(root), {
            metadataKey: ".zmetadata",
        });
        expect(store).toHaveProperty("contents");
    });
    it("falls back to original store if metadataKey is incorrect", async () => {
        let root = path.join(__dirname, "../../../fixtures/v2/data.zarr");
        let store = await tryWithConsolidated(new FileSystemStore(root), {
            metadataKey: ".nonexistent",
        });
        expect(store).toBeInstanceOf(FileSystemStore);
    });
});
describe("Listable.getRange", () => {
    it("does not expose getRange if the underlying store does not support it", async () => {
        let store = await tryWithConsolidated(new Map());
        expect("getRange" in store).toBeFalsy();
    });
    it("retrieves a byte range from an underlying store", async () => {
        let root = path.join(__dirname, "../../../fixtures/v2/data.zarr");
        let store = await tryWithConsolidated(new FileSystemStore(root));
        assert(typeof store.getRange === "function");
    });
});
//# sourceMappingURL=consolidated.test.js.map