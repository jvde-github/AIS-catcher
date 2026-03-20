export class BaseResponse {
    /**
     * Returns whether the response has an ok'ish status code
     */
    get ok(): boolean;
    /**
     * Returns the status code of the response
     * @returns {number} the status code
     */
    get status(): number;
    /**
     * Returns the value of the specified header
     * @param {string} _headerName the header name
     * @returns {string|undefined} the header value
     */
    getHeader(_headerName: string): string | undefined;
    /**
     * @returns {Promise<ArrayBuffer>} the response data of the request
     */
    getData(): Promise<ArrayBuffer>;
}
export class BaseClient {
    /** @param {string} url */
    constructor(url: string);
    url: string;
    /**
     * Send a request with the options
     * @param {RequestInit} [_options={}]
     * @returns {Promise<BaseResponse>}
     */
    request(_options?: RequestInit): Promise<BaseResponse>;
}
//# sourceMappingURL=base.d.ts.map