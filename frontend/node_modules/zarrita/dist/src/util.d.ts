import type { ArrayMetadata, ArrayMetadataV2, BigintDataType, CodecMetadata, DataType, GroupMetadata, NumberDataType, ObjectType, Scalar, StringDataType, TypedArrayConstructor } from "./metadata.js";
export declare function json_encode_object(o: Record<string, unknown>): Uint8Array;
export declare function json_decode_object(bytes: Uint8Array): any;
export declare function byteswap_inplace(view: Uint8Array, bytes_per_element: number): void;
export declare function get_ctr<D extends DataType>(data_type: D): TypedArrayConstructor<D>;
/** Compute strides for 'C' or 'F' ordered array from shape */
export declare function get_strides(shape: readonly number[], order: "C" | "F" | Array<number>): Array<number>;
export declare function create_chunk_key_encoder({ name, configuration, }: ArrayMetadata["chunk_key_encoding"]): (chunk_coords: number[]) => string;
export declare function v2_to_v3_array_metadata(meta: ArrayMetadataV2, attributes?: Record<string, unknown>): ArrayMetadata<DataType>;
export declare function v2_to_v3_group_metadata(_meta: unknown, attributes?: Record<string, unknown>): GroupMetadata;
export type DataTypeQuery = DataType | "boolean" | "number" | "bigint" | "object" | "string";
export type NarrowDataType<Dtype extends DataType, Query extends DataTypeQuery> = Query extends "number" ? NumberDataType : Query extends "bigint" ? BigintDataType : Query extends "string" ? StringDataType : Query extends "object" ? ObjectType : Extract<Query, Dtype>;
export declare function is_dtype<Query extends DataTypeQuery>(dtype: DataType, query: Query): dtype is NarrowDataType<DataType, Query>;
export type ShardingCodecMetadata = {
    name: "sharding_indexed";
    configuration: {
        chunk_shape: number[];
        codecs: CodecMetadata[];
        index_codecs: CodecMetadata[];
    };
};
export declare function is_sharding_codec(codec: CodecMetadata): codec is ShardingCodecMetadata;
export declare function ensure_correct_scalar<D extends DataType>(metadata: ArrayMetadata<D>): Scalar<D> | null;
type InstanceType<T> = T extends new (...args: any[]) => infer R ? R : never;
type ErrorConstructor = new (...args: any[]) => Error;
/**
 * Ensures an error matches expected type(s), otherwise rethrows.
 *
 * Unmatched errors bubble up, like Python's `except`. Narrows error types for
 * type-safe property access.
 *
 * @see {@link https://gist.github.com/manzt/3702f19abb714e21c22ce48851c75abf}
 *
 * @example
 * ```ts
 * class DatabaseError extends Error { }
 * class NetworkError extends Error { }
 *
 * try {
 *   await db.query();
 * } catch (err) {
 *   rethrow_unless(err, DatabaseError, NetworkError);
 *   err // DatabaseError | NetworkError
 * }
 * ```
 *
 * @param error - The error to check
 * @param errors - Expected error type(s)
 * @throws The original error if it doesn't match expected type(s)
 */
export declare function rethrow_unless<E extends ReadonlyArray<ErrorConstructor>>(error: unknown, ...errors: E): asserts error is InstanceType<E[number]>;
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
/**
 * @param {ArrayBuffer |ArrayBufferView | Response} data
 * @param {Object} options
 * @param {CompressionFormat} options.format
 * @param {AbortSignal} [options.signal]
 *
 * @returns {Promise<ArrayBuffer>}
 */
export declare function decompress(data: ArrayBuffer | ArrayBufferView | Response, { format, signal }: {
    format: CompressionFormat;
    signal?: AbortSignal;
}): Promise<ArrayBuffer>;
export {};
