import findTagByName from "./find-tag-by-name.mjs";

export default function removeTagsByName(xml, tagName, options) {
  const debug = (options && options.debug) || false;
  let tag;
  while ((tag = findTagByName(xml, tagName, { debug }))) {
    xml = xml.substring(0, tag.start) + xml.substring(tag.end);
    if (debug) console.log("[xml-utils] removed:", tag);
  }
  return xml;
}
