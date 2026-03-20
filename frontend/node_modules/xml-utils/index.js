const getAttribute = require("./get-attribute.js");
const findTagByName = require("./find-tag-by-name.js");
const findTagsByName = require("./find-tags-by-name.js");
const findTagByPath = require("./find-tag-by-path.js");
const findTagsByPath = require("./find-tags-by-path.js");
const removeComments = require("./remove-comments.js");
const removeTagsByName = require("./remove-tags-by-name.js");

module.exports = {
  getAttribute,
  findTagByName,
  findTagsByName,
  findTagByPath,
  findTagsByPath,
  removeComments,
  removeTagsByName
};
