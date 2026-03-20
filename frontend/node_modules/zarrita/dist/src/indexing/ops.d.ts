import type { Mutable, Readable } from "@zarrita/storage";
import type { Array } from "../hierarchy.js";
import type { Chunk, DataType, Scalar, TypedArray } from "../metadata.js";
import type { GetOptions, Indices, Projection, SetOptions, Slice } from "./types.js";
export declare const setter: {
    prepare<D extends DataType>(data: TypedArray<D>, shape: number[], stride: number[]): {
        data: TypedArray<D>;
        shape: number[];
        stride: number[];
    };
    set_scalar<D extends DataType>(dest: Chunk<D>, sel: (number | Indices)[], value: Scalar<D>): void;
    set_from_chunk<D extends DataType>(dest: Chunk<D>, src: Chunk<D>, projections: Projection[]): void;
};
/** @category Utility */
export declare function get<D extends DataType, Store extends Readable, Sel extends (null | Slice | number)[]>(arr: Array<D, Store>, selection?: Sel | null, opts?: GetOptions<Parameters<Store["get"]>[1]>): Promise<null extends Sel[number] ? Chunk<D> : Slice extends Sel[number] ? Chunk<D> : Scalar<D>>;
/** @category Utility */
export declare function set<D extends DataType>(arr: Array<D, Mutable>, selection: (null | Slice | number)[] | null, value: Scalar<D> | Chunk<D>, opts?: SetOptions): Promise<void>;
