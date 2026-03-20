import type {
	Chunk,
	DataType,
	Scalar,
	TypedArray,
	TypedArrayConstructor,
} from "../metadata.js";
import {
	BoolArray,
	ByteStringArray,
	UnicodeStringArray,
} from "../typedarray.js";
import { assert, get_strides } from "../util.js";

type TypedArrayProxy<D extends DataType> = {
	[x: number]: Scalar<D>;
};

function proxy<D extends DataType>(arr: TypedArray<D>): TypedArrayProxy<D> {
	if (
		arr instanceof BoolArray ||
		arr instanceof ByteStringArray ||
		arr instanceof UnicodeStringArray
	) {
		// @ts-expect-error - TS cannot infer arr is a TypedArrayProxy<D>
		const arrp: TypedArrayProxy<D> = new Proxy(arr, {
			get(target, prop) {
				return target.get(Number(prop));
			},
			set(target, prop, value) {
				// @ts-expect-error - value is OK
				target.set(Number(prop), value);
				return true;
			},
		});
		return arrp;
	}
	// @ts-expect-error - TS cannot infer arr is a TypedArrayProxy<D>
	return arr;
}

function empty_like<D extends DataType>(
	chunk: Chunk<D>,
	order: Order,
): Chunk<D> {
	let data: TypedArray<D>;
	if (
		chunk.data instanceof ByteStringArray ||
		chunk.data instanceof UnicodeStringArray
	) {
		data = new (chunk.constructor as TypedArrayConstructor<D>)(
			// @ts-expect-error
			chunk.data.length,
			chunk.data.chars,
		);
	} else {
		data = new (chunk.constructor as TypedArrayConstructor<D>)(
			chunk.data.length,
		);
	}
	return {
		data,
		shape: chunk.shape,
		stride: get_strides(chunk.shape, order),
	};
}

function convert_array_order<D extends DataType>(
	src: Chunk<D>,
	target: Order,
): Chunk<D> {
	let out = empty_like(src, target);
	let n_dims = src.shape.length;
	let size = src.data.length;
	let index = Array(n_dims).fill(0);

	let src_data = proxy(src.data);
	let out_data = proxy(out.data);

	for (let src_idx = 0; src_idx < size; src_idx++) {
		let out_idx = 0;
		for (let dim = 0; dim < n_dims; dim++) {
			out_idx += index[dim] * out.stride[dim];
		}
		out_data[out_idx] = src_data[src_idx];

		index[0] += 1;
		for (let dim = 0; dim < n_dims; dim++) {
			if (index[dim] === src.shape[dim]) {
				if (dim + 1 === n_dims) {
					break;
				}
				index[dim] = 0;
				index[dim + 1] += 1;
			}
		}
	}

	return out;
}

/** Determine the memory order (axis permutation) for a chunk */
function get_order(chunk: Chunk<DataType>): number[] {
	let rank = chunk.shape.length;
	assert(
		rank === chunk.stride.length,
		"Shape and stride must have the same length.",
	);
	return chunk.stride
		.map((s, i) => ({ stride: s, index: i }))
		.sort((a, b) => b.stride - a.stride)
		.map((entry) => entry.index);
}

function matches_order(chunk: Chunk<DataType>, target: Order) {
	let source = get_order(chunk);
	assert(source.length === target.length, "Orders must match");
	return source.every((dim, i) => dim === target[i]);
}

type Order = "C" | "F" | Array<number>;

export class TransposeCodec {
	kind = "array_to_array";
	#order: Array<number>;
	#inverseOrder: Array<number>;

	constructor(configuration: { order?: Order }, meta: { shape: number[] }) {
		let value = configuration.order ?? "C";
		let rank = meta.shape.length;
		let order = new Array<number>(rank);
		let inverseOrder = new Array<number>(rank);

		if (value === "C") {
			for (let i = 0; i < rank; ++i) {
				order[i] = i;
				inverseOrder[i] = i;
			}
		} else if (value === "F") {
			for (let i = 0; i < rank; ++i) {
				order[i] = rank - i - 1;
				inverseOrder[i] = rank - i - 1;
			}
		} else {
			order = value;
			order.forEach((x, i) => {
				assert(
					inverseOrder[x] === undefined,
					`Invalid permutation: ${JSON.stringify(value)}`,
				);
				inverseOrder[x] = i;
			});
		}

		this.#order = order;
		this.#inverseOrder = inverseOrder;
	}

	static fromConfig(
		configuration: { order: Order },
		meta: { shape: number[] },
	) {
		return new TransposeCodec(configuration, meta);
	}

	encode<D extends DataType>(arr: Chunk<D>): Chunk<D> {
		if (matches_order(arr, this.#inverseOrder)) {
			// can skip making a copy
			return arr;
		}
		return convert_array_order(arr, this.#inverseOrder);
	}

	decode<D extends DataType>(arr: Chunk<D>): Chunk<D> {
		return {
			data: arr.data,
			shape: arr.shape,
			stride: get_strides(arr.shape, this.#order),
		};
	}
}
