/**
 * Resample the input arrays using nearest neighbor value selection.
 * @param {import("./geotiff.js").TypedArray[]} valueArrays The input arrays to resample
 * @param {number} inWidth The width of the input rasters
 * @param {number} inHeight The height of the input rasters
 * @param {number} outWidth The desired width of the output rasters
 * @param {number} outHeight The desired height of the output rasters
 * @returns {import("./geotiff.js").TypedArray[]} The resampled rasters
 */
export function resampleNearest(valueArrays: import("./geotiff.js").TypedArray[], inWidth: number, inHeight: number, outWidth: number, outHeight: number): import("./geotiff.js").TypedArray[];
/**
 * Resample the input arrays using bilinear interpolation.
 * @param {import("./geotiff.js").TypedArray[]} valueArrays The input arrays to resample
 * @param {number} inWidth The width of the input rasters
 * @param {number} inHeight The height of the input rasters
 * @param {number} outWidth The desired width of the output rasters
 * @param {number} outHeight The desired height of the output rasters
 * @returns {import("./geotiff.js").TypedArray[]} The resampled rasters
 */
export function resampleBilinear(valueArrays: import("./geotiff.js").TypedArray[], inWidth: number, inHeight: number, outWidth: number, outHeight: number): import("./geotiff.js").TypedArray[];
/**
 * Resample the input arrays using the selected resampling method.
 * @param {import("./geotiff.js").TypedArray[]} valueArrays The input arrays to resample
 * @param {number} inWidth The width of the input rasters
 * @param {number} inHeight The height of the input rasters
 * @param {number} outWidth The desired width of the output rasters
 * @param {number} outHeight The desired height of the output rasters
 * @param {string} [method = 'nearest'] The desired resampling method
 * @returns {import("./geotiff.js").TypedArray[]} The resampled rasters
 */
export function resample(valueArrays: import("./geotiff.js").TypedArray[], inWidth: number, inHeight: number, outWidth: number, outHeight: number, method?: string): import("./geotiff.js").TypedArray[];
/**
 * Resample the pixel interleaved input array using nearest neighbor value selection.
 * @param {import("./geotiff.js").TypedArray} valueArray The input array to resample
 * @param {number} inWidth The width of the input rasters
 * @param {number} inHeight The height of the input rasters
 * @param {number} outWidth The desired width of the output rasters
 * @param {number} outHeight The desired height of the output rasters
 * @param {number} samples The number of samples per pixel for pixel
 *                         interleaved data
 * @returns {import("./geotiff.js").TypedArray} The resampled raster
 */
export function resampleNearestInterleaved(valueArray: import("./geotiff.js").TypedArray, inWidth: number, inHeight: number, outWidth: number, outHeight: number, samples: number): import("./geotiff.js").TypedArray;
/**
 * Resample the pixel interleaved input array using bilinear interpolation.
 * @param {import("./geotiff.js").TypedArray} valueArray The input array to resample
 * @param {number} inWidth The width of the input rasters
 * @param {number} inHeight The height of the input rasters
 * @param {number} outWidth The desired width of the output rasters
 * @param {number} outHeight The desired height of the output rasters
 * @param {number} samples The number of samples per pixel for pixel
 *                         interleaved data
 * @returns {import("./geotiff.js").TypedArray} The resampled raster
 */
export function resampleBilinearInterleaved(valueArray: import("./geotiff.js").TypedArray, inWidth: number, inHeight: number, outWidth: number, outHeight: number, samples: number): import("./geotiff.js").TypedArray;
/**
 * Resample the pixel interleaved input array using the selected resampling method.
 * @param {import("./geotiff.js").TypedArray} valueArray The input array to resample
 * @param {number} inWidth The width of the input rasters
 * @param {number} inHeight The height of the input rasters
 * @param {number} outWidth The desired width of the output rasters
 * @param {number} outHeight The desired height of the output rasters
 * @param {number} samples The number of samples per pixel for pixel
 *                                 interleaved data
 * @param {string} [method = 'nearest'] The desired resampling method
 * @returns {import("./geotiff.js").TypedArray} The resampled rasters
 */
export function resampleInterleaved(valueArray: import("./geotiff.js").TypedArray, inWidth: number, inHeight: number, outWidth: number, outHeight: number, samples: number, method?: string): import("./geotiff.js").TypedArray;
//# sourceMappingURL=resample.d.ts.map