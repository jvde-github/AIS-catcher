const indexOfMatch = require("./index-of-match.js");
const indexOfMatchEnd = require("./index-of-match-end.js");
const countSubstring = require("./count-substring.js");

function findTagByName(xml, tagName, options) {
  const debug = (options && options.debug) || false;
  const nested = !(options && typeof options.nested === false);

  const startIndex = (options && options.startIndex) || 0;

  if (debug) console.log("[xml-utils] starting findTagByName with", tagName, " and ", options);

  const start = indexOfMatch(xml, `\<${tagName}[ \n\>\/]`, startIndex);
  if (debug) console.log("[xml-utils] start:", start);
  if (start === -1) return undefined;

  const afterStart = xml.slice(start + tagName.length);

  let relativeEnd = indexOfMatchEnd(afterStart, "^[^<]*[ /]>", 0);

  const selfClosing = relativeEnd !== -1 && afterStart[relativeEnd - 1] === "/";
  if (debug) console.log("[xml-utils] selfClosing:", selfClosing);

  if (selfClosing === false) {
    // check if tag has subtags with the same name
    if (nested) {
      let startIndex = 0;
      let openings = 1;
      let closings = 0;
      while ((relativeEnd = indexOfMatchEnd(afterStart, "[ /]" + tagName + ">", startIndex)) !== -1) {
        const clip = afterStart.substring(startIndex, relativeEnd + 1);
        openings += countSubstring(clip, "<" + tagName + "[ \n\t>]");
        closings += countSubstring(clip, "</" + tagName + ">");
        // we can't have more openings than closings
        if (closings >= openings) break;
        startIndex = relativeEnd;
      }
    } else {
      relativeEnd = indexOfMatchEnd(afterStart, "[ /]" + tagName + ">", 0);
    }
  }

  const end = start + tagName.length + relativeEnd + 1;
  if (debug) console.log("[xml-utils] end:", end);
  if (end === -1) return undefined;

  const outer = xml.slice(start, end);
  // tag is like <gml:identifier codeSpace="OGP">urn:ogc:def:crs:EPSG::32617</gml:identifier>

  let inner;
  if (selfClosing) {
    inner = null;
  } else {
    inner = outer.slice(outer.indexOf(">") + 1, outer.lastIndexOf("<"));
  }

  return { inner, outer, start, end };
}

module.exports = findTagByName;
module.exports.default = findTagByName;
