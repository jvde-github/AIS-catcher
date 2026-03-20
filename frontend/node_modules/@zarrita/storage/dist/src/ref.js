import { parse } from "reference-spec-reader";
import { fetch_range, merge_init, strip_prefix, uri2href } from "./util.js";
/**
 * This is for the "binary" loader (custom code is ~2x faster than "atob") from esbuild.
 * https://github.com/evanw/esbuild/blob/150a01844d47127c007c2b1973158d69c560ca21/internal/runtime/runtime.go#L185
 */
let table = new Uint8Array(128);
for (let i = 0; i < 64; i++) {
    table[i < 26 ? i + 65 : i < 52 ? i + 71 : i < 62 ? i - 4 : i * 4 - 205] = i;
}
export function to_binary(base64) {
    const n = base64.length;
    const bytes = new Uint8Array(
    // @ts-ignore
    (((n - (base64[n - 1] === "=") - (base64[n - 2] === "=")) * 3) / 4) | 0);
    for (let i = 0, j = 0; i < n;) {
        const c0 = table[base64.charCodeAt(i++)];
        const c1 = table[base64.charCodeAt(i++)];
        const c2 = table[base64.charCodeAt(i++)];
        const c3 = table[base64.charCodeAt(i++)];
        bytes[j++] = (c0 << 2) | (c1 >> 4);
        bytes[j++] = (c1 << 4) | (c2 >> 2);
        bytes[j++] = (c2 << 6) | c3;
    }
    return bytes;
}
/** @experimental */
class ReferenceStore {
    #refs;
    #opts;
    #overrides;
    constructor(refs, opts = {}) {
        this.#refs = Promise.resolve(refs);
        this.#opts = opts;
        this.#overrides = opts.overrides || {};
    }
    async get(key, opts = {}) {
        let ref = (await this.#refs).get(strip_prefix(key));
        if (!ref)
            return;
        if (typeof ref === "string") {
            if (ref.startsWith("base64:")) {
                return to_binary(ref.slice("base64:".length));
            }
            return new TextEncoder().encode(ref);
        }
        let [urlOrNull, offset, size] = ref;
        let url = urlOrNull ?? this.#opts.target;
        if (!url) {
            throw Error(`No url for key ${key}, and no target url provided.`);
        }
        let res = await fetch_range(uri2href(url), offset, size, merge_init(this.#overrides, opts));
        if (res.status === 200 || res.status === 206) {
            return new Uint8Array(await res.arrayBuffer());
        }
        throw new Error(`Request unsuccessful for key ${key}. Response status: ${res.status}.`);
    }
    static fromSpec(spec, opts) {
        // @ts-expect-error - TS doesn't like the type of `parse`
        let refs = Promise.resolve(spec).then((spec) => parse(spec));
        return new ReferenceStore(refs, opts);
    }
    static fromUrl(url, opts) {
        let spec = fetch(url, opts?.overrides).then((res) => res.json());
        return ReferenceStore.fromSpec(spec, opts);
    }
}
export default ReferenceStore;
//# sourceMappingURL=ref.js.map