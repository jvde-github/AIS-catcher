/**
 * @typedef {Object} BaseDecoderParameters
 * @property {number} tileWidth
 * @property {number} tileHeight
 * @property {number} predictor
 * @property {number|number[]|import('../geotiff.js').TypedArray} bitsPerSample
 * @property {number} planarConfiguration
 * @property {number} [samplesPerPixel]
 */
export default class BaseDecoder {
    /**
     * @param {BaseDecoderParameters} parameters
     */
    constructor(parameters: BaseDecoderParameters);
    parameters: BaseDecoderParameters;
    /**
     * @abstract
     * @param {ArrayBufferLike} _buffer
     * @returns {Promise<ArrayBufferLike>|ArrayBufferLike}
     */
    decodeBlock(_buffer: ArrayBufferLike): Promise<ArrayBufferLike> | ArrayBufferLike;
    /**
     * @param {ArrayBufferLike} buffer
     * @returns {Promise<ArrayBufferLike>}
     */
    decode(buffer: ArrayBufferLike): Promise<ArrayBufferLike>;
}
export type BaseDecoderParameters = {
    tileWidth: number;
    tileHeight: number;
    predictor: number;
    bitsPerSample: number | number[] | import("../geotiff.js").TypedArray;
    planarConfiguration: number;
    samplesPerPixel?: number | undefined;
};
//# sourceMappingURL=basedecoder.d.ts.map