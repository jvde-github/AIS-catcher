import findTagsByPath from "./find-tags-by-path.mjs";

export default function findTagByPath(xml, path, options) {
  const debug = (options && options.debug) || false;
  const found = findTagsByPath(xml, path, { debug, returnOnFirst: true });
  if (Array.isArray(found) && found.length === 1) return found[0];
  else return undefined;
}
