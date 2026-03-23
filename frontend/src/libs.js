// OpenLayers — tree-shaken imports, reconstructed as window.ol namespace
// (script.js uses ol.* globals as set by the CDN bundle; this replicates that shape)
import Map from 'ol/Map';
import View from 'ol/View';
import TileLayer from 'ol/layer/Tile';
import VectorLayer from 'ol/layer/Vector';
import VectorSource from 'ol/source/Vector';
import OSM from 'ol/source/OSM';
import XYZ from 'ol/source/XYZ';
import TileWMS from 'ol/source/TileWMS';
import Point from 'ol/geom/Point';
import LineString from 'ol/geom/LineString';
import Polygon from 'ol/geom/Polygon';
import Circle from 'ol/geom/Circle';
import Style from 'ol/style/Style';
import Icon from 'ol/style/Icon';
import CircleStyle from 'ol/style/Circle';
import Fill from 'ol/style/Fill';
import Stroke from 'ol/style/Stroke';
import Text from 'ol/style/Text';
import Feature from 'ol/Feature';
import { fromLonLat, toLonLat, transformExtent } from 'ol/proj';
import { getWidth, containsCoordinate } from 'ol/extent';
import { getLength } from 'ol/sphere';

window.ol = {
    Map,
    View,
    layer:  { Tile: TileLayer, Vector: VectorLayer },
    source: { Vector: VectorSource, OSM, XYZ, TileWMS },
    geom:   { Point, LineString, Polygon, Circle },
    style:  { Style, Icon, Circle: CircleStyle, Fill, Stroke, Text },
    Feature,
    proj:   { fromLonLat, toLonLat, transformExtent },
    extent: { getWidth, containsCoordinate },
    sphere: { getLength },
};

// Chart.js
import { Chart, registerables } from 'chart.js';
import annotationPlugin from 'chartjs-plugin-annotation';
Chart.register(...registerables, annotationPlugin);
window.Chart = Chart;

// Tabulator
import { TabulatorFull as Tabulator } from 'tabulator-tables';
window.Tabulator = Tabulator;

// marked (Markdown parser — also used by dynamically loaded plugins)
import { marked } from 'marked';
window.marked = marked;

// CSS is imported via vendor.css and extracted by Vite to style.css
import './vendor.css';
