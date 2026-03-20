interface ZlibCodecConfig {
    level: number;
}
export declare class ZlibCodec {
    kind: string;
    static fromConfig(_: ZlibCodecConfig): ZlibCodec;
    encode(_bytes: Uint8Array): never;
    decode(bytes: Uint8Array): Promise<Uint8Array>;
}
export {};
