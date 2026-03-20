import { fetch_range, merge_init } from "./util.js";
function resolve(root, path) {
    const base = typeof root === "string" ? new URL(root) : root;
    if (!base.pathname.endsWith("/")) {
        // ensure trailing slash so that base is resolved as _directory_
        base.pathname += "/";
    }
    const resolved = new URL(path.slice(1), base);
    // copy search params to new URL
    resolved.search = base.search;
    return resolved;
}
async function handle_response(response) {
    if (response.status === 404) {
        return undefined;
    }
    if (response.status === 200 || response.status === 206) {
        return new Uint8Array(await response.arrayBuffer());
    }
    throw new Error(`Unexpected response status ${response.status} ${response.statusText}`);
}
async function fetch_suffix(url, suffix_length, init, use_suffix_request) {
    if (use_suffix_request) {
        return fetch(url, {
            ...init,
            headers: { ...init.headers, Range: `bytes=-${suffix_length}` },
        });
    }
    let response = await fetch(url, { ...init, method: "HEAD" });
    if (!response.ok) {
        // will be picked up by handle_response
        return response;
    }
    let content_length = response.headers.get("Content-Length");
    let length = Number(content_length);
    return fetch_range(url, length - suffix_length, length, init);
}
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
class FetchStore {
    url;
    #overrides;
    #use_suffix_request;
    constructor(url, options = {}) {
        this.url = url;
        this.#overrides = options.overrides ?? {};
        this.#use_suffix_request = options.useSuffixRequest ?? false;
    }
    #merge_init(overrides) {
        return merge_init(this.#overrides, overrides);
    }
    async get(key, options = {}) {
        let href = resolve(this.url, key).href;
        let response = await fetch(href, this.#merge_init(options));
        return handle_response(response);
    }
    async getRange(key, range, options = {}) {
        let url = resolve(this.url, key);
        let init = this.#merge_init(options);
        let response;
        if ("suffixLength" in range) {
            response = await fetch_suffix(url, range.suffixLength, init, this.#use_suffix_request);
        }
        else {
            response = await fetch_range(url, range.offset, range.length, init);
        }
        return handle_response(response);
    }
}
export default FetchStore;
//# sourceMappingURL=fetch.js.map