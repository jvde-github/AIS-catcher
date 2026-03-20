"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.XHRClient = void 0;
const base_js_1 = require("./base.js");
const utils_js_1 = require("../../utils.js");
class XHRResponse extends base_js_1.BaseResponse {
    /**
     * BaseResponse facade for XMLHttpRequest
     * @param {XMLHttpRequest} xhr
     * @param {ArrayBuffer} data
     */
    constructor(xhr, data) {
        super();
        this.xhr = xhr;
        this.data = data;
    }
    get status() {
        return this.xhr.status;
    }
    /**
     * @param {string} name
     * @returns {string|undefined}
     */
    getHeader(name) {
        return this.xhr.getResponseHeader(name) || undefined;
    }
    async getData() {
        return this.data;
    }
}
class XHRClient extends base_js_1.BaseClient {
    /**
     * @param {Object<string, string>} headers
     * @param {AbortSignal} [signal]
     * @returns {Promise<XHRResponse>}
     */
    constructRequest(headers, signal) {
        return new Promise((resolve, reject) => {
            const xhr = new XMLHttpRequest();
            xhr.open('GET', this.url);
            xhr.responseType = 'arraybuffer';
            for (const [key, value] of Object.entries(headers)) {
                xhr.setRequestHeader(key, value);
            }
            // hook signals
            xhr.onload = () => {
                const data = xhr.response;
                resolve(new XHRResponse(xhr, data));
            };
            xhr.onerror = reject;
            xhr.onabort = () => reject(new utils_js_1.AbortError('Request aborted'));
            xhr.send();
            if (signal) {
                if (signal.aborted) {
                    xhr.abort();
                }
                signal.addEventListener('abort', () => xhr.abort());
            }
        });
    }
    async request({ headers = {}, signal = undefined } = {}) {
        const response = await this.constructRequest(headers, signal);
        return response;
    }
}
exports.XHRClient = XHRClient;
//# sourceMappingURL=xhr.js.map
//# sourceMappingURL=xhr.js.map