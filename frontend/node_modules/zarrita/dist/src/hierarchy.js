import { create_sharded_chunk_getter } from "./codecs/sharding.js";
import { create_codec_pipeline } from "./codecs.js";
import { create_chunk_key_encoder, ensure_correct_scalar, get_ctr, get_strides, is_dtype, is_sharding_codec, } from "./util.js";
export class Location {
    store;
    path;
    constructor(store, path = "/") {
        this.store = store;
        this.path = path;
    }
    resolve(path) {
        // reuse URL resolution logic built into the browser
        // handles relative paths, absolute paths, etc.
        let root = new URL(`file://${this.path.endsWith("/") ? this.path : `${this.path}/`}`);
        return new Location(this.store, decodeURIComponent(new URL(path, root).pathname));
    }
}
export function root(store) {
    return new Location(store ?? new Map());
}
export class Group extends Location {
    kind = "group";
    #metadata;
    constructor(store, path, metadata) {
        super(store, path);
        this.#metadata = metadata;
    }
    get attrs() {
        return this.#metadata.attributes;
    }
}
function get_array_order(codecs) {
    const maybe_transpose_codec = codecs.find((c) => c.name === "transpose");
    // @ts-expect-error - TODO: Should validate?
    return maybe_transpose_codec?.configuration?.order ?? "C";
}
const CONTEXT_MARKER = Symbol("zarrita.context");
export function get_context(obj) {
    return obj[CONTEXT_MARKER];
}
function create_context(location, metadata) {
    let { configuration } = metadata.codecs.find(is_sharding_codec) ?? {};
    let shared_context = {
        encode_chunk_key: create_chunk_key_encoder(metadata.chunk_key_encoding),
        TypedArray: get_ctr(metadata.data_type),
        fill_value: metadata.fill_value,
    };
    if (configuration) {
        let native_order = get_array_order(configuration.codecs);
        return {
            ...shared_context,
            kind: "sharded",
            chunk_shape: configuration.chunk_shape,
            codec: create_codec_pipeline({
                data_type: metadata.data_type,
                shape: configuration.chunk_shape,
                codecs: configuration.codecs,
            }),
            get_strides(shape) {
                return get_strides(shape, native_order);
            },
            get_chunk_bytes: create_sharded_chunk_getter(location, metadata.chunk_grid.configuration.chunk_shape, shared_context.encode_chunk_key, configuration),
        };
    }
    let native_order = get_array_order(metadata.codecs);
    return {
        ...shared_context,
        kind: "regular",
        chunk_shape: metadata.chunk_grid.configuration.chunk_shape,
        codec: create_codec_pipeline({
            data_type: metadata.data_type,
            shape: metadata.chunk_grid.configuration.chunk_shape,
            codecs: metadata.codecs,
        }),
        get_strides(shape) {
            return get_strides(shape, native_order);
        },
        async get_chunk_bytes(chunk_coords, options) {
            let chunk_key = shared_context.encode_chunk_key(chunk_coords);
            let chunk_path = location.resolve(chunk_key).path;
            return location.store.get(chunk_path, options);
        },
    };
}
export class Array extends Location {
    kind = "array";
    #metadata;
    [CONTEXT_MARKER];
    constructor(store, path, metadata) {
        super(store, path);
        this.#metadata = {
            ...metadata,
            fill_value: ensure_correct_scalar(metadata),
        };
        this[CONTEXT_MARKER] = create_context(this, metadata);
    }
    get attrs() {
        return this.#metadata.attributes;
    }
    get shape() {
        return this.#metadata.shape;
    }
    get chunks() {
        return this[CONTEXT_MARKER].chunk_shape;
    }
    get dtype() {
        return this.#metadata.data_type;
    }
    async getChunk(chunk_coords, options) {
        let context = this[CONTEXT_MARKER];
        let maybe_bytes = await context.get_chunk_bytes(chunk_coords, options);
        if (!maybe_bytes) {
            let size = context.chunk_shape.reduce((a, b) => a * b, 1);
            let data = new context.TypedArray(size);
            // @ts-expect-error: TS can't infer that `fill_value` is union (assumes never) but this is ok
            data.fill(context.fill_value);
            return {
                data,
                shape: context.chunk_shape,
                stride: context.get_strides(context.chunk_shape),
            };
        }
        return context.codec.decode(maybe_bytes);
    }
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
    is(query) {
        return is_dtype(this.dtype, query);
    }
}
//# sourceMappingURL=hierarchy.js.map