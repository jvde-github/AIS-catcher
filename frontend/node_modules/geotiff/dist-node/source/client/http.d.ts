export class HttpClient extends BaseClient {
    parsedUrl: urlMod.UrlWithStringQuery;
    httpApi: typeof http | typeof https;
    /**
     * @param {Object<string, string>} headers
     * @param {AbortSignal} [signal]
     * @returns {Promise<HttpResponse>}
     */
    constructRequest(headers: {
        [x: string]: string;
    }, signal?: AbortSignal): Promise<HttpResponse>;
    request({ headers, signal }?: {
        headers?: {} | undefined;
        signal?: undefined;
    }): Promise<HttpResponse>;
}
import { BaseClient } from './base.js';
import urlMod from 'url';
import http from 'http';
import https from 'https';
declare class HttpResponse extends BaseResponse {
    /**
     * BaseResponse facade for node HTTP/HTTPS API Response
     * @param {import('http').IncomingMessage} response
     * @param {Promise<ArrayBuffer>} dataPromise
     */
    constructor(response: import("http").IncomingMessage, dataPromise: Promise<ArrayBuffer>);
    response: http.IncomingMessage;
    dataPromise: Promise<ArrayBuffer>;
}
import { BaseResponse } from './base.js';
export {};
//# sourceMappingURL=http.d.ts.map