import { expect, test } from "vitest";
import * as zarr from "../src/index.js";
import { json_decode_object } from "../src/util.js";
test("create root group", async () => {
    let attributes = { hello: "world" };
    let grp = await zarr.create(new Map(), { attributes });
    expect(grp.path).toBe("/");
    expect(grp.attrs).toStrictEqual(attributes);
    expect(grp.store.has("/zarr.json")).true;
    expect(
    // biome-ignore lint/style/noNonNullAssertion: we know it's there
    json_decode_object(grp.store.get("/zarr.json"))).toMatchInlineSnapshot(`
		{
		  "attributes": {
		    "hello": "world",
		  },
		  "node_type": "group",
		  "zarr_format": 3,
		}
	`);
});
test("create array", async () => {
    let h = zarr.root();
    let attributes = { question: "life", answer: 42 };
    let a = await zarr.create(h.resolve("/arthur/dent"), {
        shape: [5, 10],
        chunk_shape: [2, 5],
        data_type: "int32",
        attributes,
    });
    expect(a).toBeInstanceOf(zarr.Array);
    expect(a.path).toBe("/arthur/dent");
    expect(a.shape).toStrictEqual([5, 10]);
    expect(a.dtype).toBe("int32");
    expect(a.chunks).toStrictEqual([2, 5]);
    expect(a.attrs).toStrictEqual(attributes);
    expect(
    // biome-ignore lint/style/noNonNullAssertion: we know it's there
    json_decode_object(h.store.get("/arthur/dent/zarr.json"))).toMatchInlineSnapshot(`
		{
		  "attributes": {
		    "answer": 42,
		    "question": "life",
		  },
		  "chunk_grid": {
		    "configuration": {
		      "chunk_shape": [
		        2,
		        5,
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
		  "codecs": [],
		  "data_type": "int32",
		  "fill_value": null,
		  "node_type": "array",
		  "shape": [
		    5,
		    10,
		  ],
		  "zarr_format": 3,
		}
	`);
});
test("create group from another group", async () => {
    let h = zarr.root();
    let grp = await zarr.create(h.resolve("/tricia/mcmillan"));
    await zarr.create(grp.resolve("relative"));
    await zarr.create(grp.resolve("/absolute"));
    expect(new Set(h.store.keys())).toMatchInlineSnapshot(`
		Set {
		  "/tricia/mcmillan/zarr.json",
		  "/tricia/mcmillan/relative/zarr.json",
		  "/absolute/zarr.json",
		}
	`);
});
test("create nodes via groups", async () => {
    let h = zarr.root();
    let marvin = await zarr.create(h.resolve("/marvin"));
    let paranoid = await zarr.create(marvin.resolve("paranoid"));
    let android = await zarr.create(marvin.resolve("android"), {
        shape: [42, 42],
        data_type: "uint8",
        chunk_shape: [2, 2],
    });
    expect(marvin).toBeInstanceOf(zarr.Group);
    expect(marvin.path).toBe("/marvin");
    expect(paranoid).toBeInstanceOf(zarr.Group);
    expect(paranoid.path).toBe("/marvin/paranoid");
    expect(android).toBeInstanceOf(zarr.Array);
    expect(android.path).toBe("/marvin/android");
});
//# sourceMappingURL=create.test.js.map