/**
 * @typedef {'nearest'|'linear'} ResampleMethod
 */
/**
 * @typedef {Object} Options
 * @property {string} url The Zarr URL.
 * @property {string} group The group with arrays to render.
 * @property {Array<string>} bands The band names to render.
 * @property {import("../proj.js").ProjectionLike} [projection] Source projection.  If not provided, the GeoTIFF metadata
 * will be read for projection information.
 * @property {number} [transition=250] Duration of the opacity transition for rendering.
 * To disable the opacity transition, pass `transition: 0`.
 * @property {boolean} [wrapX=false] Render tiles beyond the tile grid extent.
 * @property {ResampleMethod} [resample='nearest'] Resamplilng method if bands are not available for all multi-scale levels.
 */
export default class GeoZarr extends DataTileSource<import("../DataTile.js").default> {
    /**
     * @param {Options} options The options.
     */
    constructor(options: Options);
    /**
     * @type {string}
     */
    url_: string;
    /**
     * @type {string}
     */
    group_: string;
    /**
     * @type {Error|null}
     */
    error_: Error | null;
    /**
     * @type {import('zarrita').Group<any>}
     */
    root_: import("zarrita").Group<any>;
    /**
     * @type {any|null}
     */
    consolidatedMetadata_: any | null;
    /**
     * @type {Array<string>}
     */
    bands_: Array<string>;
    /**
     * @type {Object<string, Array<string>> | null}
     */
    bandsByLevel_: {
        [x: string]: Array<string>;
    } | null;
    /**
     * @type {number|undefined}
     */
    fillValue_: number | undefined;
    /**
     * @type {ResampleMethod}
     */
    resampleMethod_: ResampleMethod;
    /**
     * @type {import("../tilegrid/WMTS.js").default}
     */
    tileGrid: import("../tilegrid/WMTS.js").default;
    configure_(): Promise<void>;
    /**
     * @param {number} z The z tile index.
     * @param {number} x The x tile index.
     * @param {number} y The y tile index.
     * @param {import('./DataTile.js').LoaderOptions} options The loader options.
     * @return {Promise} The composed tile data.
     * @private
     */
    private loadTile_;
}
export type DatasetAttributes = {
    /**
     * The multiscales attribute.
     */
    multiscales: Multiscales;
    /**
     * The zarr conventions attribute.
     */
    zarr_conventions: Array<{
        uuid: string;
    }>;
};
export type Multiscales = {
    /**
     * The layout.
     */
    layout: any;
};
export type LegacyDatasetAttributes = {
    /**
     * The multiscales attribute.
     */
    multiscales: LegacyMultiscales;
};
export type LegacyMultiscales = {
    /**
     * The tile matrix limits.
     */
    tile_matrix_limits: any;
    /**
     * The tile matrix set.
     */
    tile_matrix_set: any;
};
export type TileGridInfo = {
    /**
     * The tile grid.
     */
    tileGrid: WMTSTileGrid;
    /**
     * The projection.
     */
    projection: import("../proj/Projection.js").default;
    /**
     * Available bands by level.
     */
    bandsByLevel?: {
        [x: string]: string[];
    } | undefined;
    /**
     * The fill value.
     */
    fillValue?: number | undefined;
};
export type ResampleMethod = "nearest" | "linear";
export type Options = {
    /**
     * The Zarr URL.
     */
    url: string;
    /**
     * The group with arrays to render.
     */
    group: string;
    /**
     * The band names to render.
     */
    bands: Array<string>;
    /**
     * Source projection.  If not provided, the GeoTIFF metadata
     * will be read for projection information.
     */
    projection?: import("../proj.js").ProjectionLike;
    /**
     * Duration of the opacity transition for rendering.
     * To disable the opacity transition, pass `transition: 0`.
     */
    transition?: number | undefined;
    /**
     * Render tiles beyond the tile grid extent.
     */
    wrapX?: boolean | undefined;
    /**
     * Resamplilng method if bands are not available for all multi-scale levels.
     */
    resample?: ResampleMethod | undefined;
};
import DataTileSource from './DataTile.js';
import WMTSTileGrid from '../tilegrid/WMTS.js';
//# sourceMappingURL=GeoZarr.d.ts.map