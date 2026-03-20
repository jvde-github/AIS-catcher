import type { AbsolutePath, AsyncMutable, RangeQuery } from "./types.js";
declare class FileSystemStore implements AsyncMutable {
    root: string;
    constructor(root: string);
    get(key: AbsolutePath): Promise<Uint8Array | undefined>;
    getRange(key: AbsolutePath, range: RangeQuery): Promise<Uint8Array | undefined>;
    has(key: AbsolutePath): Promise<boolean>;
    set(key: AbsolutePath, value: Uint8Array): Promise<void>;
    delete(key: AbsolutePath): Promise<boolean>;
}
export default FileSystemStore;
