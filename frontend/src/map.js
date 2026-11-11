// The OpenLayers map: the map and its view, the vector layers the viewer draws
// on with their styles, and the registries of base maps and overlays with
// their rows in the settings panel. What happens on a click or a hover is the
// entry's: it attaches its handlers to the exported `map` once create() made it.

// Named imports (instead of `import * as ...`) so Vite tree-shakes everything
// outside this list. Plugins relying on other OL classes via window.ol will
// need to be updated; the contract is "what the viewer + bundled plugins use".
import OlMap from 'ol/Map';
import OlView from 'ol/View';
import OlFeature from 'ol/Feature';
import TileLayer from 'ol/layer/Tile';
import VectorLayer from 'ol/layer/Vector';
import VectorTileLayer from 'ol/layer/VectorTile';
import OSMSource from 'ol/source/OSM';
import XYZSource from 'ol/source/XYZ';
import TileWMSSource from 'ol/source/TileWMS';
import VectorSource from 'ol/source/Vector';
import TileGrid from 'ol/tilegrid/TileGrid';
import Point from 'ol/geom/Point';
import LineString from 'ol/geom/LineString';
import Polygon from 'ol/geom/Polygon';
import CircleGeom from 'ol/geom/Circle';
import Style from 'ol/style/Style';
import Stroke from 'ol/style/Stroke';
import Fill from 'ol/style/Fill';
import Icon from 'ol/style/Icon';
import CircleStyle from 'ol/style/Circle';
import Text from 'ol/style/Text';
import { fromLonLat, toLonLat, transformExtent, get as getProjection } from 'ol/proj';
import { getLength } from 'ol/sphere';
import { containsCoordinate, getWidth, getTopLeft } from 'ol/extent';

import * as markersLib from '@aiscatcher/map/markers.js';
import { getICAO } from '@aiscatcher/core/text.js';
import { ships, planes, clock, cardMmsi, cardType, hoverMmsi, hoverType } from './core/store.js';
import { settings, isAndroid } from './core/state.js';
import { saveSettings, beforeWrite, afterWrite } from './settings-store.js';
import { redraw } from './fleet.js';
import { register } from './actions.js';

// the OpenLayers classes the viewer uses, as a namespace for plugins (window.ol)
export const ol = {
    Map: OlMap,
    View: OlView,
    Feature: OlFeature,
    layer: { Tile: TileLayer, Vector: VectorLayer, VectorTile: VectorTileLayer },
    source: { OSM: OSMSource, XYZ: XYZSource, TileWMS: TileWMSSource, Vector: VectorSource },
    tilegrid: { TileGrid },
    geom: { Point, LineString, Polygon, Circle: CircleGeom },
    style: { Style, Stroke, Fill, Icon, Circle: CircleStyle, Text },
    proj: { fromLonLat, toLonLat, transformExtent, get: getProjection },
    sphere: { getLength },
    extent: { containsCoordinate, getWidth, getTopLeft },
};

export let map;

beforeWrite(() => {
    if (!map) return;
    const center = toLonLat(map.getView().getCenter());
    settings.lat = center[1];
    settings.lon = center[0];
    settings.zoom = map.getView().getZoom();
});

export function updateMapURL() {
    if (isAndroid() || !map) return;

    const view = map.getView();
    const center = toLonLat(view.getCenter());
    const newURL = window.location.href.split("?")[0] + "?lat=" + center[1].toFixed(4) + "&lon=" + center[0].toFixed(4) + "&zoom=" + view.getZoom().toFixed(2) + "&tab=" + settings.tab;
    history.replaceState(null, null, newURL);
}

afterWrite(updateMapURL);

// ─── styles ──────────────────────────────────────────────────────────────────

function fadeOpacity(age) {
    if (settings.fading == false) return 1;
    return markersLib.fadeCurve(age);
}

function getShipOpacity(ship) {
    return fadeOpacity(clock - ship.last_signal);
}

export const markers = markersLib.create({
    settings: () => settings,
    hover: () => ({ type: hoverType, id: hoverMmsi }),
    selected: () => ({ type: cardType, id: cardMmsi }),
    opacity: (ship) => getShipOpacity(ship),
    lookup: (type, id) => (type === 'ship' ? ships[id]?.raw : type === 'plane' ? planes[id]?.raw : null),
});

const labelStyle = markers.label((feature) => ('ship' in feature
    ? (feature.ship.shipname || feature.ship.mmsi.toString())
    : (feature.plane.callsign || getICAO(feature.plane))));

