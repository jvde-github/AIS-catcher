import type { Chunk, String } from "../metadata.js";
import { get_strides } from "../util.js";

export class VLenUTF8 {
	readonly kind = "array_to_bytes";
	#shape: number[];
	#strides: number[];

	constructor(shape: number[]) {
		this.#shape = shape;
		this.#strides = get_strides(shape, "C");
	}
	static fromConfig(_: unknown, meta: { shape: number[] }) {
		return new VLenUTF8(meta.shape);
	}

	encode(_chunk: Chunk<String>): Uint8Array {
		throw new Error("Method not implemented.");
	}

	decode(bytes: Uint8Array): Chunk<String> {
		let decoder = new TextDecoder();
		let view = new DataView(bytes.buffer);
		let data: Array<string> = Array(view.getUint32(0, true));
		let pos = 4;
		for (let i = 0; i < data.length; i++) {
			let item_length = view.getUint32(pos, true);
			pos += 4;
			data[i] = decoder.decode(
				(bytes.buffer as ArrayBuffer).slice(pos, pos + item_length),
			);
			pos += item_length;
		}
		return { data, shape: this.#shape, stride: this.#strides };
	}
}
