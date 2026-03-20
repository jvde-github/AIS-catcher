import { decompress } from "../util.js";
export class ZlibCodec {
    kind = "bytes_to_bytes";
    static fromConfig(_) {
        return new ZlibCodec();
    }
    encode(_bytes) {
        throw new Error("Zlib encoding is not enabled by default. Please register a codec with `numcodecs/zlib`.");
    }
    async decode(bytes) {
        const buffer = await decompress(bytes, { format: "deflate" });
        return new Uint8Array(buffer);
    }
}
//# sourceMappingURL=zlib.js.map