import { describe, expect, test } from "vitest";
import { range, slice, slice_indices } from "../../src/indexing/util.js";
describe("slice", () => {
    test("slice(null)", () => {
        expect(slice(null)).toMatchInlineSnapshot(`
			{
			  "start": null,
			  "step": null,
			  "stop": null,
			}
		`);
    });
    test("slice_indices(slice(null), 10)", () => {
        expect(slice_indices(slice(null), 10)).toStrictEqual([0, 10, 1]);
    });
    test("slice(3, 15, 2)", () => {
        expect(slice(3, 15, 2)).toMatchInlineSnapshot(`
			{
			  "start": 3,
			  "step": 2,
			  "stop": 15,
			}
		`);
    });
    test("slice_indices(slice(3, 15, 2), 10)", () => {
        expect(slice_indices(slice(3, 15, 2), 10)).toStrictEqual([3, 10, 2]);
    });
    test("slice_indices(slice(3, 15, 2), 30)", () => {
        expect(slice_indices(slice(3, 15, 2), 30)).toStrictEqual([3, 15, 2]);
    });
    test("slice(40)", () => {
        expect(slice(40)).toMatchInlineSnapshot(`
			{
			  "start": null,
			  "step": null,
			  "stop": 40,
			}
		`);
    });
    test("slice_indices(slice(40), 4)", () => {
        expect(slice_indices(slice(40), 4)).toStrictEqual([0, 4, 1]);
    });
    test("slice_indices(slice(40), 41)", () => {
        expect(slice_indices(slice(40), 41)).toStrictEqual([0, 40, 1]);
    });
});
describe("slice indices", () => {
    test.each([
        [null, 10, [0, 10, 1]],
        [40, 4, [0, 4, 1]],
        [40, 41, [0, 40, 1]],
    ])("slice_indices(slice(%o), %i) -> %o", (arg, indices, expected) => {
        expect(slice_indices(slice(arg), indices)).toStrictEqual(expected);
    });
    test.each([
        [3, 15, 2, 10, [3, 10, 2]],
        [3, 15, 2, 30, [3, 15, 2]],
        [2, 10, -1, 20, [2, 10, -1]],
        [2, 10, -1, 4, [2, 3, -1]],
        [null, null, -3, 14, [13, -1, -3]],
        [null, null, -3, 2, [1, -1, -3]],
    ])("slice_indices(slice(%o, %o, %o), %i) -> %o", (start, stop, step, indices, expected) => {
        expect(slice_indices(slice(start, stop, step), indices)).toStrictEqual(expected);
    });
    test.each([[null, null, 0, 1]])("slice_indices(slice(%o, %o, %o), %i) -> throws", (start, stop, step, indices) => {
        expect(() => slice_indices(slice(start, stop, step), indices)).toThrowError();
    });
});
describe("range", () => {
    test.each([
        [4, [0, 1, 2, 3]],
        [0, []],
    ])("range(%i) -> %o", (stop, expected) => {
        expect(Array.from(range(stop))).toStrictEqual(expected);
    });
    test.each([
        [0, 10, 2, [0, 2, 4, 6, 8]],
        [0, 10, 3, [0, 3, 6, 9]],
        [0, 2, 3, [0]],
    ])("range(%i, %i, %i) -> %o", (start, stop, step, expected) => {
        expect(Array.from(range(start, stop, step))).toStrictEqual(expected);
    });
});
//# sourceMappingURL=util.test.js.map