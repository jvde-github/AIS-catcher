export default class DataView64 {
    /**
     * @param {ArrayBufferLike} arrayBuffer
     */
    constructor(arrayBuffer: ArrayBufferLike);
    _dataView: DataView<ArrayBufferLike>;
    get buffer(): ArrayBufferLike;
    /**
     * @param {number} offset
     * @param {boolean} littleEndian
     * @returns {number}
     */
    getUint64(offset: number, littleEndian: boolean): number;
    /**
     * Adapted from https://stackoverflow.com/a/55338384/8060591
     * @param {number} offset
     * @param {boolean} littleEndian
     * @returns {number}
     */
    getInt64(offset: number, littleEndian: boolean): number;
    /**
     * @param {number} offset
     * @returns {number}
     */
    getUint8(offset: number): number;
    /**
     * @param {number} offset
     * @returns {number}
     */
    getInt8(offset: number): number;
    /**
     * @param {number} offset
     * @param {boolean} littleEndian
     * @returns {number}
     */
    getUint16(offset: number, littleEndian: boolean): number;
    /**
     * @param {number} offset
     * @param {boolean} littleEndian
     * @returns {number}
     */
    getInt16(offset: number, littleEndian: boolean): number;
    /**
     * @param {number} offset
     * @param {boolean} littleEndian
     * @returns {number}
     */
    getUint32(offset: number, littleEndian: boolean): number;
    /**
     * @param {number} offset
     * @param {boolean} littleEndian
     * @returns {number}
     */
    getInt32(offset: number, littleEndian: boolean): number;
    /**
     * @param {number} offset
     * @param {boolean} littleEndian
     * @returns {number}
     */
    getFloat16(offset: number, littleEndian: boolean): number;
    /**
     * @param {number} offset
     * @param {boolean} littleEndian
     * @returns {number}
     */
    getFloat32(offset: number, littleEndian: boolean): number;
    /**
     * @param {number} offset
     * @param {boolean} littleEndian
     * @returns {number}
     */
    getFloat64(offset: number, littleEndian: boolean): number;
}
//# sourceMappingURL=dataview64.d.ts.map