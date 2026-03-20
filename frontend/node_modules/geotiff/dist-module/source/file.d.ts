/** @param {string} path */
export function makeFileSource(path: string): FileSource;
declare class FileSource extends BaseSource {
    /**
     * @param {string} path
     */
    constructor(path: string);
    path: string;
    openRequest: Promise<number>;
}
import { BaseSource } from './basesource.js';
export {};
//# sourceMappingURL=file.d.ts.map