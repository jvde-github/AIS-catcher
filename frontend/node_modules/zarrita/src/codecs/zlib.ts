import { decompress } from "../util.js";

interface ZlibCodecConfig {
	level: number;
}

export class ZlibCodec {
	kind = "bytes_to_bytes";

	static fromConfig(_: ZlibCodecConfig) {
		return new ZlibCodec();
	}

	encode(_bytes: Uint8Array): never {
		throw new Error(
			"Zlib encoding is not enabled by default. Please register a codec with `numcodecs/zlib`.",
		);
	}

	async decode(bytes: Uint8Array): Promise<Uint8Array> {
		const buffer = await decompress(bytes, { format: "deflate" });
		return new Uint8Array(buffer);
	}
}
