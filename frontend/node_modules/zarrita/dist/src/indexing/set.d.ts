import type { Mutable } from "@zarrita/storage";
import { type Array } from "../hierarchy.js";
import type { Chunk, DataType, Scalar } from "../metadata.js";
import type { Prepare, SetFromChunk, SetOptions, SetScalar, Slice } from "./types.js";
export declare function set<Dtype extends DataType, Arr extends Chunk<Dtype>>(arr: Array<Dtype, Mutable>, selection: (number | Slice | null)[] | null, value: Scalar<Dtype> | Arr, opts: SetOptions, setter: {
    prepare: Prepare<Dtype, Arr>;
    set_scalar: SetScalar<Dtype, Arr>;
    set_from_chunk: SetFromChunk<Dtype, Arr>;
}): Promise<void>;
