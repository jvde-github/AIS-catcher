import { Color, FontSpec, BorderRadius } from 'chart.js';
import { PartialEventContext } from './events';
import { DrawTime, Scriptable, ShadowOptions } from './options';

type percentString = `${number}%`;
export type LabelPosition = 'start' | 'center' | 'end' | percentString;

export type LabelTextAlign = 'left' | 'start' | 'center' | 'right' | 'end';

export type CalloutPosition = 'left' | 'top' | 'bottom' | 'right' | 'auto';

export interface LabelPositionObject {
  x?: LabelPosition,
  y?: LabelPosition
}

export interface LabelPadding {
  top?: number,
  left?: number,
  right?: number,
  bottom?: number,
  x?: number,
  y?: number
}

export interface CalloutOptions {
  borderCapStyle?: Scriptable<CanvasLineCap, PartialEventContext>,
  borderColor?: Scriptable<Color, PartialEventContext>,
  borderDash?: Scriptable<number[], PartialEventContext>,
  borderDashOffset?: Scriptable<number, PartialEventContext>,
  borderJoinStyle?: Scriptable<CanvasLineJoin, PartialEventContext>,
  borderWidth?: Scriptable<number, PartialEventContext>,
  display?: Scriptable<boolean, PartialEventContext>,
  margin?: Scriptable<number, PartialEventContext>,
  position?: Scriptable<CalloutPosition, PartialEventContext>,
  side?: Scriptable<number, PartialEventContext>,
  start?: Scriptable<number | string, PartialEventContext>,
}

export interface CoreLabelOptions {
  drawTime?: Scriptable<DrawTime, PartialEventContext>,
  font?: Scriptable<Partial<FontSpec> | Partial<FontSpec>[], PartialEventContext>,
  color?: Scriptable<Color | Color[], PartialEventContext>,
  /**
   * Padding of label
   * @default 6
   */
  padding?: Scriptable<number | LabelPadding, PartialEventContext>,
  /**
   * Text alignment when the content of the label is multi-line.
   * @default 'center'
   */
  textAlign?: Scriptable<LabelTextAlign, PartialEventContext>,
  textStrokeColor?: Scriptable<Color, PartialEventContext>,
  textStrokeWidth?: Scriptable<number, PartialEventContext>,
  /**
   * Adjustment along x-axis (left-right) of label relative to above number (can be negative)
   * For horizontal lines positioned left or right, negative values move
   * the label toward the edge, and positive values toward the center.
   * @default 0
   */
  xAdjust?: Scriptable<number, PartialEventContext>,
  /**
   * Adjustment along y-axis (top-bottom) of label relative to above number (can be negative)
   * For vertical lines positioned top or bottom, negative values move
   * the label toward the edge, and positive values toward the center.
   * @default 0
   */
  yAdjust?: Scriptable<number, PartialEventContext>,
  /**
   * Text to display in label. Provide an array to display multiple lines
   */
  content: Scriptable<string | string[] | HTMLImageElement | HTMLCanvasElement | null, PartialEventContext>,
  /**
   * Overrides the width of the image. Could be set in pixel by a number,
   * or in percentage of current width of image by a string
   */
  width?: Scriptable<number | percentString, PartialEventContext>,
  /**
   * Overrides the height of the image. Could be set in pixel by a number,
   * or in percentage of current height of image by a string
   */
  height?: Scriptable<number | percentString, PartialEventContext>,
  /**
   * Overrides the opacity of the image.
   */
  opacity?: Scriptable<number, PartialEventContext>,
  z?: Scriptable<number, PartialEventContext>
}

export interface ContainedLabelOptions extends CoreLabelOptions {
  backgroundColor?: Scriptable<Color, PartialEventContext>,
  borderWidth?: Scriptable<number, PartialEventContext>,
  borderColor?: Scriptable<Color, PartialEventContext>,
  /**
   * Border line cap style. See MDN.
   * @default 'butt'
   */
  borderCapStyle?: Scriptable<CanvasLineCap, PartialEventContext>,
  /**
   * Border line dash. See MDN.
   * @default []
   */
  borderDash?: Scriptable<number[], PartialEventContext>,
  /**
   * Border line dash offset. See MDN.
   * @default 0.0
   */
  borderDashOffset?: Scriptable<number, PartialEventContext>,
  /**
   * Border line join style. See MDN.
   * @default 'miter'
   */
  borderJoinStyle?: Scriptable<CanvasLineJoin, PartialEventContext>,
  /**
   * Border radius of the label rectangle
   * @default 6
   */
  borderRadius?: Scriptable<number | BorderRadius, PartialEventContext>
}

export interface LabelOptions extends ContainedLabelOptions, ShadowOptions {
  position?: Scriptable<LabelPosition, PartialEventContext>,
  /**
   * Whether the label should be displayed
   * @default true
   */
  display?: Scriptable<boolean, PartialEventContext>,
  hitTolerance?: Scriptable<number, PartialEventContext>,
  /**
   * Rotation of label, in degrees, or 'auto' to use the degrees of the line, default is 0
   * @default 90
   */
  rotation?: Scriptable<number | 'auto', PartialEventContext>,
  z?: Scriptable<number, PartialEventContext>,
  callout?: CalloutOptions,
}

export interface BoxLabelOptions extends CoreLabelOptions {
  position?: Scriptable<LabelPosition | LabelPositionObject, PartialEventContext>,
  /**
   * Whether the label should be displayed
   * @default true
   */
  display?: Scriptable<boolean, PartialEventContext>,
  hitTolerance?: Scriptable<number, PartialEventContext>,
  rotation?: Scriptable<number, PartialEventContext>,
  z?: Scriptable<number, PartialEventContext>
}

export interface LabelTypeOptions extends ContainedLabelOptions {
  position?: Scriptable<LabelPosition | LabelPositionObject, PartialEventContext>,
  z?: Scriptable<number, PartialEventContext>,
  callout?: CalloutOptions,
}

export interface DoughnutLabelOptions extends Omit<ContainedLabelOptions, 'borderCapStyle' | 'borderRadius' | 'padding'> {
  position?: Scriptable<LabelPosition | LabelPositionObject, PartialEventContext>,
  /**
   * Whether the label should be displayed
   * @default true
   */
  display?: Scriptable<boolean, PartialEventContext>,
  rotation?: Scriptable<number, PartialEventContext>
}