// a vessel's marker as a table cell: the sprite turned and scaled as on the
// map, or its type in words when the settings say so
export function getTableShiptype(ship, opacity = 1) {
    if (ship == null) return "";

    markersLib.applySprite(ship);
    const style = `opacity: ${opacity};${markersLib.spriteCSS(ship)} transform: rotate(${ship.rot}rad) scale(${settings.icon_scale});`;
    return settings.table_shiptype_use_icon
        ? `<span class="table-shiptype-icon"><span class="sprites-tint" style="${style}" title="${ship.hint}"></span></span>`
        : ship.hint;
}

// ─── the vector layers ───────────────────────────────────────────────────────

export const markerVector = new VectorSource({ features: [] });
export const shapeVector = new VectorSource({ features: [] });
export const extraVector = new VectorSource({ features: [] });
export const trackVector = new VectorSource({ features: [] });
export const labelVector = new VectorSource({ features: [] });
export const planeVector = new VectorSource({ features: [] });

export const markerLayer = new VectorLayer({
    source: markerVector,
    style: markers.marker
});

export const planeLayer = new VectorLayer({
    source: planeVector,
    style: markers.plane,
    visible: false
});

export const shapeLayer = new VectorLayer({
    source: shapeVector,
    style: markers.hull
});

export const extraLayer = new VectorLayer({
    source: extraVector,
    updateWhileAnimating: true
});

export const trackLayer = new VectorLayer({
    source: trackVector,
    style: markers.track,
    updateWhileAnimating: true
});

export const labelLayer = new VectorLayer({
    source: labelVector,
    style: labelStyle,
    declutter: settings.labels_declutter ?? true
});

// a ring (hover, selection) in the top layer at a vessel's position; returns
// the feature to keep, undefined once the vessel has no position
export function syncCircleFeature(feature, raw, mmsi, styleFn) {
    if (feature) {
        if (raw && raw.lon && raw.lat) {
            feature.setGeometry(new Point(fromLonLat([raw.lon, raw.lat])));
            return feature;
        }
        extraVector.removeFeature(feature);
        return undefined;
    }

    if (raw && raw.lon && raw.lat) {
        feature = new OlFeature(new Point(fromLonLat([raw.lon, raw.lat])));
        feature.setStyle(styleFn);
        feature.mmsi = mmsi;
        extraVector.addFeature(feature);
        return feature;
    }
    return undefined;
}

// ─── base maps and overlays ──────────────────────────────────────────────────

export let basemaps = {};
export let overlapmaps = {};
let activeTileLayer = undefined;

const BASEMAP_KEYS = { day: "map_day", night: "map_night" };
function baseMapRows() { return [...document.querySelectorAll(".basemap-row")]; }
function baseMapSelect(kind) { return document.querySelector('.basemap-select[data-kind="' + kind + '"]'); }

export function addTileLayer(title, layer) {
    basemaps[title] = layer;
}

export function removeTileLayer(title) {
    delete basemaps[title];
}

export function removeTileLayerAll() {
    basemaps = {};
}

export function addOverlayLayer(title, layer) {
    overlapmaps[title] = layer;
    if (map && document.getElementById('overlayContainer')) {
        map.addLayer(layer);
        layer.setVisible(false);
        const visible = Array.isArray(settings.map_overlay) && settings.map_overlay.includes(title);
        layer.setVisible(visible);
        layer.setOpacity(getLayerOpacity(title));
        addOverlayCheckbox(title);
    }
}

export function removeOverlayLayer(title) {
    delete overlapmaps[title];
}

export function removeOverlayLayerAll() {
    overlapmaps = {};
}

function attributionHTML(layer) {
    const a = layer?.getSource()?.getAttributions();
    const raw = typeof a === 'function' ? a() : a;
    const html = Array.isArray(raw) ? raw.join(', ') : (raw || '');
    return html || layer?.get?.('attributions') || '';
}

// <option> holds text only, so the credit has to lose its markup there
function attributionPlain(layer) {
    const div = document.createElement('div');
    div.innerHTML = attributionHTML(layer);
    return (div.textContent || '').replace(/\s+/g, ' ').trim();
}

function updateBaseMapDimLabel(kind, v) {
    document.querySelector('.basemap-row[data-kind="' + kind + '"] .basemap-dim-label').textContent =
        `Dimming (${Math.round(parseFloat(v) * 100)}%)`;
}

