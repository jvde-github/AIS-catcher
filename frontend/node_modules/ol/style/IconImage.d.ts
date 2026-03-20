/**
 * @param {HTMLImageElement|HTMLCanvasElement|OffscreenCanvas|ImageBitmap|null} image Image.
 * @param {string|undefined} src Src.
 * @param {import('../dom.js').ImageAttributes} imageAttributes Image attributes options.
 * @param {import("../ImageState.js").default|undefined} imageState Image state.
 * @param {import("../color.js").Color|string|null} color Color.
 * @param {boolean} [pattern] Also cache a `repeat` pattern with the icon image.
 * @return {IconImage} Icon image.
 */
export function get(image: HTMLImageElement | HTMLCanvasElement | OffscreenCanvas | ImageBitmap | null, src: string | undefined, imageAttributes: import("../dom.js").ImageAttributes, imageState: any | undefined, color: import("../color.js").Color | string | null, pattern?: boolean): IconImage;
export default IconImage;
declare class IconImage extends EventTarget {
    /**
     * @param {HTMLImageElement|HTMLCanvasElement|OffscreenCanvas|ImageBitmap|null} image Image.
     * @param {string|undefined} src Src.
     * @param {import('../dom').ImageAttributes} imageAttributes Image attributes options.
     * @param {import("../ImageState.js").default|undefined} imageState Image state.
     * @param {import("../color.js").Color|string|null} color Color.
     */
    constructor(image: HTMLImageElement | HTMLCanvasElement | OffscreenCanvas | ImageBitmap | null, src: string | undefined, imageAttributes: import("../dom").ImageAttributes, imageState: any | undefined, color: import("../color.js").Color | string | null);
    /**
     * @private
     * @type {HTMLImageElement|OffscreenCanvas|HTMLCanvasElement|ImageBitmap}
     */
    private hitDetectionImage_;
    /**
     * @private
     * @type {HTMLImageElement|HTMLCanvasElement|OffscreenCanvas|ImageBitmap|null}
     */
    private image_;
    /**
     * @private
     * @type {string|null}
     */
    private crossOrigin_;
    /**
     * @private
     * @type {ReferrerPolicy}
     */
    private referrerPolicy_;
    /**
     * @private
     * @type {Object<number, HTMLCanvasElement|OffscreenCanvas>}
     */
    private canvas_;
    /**
     * @private
     * @type {import("../color.js").Color|string|null}
     */
    private color_;
    /**
     * @private
     * @type {import("../ImageState.js").default}
     */
    private imageState_;
    /**
     * @private
     * @type {import("../size.js").Size|null}
     */
    private size_;
    /**
     * @private
     * @type {string|undefined}
     */
    private src_;
    /**
     * @private
     * @type {Promise<void>|null}
     */
    private ready_;
    /**
     * @private
     */
    private initializeImage_;
    /**
     * @private
     * @return {boolean} The image canvas is tainted.
     */
    private isTainted_;
    tainted_: boolean | undefined;
    /**
     * @private
     */
    private dispatchChangeEvent_;
    /**
     * @private
     */
    private handleImageError_;
    /**
     * @private
     */
    private handleImageLoad_;
    /**
     * @param {number} pixelRatio Pixel ratio.
     * @return {HTMLImageElement|HTMLCanvasElement|OffscreenCanvas|ImageBitmap} Image or Canvas element or image bitmap.
     */
    getImage(pixelRatio: number): HTMLImageElement | HTMLCanvasElement | OffscreenCanvas | ImageBitmap;
    /**
     * @param {HTMLImageElement|HTMLCanvasElement|OffscreenCanvas|ImageBitmap} image Image.
     */
    setImage(image: HTMLImageElement | HTMLCanvasElement | OffscreenCanvas | ImageBitmap): void;
    /**
     * @param {number} pixelRatio Pixel ratio.
     * @return {number} Image or Canvas element.
     */
    getPixelRatio(pixelRatio: number): number;
    /**
     * @return {import("../ImageState.js").default} Image state.
     */
    getImageState(): any;
    /**
     * @return {HTMLImageElement|HTMLCanvasElement|OffscreenCanvas|ImageBitmap} Image element.
     */
    getHitDetectionImage(): HTMLImageElement | HTMLCanvasElement | OffscreenCanvas | ImageBitmap;
    /**
     * Get the size of the icon (in pixels).
     * @return {import("../size.js").Size} Image size.
     */
    getSize(): import("../size.js").Size;
    /**
     * @return {string|undefined} Image src.
     */
    getSrc(): string | undefined;
    /**
     * Load not yet loaded URI.
     */
    load(): void;
    /**
     * @param {number} pixelRatio Pixel ratio.
     * @private
     */
    private replaceColor_;
    /**
     * @return {Promise<void>} Promise that resolves when the image is loaded.
     */
    ready(): Promise<void>;
}
import EventTarget from '../events/Target.js';
//# sourceMappingURL=IconImage.d.ts.map