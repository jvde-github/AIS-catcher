export interface CodecConstructor<T> {
    fromConfig(config: T & {
        id: string;
    }): Codec;
    codecId: string;
}
export interface Codec {
    encode(data: Uint8Array): Uint8Array | Promise<Uint8Array>;
    decode(data: Uint8Array, out?: Uint8Array): Uint8Array | Promise<Uint8Array>;
}
