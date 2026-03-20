export class ImageFileDirectory {
    /**
     * Create an ImageFileDirectory.
     * @param {Map<string|number, number|string|Array<number|string>>} actualizedFields the file directory,
     * mapping tag names to values
     * @param {Map<string|number, Function>} deferredFields the deferred fields, mapping tag names to async functions
     * @param {Map<string|number, DeferredArray>} deferredArrays the deferred arrays, mapping tag names to
     * DeferredArray objects
     * @param {number} nextIFDByteOffset the byte offset to the next IFD
     */
    constructor(actualizedFields: Map<string | number, number | string | Array<number | string>>, deferredFields: Map<string | number, Function>, deferredArrays: Map<string | number, DeferredArray>, nextIFDByteOffset: number);
    actualizedFields: Map<string | number, string | number | (string | number)[]>;
    deferredFields: Map<string | number, Function>;
    deferredFieldsBeingResolved: Map<any, any>;
    deferredArrays: Map<string | number, DeferredArray>;
    nextIFDByteOffset: number;
    /**
     * @param {import('./globals.js').TagName|number} tagIdentifier The field tag ID or name
     * @returns {boolean} whether the field exists (actualized or deferred)
     */
    hasTag(tagIdentifier: import("./globals.js").TagName | number): boolean;
    /**
     * Synchronously retrieves the value for a given tag. If it is deferred, an error is thrown.
     * @template {import('./globals.js').EagerTagName | import('./globals.js').EagerTag} [T=any]
     * @param {T} tagIdentifier The field tag ID or name
     * @returns {T extends import('./globals.js').TagName ? (import('./globals.js').TagValue<T> | undefined) : any}
     * the field value,
     * or undefined if it does not exist
     * @throws {Error} If the tag is deferred and requires asynchronous loading
     */
    getValue<T extends import("./globals.js").EagerTagName | import("./globals.js").EagerTag = any>(tagIdentifier: T): T extends import("./globals.js").TagName ? (import("./globals.js").TagValue<T> | undefined) : any;
    /**
     * Retrieves the value for a given tag. If it is deferred, it will be loaded first.
     * @template {import('./globals.js').TagName} [T=any]
     * @param {T|number} tagIdentifier The field tag ID or name
     * @returns {Promise<T extends import('./globals.js').TagName ? (import('./globals.js').TagValue<T> | undefined) : any>}
     *   the field value, or undefined if it does not exist
     */
    loadValue<T extends import("./globals.js").TagName = any>(tagIdentifier: T | number): Promise<T extends import("./globals.js").TagName ? (import("./globals.js").TagValue<T> | undefined) : any>;
    /**
     * Retrieves the value at a given index for a tag that is an array. If it is deferred, it will be loaded first.
     * @param {number|string} tagIdentifier The field tag ID or name
     * @param {number} index The index within the array
     * @returns {Promise<number|string|bigint|undefined>} the field value at the given index, or undefined if it does not exist
     */
    loadValueIndexed(tagIdentifier: number | string, index: number): Promise<number | string | bigint | undefined>;
    /**
     * Parses the GeoTIFF GeoKeyDirectory tag into a structured object.
     * The GeoKeyDirectory is a special TIFF tag that contains geographic metadata
     * in a key-value format as defined by the GeoTIFF specification.
     * @returns {Partial<Record<import('./globals.js').GeoKeyName, *>>|null} Parsed geo key directory
     *     mapping key names to values, or null if not present
     * @throws {Error} If a referenced geo key value cannot be retrieved
     */
    parseGeoKeyDirectory(): Partial<Record<import("./globals.js").GeoKeyName, any>> | null;
    toObject(): Record<string, unknown>;
}
/**
 * Parser for Image File Directories (IFDs).
 */
export class ImageFileDirectoryParser {
    /**
     * @param {import("./source/basesource.js").BaseSource} source the data source to fetch from
     * @param {boolean} littleEndian the endianness of the file
     * @param {boolean} bigTiff whether the file is a BigTIFF
     * @param {boolean} [eager=false] whether to eagerly fetch deferred fields.
     *                                 When false (default), tags are loaded lazily on-demand.
     *                                 When true, all tags are loaded immediately during parsing.
     */
    constructor(source: import("./source/basesource.js").BaseSource, littleEndian: boolean, bigTiff: boolean, eager?: boolean);
    source: import("./source/basesource.js").BaseSource;
    littleEndian: boolean;
    bigTiff: boolean;
    eager: boolean;
    /**
     * Helper function to retrieve a DataSlice from the source.
     * @param {number} offset Byte offset of the slice
     * @param {number} [length] Length of the slice
     * @returns {Promise<DataSlice>}
     */
    getSlice(offset: number, length?: number): Promise<DataSlice>;
    /**
     * Instructs to parse an image file directory at the given file offset.
     * As there is no way to ensure that a location is indeed the start of an IFD,
     * this function must be called with caution (e.g only using the IFD offsets from
     * the headers or other IFDs).
     * @param {number} offset the offset to parse the IFD at
     * @returns {Promise<ImageFileDirectory>} the parsed IFD
     */
    parseFileDirectoryAt(offset: number): Promise<ImageFileDirectory>;
}
/**
 * Lazily-loaded array for large TIFF field values that are fetched on-demand.
 * Supports loading individual indices or the entire array. Uses a bitmap to track
 * which values have been loaded to avoid redundant fetches.
 */
declare class DeferredArray {
    /**
     * Creates a DeferredArray for lazy-loading of large TIFF field arrays.
     * @param {import("./source/basesource.js").BaseSource} source - Data source for fetching
     * @param {number} arrayOffset - Byte offset where the array data starts
     * @param {boolean} littleEndian - Endianness of the data
     * @param {import('./globals.js').FieldType} fieldType - TIFF field type constant
     * @param {number} length - Number of elements in the array
     */
    constructor(source: import("./source/basesource.js").BaseSource, arrayOffset: number, littleEndian: boolean, fieldType: import("./globals.js").FieldType, length: number);
    source: import("./source/basesource.js").BaseSource;
    arrayOffset: number;
    littleEndian: boolean;
    fieldType: import("./globals.js").FieldType;
    length: number;
    data: number[] | import("./geotiff.js").TypedArray;
    itemSize: number;
    maskBitmap: Uint8Array<ArrayBuffer>;
    fetchIndexPromises: Map<any, any>;
    fullFetchPromise: Promise<number[] | Uint8Array<ArrayBufferLike> | Int8Array<ArrayBufferLike> | Uint16Array<ArrayBufferLike> | Int16Array<ArrayBufferLike> | Uint32Array<ArrayBufferLike> | Int32Array<ArrayBufferLike> | Float32Array<ArrayBufferLike> | Float64Array<ArrayBufferLike>> | null;
    /**
     * Loads all values in the deferred array at once.
     * Subsequent calls return the same promise to avoid redundant fetches.
     * @returns {Promise<import('./geotiff.js').TypedArray|Array<number>>} Promise resolving to the fully loaded array
     */
    loadAll(): Promise<import("./geotiff.js").TypedArray | Array<number>>;
    /**
     * Loads and returns a single value at the specified index.
     * If the value is already loaded, returns it immediately. Otherwise, fetches it
     * from the source. Multiple calls for the same index reuse the same promise.
     * @param {number} index - Zero-based index of the value to load
     * @returns {Promise<number|bigint>} Promise resolving to the value at the given index
     * @throws {RangeError} If index is out of bounds
     */
    get(index: number): Promise<number | bigint>;
}
import DataSlice from './dataslice.js';
export {};
//# sourceMappingURL=imagefiledirectory.d.ts.map