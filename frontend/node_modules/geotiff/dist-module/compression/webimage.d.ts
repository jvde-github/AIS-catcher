/**
 * class WebImageDecoder
 *
 * This decoder uses the browsers image decoding facilities to read image
 * formats like WebP when supported.
 */
export default class WebImageDecoder extends BaseDecoder {
    /** @param {ArrayBuffer} buffer */
    decodeBlock(buffer: ArrayBuffer): Promise<ArrayBuffer>;
}
import BaseDecoder from './basedecoder.js';
//# sourceMappingURL=webimage.d.ts.map