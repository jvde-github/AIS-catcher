import type { Codec as _Codec } from "numcodecs";
import type { Chunk, CodecMetadata, DataType } from "./metadata.js";
type ChunkMetadata<D extends DataType> = {
    data_type: D;
    shape: number[];
    codecs: CodecMetadata[];
};
type CodecEntry = {
    fromConfig: (config: unknown, meta: ChunkMetadata<DataType>) => Codec;
    kind?: "array_to_array" | "array_to_bytes" | "bytes_to_bytes";
};
type Codec = _Codec & {
    kind: CodecEntry["kind"];
};
export declare const registry: Map<string, () => Promise<CodecEntry>>;
export declare function create_codec_pipeline<Dtype extends DataType>(chunk_metadata: ChunkMetadata<Dtype>): {
    encode(chunk: Chunk<Dtype>): Promise<Uint8Array>;
    decode(bytes: Uint8Array): Promise<Chunk<Dtype>>;
};
export {};
