import { AnnotationOptions } from './options';

export interface AnnotationBoxModel {
  x: number,
  y: number,
  x2: number,
  y2: number,
  centerX: number,
  centerY: number,
  height: number,
  width: number,
  radius?: number
}

export interface AnnotationElement extends AnnotationBoxModel {
  label?: AnnotationElement,
  options: AnnotationOptions
}
