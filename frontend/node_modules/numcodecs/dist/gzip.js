import "./chunk-INHXZS53.js";

// src/gzip.ts
import * as fflate from "fflate";
var GZip = class GZip2 {
  static codecId = "gzip";
  level;
  constructor(level = 1) {
    if (level < 0 || level > 9) {
      throw new Error("Invalid gzip compression level, it should be between 0 and 9");
    }
    this.level = level;
  }
  static fromConfig({ level }) {
    return new GZip2(level);
  }
  encode(data) {
    return fflate.gzipSync(data, { level: this.level });
  }
  decode(data) {
    return fflate.gunzipSync(data);
  }
};
var gzip_default = GZip;
export {
  gzip_default as default
};
