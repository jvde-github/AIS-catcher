export function strip_prefix(path) {
    // @ts-expect-error - TS can't infer this type correctly
    return path.slice(1);
}
export function uri2href(url) {
    let [protocol, rest] = (typeof url === "string" ? url : url.href).split("://");
    if (protocol === "https" || protocol === "http") {
        return url;
    }
    if (protocol === "gc") {
        return `https://storage.googleapis.com/${rest}`;
    }
    if (protocol === "s3") {
        return `https://s3.amazonaws.com/${rest}`;
    }
    throw Error(`Protocol not supported, got: ${JSON.stringify(protocol)}`);
}
export function fetch_range(url, offset, length, opts = {}) {
    if (offset !== undefined && length !== undefined) {
        // merge request opts
        opts = {
            ...opts,
            headers: {
                ...opts.headers,
                Range: `bytes=${offset}-${offset + length - 1}`,
            },
        };
    }
    return fetch(url, opts);
}
export function merge_init(storeOverrides, requestOverrides) {
    // Request overrides take precedence over storeOverrides.
    return {
        ...storeOverrides,
        ...requestOverrides,
        headers: {
            ...storeOverrides.headers,
            ...requestOverrides.headers,
        },
    };
}
/**
 * Make an assertion.
 *
 * Usage
 * @example
 * ```ts
 * const value: boolean = Math.random() <= 0.5;
 * assert(value, "value is greater than than 0.5!");
 * value // true
 * ```
 *
 * @param expression - The expression to test.
 * @param msg - The optional message to display if the assertion fails.
 * @throws an {@link Error} if `expression` is not truthy.
 */
export function assert(expression, msg = "") {
    if (!expression)
        throw new Error(msg);
}
//# sourceMappingURL=util.js.map