export function refreshBaseMapRows() {
    for (const kind of Object.keys(BASEMAP_KEYS)) {
        const sel = baseMapSelect(kind);
        if (!sel) continue;
        sel.value = settings[BASEMAP_KEYS[kind]];
        const key = sel.value;

        const note = sel.closest(".basemap-row").querySelector(".basemap-credit");
        const credit = attributionHTML(basemaps[key]);
        note.innerHTML = credit;
        note.style.display = credit ? "" : "none";

        const dim = 1 - getBaseOpacity(key);
        document.querySelector('.basemap-dim[data-kind="' + kind + '"]').value = dim;
        updateBaseMapDimLabel(kind, dim);

        const active = (kind === "night") === !!settings.dark_mode;
        baseMapRows().filter((r) => r.dataset.kind === kind).forEach((r) => r.classList.toggle("basemap-active", active));
        document.querySelector('.basemap-title[data-kind="' + kind + '"]')?.classList.toggle("basemap-active", active);
    }
}

// polling overlays get their source only once switched on
export function creditArrived(layer) {
    if (layer === activeTileLayer) document.getElementById("map_attributions").innerHTML = attributionHTML(layer);
    refreshBaseMapRows();
}

export function refreshOverlayCredits() {
    document.querySelectorAll('.overlay-row').forEach(row => {
        const credit = attributionHTML(overlapmaps[row.querySelector('input')?.id]);
        let note = row.querySelector('.map-attribution-note');

        if (!credit) {
            if (note) note.remove();
            return;
        }
        if (!note) {
            note = document.createElement('span');
            note.className = 'map-attribution-note';
            row.appendChild(note);
        }
        note.innerHTML = credit;
    });
}

function addOverlayCheckbox(title) {
    const overlayContainer = document.getElementById('overlayContainer');
    if (!overlayContainer || overlayContainer.querySelector(`#${CSS.escape(title)}`)) return;

    const checkbox = document.createElement('input');
    checkbox.type = 'checkbox';
    checkbox.id = title;
    checkbox.name = title;
    checkbox.checked = settings.map_overlay.includes(title);

    const label = document.createElement('label');
    label.setAttribute('for', title);
    label.textContent = title;

    const row = document.createElement('div');
    row.className = 'overlay-row';
    row.appendChild(checkbox);
    row.appendChild(label);

    const dim = document.createElement('div');
    dim.className = 'overlay-dim';

    const slider = document.createElement('input');
    slider.type = 'range';
    slider.className = 'slider';
    slider.min = 0;
    slider.max = 1;
    slider.step = 0.01;
    slider.title = `Dimming for ${title}`;

    const readout = document.createElement('span');
    readout.className = 'overlay-dim-value';

    slider.addEventListener('input', function () {
        setLayerOpacity(title, this.value);
        updateDimRow(row);
    });
    slider.addEventListener('change', saveSettings);

    dim.appendChild(slider);
    dim.appendChild(readout);
    row.appendChild(dim);
    row.classList.toggle('overlay-off', !checkbox.checked);
    updateDimRow(row);

    overlayContainer.appendChild(row);

    checkbox.addEventListener('change', function () {
        overlapmaps[title].setVisible(this.checked);
        if (this.checked) {
            if (!settings.map_overlay.includes(title)) settings.map_overlay.push(title);
        } else {
            const i = settings.map_overlay.indexOf(title);
            if (i > -1) settings.map_overlay.splice(i, 1);
        }
        row.classList.toggle('overlay-off', !this.checked);
        saveSettings();
        redraw();
    });
}

function updateDimRow(row) {
    const title = row.querySelector('input[type="checkbox"]')?.id;
    const slider = row.querySelector('.overlay-dim input[type="range"]');
    if (!title || !slider) return;
    const v = getLayerOpacity(title);
    slider.value = v;
    row.querySelector('.overlay-dim-value').textContent = `${Math.round(v * 100)}%`;
}

function syncOverlayDimmers() {
    document.querySelectorAll('.overlay-row').forEach(updateDimRow);
}

export function updateMapLayer() {
    if (activeTileLayer) {
        setMapOpacity();
        triggerMapLayer();
    }
}

function setMap(kind, key) {
    settings[BASEMAP_KEYS[kind]] = key;
    triggerMapLayer();
    saveSettings();
}

