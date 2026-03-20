/**
 * @param {string} baseUrl Base URL.
 * @param {import("../extent.js").Extent} extent Extent.
 * @param {import("../size.js").Size} size Size.
 * @param {import("../proj/Projection.js").default} projection Projection.
 * @param {Object} params OGC Map params. Will be modified in place.
 * @return {string} Request URL.
 */
export function getRequestUrl(baseUrl: string, extent: import("../extent.js").Extent, size: import("../size.js").Size, projection: import("../proj/Projection.js").default, params: any): string;
/**
 * @param {import("../extent").Extent} extent Extent.
 * @param {number} resolution Resolution.
 * @param {number} pixelRatio pixel ratio.
 * @param {import("../proj.js").Projection} projection Projection.
 * @param {string} url OGC Map service url.
 * @param {Object} params OGC Map params.
 * @return {string} Image src.
 */
export function getImageSrc(extent: import("../extent").Extent, resolution: number, pixelRatio: number, projection: import("../proj.js").Projection, url: string, params: any): string;
/**
 * @typedef {Object} LoaderOptions
 * @property {null|string} [crossOrigin] The `crossOrigin` attribute for loaded images.  Note that
 * you must provide a `crossOrigin` value if you want to access pixel data with the Canvas renderer.
 * See https://developer.mozilla.org/en-US/docs/Web/HTML/CORS_enabled_image for more detail.
 * @property {ReferrerPolicy} [referrerPolicy] The `referrerPolicy` property for loaded images.
 * @property {boolean} [hidpi=true] Use the `ol/Map#pixelRatio` value when requesting
 * the image from the remote server.
 * @property {Object<string,*>} [params] OGC Map request parameters.
 * No param is required by default. `width`, `height`, `bbox`, `crs` and `bbox-crs` will be set dynamically.
 * @property {import("../proj.js").ProjectionLike} [projection] Projection. Default is 'EPSG:3857'.
 * @property {number} [ratio=1.5] Ratio. `1` means image requests are the size of the map viewport, `2` means
 * twice the width and height of the map viewport, and so on. Must be `1` or higher.
 * @property {string} url OGC Map service URL.
 * @property {function(HTMLImageElement, string): Promise<import('../DataTile.js').ImageLike>} [load] Function
 * to perform loading of the image. Receives the created `HTMLImageElement` and the desired `src` as argument and
 * returns a promise resolving to the loaded or decoded image. Default is {@link module:ol/Image.decode}.
 */
/**
 * Creates a loader for OGC Map images.
 * @param {LoaderOptions} options Loader options.
 * @return {import("../Image.js").ImageObjectPromiseLoader} Loader.
 * @api
 */
export function createLoader(options: LoaderOptions): import("../Image.js").ImageObjectPromiseLoader;
export type LoaderOptions = {
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
     * OGC Map request parameters.
     * No param is required by default. `width`, `height`, `bbox`, `crs` and `bbox-crs` will be set dynamically.
     */
    params?: {
        [x: string]: any;
    } | undefined;
    /**
     * Projection. Default is 'EPSG:3857'.
     */
    projection?: import("../proj.js").ProjectionLike;
    /**
     * Ratio. `1` means image requests are the size of the map viewport, `2` means
     * twice the width and height of the map viewport, and so on. Must be `1` or higher.
     */
    ratio?: number | undefined;
    /**
     * OGC Map service URL.
     */
    url: string;
    /**
     * Function
     * to perform loading of the image. Receives the created `HTMLImageElement` and the desired `src` as argument and
     * returns a promise resolving to the loaded or decoded image. Default is {@link module :ol/Image.decode}.
     */
    load?: ((arg0: HTMLImageElement, arg1: string) => Promise<import("../DataTile.js").ImageLike>) | undefined;
};
//# sourceMappingURL=ogcMapUtil.d.ts.map