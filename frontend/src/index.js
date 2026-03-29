import Chart from 'chart.js/auto';
import Annotation from 'chartjs-plugin-annotation';
import { TabulatorFull as Tabulator } from 'tabulator-tables';
import 'tabulator-tables/dist/css/tabulator.min.css';
import { Map, View, Feature } from 'ol';
import * as olLayer from 'ol/layer';
import * as olSource from 'ol/source';
import * as olGeom from 'ol/geom';
import * as olStyle from 'ol/style';
import * as olProj from 'ol/proj';
import * as olSphere from 'ol/sphere';
import * as olExtent from 'ol/extent';
import 'ol/ol.css';

Chart.register(Annotation);

window.Chart = Chart;
window.Tabulator = Tabulator;
window.ol = {
  Map, View, Feature,
  layer: olLayer,
  source: olSource,
  geom: olGeom,
  style: olStyle,
  proj: olProj,
  sphere: olSphere,
  extent: olExtent,
};
