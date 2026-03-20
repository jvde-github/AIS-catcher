export default OGCMap;
export type Options = {
    /**
     * Attributions.
     */
    attributions?: import("./Source.js").AttributionLike | undefined;
    /**
     * The `crossOrigin` attribute for loaded images.  Note that
     * you must provide a `crossOrigin` value if you want to access pixel data with the Canvas renderer.
     * See https://developer.mozilla.org/en-US/docs/Web/HTML/CORS_enabled_image for more detail.
     */
    crossOrigin?: string | null | undefined;
    /**
     * The `referrerPolicy` property for loaded images.
     */
    referrerPolicy?: ReferrerPolicy | undefined;
    /**
     * Use the `ol/Map#pixelRatio` value when requesting
     * the image from the remote server.
     */
    hidpi?: boolean | undefined;
    /**
     * Optional function to load an image given a URL.
     */
    imageLoadFunction?: import("../Image.js").LoadFunction | undefined;
    /**
     * Use interpolated values when resampling.  By default,
     * linear interpolation is used when resampling.  Set to false to use the nearest neighbor instead.
     */
    interpolate?: boolean | undefined;
    /**
     * OGC Maps request parameters.
     * No param is required by default. `width`, `height`, `bbox`, `crs` and `bbox-crs` will be set dynamically.
     */
    params?: {
        [x: string]: any;
    } | undefined;
    /**
     * Projection. Default is the view projection.
     */
    projection?: import("../proj.js").ProjectionLike;
    /**
     * Ratio. `1` means image requests are the size of the map viewport, `2` means
     * twice the width and height of the map viewport, and so on. Must be `1` or higher.
     */
    ratio?: number | undefined;
    /**
     * Resolutions.
     * If specified, requests will be made for these resolutions only.
     */
    resolutions?: number[] | undefined;
    /**
     * OGC Maps service URL.
     */
    url?: string | undefined;
};
/**
 * @typedef {Object} Options
 * @property {import("./Source.js").AttributionLike} [attributions] Attributions.
 * @property {null|string} [crossOrigin] The `crossOrigin` attribute for loaded images.  Note that
 * you must provide a `crossOrigin` value if you want to access pixel data with the Canvas renderer.
 * See https://developer.mozilla.org/en-US/docs/Web/HTML/CORS_enabled_image for more detail.
 * @property {ReferrerPolicy} [referrerPolicy] The `referrerPolicy` property for loaded images.
 * @property {boolean} [hidpi=true] Use the `ol/Map#pixelRatio` value when requesting
 * the image from the remote server.
 * @property {import("../Image.js").LoadFunction} [imageLoadFunction] Optional function to load an image given a URL.
 * @property {boolean} [interpolate=true] Use interpolated values when resampling.  By default,
 * linear interpolation is used when resampling.  Set to false to use the nearest neighbor instead.
 * @property {Object<string,*>} [params] OGC Maps request parameters.
 * No param is required by default. `width`, `height`, `bbox`, `crs` and `bbox-crs` will be set dynamically.
 * @property {import("../proj.js").ProjectionLike} [projection] Projection. Default is the view projection.
 * @property {number} [ratio=1.5] Ratio. `1` means image requests are the size of the map viewport, `2` means
 * twice the width and height of the map viewport, and so on. Must be `1` or higher.
 * @property {Array<number>} [resolutions] Resolutions.
 * If specified, requests will be made for these resolutions only.
 * @property {string} [url] OGC Maps service URL.
 */
/**
 * @classdesc
 * Source for OGC Maps servers providing single, untiled images.
 *
 * @fires module:ol/source/Image.ImageSourceEvent
 * @api
 */
declare class OGCMap extends ImageSource {
    /**
     * @param {Options} [options] OGCMapOptions options.
     */
    constructor(options?: Options);
    /**
     * @private
     * @type {?string}
     */
    private crossOrigin_;
    /**
     * @private
     * @type {ReferrerPolicy}
     */
    private referrerPolicy_;
    /**
     * @private
     * @type {string|undefined}
     */
    private url_;
    /**
     * @private
     * @type {import("../Image.js").LoadFunction}
     */
    private imageLoadFunction_;
    /**
     * @private
     * @type {!Object}
     */
    private params_;
    /**
     * @private
     * @type {boolean}
     */
    private hidpi_;
    /**
     * @private
     * @type {number}
     */
    private renderedRevision_;
    /**
     * @private
     * @type {number}
     */
    private ratio_;
    /**
     * @private
     * @type {import("../proj/Projection.js").default}
     */
    private loaderProjection_;
    /**
     * Get the user-provided params, i.e. those passed to the constructor through
     * the "params" option, and possibly updated using the updateParams method.
     * @return {Object} Params.
     * @api
     */
    getParams(): any;
    /**
     * Return the image load function of the source.
     * @return {import("../Image.js").LoadFunction} The image load function.
     * @api
     */
    getImageLoadFunction(): import("../Image.js").LoadFunction;
    /**
     * Return the URL used for this OGC Maps source.
     * @return {string|undefined} URL.
     * @api
     */
    getUrl(): string | undefined;
    /**
     * Set the image load function of the source.
     * @param {import("../Image.js").LoadFunction} imageLoadFunction Image load function.
     * @api
     */
    setImageLoadFunction(imageLoadFunction: import("../Image.js").LoadFunction): void;
    /**
     * Set the URL to use for requests.
     * @param {string|undefined} url URL.
     * @api
     */
    setUrl(url: string | undefined): void;
    /**
     * Set the user-provided params.
     * @param {Object} params Params.
     * @api
     */
    setParams(params: any): void;
    /**
     * Update the user-provided params.
     * @param {Object} params Params.
     * @api
     */
    updateParams(params: any): void;
}
import ImageSource from './Image.js';
//# sourceMappingURL=OGCMap.d.ts.map