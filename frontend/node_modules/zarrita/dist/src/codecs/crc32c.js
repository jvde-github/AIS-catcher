export class Crc32cCodec {
    kind = "bytes_to_bytes";
    static fromConfig() {
        return new Crc32cCodec();
    }
    encode(_) {
        throw new Error("Not implemented");
    }
    decode(arr) {
        return new Uint8Array(arr.buffer, arr.byteOffset, arr.byteLength - 4);
    }
}
//# sourceMappingURL=crc32c.js.map