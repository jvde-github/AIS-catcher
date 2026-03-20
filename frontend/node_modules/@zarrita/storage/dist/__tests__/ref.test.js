import { afterEach, describe, expect, it, vi } from "vitest";
import ReferenceStore from "../src/ref.js";
describe("ReferenceStore", () => {
    afterEach(() => {
        vi.restoreAllMocks();
    });
    it("store creation is not async", async () => {
        let spec = Promise.resolve({
            version: 1,
            refs: {
                ".zgroup": '{"zarr_format":2}',
                ".zattrs": '{"encoding-type":"anndat…oding-version":"0.1.0"}',
            },
        });
        let store = ReferenceStore.fromSpec(spec);
        let bytes = await store.get("/.zgroup");
        expect(bytes).toBeInstanceOf(Uint8Array);
        expect(JSON.parse(new TextDecoder().decode(bytes))).toMatchInlineSnapshot(`
			{
              "zarr_format": 2,
            }
		`);
    });
    it("store creation can still accept a non-promise", async () => {
        let spec = {
            version: 1,
            refs: {
                ".zgroup": '{"zarr_format":2}',
                ".zattrs": '{"encoding-type":"anndat…oding-version":"0.1.0"}',
            },
        };
        let store = ReferenceStore.fromSpec(spec);
        let bytes = await store.get("/.zgroup");
        expect(bytes).toBeInstanceOf(Uint8Array);
        expect(JSON.parse(new TextDecoder().decode(bytes))).toMatchInlineSnapshot(`
			{
              "zarr_format": 2,
            }
		`);
    });
});
//# sourceMappingURL=ref.test.js.map