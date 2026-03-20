export default class JpegDecoder extends BaseDecoder {
    /**
     * @param {import('./basedecoder.js').BaseDecoderParameters & { JPEGTables?: Uint8Array }} parameters
     */
    constructor(parameters: import("./basedecoder.js").BaseDecoderParameters & {
        JPEGTables?: Uint8Array;
    });
    reader: JpegStreamReader;
    /** @param {ArrayBuffer} buffer */
    decodeBlock(buffer: ArrayBuffer): ArrayBuffer;
}
export type HuffmanNode = (number | HuffmanNode)[];
export type Code = {
    children: HuffmanNode;
    index: number;
};
export type JpegComponent = {
    h: number;
    v: number;
    quantizationIdx?: number | undefined;
    quantizationTable?: Int32Array<ArrayBufferLike> | undefined;
    blocksPerLine: number;
    blocksPerColumn: number;
    blocks: Int32Array[][];
    huffmanTableDC?: HuffmanNode | undefined;
    huffmanTableAC?: HuffmanNode | undefined;
    pred?: number | undefined;
};
export type JpegFrame = {
    extended: boolean;
    progressive: boolean;
    precision: number;
    scanLines: number;
    samplesPerLine: number;
    components: {
        [x: string]: JpegComponent;
    };
    componentsOrder: number[];
    maxH: number;
    maxV: number;
    mcusPerLine: number;
    mcusPerColumn: number;
};
import BaseDecoder from './basedecoder.js';
declare class JpegStreamReader {
    jfif: {
        version: {
            major: number;
            minor: number;
        };
        densityUnits: number;
        xDensity: number;
        yDensity: number;
        thumbWidth: number;
        thumbHeight: number;
        thumbData: Uint8Array<ArrayBufferLike>;
    } | null;
    adobe: {
        version: number;
        flags0: number;
        flags1: number;
        transformCode: number;
    } | null;
    /** @type {number} */
    resetInterval: number;
    /** @type {Int32Array[]} */
    quantizationTables: Int32Array[];
    /** @type {HuffmanNode[]} */
    huffmanTablesAC: HuffmanNode[];
    /** @type {HuffmanNode[]} */
    huffmanTablesDC: HuffmanNode[];
    /** @type {JpegFrame[]} */
    frames: JpegFrame[];
    resetFrames(): void;
    /** @param {Uint8Array} data */
    parse(data: Uint8Array): void;
    getResult(): Uint8Array<ArrayBuffer>;
}
export {};
//# sourceMappingURL=jpeg.d.ts.map