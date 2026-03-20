/**
 * @module ol/dom
 */
/**
 * @typedef {Object} ImageAttributes
 * @property {string|null} [crossOrigin] Cross origin.
 * @property {ReferrerPolicy} [referrerPolicy]  Referrer policy.
 */
/**
 * Create an html canvas element and returns its 2d context.
 * @param {number} [width] Canvas width.
 * @param {number} [height] Canvas height.
 * @param {Array<HTMLCanvasElement|OffscreenCanvas>} [canvasPool] Canvas pool to take existing canvas from.
 * @param {CanvasRenderingContext2DSettings} [settings] CanvasRenderingContext2DSettings
 * @return {CanvasRenderingContext2D|OffscreenCanvasRenderingContext2D} The context.
 */
export function createCanvasContext2D(width?: number, height?: number, canvasPool?: Array<HTMLCanvasElement | OffscreenCanvas>, settings?: CanvasRenderingContext2DSettings): CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D;
/**
 * @return {CanvasRenderingContext2D|OffscreenCanvasRenderingContext2D} Shared canvas context.
 */
export function getSharedCanvasContext2D(): CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D;
/**
 * Releases canvas memory to avoid exceeding memory limits in Safari.
 * See https://pqina.nl/blog/total-canvas-memory-use-exceeds-the-maximum-limit/
 * @param {CanvasRenderingContext2D|OffscreenCanvasRenderingContext2D} context Context.
 */
export function releaseCanvas(context: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D): void;
/**
 * Get the current computed width for the given element including margin,
 * padding and border.
 * Equivalent to jQuery's `$(el).outerWidth(true)`.
 * @param {!HTMLElement} element Element.
 * @return {number} The width.
 */
export function outerWidth(element: HTMLElement): number;
/**
 * Get the current computed height for the given element including margin,
 * padding and border.
 * Equivalent to jQuery's `$(el).outerHeight(true)`.
 * @param {!HTMLElement} element Element.
 * @return {number} The height.
 */
export function outerHeight(element: HTMLElement): number;
/**
 * @param {Node} newNode Node to replace old node
 * @param {Node} oldNode The node to be replaced
 */
export function replaceNode(newNode: Node, oldNode: Node): void;
/**
 * @param {Node} node The node to remove the children from.
 */
export function removeChildren(node: Node): void;
/**
 * Transform the children of a parent node so they match the
 * provided list of children.  This function aims to efficiently
 * remove, add, and reorder child nodes while maintaining a simple
 * implementation (it is not guaranteed to minimize DOM operations).
 * @param {Node} node The parent node whose children need reworking.
 * @param {Array<Node>} children The desired children.
 */
export function replaceChildren(node: Node, children: Array<Node>): void;
/**
 * Creates a minimal structure that mocks a DIV to be used by the composite and
 * layer renderer in a worker environment
 * @return {HTMLDivElement} mocked DIV
 */
export function createMockDiv(): HTMLDivElement;
/***
 * @param {*} obj The object to check.
 * @return {obj is (HTMLCanvasElement | OffscreenCanvas)} The object is a canvas.
 */
export function isCanvas(obj: any): obj is (HTMLCanvasElement | OffscreenCanvas);
export type ImageAttributes = {
    /**
     * Cross origin.
     */
    crossOrigin?: string | null | undefined;
    /**
     * Referrer policy.
     */
    referrerPolicy?: ReferrerPolicy | undefined;
};
//# sourceMappingURL=dom.d.ts.map