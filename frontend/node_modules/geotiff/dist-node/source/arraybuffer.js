"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.makeBufferSource = makeBufferSource;
const basesource_js_1 = require("./basesource.js");
const utils_js_1 = require("../utils.js");
class ArrayBufferSource extends basesource_js_1.BaseSource {
    /**
     * @param {ArrayBuffer} arrayBuffer
     */
    constructor(arrayBuffer) {
        super();
        this.arrayBuffer = arrayBuffer;
    }
    /**
     * @param {import('./basesource.js').Slice} slice
     * @param {AbortSignal} [signal]
     * @returns {Promise<import('./basesource.js').SliceWithData>}
     */
    fetchSlice(slice, signal) {
        if (signal && signal.aborted) {
            throw new utils_js_1.AbortError('Request aborted');
        }
        return Promise.resolve({
            data: this.arrayBuffer.slice(slice.offset, slice.offset + slice.length),
            offset: slice.offset,
            length: slice.length,
        });
    }
}
/** @param {ArrayBuffer} arrayBuffer */
function makeBufferSource(arrayBuffer) {
    return new ArrayBufferSource(arrayBuffer);
}
//# sourceMappingURL=arraybuffer.js.map
//# sourceMappingURL=arraybuffer.js.map