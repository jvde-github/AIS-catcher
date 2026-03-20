import { describe, expect, test } from "vitest";
import { BoolArray, ByteStringArray, UnicodeStringArray, } from "../src/typedarray.js";
describe("BoolArray.constructor", () => {
    test("new (size: number) -> BoolArray", () => {
        let arr = new BoolArray(5);
        expect({
            length: arr.length,
            BYTES_PER_ELEMENT: arr.BYTES_PER_ELEMENT,
            byteOffset: arr.byteOffset,
            byteLength: arr.byteLength,
            data: Array.from(arr),
        }).toMatchInlineSnapshot(`
			{
			  "BYTES_PER_ELEMENT": 1,
			  "byteLength": 5,
			  "byteOffset": 0,
			  "data": [
			    false,
			    false,
			    false,
			    false,
			    false,
			  ],
			  "length": 5,
			}
		`);
    });
    test("new (buffer: ArrayBuffer) -> BoolArray", () => {
        let arr = new BoolArray(new Uint8Array([1, 1, 0, 0, 1]).buffer, 1, 3);
        expect({
            length: arr.length,
            BYTES_PER_ELEMENT: arr.BYTES_PER_ELEMENT,
            byteOffset: arr.byteOffset,
            byteLength: arr.byteLength,
            data: Array.from(arr),
        }).toMatchInlineSnapshot(`
			{
			  "BYTES_PER_ELEMENT": 1,
			  "byteLength": 3,
			  "byteOffset": 1,
			  "data": [
			    true,
			    false,
			    false,
			  ],
			  "length": 3,
			}
		`);
    });
    test("new (buffer: ArrayBuffer, byteOffset: number, length: number) -> BoolArray", () => {
        let arr = new BoolArray(new Uint8Array([0, 0, 1, 1, 0, 0, 1]).buffer, 2, 4);
        expect({
            length: arr.length,
            BYTES_PER_ELEMENT: arr.BYTES_PER_ELEMENT,
            byteOffset: arr.byteOffset,
            byteLength: arr.byteLength,
            data: Array.from(arr),
        }).toMatchInlineSnapshot(`
			{
			  "BYTES_PER_ELEMENT": 1,
			  "byteLength": 4,
			  "byteOffset": 2,
			  "data": [
			    true,
			    true,
			    false,
			    false,
			  ],
			  "length": 4,
			}
		`);
    });
    test("new (values: Iterable<boolean>) -> BoolArray", () => {
        let arr = new BoolArray((function* () {
            yield* [true, true, false, false, true];
        })());
        expect({
            length: arr.length,
            BYTES_PER_ELEMENT: arr.BYTES_PER_ELEMENT,
            byteOffset: arr.byteOffset,
            byteLength: arr.byteLength,
            data: Array.from(arr),
        }).toMatchInlineSnapshot(`
			{
			  "BYTES_PER_ELEMENT": 1,
			  "byteLength": 5,
			  "byteOffset": 0,
			  "data": [
			    true,
			    true,
			    false,
			    false,
			    true,
			  ],
			  "length": 5,
			}
		`);
    });
    test("get(idx) -> boolean", () => {
        let arr = new BoolArray(new Uint8Array([1, 1, 0, 0, 1]).buffer);
        expect([
            arr.get(0),
            arr.get(1),
            arr.get(2),
            arr.get(3),
            arr.get(4),
            arr.get(5),
        ]).toMatchInlineSnapshot(`
			[
			  true,
			  true,
			  false,
			  false,
			  true,
			  undefined,
			]
		`);
    });
    test("set(idx, value) -> void", () => {
        let arr = new BoolArray(5);
        [true, true, false, false, true].forEach((v, idx) => arr.set(idx, v));
        expect(new Uint8Array(arr.buffer)).toMatchInlineSnapshot(`
			Uint8Array [
			  1,
			  1,
			  0,
			  0,
			  1,
			]
		`);
    });
    test("fill(true) -> void", () => {
        let arr = new BoolArray(5);
        arr.fill(true);
        expect(new Uint8Array(arr.buffer)).toMatchInlineSnapshot(`
			Uint8Array [
			  1,
			  1,
			  1,
			  1,
			  1,
			]
		`);
    });
});
describe("ByteStringArray", () => {
    test("new (size: number) -> ByteStringArray", () => {
        let arr = new ByteStringArray(10, 5);
        expect({
            length: arr.length,
            BYTES_PER_ELEMENT: arr.BYTES_PER_ELEMENT,
            byteOffset: arr.byteOffset,
            byteLength: arr.byteLength,
            data: Array.from(arr),
        }).toMatchInlineSnapshot(`
			{
			  "BYTES_PER_ELEMENT": 10,
			  "byteLength": 50,
			  "byteOffset": 0,
			  "data": [
			    "",
			    "",
			    "",
			    "",
			    "",
			  ],
			  "length": 5,
			}
		`);
    });
    test("new (buffer: ArrayBuffer, byteOffset: number, length: number) -> ByteStringArray", () => {
        let data = new TextEncoder().encode("Hello\x00world!\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00");
        // @ts-expect-error - we know this is an ArrayBuffer, TS just isn't smart enough to know it's not a SharedArrayBuffer
        let buffer = data.buffer;
        let arr = new ByteStringArray(2, buffer, 3, 4);
        expect({
            length: arr.length,
            BYTES_PER_ELEMENT: arr.BYTES_PER_ELEMENT,
            byteOffset: arr.byteOffset,
            byteLength: arr.byteLength,
            data: Array.from(arr),
        }).toMatchInlineSnapshot(`
			{
			  "BYTES_PER_ELEMENT": 2,
			  "byteLength": 8,
			  "byteOffset": 3,
			  "data": [
			    "lo",
			    "w",
			    "or",
			    "ld",
			  ],
			  "length": 4,
			}
		`);
    });
    test("new (values: Iterable<string>) -> ByteStringArray", () => {
        let data = ["Hello", "world!", "", "", ""];
        let arr = new ByteStringArray(6, data[Symbol.iterator]());
        expect({
            length: arr.length,
            data: Array.from(arr),
            text: new TextDecoder().decode(arr.buffer),
        }).toStrictEqual({
            length: 5,
            data,
            text: "Hello\x00world!\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
        });
    });
    test("get(idx: number) -> string", () => {
        let data = ["Hello", "world!", "", "", ""];
        let arr = new ByteStringArray(6, data[Symbol.iterator]());
        expect(Array.from(data, (_, i) => arr.get(i))).toStrictEqual(data);
    });
    test("get(idx: number) -> throws", () => {
        let data = ["Hello", "world!", "", "", ""];
        let arr = new ByteStringArray(6, data[Symbol.iterator]());
        expect(() => arr.get(5)).toThrow();
    });
    test("set(idx: number, value: string) -> void", () => {
        let expected = ["what", "is", "the", "meaning", "of", "life?"];
        let arr = new ByteStringArray(7, expected.length);
        expected.forEach((v, idx) => arr.set(idx, v));
        expect(Array.from(arr)).toStrictEqual(expected);
    });
    test("fill(value: string) -> void", () => {
        let arr = new ByteStringArray(3, 5);
        arr.fill("foo");
        expect(Array.from(arr)).toStrictEqual(["foo", "foo", "foo", "foo", "foo"]);
    });
});
describe("UnicodeStringArray", () => {
    test("new (size: number) -> UnicodeStringArray", () => {
        let arr = new UnicodeStringArray(10, 5);
        expect({
            length: arr.length,
            BYTES_PER_ELEMENT: arr.BYTES_PER_ELEMENT,
            byteOffset: arr.byteOffset,
            byteLength: arr.byteLength,
            data: Array.from(arr),
        }).toStrictEqual({
            length: 5,
            BYTES_PER_ELEMENT: 10 * 4,
            byteOffset: 0,
            byteLength: 5 * 10 * 4,
            data: ["", "", "", "", ""],
        });
    });
    test("new (buffer: ArrayBuffer, byteOffset: number, length: number) -> UnicodeStringArray", () => {
        // biome-ignore format: the array should not be formatted
        let data = new Int32Array([161, 72, 111, 108, 97, 32, 109, 117, 110, 100, 111, 33, 0, 0, 0, 0, 0, 0, 0, 0, 72, 101, 106, 32, 86, 228, 114, 108, 100, 101, 110, 33, 0, 0, 0, 0, 0, 0, 0, 0, 88, 105, 110, 32, 99, 104, 224, 111, 32, 116, 104, 7871, 32, 103, 105, 7899, 105, 0, 0, 0]);
        let chars = 20;
        let arr = new UnicodeStringArray(chars, data.buffer, chars * 4, 2);
        expect({
            length: arr.length,
            BYTES_PER_ELEMENT: arr.BYTES_PER_ELEMENT,
            byteOffset: arr.byteOffset,
            byteLength: arr.byteLength,
            data: Array.from(arr),
        }).toStrictEqual({
            length: 2,
            BYTES_PER_ELEMENT: chars * 4,
            byteOffset: chars * 4,
            byteLength: chars * 4 * 2,
            data: ["Hej Världen!", "Xin chào thế giới"],
        });
    });
    test("new (values: Iterable<string>) -> UnicodeStringArray", () => {
        let chars = 20;
        let data = ["¡Hola mundo!", "Hej Världen!", "Xin chào thế giới"];
        let arr = new UnicodeStringArray(20, data);
        expect({
            length: arr.length,
            BYTES_PER_ELEMENT: arr.BYTES_PER_ELEMENT,
            byteOffset: arr.byteOffset,
            byteLength: arr.byteLength,
            encoded: new Int32Array(arr.buffer, arr.byteOffset, arr.byteLength / Int32Array.BYTES_PER_ELEMENT),
            encoded_sub_view: new Int32Array(arr.buffer, arr.byteOffset + arr.BYTES_PER_ELEMENT, chars),
        }).toStrictEqual({
            length: data.length,
            BYTES_PER_ELEMENT: chars * Int32Array.BYTES_PER_ELEMENT,
            byteOffset: 0,
            byteLength: data.length * chars * Int32Array.BYTES_PER_ELEMENT,
            // biome-ignore format: the array should not be formatted
            encoded: new Int32Array([
                161, 72, 111, 108, 97, 32, 109, 117, 110, 100, 111, 33, 0, 0, 0, 0, 0, 0, 0, 0,
                72, 101, 106, 32, 86, 228, 114, 108, 100, 101, 110, 33, 0, 0, 0, 0, 0, 0, 0, 0,
                88, 105, 110, 32, 99, 104, 224, 111, 32, 116, 104, 7871, 32, 103, 105, 7899, 105, 0, 0, 0,
            ]),
            // biome-ignore format: the array should not be formatted
            encoded_sub_view: new Int32Array([
                72, 101, 106, 32, 86, 228, 114, 108, 100, 101, 110, 33, 0, 0, 0, 0, 0, 0, 0, 0,
            ]),
        });
    });
    test("get(idx) -> string", () => {
        let data = ["¡Hola mundo!", "Hej Världen!", "Xin chào thế giới"];
        let arr = new UnicodeStringArray(20, data[Symbol.iterator]());
        expect(Array.from(data, (_, i) => arr.get(i))).toStrictEqual(data);
    });
    test("get(idx) -> throws", () => {
        let arr = new UnicodeStringArray(20, 3);
        expect(() => arr.get(3)).toThrow();
    });
    test("set(idx, value) -> void", () => {
        let expected = ["what", "is", "the", "meaning", "of", "life?"];
        let arr = new UnicodeStringArray(7, expected.length);
        expected.forEach((v, idx) => arr.set(idx, v));
        expect(Array.from(arr)).toStrictEqual(expected);
    });
    test("fill('foo') -> void", () => {
        let arr = new UnicodeStringArray(3, 5);
        arr.fill("foo");
        expect(Array.from(arr)).toStrictEqual(["foo", "foo", "foo", "foo", "foo"]);
    });
});
//# sourceMappingURL=typedarray.test.js.map