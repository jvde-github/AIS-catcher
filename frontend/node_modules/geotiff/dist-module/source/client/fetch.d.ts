export class FetchClient extends BaseClient {
    /**
     * @param {string} url
     * @param {RequestCredentials} [credentials]
     */
    constructor(url: string, credentials?: RequestCredentials);
    credentials: RequestCredentials | undefined;
    /**
     * @param {RequestInit} [options={}]
     * @returns {Promise<FetchResponse>}
     */
    request({ headers, signal }?: RequestInit): Promise<FetchResponse>;
}
import { BaseClient } from './base.js';
declare class FetchResponse extends BaseResponse {
    /**
     * BaseResponse facade for fetch API Response
     * @param {Response} response
     */
    constructor(response: Response);
    response: Response;
    getData(): Promise<any>;
}
import { BaseResponse } from './base.js';
export {};
//# sourceMappingURL=fetch.d.ts.map