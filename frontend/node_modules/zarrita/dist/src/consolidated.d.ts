import type { AbsolutePath, Readable } from "@zarrita/storage";
/**
 * Represents a read-only store that can list its contents.
 */
export interface Listable<Store extends Readable> {
    /** Get the bytes at a given path. */
    get: (...args: Parameters<Store["get"]>) => Promise<Uint8Array | undefined>;
    /** Get a byte range at a given path. */
    getRange: Store["getRange"];
    /** List the contents of the store. */
    contents(): {
        path: AbsolutePath;
        kind: "array" | "group";
    }[];
}
/** Options for {@linkcode withConsolidated} and {@linkcode tryWithConsolidated}. */
export interface WithConsolidatedOptions {
    /**
     * Key to read consolidated metadata from.
     *
     * @default {".zmetadata"}
     */
    readonly metadataKey?: string;
}
/**
 * Open a consolidated store.
 *
 * This will open a store with Zarr v2 consolidated metadata (`.zmetadata`).
 * @see {@link https://zarr.readthedocs.io/en/stable/spec/v2.html#consolidated-metadata}
 *
 * @param store The store to open.
 * @param opts Options object.
 * @returns A listable store.
 *
 * @example
 * ```js
 * let store = await withConsolidated(
 *   new zarr.FetchStore("https://my-bucket.s3.amazonaws.com");
 * );
 * store.contents(); // [{ path: "/", kind: "group" }, { path: "/foo", kind: "array" }, ...]
 * let grp = zarr.open(store); // Open the root group.
 * let foo = zarr.open(grp.resolve(contents[1].path)); // Open the foo array
 * ```
 */
export declare function withConsolidated<Store extends Readable>(store: Store, opts?: WithConsolidatedOptions): Promise<Listable<Store>>;
/**
 * Try to open a consolidated store, but fall back to the original store if the
 * consolidated metadata is missing.
 *
 * Provides a convenient way to open a store that may or may not have consolidated,
 * returning a consistent interface for both cases. Ideal for usage senarios with
 * known access paths, since store with consolidated metadata do not incur
 * additional network requests when accessing underlying groups and arrays.
 *
 * @param store The store to open.
 * @param opts Options to pass to withConsolidated.
 * @returns A listable store.
 */
export declare function tryWithConsolidated<Store extends Readable>(store: Store, opts?: WithConsolidatedOptions): Promise<Listable<Store> | Store>;
