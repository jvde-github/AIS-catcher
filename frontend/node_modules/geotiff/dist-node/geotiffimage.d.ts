export default GeoTIFFImage;
/**
 * GeoTIFF sub-file image.
 */
declare class GeoTIFFImage {
    /**
     * @constructor
     * @param {import("./imagefiledirectory.js").ImageFileDirectory} fileDirectory The parsed file directory
     * @param {Boolean} littleEndian Whether the file is encoded in little or big endian
     * @param {Boolean} cache Whether or not decoded tiles shall be cached
     * @param {import('./source/basesource.js').BaseSource} source The datasource to read from
     */
    constructor(fileDirectory: import("./imagefiledirectory.js").ImageFileDirectory, littleEndian: boolean, cache: boolean, source: import("./source/basesource.js").BaseSource);
    fileDirectory: import("./imagefiledirectory.js").ImageFileDirectory;
    littleEndian: boolean;
    /** @type {Array<Promise<ArrayBufferLike>>|null} */
    tiles: Array<Promise<ArrayBufferLike>> | null;
    isTiled: boolean;
    /** @type {1 | 2} */
    planarConfiguration: 1 | 2;
    source: import("./source/basesource.js").BaseSource;
    /**
     * Returns the associated parsed file directory.
     * @returns {import("./imagefiledirectory.js").ImageFileDirectory} the parsed file directory
     */
    getFileDirectory(): import("./imagefiledirectory.js").ImageFileDirectory;
    /**
     * Returns the associated parsed geo keys.
     * @returns {Partial<Record<import('./globals.js').GeoKeyName, *>>|null} the parsed geo keys
     */
    getGeoKeys(): Partial<Record<import("./globals.js").GeoKeyName, any>> | null;
    /**
     * Returns the width of the image.
     * @returns {Number} the width of the image
     */
    getWidth(): number;
    /**
     * Returns the height of the image.
     * @returns {Number} the height of the image
     */
    getHeight(): number;
    /**
     * Returns the number of samples per pixel.
     * @returns {number} the number of samples per pixel
     */
    getSamplesPerPixel(): number;
    /**
     * Returns the width of each tile.
     * @returns {number} the width of each tile
     */
    getTileWidth(): number;
    /**
     * Returns the height of each tile.
     * @returns {number} the height of each tile
     */
    getTileHeight(): number;
    getBlockWidth(): number;
    /**
     * @param {number} y
     * @returns {number}
     */
    getBlockHeight(y: number): number;
    /**
     * Calculates the number of bytes for each pixel across all samples. Only full
     * bytes are supported, an exception is thrown when this is not the case.
     * @returns {Number} the bytes per pixel
     */
    getBytesPerPixel(): number;
    /**
     * @param {number} i
     * @returns {number}
     */
    getSampleByteSize(i: number): number;
    /**
     * @param {number} sampleIndex
     * @returns {(this: DataView, byteOffset: number, littleEndian: boolean) => number}
     */
    getReaderForSample(sampleIndex: number): (this: DataView, byteOffset: number, littleEndian: boolean) => number;
    getSampleFormat(sampleIndex?: number): number;
    getBitsPerSample(sampleIndex?: number): number;
    /**
     * @param {number} sampleIndex
     * @param {number|ArrayBufferLike} sizeOrData
     * @returns {TypedArray}
     */
    getArrayForSample(sampleIndex: number, sizeOrData: number | ArrayBufferLike): TypedArray;
    /**
     * Returns the decoded strip or tile.
     * @param {Number} x the strip or tile x-offset
     * @param {Number} y the tile y-offset (0 for stripped images)
     * @param {Number} sample the sample to get for separated samples
     * @param {DecoderWorker|import("./geotiff.js").BaseDecoder} poolOrDecoder the decoder or decoder pool
     * @param {AbortSignal} [signal] An AbortSignal that may be signalled if the request is
     *                               to be aborted
     * @returns {Promise.<{x: number, y: number, sample: number, data: ArrayBufferLike}>} the decoded strip or tile
     */
    getTileOrStrip(x: number, y: number, sample: number, poolOrDecoder: DecoderWorker | import("./geotiff.js").BaseDecoder, signal?: AbortSignal): Promise<{
        x: number;
        y: number;
        sample: number;
        data: ArrayBufferLike;
    }>;
    /**
     * Internal read function.
     * @private
     * @param {Array<number>} imageWindow The image window in pixel coordinates
     * @param {Array<number>} samples The selected samples (0-based indices)
     * @param {TypedArray|TypedArray[]} valueArrays The array(s) to write into
     * @param {boolean|undefined} interleave Whether or not to write in an interleaved manner
     * @param {DecoderWorker|import("./geotiff.js").BaseDecoder} poolOrDecoder the decoder or decoder pool
     * @param {number} [width] the width of window to be read into
     * @param {number} [height] the height of window to be read into
     * @param {string} [resampleMethod] the resampling method to be used when interpolating
     * @param {AbortSignal} [signal] An AbortSignal that may be signalled if the request is
     *                               to be aborted
     * @returns {Promise<ReadRasterResult>}
     */
    private _readRaster;
    /**
     * @overload
     * @param {ReadRastersOptions & {interleave: true}} options optional parameters
     * @returns {Promise<import("./geotiff.js").TypedArrayWithDimensions>} the decoded arrays as a promise
     */
    readRasters(options: ReadRastersOptions & {
        interleave: true;
    }): Promise<import("./geotiff.js").TypedArrayWithDimensions>;
    /**
     * @overload
     * @param {ReadRastersOptions & {interleave: false}} options optional parameters
     * @returns {Promise<import("./geotiff.js").TypedArrayArrayWithDimensions>} the decoded arrays as a promise
     */
    readRasters(options: ReadRastersOptions & {
        interleave: false;
    }): Promise<import("./geotiff.js").TypedArrayArrayWithDimensions>;
    /**
     * @overload
     * @param {ReadRastersOptions & {interleave: boolean}} options optional parameters
     * @returns {Promise<ReadRasterResult>} the decoded arrays as a promise
     */
    readRasters(options: ReadRastersOptions & {
        interleave: boolean;
    }): Promise<ReadRasterResult>;
    /**
     * @overload
     * @param {ReadRastersOptions} [options={}] optional parameters
     * @returns {Promise<import("./geotiff.js").TypedArrayArrayWithDimensions>} the decoded arrays as a promise
     */
    readRasters(options?: ReadRastersOptions | undefined): Promise<import("./geotiff.js").TypedArrayArrayWithDimensions>;
    /**
     * @overload
     * @param {ReadRGBOptions & {interleave: true}} options optional parameters
     * @returns {Promise<import("./geotiff.js").TypedArrayWithDimensions>} the RGB array as a Promise
     */
    readRGB(options: ReadRGBOptions & {
        interleave: true;
    }): Promise<import("./geotiff.js").TypedArrayWithDimensions>;
    /**
     * @overload
     * @param {ReadRGBOptions & {interleave: false}} options optional parameters
     * @returns {Promise<import("./geotiff.js").TypedArrayArrayWithDimensions>} the RGB array as a Promise
     */
    readRGB(options: ReadRGBOptions & {
        interleave: false;
    }): Promise<import("./geotiff.js").TypedArrayArrayWithDimensions>;
    /**
     * @overload
     * @param {ReadRGBOptions & {interleave: boolean}} options optional parameters
     * @returns {Promise<ReadRasterResult>} the RGB array as a Promise
     */
    readRGB(options: ReadRGBOptions & {
        interleave: boolean;
    }): Promise<ReadRasterResult>;
    /**
     * @overload
     * @param {ReadRGBOptions} [options={}] optional parameters
     * @returns {Promise<import("./geotiff.js").TypedArrayArrayWithDimensions>} the RGB array as a Promise
     */
    readRGB(options?: ReadRGBOptions | undefined): Promise<import("./geotiff.js").TypedArrayArrayWithDimensions>;
    /**
     * Returns an array of tiepoints.
     * @returns {Promise<Array<{i: number, j: number, k: number, x: number, y: number, z: number}>>} the tiepoints
     */
    getTiePoints(): Promise<Array<{
        i: number;
        j: number;
        k: number;
        x: number;
        y: number;
        z: number;
    }>>;
    /**
     * Returns the parsed GDAL metadata items.
     *
     * If sample is passed to null, dataset-level metadata will be returned.
     * Otherwise only metadata specific to the provided sample will be returned.
     *
     * @param {number|null} [sample=null] The sample index.
     * @returns {Promise<Record<string, unknown>|null>} The GDAL metadata items
     */
    getGDALMetadata(sample?: number | null): Promise<Record<string, unknown> | null>;
    /**
     * Returns the GDAL nodata value
     * @returns {number|null}
     */
    getGDALNoData(): number | null;
    /**
     * Returns the image origin as a XYZ-vector. When the image has no affine
     * transformation, then an exception is thrown.
     * @returns {Array<number>} The origin as a vector
     */
    getOrigin(): Array<number>;
    /**
     * Returns the image resolution as a XYZ-vector. When the image has no affine
     * transformation, then an exception is thrown.
     * @param {GeoTIFFImage|null} [referenceImage=null] A reference image to calculate the resolution from
     *                                             in cases when the current image does not have the
     *                                             required tags on its own.
     * @returns {Array<number>} The resolution as a vector
     */
    getResolution(referenceImage?: GeoTIFFImage | null): Array<number>;
    /**
     * Returns whether or not the pixels of the image depict an area (or point).
     * @returns {Boolean} Whether the pixels are a point
     */
    pixelIsArea(): boolean;
    /**
     * Returns the image bounding box as an array of 4 values: min-x, min-y,
     * max-x and max-y. When the image has no affine transformation, then an
     * exception is thrown.
     * @param {boolean} [tilegrid=false] If true return extent for a tilegrid
     *                                   without adjustment for ModelTransformation.
     * @returns {Array<number>} The bounding box
     */
    getBoundingBox(tilegrid?: boolean): Array<number>;
}
import type { TypedArray } from "./geotiff.js";
import type { DecoderWorker } from "./geotiff.js";
import type { ReadRastersOptions } from "./geotiff.js";
import type { ReadRasterResult } from "./geotiff.js";
import type { ReadRGBOptions } from "./geotiff.js";
//# sourceMappingURL=geotiffimage.d.ts.map