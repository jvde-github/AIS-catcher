import { render } from "./render.js";

/**
 * @param {import('./types.js').ReferencesV0 | import('./types.js').ReferencesV1} spec
 * @param {import('./types.js').RenderFn=} renderString
 */
export function parse(spec, renderString = render) {
	// @ts-ignore
	return "version" in spec ? parseV1(spec, renderString) : parseV0(spec);
}

/**
 * @param {import('./types.js').ReferencesV0} spec
 * @returns {Map<string, import('./types.js').Ref>}
 */
function parseV0(spec) {
	return new Map(Object.entries(spec));
}

/**
 * @param {import('./types.js').ReferencesV1} spec
 * @param {import('./types.js').RenderFn} renderString
 * @returns {Map<string, import('./types.js').Ref>}
 */
function parseV1(spec, renderString) {
	/** @type {import('./types.js').RenderContext} */
	const context = {};
	for (const [key, template] of Object.entries(spec.templates ?? {})) {
		// TODO: better check for whether a template or not
		if (template.includes("{{")) {
			// Need to register filter in environment
			context[key] = (ctx) => renderString(template, ctx);
		} else {
			context[key] = template;
		}
	}

	/** @type {(t: string, o?: Record<string, string | number>) => string} */
	const render = (t, o) => {
		return renderString(t, { ...context, ...o });
	};

	/** @type {Map<string, import('./types.js').Ref>} */
	const refs = new Map();

	for (const [key, ref] of Object.entries(spec.refs ?? {})) {
		if (typeof ref === "string") {
			refs.set(key, ref);
		} else {
			const url = ref[0]?.includes("{{") ? render(ref[0]) : ref[0];
			refs.set(key, ref.length === 1 ? [url] : [url, ref[1], ref[2]]);
		}
	}

	for (const g of spec.gen ?? []) {
		for (const dims of iterDims(g.dimensions)) {
			const key = render(g.key, dims);
			const url = render(g.url, dims);
			if ("offset" in g && "length" in g) {
				// [url, offset, length]
				const offset = render(g.offset, dims);
				const length = render(g.length, dims);
				refs.set(key, [url, parseInt(offset), parseInt(length)]);
			} else {
				// [url]
				refs.set(key, [url]);
			}
		}
	}

	return refs;
}

/**
 * @param {Record<string, import('./types.js').Range | number[]>} dimensions
 * @returns {Generator<Record<string, number>>}
 */
function* iterDims(dimensions) {
	const keys = Object.keys(dimensions);
	const iterables = Object.values(dimensions).map((i) =>
		Array.isArray(i) ? i : [...range(i)],
	);
	for (const values of product(...iterables)) {
		yield Object.fromEntries(keys.map((key, i) => [key, values[i]]));
	}
}

/** @param {...any[]} iterables */
function* product(...iterables) {
	if (iterables.length === 0) {
		return;
	}
	// make a list of iterators from the iterables
	const iterators = iterables.map((it) => it[Symbol.iterator]());
	const results = iterators.map((it) => it.next());
	if (results.some((r) => r.done)) {
		throw new Error("Input contains an empty iterator.");
	}
	for (let i = 0; ; ) {
		if (results[i].done) {
			// reset the current iterator
			iterators[i] = iterables[i][Symbol.iterator]();
			results[i] = iterators[i].next();
			// advance, and exit if we've reached the end
			if (++i >= iterators.length) {
				return;
			}
		} else {
			yield results.map(({ value }) => value);
			i = 0;
		}
		results[i] = iterators[i].next();
	}
}

/** @param {import('./types.js').Range} rng */
function* range({ stop, start = 0, step = 1 }) {
	for (let i = start; i < stop; i += step) {
		yield i;
	}
}
