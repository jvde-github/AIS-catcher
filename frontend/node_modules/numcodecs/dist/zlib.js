import "./chunk-INHXZS53.js";

// src/zlib.ts
import * as fflate from "fflate";
var Zlib = class Zlib2 {
  static codecId = "zlib";
  level;
  constructor(level = 1) {
    if (level < -1 || level > 9) {
      throw new Error("Invalid zlib compression level, it should be between -1 and 9");
    }
    this.level = level;
  }
  static fromConfig({ level }) {
    return new Zlib2(level);
  }
  encode(data) {
    return fflate.zlibSync(data, { level: this.level });
  }
  decode(data) {
    return fflate.unzlibSync(data);
  }
};
var zlib_default = Zlib;
export {
  zlib_default as default
};
