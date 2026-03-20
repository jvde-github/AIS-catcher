"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.zstd = void 0;
const pako_1 = require("pako");
// @ts-expect-error
const lerc_1 = __importDefault(require("lerc"));
const zstddec_1 = require("zstddec");
const basedecoder_js_1 = __importDefault(require("./basedecoder.js"));
const globals_js_1 = require("../globals.js");
/**
 * @typedef {import('./basedecoder.js').BaseDecoderParameters & { LercParameters?: any }} LercDecoderParameters
 */
exports.zstd = new zstddec_1.ZSTDDecoder();
class LercDecoder extends basedecoder_js_1.default {
    /**
     * @param {ArrayBufferLike} buffer
     * @returns {ArrayBufferLike}
     */
    decodeBlock(buffer) {
        const params = /** @type {LercDecoderParameters} */ (this.parameters);
        const addCompression = params.LercParameters?.[globals_js_1.LercParameters.AddCompression];
        /** @type {ArrayBufferLike} */
        let decoded = buffer;
        switch (addCompression) {
            case globals_js_1.LercAddCompression.None:
                break;
            case globals_js_1.LercAddCompression.Deflate:
                decoded = (0, pako_1.inflate)(new Uint8Array(decoded)).buffer;
                break;
            case globals_js_1.LercAddCompression.Zstandard:
                decoded = exports.zstd.decode(new Uint8Array(decoded)).buffer;
                break;
            default:
                throw new Error(`Unsupported LERC additional compression method identifier: ${addCompression}`);
        }
        const lercResult = lerc_1.default.decode(decoded, { returnPixelInterleavedDims: this.parameters.planarConfiguration === 1 });
        const lercData = lercResult.pixels[0];
        return lercData.buffer;
    }
}
exports.default = LercDecoder;
//# sourceMappingURL=lerc.js.map
//# sourceMappingURL=lerc.js.map