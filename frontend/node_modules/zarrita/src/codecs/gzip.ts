import { decompress } from "../util.js";

interface GzipCodecConfig {
	level: number;
}

export class GzipCodec {
	kind = "bytes_to_bytes";

	static fromConfig(_: GzipCodecConfig) {
		return new GzipCodec();
	}

	encode(_bytes: Uint8Array): never {
		throw new Error(
			"Gzip encoding is not enabled by default. Please register a custom codec with `numcodecs/gzip`.",
		);
	}

	async decode(bytes: Uint8Array): Promise<Uint8Array> {
		const buffer = await decompress(bytes, { format: "gzip" });
		return new Uint8Array(buffer);
	}
}
