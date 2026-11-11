// The built-in base maps and overlays, registered with map.js.

import TileLayer from 'ol/layer/Tile';
import VectorTileLayer from 'ol/layer/VectorTile';
import OSMSource from 'ol/source/OSM';
import XYZSource from 'ol/source/XYZ';
import TileWMSSource from 'ol/source/TileWMS';
import { transformExtent } from 'ol/proj';
import { addTileLayer, addOverlayLayer, creditArrived, planeLayer } from './map.js';
import { init as initRainRadar } from './overlays/rainradar.js';

addTileLayer("OpenStreetMap", new TileLayer({
    source: new OSMSource({ maxZoom: 19 })
}));

const LOCAL_LABEL_FONT = ['Arial'];
const OPENFREEMAP_ATTRIBUTION =
    '<a href="https://openfreemap.org">OpenFreeMap</a> ' +
    '<a href="https://www.openmaptiles.org/">&copy; OpenMapTiles</a> ' +
    'Data from <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a>';

// AIS-catcher's own base maps: an OpenFreeMap style in the blues of aiscatcher.org. Every colour of the
// style goes through a tone curve on its lightness: land and labels on one, the water on another, the roads on a third,
// the sea lighter than the land by day and by night, and the other layers keep their order of light and dark.
const CHART_TONES = {
    light: {
        land: [[0, [40, 52, 74]], [0.5, [107, 122, 144]], [0.86, [230, 235, 242]], [0.94, [246, 248, 251]], [1, [255, 255, 255]]],
        water: [[0, [150, 178, 214]], [0.78, [204, 219, 237]], [1, [218, 229, 242]]],
        waterText: 'rgb(84, 106, 148)', waterHalo: 'rgba(218, 229, 242, 0.85)',
    },
    dark: {
        land: [[0, [20, 20, 20]], [0.05, [32, 32, 32]], [0.12, [45, 45, 45]], [0.4, [136, 136, 136]], [1, [229, 229, 229]]],
        water: [[0, [36, 36, 36]], [0.11, [50, 50, 50]], [1, [100, 100, 100]]],
        // roads and rails close to the land, so they guide without taking over the map
        roads: [[0, [26, 26, 26]], [0.25, [46, 46, 46]], [1, [74, 74, 74]]],
        waterText: 'rgb(150, 150, 150)', waterHalo: 'rgba(50, 50, 50, 0.85)',
    },
};

function parseColor(text) {
    if (typeof text !== 'string') return null;
    const t = text.trim();
    let m = t.match(/^#([0-9a-f]{3,8})$/i);
    if (m) {
        let h = m[1];
        if (h.length === 3 || h.length === 4) h = h.split('').map((c) => c + c).join('');
        if (h.length !== 6 && h.length !== 8) return null;
        const n = (i) => parseInt(h.slice(i, i + 2), 16);
        return [n(0), n(2), n(4), h.length === 8 ? n(6) / 255 : 1];
    }
    m = t.match(/^rgba?\(\s*([\d.]+)\s*,\s*([\d.]+)\s*,\s*([\d.]+)\s*(?:,\s*([\d.]+)\s*)?\)$/i);
    if (m) return [+m[1], +m[2], +m[3], m[4] === undefined ? 1 : +m[4]];
    m = t.match(/^hsla?\(\s*([\d.]+)\s*,\s*([\d.]+)%\s*,\s*([\d.]+)%\s*(?:,\s*([\d.]+)\s*)?\)$/i);
    if (m) {
        const h = +m[1] / 360, sat = +m[2] / 100, l = +m[3] / 100;
        const q = l < 0.5 ? l * (1 + sat) : l + sat - l * sat, p = 2 * l - q;
        const ch = (x) => { x = (x + 1) % 1; return 255 * (x < 1 / 6 ? p + (q - p) * 6 * x : x < 1 / 2 ? q : x < 2 / 3 ? p + (q - p) * (2 / 3 - x) * 6 : p); };
        return [ch(h + 1 / 3), ch(h), ch(h - 1 / 3), m[4] === undefined ? 1 : +m[4]];
    }
    return null;
}

function toneColor(text, curve) {
    const c = parseColor(text);
    if (!c) return text;
    const l = (0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]) / 255;
    let i = 1;
    while (i < curve.length - 1 && l > curve[i][0]) i++;
    const [l0, a] = curve[i - 1], [l1, b] = curve[i];
    const f = l1 > l0 ? Math.min(1, Math.max(0, (l - l0) / (l1 - l0))) : 0;
    const rgb = a.map((v, k) => Math.round(v + (b[k] - v) * f));
    return c[3] < 1 ? `rgba(${rgb.join(',')},${c[3]})` : `rgb(${rgb.join(',')})`;
}

