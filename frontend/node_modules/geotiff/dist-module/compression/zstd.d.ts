export const zstd: ZSTDDecoder;
export default class ZstdDecoder extends BaseDecoder {
    /** @param {ArrayBuffer} buffer */
    decodeBlock(buffer: ArrayBuffer): ArrayBuffer;
}
import { ZSTDDecoder } from 'zstddec/stream';
import BaseDecoder from './basedecoder.js';
//# sourceMappingURL=zstd.d.ts.map