/**
 * @typedef {import('./basedecoder.js').BaseDecoderParameters & { LercParameters?: any }} LercDecoderParameters
 */
export const zstd: ZSTDDecoder;
export default class LercDecoder extends BaseDecoder {
    /**
     * @param {ArrayBufferLike} buffer
     * @returns {ArrayBufferLike}
     */
    decodeBlock(buffer: ArrayBufferLike): ArrayBufferLike;
}
export type LercDecoderParameters = import("./basedecoder.js").BaseDecoderParameters & {
    LercParameters?: any;
};
import { ZSTDDecoder } from 'zstddec';
import BaseDecoder from './basedecoder.js';
//# sourceMappingURL=lerc.d.ts.map