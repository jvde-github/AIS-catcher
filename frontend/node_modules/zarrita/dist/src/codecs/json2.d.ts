import type { Chunk, ObjectType } from "../metadata.js";
type EncoderConfig = {
    encoding?: "utf-8";
    skipkeys?: boolean;
    ensure_ascii?: boolean;
    check_circular?: boolean;
    allow_nan?: boolean;
    sort_keys?: boolean;
    indent?: number;
    separators?: [string, string];
};
type DecoderConfig = {
    strict?: boolean;
};
type JsonCodecConfig = EncoderConfig & DecoderConfig;
export declare class JsonCodec {
    #private;
    configuration: JsonCodecConfig;
    kind: string;
    constructor(configuration?: JsonCodecConfig);
    static fromConfig(configuration: JsonCodecConfig): JsonCodec;
    encode(buf: Chunk<ObjectType>): Uint8Array;
    decode(bytes: Uint8Array): Chunk<ObjectType>;
}
export {};
