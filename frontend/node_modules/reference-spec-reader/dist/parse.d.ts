/**
 * @param {import('./types.js').ReferencesV0 | import('./types.js').ReferencesV1} spec
 * @param {import('./types.js').RenderFn=} renderString
 */
export function parse(spec: import('./types.js').ReferencesV0 | import('./types.js').ReferencesV1, renderString?: import('./types.js').RenderFn | undefined): Map<string, import("./types.js").Ref>;
