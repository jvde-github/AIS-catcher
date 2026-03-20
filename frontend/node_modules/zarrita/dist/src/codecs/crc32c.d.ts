export declare class Crc32cCodec {
    readonly kind = "bytes_to_bytes";
    static fromConfig(): Crc32cCodec;
    encode(_: Uint8Array): Uint8Array;
    decode(arr: Uint8Array): Uint8Array;
}
