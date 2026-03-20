"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.FetchClient = void 0;
const base_js_1 = require("./base.js");
class FetchResponse extends base_js_1.BaseResponse {
    /**
     * BaseResponse facade for fetch API Response
     * @param {Response} response
     */
    constructor(response) {
        super();
        this.response = response;
    }
    get status() {
        return this.response.status;
    }
    /**
     * @param {string} name
     * @returns {string|undefined}
     */
    getHeader(name) {
        return this.response.headers.get(name) || undefined;
    }
    async getData() {
        const data = this.response.arrayBuffer
            ? await this.response.arrayBuffer()
            // FIXME Can this really have a buffer property?
            : (await /** @type {*} */ (this.response).buffer()).buffer;
        return data;
    }
}
class FetchClient extends base_js_1.BaseClient {
    /**
     * @param {string} url
     * @param {RequestCredentials} [credentials]
     */
    constructor(url, credentials) {
        super(url);
        this.credentials = credentials;
    }
    /**
     * @param {RequestInit} [options={}]
     * @returns {Promise<FetchResponse>}
     */
    async request({ headers, signal } = {}) {
        const response = await fetch(this.url, {
            headers, credentials: this.credentials, signal,
        });
        return new FetchResponse(response);
    }
}
exports.FetchClient = FetchClient;
//# sourceMappingURL=fetch.js.map
//# sourceMappingURL=fetch.js.map