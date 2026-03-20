export class XHRClient extends BaseClient {
    /**
     * @param {Object<string, string>} headers
     * @param {AbortSignal} [signal]
     * @returns {Promise<XHRResponse>}
     */
    constructRequest(headers: {
        [x: string]: string;
    }, signal?: AbortSignal): Promise<XHRResponse>;
    request({ headers, signal }?: {
        headers?: {} | undefined;
        signal?: undefined;
    }): Promise<XHRResponse>;
}
import { BaseClient } from './base.js';
declare class XHRResponse extends BaseResponse {
    /**
     * BaseResponse facade for XMLHttpRequest
     * @param {XMLHttpRequest} xhr
     * @param {ArrayBuffer} data
     */
    constructor(xhr: XMLHttpRequest, data: ArrayBuffer);
    xhr: XMLHttpRequest;
    data: ArrayBuffer;
    get status(): any;
}
import { BaseResponse } from './base.js';
export {};
//# sourceMappingURL=xhr.d.ts.map