// every colour of a paint value: a plain colour, an expression or the stops of an old style function
function toneValue(value, curve) {
    if (typeof value === 'string') return toneColor(value, curve);
    if (Array.isArray(value)) return value.map((v) => toneValue(v, curve));
    if (value && typeof value === 'object') {
        const out = {};
        for (const k in value) out[k] = toneValue(value[k], curve);
        return out;
    }
    return value;
}

const CHART_LABELS = /^(label|place)_(town|city|country)/;

function waterLabels(tones) {
    const name = ['coalesce', ['get', 'name_en'], ['get', 'name']];
    const paint = { 'text-color': tones.waterText, 'text-halo-color': tones.waterHalo, 'text-halo-width': 1.2 };
    const lines = ['match', ['geometry-type'], ['LineString', 'MultiLineString'], true, false];
    const along = (size) => ({ 'symbol-placement': 'line', 'symbol-spacing': 350, 'text-field': name, 'text-font': ['Arial Italic'], 'text-letter-spacing': 0.1, 'text-size': size });
    return [
        { id: 'waterway_name', type: 'symbol', source: 'openmaptiles', 'source-layer': 'waterway', minzoom: 9, filter: lines, layout: along(12), paint },
        { id: 'water_name_line', type: 'symbol', source: 'openmaptiles', 'source-layer': 'water_name', filter: lines, layout: along(13), paint },
        {
            id: 'water_name_point', type: 'symbol', source: 'openmaptiles', 'source-layer': 'water_name',
            filter: ['match', ['geometry-type'], ['MultiPoint', 'Point'], true, false],
            layout: { 'text-field': name, 'text-font': ['Arial Italic'], 'text-letter-spacing': 0.2, 'text-max-width': 6, 'text-size': ['interpolate', ['linear'], ['zoom'], 0, 10, 8, 14] },
            paint,
        },
    ];
}

function toneStyle(glStyle, tones, labels) {
    glStyle.layers = (glStyle.layers || []).filter((l) => l.type !== 'symbol' || CHART_LABELS.test(l.id));
    for (const l of glStyle.layers) {
        // a wide view names the countries and the large cities
        if (l.type === 'symbol' && /town/.test(l.id)) l.minzoom = Math.max(l.minzoom || 0, 8);
        if (l.type === 'symbol' && /_city$|country_other/.test(l.id)) l.minzoom = Math.max(l.minzoom || 0, 6);
        const water = /^water/.test(l.id) && l.type !== 'symbol';
        const road = /^(highway|road|aeroway|railway|tunnel|bridge)/.test(l.id) && l.type !== 'symbol';
        const curve = water ? tones.water : road && tones.roads ? tones.roads : tones.land;
        for (const k in l.paint || {})
            if (/color/.test(k)) l.paint[k] = toneValue(l.paint[k], curve);
        if (l.type === 'line' && /^boundary/.test(l.id))
            l.paint = { ...l.paint, 'line-opacity': /state|_3/.test(l.id) ? 0.15 : 0.22 };
    }
    if (labels) {
        const at = glStyle.layers.findIndex((l) => l.type === 'symbol');
        glStyle.layers.splice(at < 0 ? glStyle.layers.length : at, 0, ...waterLabels(tones));
    }
}

