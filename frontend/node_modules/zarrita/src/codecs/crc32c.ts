export class Crc32cCodec {
	readonly kind = "bytes_to_bytes";
	static fromConfig() {
		return new Crc32cCodec();
	}
	encode(_: Uint8Array): Uint8Array {
		throw new Error("Not implemented");
	}
	decode(arr: Uint8Array): Uint8Array {
		return new Uint8Array(arr.buffer, arr.byteOffset, arr.byteLength - 4);
	}
}
