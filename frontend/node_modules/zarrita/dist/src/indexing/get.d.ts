import type { Readable } from "@zarrita/storage";
import { type Array } from "../hierarchy.js";
import type { Chunk, DataType, Scalar } from "../metadata.js";
import type { GetOptions, Prepare, SetFromChunk, SetScalar, Slice } from "./types.js";
export declare function get<D extends DataType, Store extends Readable, Arr extends Chunk<D>, Sel extends (null | Slice | number)[]>(arr: Array<D, Store>, selection: null | Sel, opts: GetOptions<Parameters<Store["get"]>[1]>, setter: {
    prepare: Prepare<D, Arr>;
    set_scalar: SetScalar<D, Arr>;
    set_from_chunk: SetFromChunk<D, Arr>;
}): Promise<null extends Sel[number] ? Arr : Slice extends Sel[number] ? Arr : Scalar<D>>;
