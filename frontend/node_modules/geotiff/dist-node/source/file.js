"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.makeFileSource = makeFileSource;
const fs_1 = __importDefault(require("fs"));
const basesource_js_1 = require("./basesource.js");
/**
 * @param {number} fd
 * @returns {Promise<void>}
 */
function closeAsync(fd) {
    return new Promise((resolve, reject) => {
        fs_1.default.close(fd, (err) => {
            if (err) {
                reject(err);
            }
            else {
                resolve();
            }
        });
    });
}
/**
 * @param {string} path
 * @param {string} flags
 * @param {number|undefined} mode
 * @returns {Promise<number>}
 */
function openAsync(path, flags, mode = undefined) {
    return new Promise((resolve, reject) => {
        fs_1.default.open(path, flags, mode, (err, fd) => {
            if (err) {
                reject(err);
            }
            else {
                resolve(fd);
            }
        });
    });
}
/**
 * @param {number} fd
 * @param {Uint8Array} buffer
 * @param {number} offset
 * @param {number} length
 * @param {number} position
 * @returns {Promise<{bytesRead: number, buffer: Uint8Array}>}
 */
function readAsync(fd, buffer, offset, length, position) {
    return new Promise((resolve, reject) => {
        fs_1.default.read(fd, buffer, offset, length, position, (err, bytesRead, outBuffer) => {
            if (err) {
                reject(err);
            }
            else {
                resolve({ bytesRead, buffer: outBuffer });
            }
        });
    });
}
class FileSource extends basesource_js_1.BaseSource {
    /**
     * @param {string} path
     */
    constructor(path) {
        super();
        this.path = path;
        this.openRequest = openAsync(path, 'r');
    }
    /**
     * @param {import('./basesource.js').Slice} slice
     * @param {AbortSignal} [_signal] not implemented
     * @returns {Promise<import('./basesource.js').SliceWithData>}
     */
    async fetchSlice(slice, _signal) {
        // TODO: use `signal`
        const fd = await this.openRequest;
        const { buffer } = await readAsync(fd, new Uint8Array(slice.length), 0, slice.length, slice.offset);
        return {
            data: buffer.buffer,
            offset: slice.offset,
            length: slice.length,
        };
    }
    async close() {
        const fd = await this.openRequest;
        await closeAsync(fd);
    }
}
/** @param {string} path */
function makeFileSource(path) {
    return new FileSource(path);
}
//# sourceMappingURL=file.js.map
//# sourceMappingURL=file.js.map