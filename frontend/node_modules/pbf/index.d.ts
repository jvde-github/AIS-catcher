export default class Pbf {
    /**
     * @param {Uint8Array | ArrayBuffer} [buf]
     */
    constructor(buf?: ArrayBuffer | Uint8Array | undefined);
    buf: Uint8Array;
    dataView: DataView;
    pos: number;
    type: number;
    length: number;
    /**
     * @template T
     * @param {(tag: number, result: T, pbf: Pbf) => void} readField
     * @param {T} result
     * @param {number} [end]
     */
    readFields<T>(readField: (tag: number, result: T, pbf: Pbf) => void, result: T, end?: number | undefined): T;
    /**
     * @template T
     * @param {(tag: number, result: T, pbf: Pbf) => void} readField
     * @param {T} result
     */
    readMessage<T>(readField: (tag: number, result: T, pbf: Pbf) => void, result: T): T;
    readFixed32(): number;
    readSFixed32(): number;
    readFixed64(): number;
    readSFixed64(): number;
    readFloat(): number;
    readDouble(): number;
    /**
     * @param {boolean} [isSigned]
     */
    readVarint(isSigned?: boolean | undefined): number;
    readVarint64(): number;
    readSVarint(): number;
    readBoolean(): boolean;
    readString(): string;
    readBytes(): Uint8Array;
    /**
     * @param {number[]} [arr]
     * @param {boolean} [isSigned]
     */
    readPackedVarint(arr?: number[] | undefined, isSigned?: boolean | undefined): number[];
    /** @param {number[]} [arr] */
    readPackedSVarint(arr?: number[] | undefined): number[];
    /** @param {boolean[]} [arr] */
    readPackedBoolean(arr?: boolean[] | undefined): boolean[];
    /** @param {number[]} [arr] */
    readPackedFloat(arr?: number[] | undefined): number[];
    /** @param {number[]} [arr] */
    readPackedDouble(arr?: number[] | undefined): number[];
    /** @param {number[]} [arr] */
    readPackedFixed32(arr?: number[] | undefined): number[];
    /** @param {number[]} [arr] */
    readPackedSFixed32(arr?: number[] | undefined): number[];
    /** @param {number[]} [arr] */
    readPackedFixed64(arr?: number[] | undefined): number[];
    /** @param {number[]} [arr] */
    readPackedSFixed64(arr?: number[] | undefined): number[];
    readPackedEnd(): number;
    /** @param {number} val */
    skip(val: number): void;
    /**
     * @param {number} tag
     * @param {number} type
     */
    writeTag(tag: number, type: number): void;
    /** @param {number} min */
    realloc(min: number): void;
    finish(): Uint8Array;
    /** @param {number} val */
    writeFixed32(val: number): void;
    /** @param {number} val */
    writeSFixed32(val: number): void;
    /** @param {number} val */
    writeFixed64(val: number): void;
    /** @param {number} val */
    writeSFixed64(val: number): void;
    /** @param {number} val */
    writeVarint(val: number): void;
    /** @param {number} val */
    writeSVarint(val: number): void;
    /** @param {boolean} val */
    writeBoolean(val: boolean): void;
    /** @param {string} str */
    writeString(str: string): void;
    /** @param {number} val */
    writeFloat(val: number): void;
    /** @param {number} val */
    writeDouble(val: number): void;
    /** @param {Uint8Array} buffer */
    writeBytes(buffer: Uint8Array): void;
    /**
     * @template T
     * @param {(obj: T, pbf: Pbf) => void} fn
     * @param {T} obj
     */
    writeRawMessage<T>(fn: (obj: T, pbf: Pbf) => void, obj: T): void;
    /**
     * @template T
     * @param {number} tag
     * @param {(obj: T, pbf: Pbf) => void} fn
     * @param {T} obj
     */
    writeMessage<T>(tag: number, fn: (obj: T, pbf: Pbf) => void, obj: T): void;
    /**
     * @param {number} tag
     * @param {number[]} arr
     */
    writePackedVarint(tag: number, arr: number[]): void;
    /**
     * @param {number} tag
     * @param {number[]} arr
     */
    writePackedSVarint(tag: number, arr: number[]): void;
    /**
     * @param {number} tag
     * @param {boolean[]} arr
     */
    writePackedBoolean(tag: number, arr: boolean[]): void;
    /**
     * @param {number} tag
     * @param {number[]} arr
     */
    writePackedFloat(tag: number, arr: number[]): void;
    /**
     * @param {number} tag
     * @param {number[]} arr
     */
    writePackedDouble(tag: number, arr: number[]): void;
    /**
     * @param {number} tag
     * @param {number[]} arr
     */
    writePackedFixed32(tag: number, arr: number[]): void;
    /**
     * @param {number} tag
     * @param {number[]} arr
     */
    writePackedSFixed32(tag: number, arr: number[]): void;
    /**
     * @param {number} tag
     * @param {number[]} arr
     */
    writePackedFixed64(tag: number, arr: number[]): void;
    /**
     * @param {number} tag
     * @param {number[]} arr
     */
    writePackedSFixed64(tag: number, arr: number[]): void;
    /**
     * @param {number} tag
     * @param {Uint8Array} buffer
     */
    writeBytesField(tag: number, buffer: Uint8Array): void;
    /**
     * @param {number} tag
     * @param {number} val
     */
    writeFixed32Field(tag: number, val: number): void;
    /**
     * @param {number} tag
     * @param {number} val
     */
    writeSFixed32Field(tag: number, val: number): void;
    /**
     * @param {number} tag
     * @param {number} val
     */
    writeFixed64Field(tag: number, val: number): void;
    /**
     * @param {number} tag
     * @param {number} val
     */
    writeSFixed64Field(tag: number, val: number): void;
    /**
     * @param {number} tag
     * @param {number} val
     */
    writeVarintField(tag: number, val: number): void;
    /**
     * @param {number} tag
     * @param {number} val
     */
    writeSVarintField(tag: number, val: number): void;
    /**
     * @param {number} tag
     * @param {string} str
     */
    writeStringField(tag: number, str: string): void;
    /**
     * @param {number} tag
     * @param {number} val
     */
    writeFloatField(tag: number, val: number): void;
    /**
     * @param {number} tag
     * @param {number} val
     */
    writeDoubleField(tag: number, val: number): void;
    /**
     * @param {number} tag
     * @param {boolean} val
     */
    writeBooleanField(tag: number, val: boolean): void;
}
