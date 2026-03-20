export type ReferencesV0 = Record<string, Ref>;

export interface ReferencesV1 {
	version: 1;
	templates?: Record<string, string>;
	gen?: (RefGenerator | Omit<RefGenerator, "offset" | "length">)[];
	refs?: Record<string, Ref>;
}

interface RefGenerator {
	key: string;
	url: string;
	offset: string;
	length: string;
	dimensions: Record<string, Range | number[]>;
}

export type Ref =
	| string
	| [url: string | null]
	| [url: string | null, offset: number, length: number];

export type RenderContext = Record<
	string,
	number | string | ((ctx: Record<string, string | number>) => string)
>;

export type RenderFn = (template: string, ctx: RenderContext) => string;

export interface Range {
	start?: number;
	stop: number;
	step?: number;
}