function addVectorBasemap(title, styleUrl, noLabels, tones) {
    const layer = new VectorTileLayer({ declutter: true });
    let applied = false;
    layer.on('change:visible', () => {
        if (!layer.getVisible() || applied) return;
        applied = true;
        Promise.all([import('ol-mapbox-style'), fetch(styleUrl).then((r) => r.json())])
            .then(([{ applyStyle }, glStyle]) => {
                for (const l of glStyle.layers || [])
                    if (l.layout && l.layout['text-font']) l.layout['text-font'] = LOCAL_LABEL_FONT;
                if (noLabels) glStyle.layers = (glStyle.layers || []).filter((l) => l.type !== 'symbol');
                if (tones) toneStyle(glStyle, tones, !noLabels);
                const background = (glStyle.layers || []).find((l) => l.type === 'background')?.paint?.['background-color'];
                return applyStyle(layer, glStyle, { styleUrl }).then(() => {
                    if (background)
                        layer.on('prerender', (evt) => {
                            const ctx = evt.context;
                            ctx.save();
                            ctx.fillStyle = background;
                            ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);
                            ctx.restore();
                        });
                    layer.on('postrender', (evt) => {
                        const dim = 1 - layer.getOpacity();
                        if (dim <= 0) return;
                        const ctx = evt.context;
                        ctx.save();
                        ctx.globalAlpha = dim;
                        ctx.fillStyle = '#000';
                        ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);
                        ctx.restore();
                    });
                    layer.getSource()?.setAttributions(OPENFREEMAP_ATTRIBUTION);
                    creditArrived(layer);
                });
            })
            .catch((err) => {
                applied = false;
                console.error('basemap "' + title + '" failed to load:', err);
            });
    });
    addTileLayer(title, layer);
}

addVectorBasemap("AIS-catcher Light", 'https://tiles.openfreemap.org/styles/positron', false, CHART_TONES.light);
addVectorBasemap("AIS-catcher Light (no labels)", 'https://tiles.openfreemap.org/styles/positron', true, CHART_TONES.light);
addVectorBasemap("AIS-catcher Dark", 'https://tiles.openfreemap.org/styles/dark', false, CHART_TONES.dark);
addVectorBasemap("OpenFreeMap Positron", 'https://tiles.openfreemap.org/styles/positron');
addVectorBasemap("OpenFreeMap Positron (no labels)", 'https://tiles.openfreemap.org/styles/positron', true);
addVectorBasemap("OpenFreeMap Bright", 'https://tiles.openfreemap.org/styles/bright');
addVectorBasemap("OpenFreeMap Liberty", 'https://tiles.openfreemap.org/styles/liberty');
addVectorBasemap("OpenFreeMap Dark", 'https://tiles.openfreemap.org/styles/dark');
addVectorBasemap("OpenFreeMap Dark (no labels)", 'https://tiles.openfreemap.org/styles/dark', true);

addTileLayer("Satellite", new TileLayer({
    source: new XYZSource({
        url: 'https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}',
        attributions: 'Esri et al.'
        // maxZoom is not specified, so it defaults to the OpenLayers default
    })
}));

addOverlayLayer("OpenSeaMap", new TileLayer({
    source: new XYZSource({
        url: 'https://tiles.openseamap.org/seamark/{z}/{x}/{y}.png',
        attributions: 'Map data: &copy; <a href="https://www.openseamap.org">OpenSeaMap</a>'
    })
}));

addOverlayLayer("NOAA", new TileLayer({
    source: new TileWMSSource({
        url: 'https://gis.charttools.noaa.gov/arcgis/rest/services/MCS/ENCOnline/MapServer/exts/MaritimeChartService/WMSServer?',
        params: {
            'LAYERS': '1,2,3,4,5,6,7',
            'FORMAT': 'image/png',
            'TRANSPARENT': 'true',
            'VERSION': '1.3.0'
        },
        serverType: 'geoserver',
        attributions: 'Charts: &copy; <a href="https://nauticalcharts.noaa.gov">NOAA</a>'
    })
}));

