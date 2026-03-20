import type { Chunk, DataType } from "../metadata.js";
type Order = "C" | "F" | Array<number>;
export declare class TransposeCodec {
    #private;
    kind: string;
    constructor(configuration: {
        order?: Order;
    }, meta: {
        shape: number[];
    });
    static fromConfig(configuration: {
        order: Order;
    }, meta: {
        shape: number[];
    }): TransposeCodec;
    encode<D extends DataType>(arr: Chunk<D>): Chunk<D>;
    decode<D extends DataType>(arr: Chunk<D>): Chunk<D>;
}
export {};
