import type { Mutable } from "@zarrita/storage";
import { Array, Group, Location } from "./hierarchy.js";
import type { Attributes, CodecMetadata, DataType, Scalar } from "./metadata.js";
interface CreateGroupOptions {
    attributes?: Record<string, unknown>;
}
interface CreateArrayOptions<Dtype extends DataType> {
    shape: number[];
    chunk_shape: number[];
    data_type: Dtype;
    codecs?: CodecMetadata[];
    fill_value?: Scalar<Dtype>;
    chunk_separator?: "." | "/";
    attributes?: Attributes;
}
export declare function create<Store extends Mutable, _Dtype extends DataType = DataType>(location: Location<Store> | Store): Promise<Group<Store>>;
export declare function create<Store extends Mutable, _Dtype extends DataType = DataType>(location: Location<Store> | Store, options: CreateGroupOptions): Promise<Group<Store>>;
export declare function create<Store extends Mutable, Dtype extends DataType>(location: Location<Store> | Store, options: CreateArrayOptions<Dtype>): Promise<Array<Dtype, Store>>;
export {};