// the Spanish Navy's hydrographic office: Spain, the Canaries and a few Portuguese cells. The
// chart comes whole, land and all, so the layer keeps to where it has one.
addOverlayLayer("ENC Spain (IHM)", new TileLayer({
    extent: transformExtent([-21, 19.3, 6.3, 47], 'EPSG:4326', 'EPSG:3857'),
    source: new XYZSource({
        url: 'https://ideihm.covam.es/ihmcache/wmts/1.0.0/RasterENC/default/googlemapscompatible/{z}/{y}/{x}.png',
        attributions: 'Charts: &copy; <a href="https://ideihm.covam.es/">Instituto Hidrogr&aacute;fico de la Marina</a>, <a href="https://ideihm.covam.es/portal/licencias/">licence</a>, not for navigation'
    })
}));

// more official charts, free for any use: each draws its chart whole, land and all, so it keeps
// to where it has one
const chartExtent = (lonlat) => transformExtent(lonlat, 'EPSG:4326', 'EPSG:3857');
const chartWMS = (url, layers, attributions) => new TileWMSSource({
    url, params: { 'LAYERS': layers, 'FORMAT': 'image/png', 'TRANSPARENT': 'true', 'VERSION': '1.3.0' }, attributions
});

addOverlayLayer("Charts Norway (Kartverket)", new TileLayer({
    extent: chartExtent([-15, 53.7, 44.3, 81.8]),
    source: new XYZSource({
        url: 'https://cache.kartverket.no/v1/wmts/1.0.0/sjokartraster/default/webmercator/{z}/{y}/{x}.png',
        attributions: 'Charts: &copy; <a href="https://www.kartverket.no/">Kartverket</a> (<a href="https://creativecommons.org/licenses/by/4.0/">CC BY 4.0</a>)'
    })
}));

addOverlayLayer("Charts Finland (Traficom)", new TileLayer({
    extent: chartExtent([19, 59.3, 32, 70.1]),
    source: new XYZSource({
        url: 'https://julkinen.traficom.fi/rasteripalvelu/wmts/rest/Traficom:Merikarttasarjat%20public/default/WGS84_Pseudo-Mercator/WGS84_Pseudo-Mercator:{z}/{y}/{x}?format=image/png',
        attributions: 'Charts: &copy; <a href="https://www.traficom.fi/">Traficom</a> (<a href="https://creativecommons.org/licenses/by/4.0/">CC BY 4.0</a>)'
    })
}));

addOverlayLayer("ENC Canada (CHS)", new TileLayer({
    extent: chartExtent([-141, 39.4, -32.2, 84]),
    source: chartWMS('https://egisp.dfo-mpo.gc.ca/arcgis/rest/services/chs/ENC_MaritimeChartService/MapServer/exts/MaritimeChartService/WMSServer',
        '0,1,2,3,4,5,6,7,8,9,10,11,12', 'Charts: &copy; <a href="https://www.charts.gc.ca/">Canadian Hydrographic Service</a>, contains information licensed under the <a href="https://open.canada.ca/en/open-government-licence-canada">Open Government Licence – Canada</a>')
}));

addOverlayLayer("Inland ENC Germany (WSV)", new TileLayer({
    extent: chartExtent([6.0, 47.5, 15.1, 55.1]),
    source: chartWMS('https://via.bund.de/wsv/ienc/wms', 'IENC', 'Charts: &copy; <a href="https://www.elwis.de/">WSV</a> (<a href="https://www.govdata.de/dl-de/zero-2-0">DL-DE Zero 2.0</a>)')
}));

addOverlayLayer("Inland ENC Netherlands (RWS)", new TileLayer({
    extent: chartExtent([3.13, 50.76, 7.22, 53.6]),
    source: chartWMS('https://geo.rijkswaterstaat.nl/arcgis/rest/services/ENC/mcs_inland/MapServer/exts/MaritimeChartService/WMSServer',
        '0,1,2,3,4,5,6,7,8,9,10', 'Charts: &copy; <a href="https://www.rijkswaterstaat.nl/">Rijkswaterstaat</a> (<a href="https://creativecommons.org/publicdomain/zero/1.0/">CC0</a>)')
}));

initRainRadar(addOverlayLayer);

addOverlayLayer("Aircraft", planeLayer);
