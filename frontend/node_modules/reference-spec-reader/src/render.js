/**
 * @param {string} template
 * @param {RegExp=} re
 */
function parse(template, re = /{{(.*?)}}/) {
	let result = re.exec(template);
	const parts = [];
	let pos;

	while (result) {
		pos = result.index;
		if (pos !== 0) {
			parts.push({ match: false, str: template.substring(0, pos) });
			template = template.slice(pos);
		}
		parts.push({ match: true, str: result[0] });
		template = template.slice(result[0].length);
		result = re.exec(template);
	}

	if (template) {
		parts.push({ match: false, str: template });
	}

	return parts;
}

/** @param {string} str */
function matchFn(str) {
	const match = str.match(/(?<fname>[A-Z_][A-Z_1-9]*)\((?<args>[^)]+)\)/i);
	if (!match?.groups) return;

	const { fname, args } = match.groups;
	const ctx = Object.fromEntries(
		args.split(",").map((kwarg) => {
			const { key, num, str } =
				kwarg.match(
					/(?<key>[a-z_0-9]*)\s*=\s*((?<num>[0-9.]+)|('|")(?<str>.*)('|"))/i,
				)?.groups ?? {};
			if (!key || !(num || str)) {
				throw Error(`Failed to match fn kwarg: ${kwarg}`);
			}
			return [key, num ? Number(num) : str];
		}),
	);

	return { fname, ctx };
}

const alphabet = "abcdefghijklmnopqrstuvwxyz";
const numbers = "01234569789";
const expr = "()*/+-";
const space = " ";
const valid = new Set(
	alphabet + alphabet.toUpperCase() + numbers + expr + space,
);
/** @param {string} str */
function matchMathEval(str) {
	for (let i = 0; i < str.length; i++) {
		if (!valid.has(str.charAt(i))) return;
	}
	return parse(str, /[A-Za-z_][A-Za-z0-9_]*/gi);
}

/** @type {import('./types.js').RenderFn} */
export function render(template, context) {
	const grps = parse(template);
	return grps
		.map((grp) => {
			if (!grp.match) {
				return grp.str;
			}

			let inner = grp.str.split(/{{|}}/).filter(Boolean)[0].trim();
			if (inner in context) {
				return context[inner];
			}

			const fnMatch = matchFn(inner);
			if (fnMatch) {
				const { fname, ctx } = fnMatch;
				const fn = context[fname];
				if (typeof fn === "function") {
					return fn(ctx);
				}
				throw Error(
					`Cannot find function named ${fname} in rendering context.`,
				);
			}

			const matches = matchMathEval(inner);
			if (matches) {
				const exprParts = matches.map((match) => {
					if (!match.match) return match.str;
					const value = context[match.str];
					if (value == null) {
						throw Error(
							`Cannot find number named ${match.str} in rendering context.`,
						);
					}
					if (typeof value !== "number") {
						throw Error(
							`The provided value for ${match.str} must be a number.`,
						);
					}
					return value;
				});
				return Function('"use strict";return (' + exprParts.join("") + ")")();
			}

			throw new Error(`Unable to match ${grp.str}`);
		})
		.join("");
}
