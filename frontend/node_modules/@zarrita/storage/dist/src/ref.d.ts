import type { AbsolutePath, AsyncReadable } from "./types.js";
export declare function to_binary(base64: string): Uint8Array;
type ReferenceEntry = string | [url: string | null] | [url: string | null, offset: number, length: number];
interface ReferenceStoreOptions {
    target?: string | URL;
    overrides?: RequestInit;
}
/** @experimental */
declare class ReferenceStore implements AsyncReadable<RequestInit> {
    #private;
    constructor(refs: Promise<Map<string, ReferenceEntry>> | Map<string, ReferenceEntry>, opts?: ReferenceStoreOptions);
    get(key: AbsolutePath, opts?: RequestInit): Promise<Uint8Array | undefined>;
    static fromSpec(spec: Promise<Record<string, unknown>> | Record<string, unknown>, opts?: ReferenceStoreOptions): ReferenceStore;
    static fromUrl(url: string | URL, opts?: ReferenceStoreOptions): ReferenceStore;
}
export default ReferenceStore;
