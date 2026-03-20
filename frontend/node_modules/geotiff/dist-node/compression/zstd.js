"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.zstd = void 0;
const stream_1 = require("zstddec/stream");
const basedecoder_js_1 = __importDefault(require("./basedecoder.js"));
exports.zstd = new stream_1.ZSTDDecoder();
class ZstdDecoder extends basedecoder_js_1.default {
    /** @param {ArrayBuffer} buffer */
    decodeBlock(buffer) {
        return /** @type {ArrayBuffer} */ (exports.zstd.decode(new Uint8Array(buffer)).buffer);
    }
}
exports.default = ZstdDecoder;
//# sourceMappingURL=zstd.js.map
//# sourceMappingURL=zstd.js.map