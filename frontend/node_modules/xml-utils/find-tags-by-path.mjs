import findTagsByName from "./find-tags-by-name.mjs";

export default function findTagsByPath(xml, path, options) {
  const debug = (options && options.debug) || false;
  if (debug) console.log("[xml-utils] starting findTagsByPath with: ", xml.substring(0, 500));
  const returnOnFirst = (options && options.returnOnFirst) || false;

  if (Array.isArray(path) === false) throw new Error("[xml-utils] path should be an array");

  const path0 = typeof path[0] === "string" ? { name: path[0] } : path[0];
  let tags = findTagsByName(xml, path0.name, { debug, nested: false });
  if (typeof tags !== "undefined" && typeof path0.index === "number") {
    if (typeof tags[path0.index] === "undefined") {
      tags = [];
    } else {
      tags = [tags[path0.index]];
    }
  }
  if (debug) console.log("first tags are:", tags);

  path = path.slice(1);

  for (let pathIndex = 0; pathIndex < path.length; pathIndex++) {
    const part = typeof path[pathIndex] === "string" ? { name: path[pathIndex] } : path[pathIndex];
    if (debug) console.log("part.name:", part.name);
    let allSubTags = [];
    for (let tagIndex = 0; tagIndex < tags.length; tagIndex++) {
      const tag = tags[tagIndex];
      const subTags = findTagsByName(tag.outer, part.name, {
        debug,
        startIndex: 1
      });

      if (debug) console.log("subTags.length:", subTags.length);
      if (subTags.length > 0) {
        subTags.forEach(subTag => {
          (subTag.start += tag.start), (subTag.end += tag.start);
        });
        if (returnOnFirst && pathIndex === path.length - 1) return [subTags[0]];
        allSubTags = allSubTags.concat(subTags);
      }
    }
    tags = allSubTags;
    if (typeof part.index === "number") {
      if (typeof tags[part.index] === "undefined") {
        tags = [];
      } else {
        tags = [tags[part.index]];
      }
    }
  }
  return tags;
}
