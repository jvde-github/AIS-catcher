import { create_codec_pipeline } from "../codecs.js";
import { assert } from "../util.js";
const MAX_BIG_UINT = 18446744073709551615n;
export function create_sharded_chunk_getter(location, shard_shape, encode_shard_key, sharding_config) {
    assert(location.store.getRange, "Store does not support range requests");
    let get_range = location.store.getRange.bind(location.store);
    let index_shape = shard_shape.map((d, i) => d / sharding_config.chunk_shape[i]);
    let index_codec = create_codec_pipeline({
        data_type: "uint64",
        shape: [...index_shape, 2],
        codecs: sharding_config.index_codecs,
    });
    let cache = {};
    return async (chunk_coord, options) => {
        let shard_coord = chunk_coord.map((d, i) => Math.floor(d / index_shape[i]));
        let shard_path = location.resolve(encode_shard_key(shard_coord)).path;
        let index;
        if (shard_path in cache) {
            index = cache[shard_path];
        }
        else {
            let checksum_size = 4;
            let index_size = 16 * index_shape.reduce((a, b) => a * b, 1);
            let bytes = await get_range(shard_path, {
                suffixLength: index_size + checksum_size,
            }, options);
            index = cache[shard_path] = bytes
                ? await index_codec.decode(bytes)
                : null;
        }
        if (index === null) {
            return undefined;
        }
        let { data, shape, stride } = index;
        let linear_offset = chunk_coord
            .map((d, i) => d % shape[i])
            .reduce((acc, sel, idx) => acc + sel * stride[idx], 0);
        let offset = data[linear_offset];
        let length = data[linear_offset + 1];
        // write null chunk when 2^64-1 indicates fill value
        if (offset === MAX_BIG_UINT && length === MAX_BIG_UINT) {
            return undefined;
        }
        return get_range(shard_path, {
            offset: Number(offset),
            length: Number(length),
        }, options);
    };
}
//# sourceMappingURL=sharding.js.map