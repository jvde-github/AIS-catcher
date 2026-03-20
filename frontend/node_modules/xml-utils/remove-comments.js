function removeComments(xml) {
  return xml.replace(/<!--[^]*-->/g, "");
}

module.exports = removeComments;
module.exports.default = removeComments;
