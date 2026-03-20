import { afterEach, describe, expect, it, vi } from "vitest";
import FetchStore from "../src/fetch.js";
// `vitest --api` exposes the port 51204
// ref: https://vitest.dev/config/#api
let href = "http://localhost:51204/fixtures/v3/data.zarr";
describe("FetchStore", () => {
    afterEach(() => {
        vi.restoreAllMocks();
    });
    it("reads a file from string url", async () => {
        let store = new FetchStore(href);
        let bytes = await store.get("/zarr.json");
        expect(bytes).toBeInstanceOf(Uint8Array);
        expect(JSON.parse(new TextDecoder().decode(bytes))).toMatchInlineSnapshot(`
			{
			  "attributes": {},
			  "consolidated_metadata": null,
			  "node_type": "group",
			  "zarr_format": 3,
			}
		`);
    });
    it("reads data when path contains spaces", async () => {
        let store = new FetchStore(href);
        let bytes = await store.get("/my group with spaces/zarr.json");
        expect(bytes).toBeInstanceOf(Uint8Array);
        expect(JSON.parse(new TextDecoder().decode(bytes))).toMatchInlineSnapshot(`
			{
			  "attributes": {
			    "description": "A group with spaces in the name",
			  },
			  "consolidated_metadata": null,
			  "node_type": "group",
			  "zarr_format": 3,
			}
		`);
    });
    it("reads a file from URL", async () => {
        let store = new FetchStore(new URL(href));
        let bytes = await store.get("/zarr.json");
        expect(bytes).toBeInstanceOf(Uint8Array);
        expect(JSON.parse(new TextDecoder().decode(bytes))).toMatchInlineSnapshot(`
			{
			  "attributes": {},
			  "consolidated_metadata": null,
			  "node_type": "group",
			  "zarr_format": 3,
			}
		`);
    });
    it("reads multi-part path", async () => {
        let store = new FetchStore(href);
        let bytes = await store.get("/1d.chunked.i2/zarr.json");
        expect(bytes).toBeInstanceOf(Uint8Array);
        expect(JSON.parse(new TextDecoder().decode(bytes))).toMatchInlineSnapshot(`
			{
			  "attributes": {},
			  "chunk_grid": {
			    "configuration": {
			      "chunk_shape": [
			        2,
			      ],
			    },
			    "name": "regular",
			  },
			  "chunk_key_encoding": {
			    "configuration": {
			      "separator": "/",
			    },
			    "name": "default",
			  },
			  "codecs": [
			    {
			      "configuration": {
			        "endian": "little",
			      },
			      "name": "bytes",
			    },
			    {
			      "configuration": {
			        "blocksize": 0,
			        "clevel": 5,
			        "cname": "zstd",
			        "shuffle": "noshuffle",
			        "typesize": 4,
			      },
			      "name": "blosc",
			    },
			  ],
			  "data_type": "int16",
			  "dimension_names": null,
			  "fill_value": 0,
			  "node_type": "array",
			  "shape": [
			    4,
			  ],
			  "storage_transformers": [],
			  "zarr_format": 3,
			}
		`);
    });
    it("returns undefined for missing file", async () => {
        let store = new FetchStore(href);
        expect(await store.get("/some/random/file/missing.duh")).toBeUndefined();
    });
    it("forwards request options to fetch", async () => {
        let headers = { "x-test": "test" };
        let store = new FetchStore(href);
        let spy = vi.spyOn(globalThis, "fetch");
        await store.get("/zarr.json", { headers });
        expect(spy).toHaveBeenCalledWith(`${href}/zarr.json`, { headers });
    });
    it("forwards request options to fetch when configured globally", async () => {
        let headers = { "x-test": "test" };
        let store = new FetchStore(href, { overrides: { headers } });
        let spy = vi.spyOn(globalThis, "fetch");
        await store.get("/zarr.json");
        expect(spy).toHaveBeenCalledWith(`${href}/zarr.json`, { headers });
    });
    it("merges request options", async () => {
        let overrides = {
            headers: { "x-test": "root", "x-test2": "root" },
            cache: "no-cache",
        };
        let store = new FetchStore(href, { overrides });
        let spy = vi.spyOn(globalThis, "fetch");
        await store.get("/zarr.json", { headers: { "x-test": "override" } });
        expect(spy).toHaveBeenCalledWith(`${href}/zarr.json`, {
            headers: { "x-test": "override", "x-test2": "root" },
            cache: "no-cache",
        });
    });
    it("reads partial - suffixLength", async () => {
        let store = new FetchStore(href);
        let bytes = await store.getRange("/zarr.json", { suffixLength: 50 });
        expect(new TextDecoder().decode(bytes)).toMatchInlineSnapshot(`
			"olidated_metadata": null,
			  "node_type": "group"
			}"
		`);
    });
    it("reads partial - offset, length", async () => {
        let store = new FetchStore(href);
        let bytes = await store.getRange("/zarr.json", { offset: 4, length: 50 });
        expect(new TextDecoder().decode(bytes)).toMatchInlineSnapshot(`
			""attributes": {},
			  "zarr_format": 3,
			  "consolida"
		`);
    });
});
//# sourceMappingURL=fetch.test.js.map