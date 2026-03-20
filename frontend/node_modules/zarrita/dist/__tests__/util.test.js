import { afterAll, beforeAll, describe, expect, test, vi } from "vitest";
import { BoolArray, ByteStringArray, UnicodeStringArray, } from "../src/typedarray.js";
import { byteswap_inplace, get_ctr, get_strides, is_dtype, } from "../src/util.js";
describe("get_ctr", () => {
    describe("without Float16Array", () => {
        beforeAll(() => {
            vi.stubGlobal("Float16Array", undefined);
        });
        afterAll(() => {
            vi.unstubAllGlobals();
        });
        test.each([
            ["int8", Int8Array],
            ["int16", Int16Array],
            ["int32", Int32Array],
            ["int64", BigInt64Array],
            ["uint8", Uint8Array],
            ["uint16", Uint16Array],
            ["uint32", Uint32Array],
            ["uint64", BigUint64Array],
            ["float32", Float32Array],
            ["float64", Float64Array],
            ["bool", BoolArray],
            ["v2:U6", UnicodeStringArray],
            ["v2:S6", ByteStringArray],
            ["string", Array],
        ])("%s -> %o", (dtype, ctr) => {
            const T = get_ctr(dtype);
            expect(new T(1)).toBeInstanceOf(ctr);
        });
        test.each(["float16"])("%s -> throws", (dtype) => {
            expect(() => get_ctr(dtype)).toThrowError();
        });
    });
    describe("with Float16Array", () => {
        class Float16ArrayStub {
        }
        beforeAll(() => {
            vi.stubGlobal("Float16Array", Float16ArrayStub);
        });
        afterAll(() => {
            vi.unstubAllGlobals();
        });
        test.each([
            ["int8", Int8Array],
            ["int16", Int16Array],
            ["int32", Int32Array],
            ["int64", BigInt64Array],
            ["uint8", Uint8Array],
            ["uint16", Uint16Array],
            ["uint32", Uint32Array],
            ["uint64", BigUint64Array],
            ["float16", Float16ArrayStub],
            ["float32", Float32Array],
            ["float64", Float64Array],
            ["bool", BoolArray],
            ["v2:U6", UnicodeStringArray],
            ["v2:S6", ByteStringArray],
            ["string", Array],
        ])("%s -> %o", (dtype, ctr) => {
            const T = get_ctr(dtype);
            expect(new T(1)).toBeInstanceOf(ctr);
        });
    });
});
describe("byteswap_inplace", () => {
    test.each([
        new Uint32Array([1, 2, 3, 4, 5]),
        new Float64Array([20, 3333, 444.4, 222, 3123]),
        new Float32Array([1, 2, 3, 42, 5]),
        new Uint8Array([1, 2, 3, 4]),
        new Int8Array([-3, 2, 3, 10]),
    ])("%o", (arr) => {
        // make a copy and then byteswap original twice
        const expected = arr.slice();
        byteswap_inplace(new Uint8Array(arr.buffer), arr.BYTES_PER_ELEMENT);
        byteswap_inplace(new Uint8Array(arr.buffer), arr.BYTES_PER_ELEMENT);
        expect(arr).toStrictEqual(expected);
    });
});
describe("get_strides", () => {
    test.each([
        [[3], "C", [1]],
        [[3], "F", [1]],
        [[3, 4], "C", [4, 1]],
        [[3, 4], "F", [1, 3]],
        [[3, 4, 10], "C", [40, 10, 1]],
        [[3, 4, 10], "F", [1, 3, 12]],
        [[3, 4, 10, 2], "C", [80, 20, 2, 1]],
        [[3, 4, 10, 2], "F", [1, 3, 12, 120]],
    ])("get_strides(%o, %s) -> %o", (shape, order, expected) => {
        expect(get_strides(shape, order)).toStrictEqual(expected);
    });
});
describe("is_dtype", () => {
    test.each([
        ["int8", true],
        ["int16", true],
        ["int32", true],
        ["uint8", true],
        ["uint16", true],
        ["uint32", true],
        ["float16", true],
        ["float32", true],
        ["float64", true],
        ["bool", false],
        ["int64", false],
        ["uint64", false],
        ["v2:U6", false],
        ["v2:S6", false],
        ["v2:object", false],
        ["string", false],
    ])("is_dtype(%s, 'number') -> %s", (dtype, expected) => {
        expect(is_dtype(dtype, "number")).toBe(expected);
    });
    test.each([
        ["int8", false],
        ["int16", false],
        ["int32", false],
        ["uint8", false],
        ["uint16", false],
        ["uint32", false],
        ["float16", false],
        ["float32", false],
        ["float64", false],
        ["bool", true],
        ["int64", false],
        ["uint64", false],
        ["v2:U6", false],
        ["v2:S6", false],
        ["v2:object", false],
        ["string", false],
    ])("is_dtype(%s, 'boolean') -> %s", (dtype, expected) => {
        expect(is_dtype(dtype, "boolean")).toBe(expected);
    });
    test.each([
        ["int8", false],
        ["int16", false],
        ["int32", false],
        ["uint8", false],
        ["uint16", false],
        ["uint32", false],
        ["float16", false],
        ["float32", false],
        ["float64", false],
        ["bool", false],
        ["int64", true],
        ["uint64", true],
        ["v2:U6", false],
        ["v2:S6", false],
        ["v2:object", false],
        ["string", false],
    ])("is_dtype(%s, 'bigint') -> %s", (dtype, expected) => {
        expect(is_dtype(dtype, "bigint")).toBe(expected);
    });
    test.each([
        ["int8", false],
        ["int16", false],
        ["int32", false],
        ["uint8", false],
        ["uint16", false],
        ["uint32", false],
        ["float16", false],
        ["float32", false],
        ["float64", false],
        ["bool", false],
        ["int64", false],
        ["uint64", false],
        ["v2:U6", true],
        ["v2:S6", true],
        ["v2:object", false],
        ["string", true],
    ])("is_dtype(%s, 'string') -> %s", (dtype, expected) => {
        expect(is_dtype(dtype, "string")).toBe(expected);
    });
    test.each([
        "int8",
        "int16",
        "int32",
        "uint8",
        "uint16",
        "uint32",
        "float16",
        "float32",
        "float64",
        "bool",
        "int64",
        "uint64",
        "v2:U6",
        "v2:S6",
        "v2:object",
        "string",
    ])("is_dtype(%s, %s) -> true", (dtype) => {
        expect(is_dtype(dtype, dtype)).toBe(true);
    });
});
//# sourceMappingURL=util.test.js.map