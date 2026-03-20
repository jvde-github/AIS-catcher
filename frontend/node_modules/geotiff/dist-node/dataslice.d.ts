export default class DataSlice {
    /**
     * @param {ArrayBufferLike} arrayBuffer
     * @param {number} sliceOffset
     * @param {boolean} littleEndian
     * @param {boolean} bigTiff
     */
    constructor(arrayBuffer: ArrayBufferLike, sliceOffset: number, littleEndian: boolean, bigTiff: boolean);
    _dataView: DataView<ArrayBufferLike>;
    _sliceOffset: number;
    _littleEndian: boolean;
    _bigTiff: boolean;
    get sliceOffset(): number;
    get sliceTop(): number;
    get littleEndian(): boolean;
    get bigTiff(): boolean;
    get buffer(): ArrayBufferLike;
    /**
     * @param {number} offset
     * @param {number} length
     * @returns {boolean}
     */
    covers(offset: number, length: number): boolean;
    /**
     * @param {number} offset
     * @returns {number}
     */
    readUint8(offset: number): number;
    /**
     * @param {number} offset
     * @returns {number}
     */
    readInt8(offset: number): number;
    /**
     * @param {number} offset
     * @returns {number}
     */
    readUint16(offset: number): number;
    /**
     * @param {number} offset
     * @returns {number}
     */
    readInt16(offset: number): number;
    /**
     * @param {number} offset
     * @returns {number}
     */
    readUint32(offset: number): number;
    /**
     * @param {number} offset
     * @returns {number}
     */
    readInt32(offset: number): number;
    /**
     * @param {number} offset
     * @returns {number}
     */
    readFloat32(offset: number): number;
    /**
     * @param {number} offset
     * @returns {number}
     */
    readFloat64(offset: number): number;
    /**
     * @param {number} offset
     * @returns {number}
     */
    readUint64(offset: number): number;
    /**
     * Adapted from https://stackoverflow.com/a/55338384/8060591
     * @param {number} offset
     * @returns {number}
     */
    readInt64(offset: number): number;
    /**
     * @param {number} offset
     * @returns {number}
     */
    readOffset(offset: number): number;
}
//# sourceMappingURL=dataslice.d.ts.map