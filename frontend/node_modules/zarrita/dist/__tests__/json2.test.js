import { describe, expect, test } from "vitest";
import { JsonCodec } from "../src/codecs/json2.js";
describe("JsonCodec", () => {
    test("can decode", () => {
        // from numcodecs.json import JSON
        // import numpy as np
        // json_codec = JSON()
        // json_codec.encode(np.array(['ASC1', 'ASC2', 'END', 'GABA1', 'GABA2', 'MG', 'NSC', 'ODC1', 'OPC', 'Unclassified', 'exCA1', 'exCA3', 'exDG', 'exPFC1', 'exPFC2'], dtype=object))
        const encodedStr = `["ASC1","ASC2","END","GABA1","GABA2","MG","NSC","ODC1","OPC","Unclassified","exCA1","exCA3","exDG","exPFC1","exPFC2","|O",[15]]`;
        const encodedBytes = new TextEncoder().encode(encodedStr);
        const jsonCodec = new JsonCodec({ encoding: "utf-8" });
        const decodedResult = jsonCodec.decode(encodedBytes);
        expect(decodedResult).toStrictEqual({
            data: [
                "ASC1",
                "ASC2",
                "END",
                "GABA1",
                "GABA2",
                "MG",
                "NSC",
                "ODC1",
                "OPC",
                "Unclassified",
                "exCA1",
                "exCA3",
                "exDG",
                "exPFC1",
                "exPFC2",
            ],
            shape: [15],
            stride: [1],
        });
    });
    test("can encode", () => {
        const encodedStr = `["ASC1","ASC2","END","GABA1","GABA2","MG","NSC","ODC1","OPC","Unclassified","exCA1","exCA3","exDG","exPFC1","exPFC2","|O",[15]]`;
        const encodedBytes = new TextEncoder().encode(encodedStr);
        const chunk = {
            data: [
                "ASC1",
                "ASC2",
                "END",
                "GABA1",
                "GABA2",
                "MG",
                "NSC",
                "ODC1",
                "OPC",
                "Unclassified",
                "exCA1",
                "exCA3",
                "exDG",
                "exPFC1",
                "exPFC2",
            ],
            shape: [15],
            stride: [1],
        };
        const jsonCodec = new JsonCodec({ encoding: "utf-8" });
        const encodedResult = jsonCodec.encode(chunk);
        expect(encodedResult).toStrictEqual(encodedBytes);
    });
    test("throws on decode when !strict", () => {
        const encodedStr = `["A","B","C","|O",[3]]`;
        const encodedBytes = new TextEncoder().encode(encodedStr);
        const jsonCodec = new JsonCodec({ strict: false });
        expect(() => jsonCodec.decode(encodedBytes)).toThrowError();
    });
    test("throws on encode with non-supported encoding", () => {
        const chunk = {
            data: ["A", "B", "C"],
            shape: [3],
            stride: [1],
        };
        const jsonCodec = new JsonCodec({ check_circular: false });
        expect(() => jsonCodec.encode(chunk)).toThrowError();
    });
    test("throws on encode with !check_circular", () => {
        const chunk = {
            data: ["A", "B", "C"],
            shape: [3],
            stride: [1],
        };
        const jsonCodec = new JsonCodec({ check_circular: false });
        expect(() => jsonCodec.encode(chunk)).toThrowError();
    });
    test("throws on encode with check_circular and circular reference", () => {
        let data = ["A", null];
        data[1] = data;
        const chunk = {
            data,
            shape: [2],
            stride: [1],
        };
        const jsonCodec = new JsonCodec({ check_circular: true });
        expect(() => jsonCodec.encode(chunk)).toThrowError();
    });
    test("supports !allow_nan", () => {
        const chunk = {
            data: [1, 2, Number.NaN],
            shape: [3],
            stride: [1],
        };
        const jsonCodec = new JsonCodec({ allow_nan: false });
        expect(() => jsonCodec.encode(chunk)).toThrowError();
    });
    test("supports sort_keys", () => {
        const chunk = {
            data: [{ "1": 1, "3": 3, "2": 2 }],
            shape: [1],
            stride: [1],
        };
        const jsonCodec = new JsonCodec({ sort_keys: true });
        const decodedChunk = jsonCodec.decode(jsonCodec.encode(chunk));
        expect(decodedChunk.data[0]).toEqual({ "1": 1, "2": 2, "3": 3 });
    });
    test("supports ensure_ascii", () => {
        const chunk = {
            data: ["£"],
            shape: [1],
            stride: [1],
        };
        const jsonCodec = new JsonCodec({ ensure_ascii: true });
        const encodedChunk = jsonCodec.encode(chunk);
        const decodedChunk = jsonCodec.decode(encodedChunk);
        expect(decodedChunk.data).toEqual(["£"]);
        expect(Array.from(encodedChunk)).toEqual([
            91, 34, 92, 117, 48, 48, 97, 51, 34, 44, 34, 124, 79, 34, 44, 91, 49, 93,
            93,
        ]);
    });
    test("supports !ensure_ascii", () => {
        const chunk = {
            data: ["£"],
            shape: [1],
            stride: [1],
        };
        const jsonCodec = new JsonCodec({ ensure_ascii: false });
        const encodedChunk = jsonCodec.encode(chunk);
        const decodedChunk = jsonCodec.decode(encodedChunk);
        expect(decodedChunk.data).toEqual(["£"]);
        expect(Array.from(encodedChunk)).toEqual([
            91, 34, 194, 163, 34, 44, 34, 124, 79, 34, 44, 91, 49, 93, 93,
        ]);
    });
});
//# sourceMappingURL=json2.test.js.map