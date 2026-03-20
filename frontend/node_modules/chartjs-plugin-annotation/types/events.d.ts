import { Chart, ChartEvent } from 'chart.js';
import { AnnotationElement } from './element';

export interface EventContext {
  chart: Chart,
  element: AnnotationElement,
  elements: AnnotationElement[],
  id: string,
  type: string
}

/**
 * Some scriptable options may be called with during the chart's initial
 * display, when the element isn't fully initialized.
 */
export interface PartialEventContext {
  chart: Chart,
  element?: Partial<AnnotationElement>,
  elements?: AnnotationElement[],
  id?: string,
  type?: string
}

export interface AnnotationEvents {
  enter?(context: EventContext, event: ChartEvent): boolean | void,
  leave?(context: EventContext, event: ChartEvent): boolean | void,
  click?(context: EventContext, event: ChartEvent): boolean | void,
}
