import type { AbsolutePath, AsyncReadable, RangeQuery } from "./types.js";
/**
 * Readonly store based in the [Fetch API](https://developer.mozilla.org/en-US/docs/Web/API/Fetch_API).
 * Must polyfill `fetch` for use in Node.js.
 *
 * ```typescript
 * import * as zarr from "zarrita";
 * const store = new FetchStore("http://localhost:8080/data.zarr");
 * const arr = await zarr.get(store, { kind: "array" });
 * ```
 */
declare class FetchStore implements AsyncReadable<RequestInit> {
    #private;
    url: string | URL;
    constructor(url: string | URL, options?: {
        overrides?: RequestInit;
        useSuffixRequest?: boolean;
    });
    get(key: AbsolutePath, options?: RequestInit): Promise<Uint8Array | undefined>;
    getRange(key: AbsolutePath, range: RangeQuery, options?: RequestInit): Promise<Uint8Array | undefined>;
}
export default FetchStore;
