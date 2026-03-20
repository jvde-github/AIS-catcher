const findTagsByPath = require("./find-tags-by-path.js");

function findTagByPath(xml, path, options) {
  const debug = (options && options.debug) || false;
  const found = findTagsByPath(xml, path, { debug, returnOnFirst: true });
  if (Array.isArray(found) && found.length === 1) return found[0];
  else return undefined;
}
module.exports = findTagByPath;
module.exports.default = findTagByPath;
