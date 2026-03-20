export default CanvasPolygonBuilder;
declare class CanvasPolygonBuilder extends CanvasBuilder {
    /**
     * @param {Array<number>} flatCoordinates Flat coordinates.
     * @param {number} offset Offset.
     * @param {Array<number>} ends Ends.
     * @param {number} stride Stride.
     * @param {number} [strokeOffset] Stroke Offset in pixels.
     * @private
     * @return {number} End.
     */
    private drawFlatCoordinatess_;
    /**
     * @private
     */
    private setFillStrokeStyles_;
    handleStrokeOffset_(drawGeometryCallback: any): boolean;
}
import CanvasBuilder from './Builder.js';
//# sourceMappingURL=PolygonBuilder.d.ts.map