function triggerMapLayer() {

    if (activeTileLayer)
        activeTileLayer.setVisible(false);

    const key = settings.dark_mode ? settings.map_night : settings.map_day;
    activeTileLayer = key in basemaps ? basemaps[key] : basemaps[Object.keys(basemaps)[0]];
    if (!activeTileLayer) return;

    activeTileLayer.setVisible(true);

    for (const overlay of settings.map_overlay) {
        if (overlay in overlapmaps) overlapmaps[overlay].setVisible(true);
    }

    document.getElementById("map_attributions").innerHTML = attributionHTML(activeTileLayer);

    flashAttribution();
}

function opacityOf(bag, title) {
    const v = (bag || {})[title];
    return Number(v === undefined ? settings.map_opacity : v);
}

function getLayerOpacity(title) {
    return opacityOf(settings.layer_opacity, title);
}

function setLayerOpacity(title, value) {
    settings.layer_opacity[title] = Number(value);
    overlapmaps[title]?.setOpacity(Number(value));
}

function getBaseOpacity(title) {
    return opacityOf(settings.basemap_opacity, title);
}

function setMapOpacity() {
    for (let key in basemaps)
        basemaps[key].setOpacity(getBaseOpacity(key));

    for (let key in overlapmaps)
        overlapmaps[key].setOpacity(getLayerOpacity(key));

    syncOverlayDimmers();
}

// the dimming slider of the day or the night base map, released
function setBaseMapDim(kind, value) {
    if (!settings.basemap_opacity) settings.basemap_opacity = {};
    settings.basemap_opacity[baseMapSelect(kind).value] = Math.round((1 - Number(value)) * 100) / 100;
    setMapOpacity();
    saveSettings();
}

// the server's own tile sources changed: the lists in the panel follow
export function baseMapsChanged() {
    for (const kind of Object.keys(BASEMAP_KEYS)) {
        const select = baseMapSelect(kind);
        select.replaceChildren(...Object.keys(basemaps).map(name => new Option(name, name)));
        const key = BASEMAP_KEYS[kind];
        if (!(settings[key] in basemaps)) settings[key] = Object.keys(basemaps)[0];
    }
    Object.keys(overlapmaps).forEach(addOverlayCheckbox);
    refreshBaseMapRows();
    setMapOpacity();
    triggerMapLayer();
}

// ─── the credit line ─────────────────────────────────────────────────────────

let attributionTimer = null;
let attributionPinned = false;

function showAttribution(on) {
    const foldout = document.getElementById('map-attribution-foldout');
    clearTimeout(attributionTimer);
    foldout.classList.toggle('visible', on);
}

function flashAttribution() {
    showAttribution(true);
    attributionTimer = setTimeout(() => showAttribution(attributionPinned), 3000);
}

export function isAttributionPinned() {
    return attributionPinned;
}

// the panel also flashes for 3s on a basemap change, and a menu tick must not
// report that passing state as something the user switched on
function toggleAttribution() {
    attributionPinned = !attributionPinned;
    showAttribution(attributionPinned);
}

// ─── the map ─────────────────────────────────────────────────────────────────

// `stack`: the vector layers bottom to top, the features' among the viewer's
export function create(stack) {
    map = new OlMap({

        target: 'map',
        view: new OlView({
            center: fromLonLat([settings.lon || 0, settings.lat || 0]),
            zoom: settings.zoom || 6,
            enableRotation: false,
        }),
        controls: []
    })

    for (const value of Object.values(basemaps)) {
        map.addLayer(value);
        value.setVisible(false);
    }

    for (const value of Object.values(overlapmaps)) {
        map.addLayer(value);
        value.setVisible(false);
    }

    stack.forEach(layer => {
        map.addLayer(layer);
    });

    triggerMapLayer();

    for (const kind of Object.keys(BASEMAP_KEYS)) {
        const sel = baseMapSelect(kind);
        sel.innerHTML = '';
        Object.keys(basemaps).forEach(key => {
            const option = document.createElement("option");
            option.value = key;
            option.textContent = key;
            option.title = attributionPlain(basemaps[key]) || key;
            sel.appendChild(option);
        });
        sel.addEventListener('change', function () {
            setMap(kind, this.value);
            refreshBaseMapRows();
        });
    }
    refreshBaseMapRows();

    Object.keys(overlapmaps).forEach(addOverlayCheckbox);

    setMapOpacity();
    return map;
}

register({
    // the base map's dimming slider: dragged (the caption) and released (saved)
    setMapOpacity: (e, d, el) => setBaseMapDim(el.dataset.kind, el.value),
    updateBaseMapDim: (e, d, el) => updateBaseMapDimLabel(el.dataset.kind, el.value),
    toggleAttribution: () => toggleAttribution(),
});
