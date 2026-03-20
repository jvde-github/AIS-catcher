export default ImageTile;
declare class ImageTile extends Tile {
    /**
     * @param {import("./tilecoord.js").TileCoord} tileCoord Tile coordinate.
     * @param {import("./TileState.js").default} state State.
     * @param {string} src Image source URI.
     * @param {import('./dom.js').ImageAttributes} imageAttributes Image attributes options.
     * @param {import("./Tile.js").LoadFunction} tileLoadFunction Tile load function.
     * @param {import("./Tile.js").Options} [options] Tile options.
     */
    constructor(tileCoord: import("./tilecoord.js").TileCoord, state: any, src: string, imageAttributes: import("./dom.js").ImageAttributes, tileLoadFunction: import("./Tile.js").LoadFunction, options?: import("./Tile.js").Options);
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
     * Image URI
     *
     * @private
     * @type {string}
     */
    private src_;
    /**
     * @private
     * @type {HTMLImageElement|HTMLCanvasElement|OffscreenCanvas}
     */
    private image_;
    /**
     * @private
     * @type {?function():void}
     */
    private unlisten_;
    /**
     * @private
     * @type {import("./Tile.js").LoadFunction}
     */
    private tileLoadFunction_;
    /**
     * Get the HTML image element for this tile (may be a Canvas, OffscreenCanvas, Image, or Video).
     * @return {HTMLCanvasElement|OffscreenCanvas|HTMLImageElement|HTMLVideoElement} Image.
     * @api
     */
    getImage(): HTMLCanvasElement | OffscreenCanvas | HTMLImageElement | HTMLVideoElement;
    /**
     * Sets an HTML image element for this tile (may be a Canvas or preloaded Image).
     * @param {HTMLCanvasElement|OffscreenCanvas|HTMLImageElement} element Element.
     */
    setImage(element: HTMLCanvasElement | OffscreenCanvas | HTMLImageElement): void;
    /**
     * Get the cross origin of the ImageTile.
     * @return {string} Cross origin.
     */
    getCrossOrigin(): string;
    /**
     * Get the referrer policy of the ImageTile.
     * @return {ReferrerPolicy} Referrer policy.
     */
    getReferrerPolicy(): ReferrerPolicy;
    /**
     * Tracks loading or read errors.
     *
     * @private
     */
    private handleImageError_;
    /**
     * Tracks successful image load.
     *
     * @private
     */
    private handleImageLoad_;
    /**
     * Discards event handlers which listen for load completion or errors.
     *
     * @private
     */
    private unlistenImage_;
}
import Tile from './Tile.js';
//# sourceMappingURL=ImageTile.d.ts.map