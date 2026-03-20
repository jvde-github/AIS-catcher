export type Tag = { inner: null | string, outer: string };
export type Step = { name: string, index?: number | undefined | null };
export type Path = Array<string | Step> | ReadonlyArray<string | Step>;

export function findTagByName(
  xml: string,
  tagName: string,
  options?: { debug?: boolean, nested?: boolean, startIndex?: number }
): Tag | undefined;

export function findTagsByName(
  xml: string,
  tagName: string,
  options?: { debug?: boolean, nested?: boolean, startIndex?: boolean }
): Tag[];

export function findTagsByPath(
  xml: string,
  path: Path,
  options?: { debug?: boolean, returnOnFirst?: boolean }
): Tag[];

export function findTagByPath(
  xml: string,
  path: Path,
  options?: { debug?: boolean }
): Tag | undefined;

export function getAttribute(tag: string | Tag, attributeName: string, options?: { debug?: boolean } ): string;

export function removeTagsByName(xml: string, tagName: string, options?: { debug?: boolean }): string;
