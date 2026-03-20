import { decompress } from "../util.js";
export class GzipCodec {
    kind = "bytes_to_bytes";
    static fromConfig(_) {
        return new GzipCodec();
    }
    encode(_bytes) {
        throw new Error("Gzip encoding is not enabled by default. Please register a custom codec with `numcodecs/gzip`.");
    }
    async decode(bytes) {
        const buffer = await decompress(bytes, { format: "gzip" });
        return new Uint8Array(buffer);
    }
}
//# sourceMappingURL=gzip.js.map