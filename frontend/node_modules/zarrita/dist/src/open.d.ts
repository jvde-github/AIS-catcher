import type { Readable } from "@zarrita/storage";
import { Array, Group, Location } from "./hierarchy.js";
import type { DataType } from "./metadata.js";
declare function open_v2<Store extends Readable>(location: Location<Store> | Store, options: {
    kind: "group";
    attrs?: boolean;
}): Promise<Group<Store>>;
declare function open_v2<Store extends Readable>(location: Location<Store> | Store, options: {
    kind: "array";
    attrs?: boolean;
}): Promise<Array<DataType, Store>>;
declare function open_v2<Store extends Readable>(location: Location<Store> | Store, options?: {
    kind?: "array" | "group";
    attrs?: boolean;
}): Promise<Array<DataType, Store> | Group<Store>>;
declare function open_v3<Store extends Readable>(location: Location<Store> | Store, options: {
    kind: "group";
}): Promise<Group<Store>>;
declare function open_v3<Store extends Readable>(location: Location<Store> | Store, options: {
    kind: "array";
}): Promise<Array<DataType, Store>>;
declare function open_v3<Store extends Readable>(location: Location<Store> | Store): Promise<Array<DataType, Store> | Group<Store>>;
declare function open_v3<Store extends Readable>(location: Location<Store> | Store): Promise<Array<DataType, Store> | Group<Store>>;
export declare function open<Store extends Readable>(location: Location<Store> | Store, options: {
    kind: "group";
}): Promise<Group<Store>>;
export declare function open<Store extends Readable>(location: Location<Store> | Store, options: {
    kind: "array";
}): Promise<Array<DataType, Store>>;
export declare function open<Store extends Readable>(location: Location<Store> | Store, options: {
    kind?: "array" | "group";
}): Promise<Array<DataType, Store> | Group<Store>>;
export declare function open<Store extends Readable>(location: Location<Store> | Store): Promise<Array<DataType, Store> | Group<Store>>;
export declare function open<Store extends Readable>(location: Location<Store> | Store): Promise<Array<DataType, Store> | Group<Store>>;
export declare namespace open {
    var v2: typeof open_v2;
    var v3: typeof open_v3;
}
export {};
