interface GzipCodecConfig {
    level: number;
}
export declare class GzipCodec {
    kind: string;
    static fromConfig(_: GzipCodecConfig): GzipCodec;
    encode(_bytes: Uint8Array): never;
    decode(bytes: Uint8Array): Promise<Uint8Array>;
}
export {};
