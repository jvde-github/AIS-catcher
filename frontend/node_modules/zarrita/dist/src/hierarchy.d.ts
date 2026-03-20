import type { AbsolutePath, Readable } from "@zarrita/storage";
import { create_codec_pipeline } from "./codecs.js";
import type { ArrayMetadata, Attributes, Chunk, DataType, GroupMetadata, Scalar, TypedArrayConstructor } from "./metadata.js";
import { type DataTypeQuery, type NarrowDataType } from "./util.js";
export declare class Location<Store> {
    readonly store: Store;
    readonly path: AbsolutePath;
    constructor(store: Store, path?: AbsolutePath);
    resolve(path: string): Location<Store>;
}
export declare function root<Store>(store: Store): Location<Store>;
export declare function root(): Location<Map<string, Uint8Array>>;
export declare class Group<Store extends Readable> extends Location<Store> {
    #private;
    readonly kind = "group";
    constructor(store: Store, path: AbsolutePath, metadata: GroupMetadata);
    get attrs(): Attributes;
}
declare const CONTEXT_MARKER: unique symbol;
export declare function get_context<T>(obj: {
    [CONTEXT_MARKER]: T;
}): T;
/** For internal use only, and is subject to change. */
interface ArrayContext<Store extends Readable, D extends DataType> {
    kind: "sharded" | "regular";
    /** The codec pipeline for this array. */
    codec: ReturnType<typeof create_codec_pipeline<D>>;
    /** Encode a chunk key from chunk coordinates. */
    encode_chunk_key(chunk_coords: number[]): string;
    /** The TypedArray constructor for this array chunks. */
    TypedArray: TypedArrayConstructor<D>;
    /** A function to get the strides for a given shape, using the array order */
    get_strides(shape: number[]): number[];
    /** The fill value for this array. */
    fill_value: Scalar<D> | null;
    /** A function to get the bytes for a given chunk. */
    get_chunk_bytes(chunk_coords: number[], options?: Parameters<Store["get"]>[1]): Promise<Uint8Array | undefined>;
    /** The chunk shape for this array. */
    chunk_shape: number[];
}
export declare class Array<Dtype extends DataType, Store extends Readable = Readable> extends Location<Store> {
    #private;
    readonly kind = "array";
    [CONTEXT_MARKER]: ArrayContext<Store, Dtype>;
    constructor(store: Store, path: AbsolutePath, metadata: ArrayMetadata<Dtype>);
    get attrs(): Attributes;
    get shape(): number[];
    get chunks(): number[];
    get dtype(): Dtype;
    getChunk(chunk_coords: number[], options?: Parameters<Store["get"]>[1]): Promise<Chunk<Dtype>>;
    /**
     * A helper method to narrow `zarr.Array` Dtype.
     *
     * ```typescript
     * let arr: zarr.Array<DataType, FetchStore> = zarr.open(store, { kind: "array" });
     *
     * // Option 1: narrow by scalar type (e.g. "bool", "raw", "bigint", "number")
     * if (arr.is("bigint")) {
     *   // zarr.Array<"int64" | "uint64", FetchStore>
     * }
     *
     * // Option 3: exact match
     * if (arr.is("float32")) {
     *   // zarr.Array<"float32", FetchStore, "/">
     * }
     * ```
     */
    is<Query extends DataTypeQuery>(query: Query): this is Array<NarrowDataType<Dtype, Query>, Store>;
}
export {};
