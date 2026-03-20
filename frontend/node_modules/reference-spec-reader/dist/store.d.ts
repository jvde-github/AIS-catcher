export class ReferenceStore {
    /**
     * @param {string | import('./types.js').ReferencesV0 | import('./types.js').ReferencesV1} data
     * @param {{
     *   target?: string;
     *   renderString?: import('./types.js').RenderFn;
     * }=} opts
     */
    static fromJSON(data: string | import('./types.js').ReferencesV0 | import('./types.js').ReferencesV1, opts?: {
        target?: string;
        renderString?: import('./types.js').RenderFn;
    } | undefined): ReferenceStore;
    /**
     * @param {Map<string, import('./types.js').Ref>} references
     * @param {{ target?: string }=} opts
     */
    constructor(references: Map<string, import('./types.js').Ref>, opts?: {
        target?: string;
    } | undefined);
    ref: Map<string, import("./types.js").Ref>;
    target: string | undefined;
    /** @param {string} url */
    _url(url: string): string;
    /**
     * @param {{ url: string, offset?: number, size?: number }} props
     * @param {RequestInit} opts
     */
    _fetch({ url, offset, size }: {
        url: string;
        offset?: number;
        size?: number;
    }, opts: RequestInit): Promise<Response>;
    /**
     * @param {string} key
     * @param {RequestInit} opts
     */
    getItem(key: string, opts?: RequestInit): Promise<ArrayBufferLike>;
    /** @param {string} key */
    containsItem(key: string): Promise<boolean>;
    keys(): Promise<string[]>;
    /**
     * @param {string} key
     * @param {ArrayBuffer} value
     * @returns {never}
     */
    setItem(key: string, value: ArrayBuffer): never;
    /**
     * @param {string} key
     * @returns {never}
     */
    deleteItem(key: string): never;
}
