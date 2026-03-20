import type { Codec as _Codec } from "numcodecs";
import { BitroundCodec } from "./codecs/bitround.js";
import { BytesCodec } from "./codecs/bytes.js";
import { Crc32cCodec } from "./codecs/crc32c.js";
import { GzipCodec } from "./codecs/gzip.js";
import { JsonCodec } from "./codecs/json2.js";
import { TransposeCodec } from "./codecs/transpose.js";
import { VLenUTF8 } from "./codecs/vlen-utf8.js";
import { ZlibCodec } from "./codecs/zlib.js";
import type { Chunk, CodecMetadata, DataType } from "./metadata.js";
import { assert } from "./util.js";

type ChunkMetadata<D extends DataType> = {
	data_type: D;
	shape: number[];
	codecs: CodecMetadata[];
};

type CodecEntry = {
	fromConfig: (config: unknown, meta: ChunkMetadata<DataType>) => Codec;
	kind?: "array_to_array" | "array_to_bytes" | "bytes_to_bytes";
};

type Codec = _Codec & { kind: CodecEntry["kind"] };

function create_default_registry(): Map<string, () => Promise<CodecEntry>> {
	return new Map()
		.set("blosc", () => import("numcodecs/blosc").then((m) => m.default))
		.set("lz4", () => import("numcodecs/lz4").then((m) => m.default))
		.set("zstd", () => import("numcodecs/zstd").then((m) => m.default))
		.set("gzip", () => GzipCodec)
		.set("zlib", () => ZlibCodec)
		.set("transpose", () => TransposeCodec)
		.set("bytes", () => BytesCodec)
		.set("crc32c", () => Crc32cCodec)
		.set("vlen-utf8", () => VLenUTF8)
		.set("json2", () => JsonCodec)
		.set("bitround", () => BitroundCodec);
}

export const registry: Map<string, () => Promise<CodecEntry>> =
	create_default_registry();

export function create_codec_pipeline<Dtype extends DataType>(
	chunk_metadata: ChunkMetadata<Dtype>,
): {
	encode(chunk: Chunk<Dtype>): Promise<Uint8Array>;
	decode(bytes: Uint8Array): Promise<Chunk<Dtype>>;
} {
	let codecs: Awaited<ReturnType<typeof load_codecs>>;
	return {
		async encode(chunk: Chunk<Dtype>): Promise<Uint8Array> {
			if (!codecs) codecs = await load_codecs(chunk_metadata);
			for (const codec of codecs.array_to_array) {
				chunk = await codec.encode(chunk);
			}
			let bytes = await codecs.array_to_bytes.encode(chunk);
			for (const codec of codecs.bytes_to_bytes) {
				bytes = await codec.encode(bytes);
			}
			return bytes;
		},
		async decode(bytes: Uint8Array): Promise<Chunk<Dtype>> {
			if (!codecs) codecs = await load_codecs(chunk_metadata);
			for (let i = codecs.bytes_to_bytes.length - 1; i >= 0; i--) {
				bytes = await codecs.bytes_to_bytes[i].decode(bytes);
			}
			let chunk = await codecs.array_to_bytes.decode(bytes);
			for (let i = codecs.array_to_array.length - 1; i >= 0; i--) {
				chunk = await codecs.array_to_array[i].decode(chunk);
			}
			return chunk;
		},
	};
}

type ArrayToArrayCodec<D extends DataType> = {
	encode: (data: Chunk<D>) => Promise<Chunk<D>> | Chunk<D>;
	decode: (data: Chunk<D>) => Promise<Chunk<D>> | Chunk<D>;
};

type ArrayToBytesCodec<D extends DataType> = {
	encode: (data: Chunk<D>) => Promise<Uint8Array> | Uint8Array;
	decode: (data: Uint8Array) => Promise<Chunk<D>> | Chunk<D>;
};

type BytesToBytesCodec = {
	encode: (data: Uint8Array) => Promise<Uint8Array>;
	decode: (data: Uint8Array) => Promise<Uint8Array>;
};

async function load_codecs<D extends DataType>(chunk_meta: ChunkMetadata<D>) {
	let promises = chunk_meta.codecs.map(async (meta) => {
		let Codec = await registry.get(meta.name)?.();
		assert(Codec, `Unknown codec: ${meta.name}`);
		return { Codec, meta };
	});
	let array_to_array: ArrayToArrayCodec<D>[] = [];
	let array_to_bytes: ArrayToBytesCodec<D> | undefined;
	let bytes_to_bytes: BytesToBytesCodec[] = [];
	for await (let { Codec, meta } of promises) {
		let codec = Codec.fromConfig(meta.configuration, chunk_meta);
		switch (codec.kind) {
			case "array_to_array":
				array_to_array.push(codec as unknown as ArrayToArrayCodec<D>);
				break;
			case "array_to_bytes":
				array_to_bytes = codec as unknown as ArrayToBytesCodec<D>;
				break;
			default:
				bytes_to_bytes.push(codec as unknown as BytesToBytesCodec);
		}
	}
	if (!array_to_bytes) {
		assert(
			is_typed_array_like_meta(chunk_meta),
			`Cannot encode ${chunk_meta.data_type} to bytes without a codec`,
		);
		array_to_bytes = BytesCodec.fromConfig({ endian: "little" }, chunk_meta);
	}
	return { array_to_array, array_to_bytes, bytes_to_bytes };
}

function is_typed_array_like_meta<D extends DataType>(
	meta: ChunkMetadata<D>,
): meta is ChunkMetadata<Exclude<D, "v2:object" | "string">> {
	return meta.data_type !== "v2:object" && meta.data_type !== "string";
}
