import type { AbsolutePath } from "./types.js";
export declare function strip_prefix<Path extends AbsolutePath>(path: Path): Path extends AbsolutePath<infer Rest> ? Rest : never;
export declare function uri2href(url: string | URL): string | URL;
export declare function fetch_range(url: string | URL, offset?: number, length?: number, opts?: RequestInit): Promise<Response>;
export declare function merge_init(storeOverrides: RequestInit, requestOverrides: RequestInit): {
    headers: {};
    body?: BodyInit | null;
    cache?: RequestCache;
    credentials?: RequestCredentials;
    integrity?: string;
    keepalive?: boolean;
    method?: string;
    mode?: RequestMode;
    priority?: RequestPriority;
    redirect?: RequestRedirect;
    referrer?: string;
    referrerPolicy?: ReferrerPolicy;
    signal?: AbortSignal | null;
    window?: null;
};
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
export declare function assert(expression: unknown, msg?: string | undefined): asserts expression;
