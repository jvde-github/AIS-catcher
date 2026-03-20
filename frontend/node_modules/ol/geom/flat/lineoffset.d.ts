/**
 * Offsets a line string to the left / right along its segments direction.
 * Offset is applied to each segment of the line in the direciton of the segment normal (positive offset goes "right" relative to the line direction).
 * For very sharp angles between segments, the function falls back to offsetting along the segment normal direction to avoid excessively long miters.
 *
 * Coordinates and the offset should be in the same units — either pixels or the same spatial reference system as the input line coordinates.
 *
 * @param {Array<number>} flatCoordinates Flat coordinates.
 * @param {number} stride Stride.
 * @param {number} offset Offset distance along the segment normal direction.
 *   Positive values offset to the right relative to the direction of the line.
 *   Negative values offset to the left.
 * @param {boolean} isClosedRing If coordinates build a closed circle (in this the first and the last coordinate offsets will consider previous / next ring coordinate)
 * @param {Array<number>} [dest] Destination coordinate array. If not provided a new one will be created
 * @param {number} [destinationStride] Stride of destination coordinates. If unspecified, assumed to be same as the source coordinates stride.
 * @return {Array<number>} Result flat coordinates of the offset line.
 */
export function offsetLineString(flatCoordinates: Array<number>, stride: number, offset: number, isClosedRing: boolean, dest?: Array<number>, destinationStride?: number): Array<number>;
/**
 * Computes the offset of a single vertex of a line string.
 *
 * The function calculates a new vertex coordinate offset along the normal/miter direction of the line at this vertex.
 * Offset is applied along the segment normal (positive offset goes "right" relative to the line direction).
 * It handles first and last vertices (caps) as well as joins between two segments (mitering).
 * For very sharp angles, the function falls back to offsetting along the segment normal direction to avoid excessively long miters.
 *
 * Coordinates and the offset should be in the same units — either pixels or the same spatial reference system as the input line coordinates.
 *
 * @param {number} x Vertex x-coordinate.
 * @param {number} y Vertex y-coordinate.
 * @param {number|undefined} prevX Previous vertex x-coordinate.
 *   Pass undefined if computing the offset for the first vertex (no previous vertex).
 * @param {number|undefined} prevY Previous vertex y-coordinate.
 *   Pass undefined if computing the offset for the first vertex (no previous vertex).
 * @param {number|undefined} nextX Next vertex x-coordinate.
 *   Pass undefined if computing the offset for the last vertex (no next vertex).
 * @param {number|undefined} nextY Next vertex y-coordinate.
 *   Pass undefined if computing the offset for the last vertex (no next vertex).
 * @param {number} offset Offset distance along the segment normal direction.
 *   Positive values offset to the right relative to the direction from previous to next vertex.
 *   Negative values offset to the left.
 * @return {import("../../coordinate.js").Coordinate} Offset vertex coordinate as `[x, y]`.
 */
export function offsetLineVertex(x: number, y: number, prevX: number | undefined, prevY: number | undefined, nextX: number | undefined, nextY: number | undefined, offset: number): import("../../coordinate.js").Coordinate;
//# sourceMappingURL=lineoffset.d.ts.map