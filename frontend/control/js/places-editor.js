import {placeFeature} from '@aiscatcher/core/places.js';
import GeoJSON from 'ol/format/GeoJSON.js';
import Draw from 'ol/interaction/Draw.js';
import Modify from 'ol/interaction/Modify.js';
import { shapeFromGeoJSON } from './shape.js';
import * as tooltip from '@aiscatcher/ui/tooltip.js';
import { ConfigStore } from './config-manager.js';
const TOOL_ICONS = {
    pentagon: 'M298-200h364l123-369-305-213-305 213 123 369Zm-58 80L80-600l400-280 400 280-160 480H240Zm240-371Z',
    add_location_alt: 'M480-80Q319-217 239.5-334.5T160-552q0-150 96.5-239T480-880h20q10 0 20 2v81q-10-2-19.5-2.5T480-800q-101 0-170.5 69.5T240-552q0 71 59 162.5T480-186q122-112 181-203.5T720-552v-8h80v8q0 100-79.5 217.5T480-80Zm56.5-423.5Q560-527 560-560t-23.5-56.5Q513-640 480-640t-56.5 23.5Q400-593 400-560t23.5 56.5Q447-480 480-480t56.5-23.5ZM480-560Zm240-80h80v-120h120v-80H800v-120h-80v120H600v80h120v120Z',
    undo: 'M280-200v-80h284q63 0 109.5-40T720-420q0-60-46.5-100T564-560H312l104 104-56 56-200-200 200-200 56 56-104 104h252q97 0 166.5 63T800-420q0 94-69.5 157T564-200H280Z',
    redo: 'M396-200q-97 0-166.5-63T160-420q0-94 69.5-157T396-640h252L544-744l56-56 200 200-200 200-56-56 104-104H396q-63 0-109.5 40T240-420q0 60 46.5 100T396-280h284v80H396Z',
    fit_screen: 'M800-600v-120H680v-80h120q33 0 56.5 23.5T880-720v120h-80Zm-720 0v-120q0-33 23.5-56.5T160-800h120v80H160v120H80Zm600 440v-80h120v-120h80v120q0 33-23.5 56.5T800-160H680Zm-520 0q-33 0-56.5-23.5T80-240v-120h80v120h120v80H160Zm80-160v-320h480v320H240Zm80-80h320v-160H320v160Zm0 0v-160 160Z',
    upload: 'M440-320v-326L336-542l-56-58 200-200 200 200-56 58-104-104v326h-80ZM240-160q-33 0-56.5-23.5T160-240v-120h80v120h480v-120h80v120q0 33-23.5 56.5T720-160H240Z',
    download: 'M480-320 280-520l56-58 104 104v-326h80v326l104-104 56 58-200 200ZM240-160q-33 0-56.5-23.5T160-240v-120h80v120h480v-120h80v120q0 33-23.5 56.5T720-160H240Z',
    content_paste: 'M200-120q-33 0-56.5-23.5T120-200v-560q0-33 23.5-56.5T200-840h167q11-35 43-57.5t70-22.5q40 0 71.5 22.5T594-840h166q33 0 56.5 23.5T840-760v560q0 33-23.5 56.5T760-120H200Zm0-80h560v-560h-80v120H280v-120h-80v560Zm308.5-571.5Q520-783 520-800t-11.5-28.5Q497-840 480-840t-28.5 11.5Q440-817 440-800t11.5 28.5Q463-760 480-760t28.5-11.5Z',
    fullscreen: 'M120-120v-200h80v120h120v80H120Zm520 0v-80h120v-120h80v200H640ZM120-640v-200h200v80H200v120h-80Zm640 0v-120H640v-80h200v200h-80Z',
    fullscreen_exit: 'M240-120v-120H120v-80h200v200h-80Zm400 0v-200h200v80H720v120h-80ZM120-640v-80h120v-120h80v200H120Zm520 0v-200h80v120h120v80H640Z',
    add: 'M440-440H200v-80h240v-240h80v240h240v80H520v240h-80v-240Z',
    remove: 'M200-440v-80h560v80H200Z',
    delete: 'M280-120q-33 0-56.5-23.5T200-200v-520h-40v-80h200v-40h240v40h200v80h-40v520q0 33-23.5 56.5T680-120H280Zm400-600H280v520h400v-520ZM360-280h80v-360h-80v360Zm160 0h80v-360h-80v360ZM280-720v520-520Z'
};
const toolSvg = icon => `<svg class="pe-icon" xmlns="http://www.w3.org/2000/svg" viewBox="0 -960 960 960" aria-hidden="true"><path d="${TOOL_ICONS[icon]}"/></svg>`;
// a map tool: an icon with a tooltip, or with `text` a labelled one (the toggles)
const tool = (name, label, icon, text) => text
    ? `<button type="button" class="pe-tool" data-do="${name}" aria-pressed="false" aria-label="${label}">${toolSvg(icon)}<span class="pe-tool__label">${text}</span></button>`
    : `<button type="button" class="pe-tool pe-tool--icon" data-do="${name}" title="${label}">${toolSvg(icon)}</button>`;
const sep = '<span class="pe-toolbar__sep" aria-hidden="true"></span>';
// line icons in the style of the design: 24-unit grid, stroked
const ICON = {
    plus: '<svg class="pe-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round" aria-hidden="true"><path d="M12 5v14M5 12h14"/></svg>',
    dots: '<svg class="pe-icon" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><circle cx="5" cy="12" r="1.8"/><circle cx="12" cy="12" r="1.8"/><circle cx="19" cy="12" r="1.8"/></svg>',
    search: '<svg class="pe-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" aria-hidden="true"><circle cx="11" cy="11" r="6.5"/><path d="M20 20l-4.2-4.2"/></svg>',
    chevron: '<svg class="pe-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M6 9l6 6 6-6"/></svg>',
    trash: '<svg class="pe-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 7h16M10 11v6M14 11v6M6 7l1 13h10l1-13M9 7V4h6v3"/></svg>',
    anchor: '<svg class="pe-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><circle cx="12" cy="5" r="2"/><path d="M12 7v14M5 13a7 7 0 0 0 14 0M8 10h8"/></svg>',
    area: '<svg class="pe-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linejoin="round" aria-hidden="true"><path d="M4 7l7-3 9 5-3 10-11-1z"/></svg>',
    streets: '<svg class="pe-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linejoin="round" aria-hidden="true"><path d="M3 6l6-2 6 2 6-2v14l-6 2-6-2-6 2z"/><path d="M9 4v14M15 6v14"/></svg>',
    light: '<svg class="pe-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linejoin="round" aria-hidden="true"><path d="M3 6l6-2 6 2 6-2v14l-6 2-6-2-6 2z" fill="currentColor" fill-opacity=".15"/></svg>',
    satellite: '<svg class="pe-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><circle cx="12" cy="12" r="9"/><path d="M3 12h18M12 3c2.5 2.6 3.8 5.6 3.8 9s-1.3 6.4-3.8 9c-2.5-2.6-3.8-5.6-3.8-9S9.5 5.6 12 3z"/></svg>',
};
// the three backgrounds, each with the credit its source asks for
const OSM_CREDIT = '© <a href="https://www.openstreetmap.org/copyright" target="_blank" rel="noopener">OpenStreetMap</a> contributors';
const ESRI = 'https://server.arcgisonline.com/ArcGIS/rest/services/';
const BASEMAPS = {
    streets: {label: 'Streets', dim: 0.1, credit: OSM_CREDIT},
    light: {label: 'Light', url: ESRI + 'Canvas/World_Light_Gray_Base/MapServer/tile/{z}/{y}/{x}', maxZoom: 16,
            credit: 'Tiles © Esri — Esri, HERE, Garmin, ' + OSM_CREDIT},
    satellite: {label: 'Satellite', url: ESRI + 'World_Imagery/MapServer/tile/{z}/{y}/{x}', maxZoom: 19,
                credit: 'Imagery © Esri — Esri, Maxar, Earthstar Geographics and the GIS User Community'},
};
const BASEMAP_KEY = 'places.basemap';
const AREA_TYPES = new Set(['area', 'section']);
let toolTips = null;
import TileLayer from 'ol/layer/Tile.js';
import VectorLayer from 'ol/layer/Vector.js';
import Map from 'ol/Map.js';
import Point from 'ol/geom/Point.js';
import MultiPoint from 'ol/geom/MultiPoint.js';
import {fromLonLat, toLonLat} from 'ol/proj.js';
import OSM from 'ol/source/OSM.js';
import XYZ from 'ol/source/XYZ.js';
import VectorSource from 'ol/source/Vector.js';
import Fill from 'ol/style/Fill.js';
import CircleStyle from 'ol/style/Circle.js';
import Stroke from 'ol/style/Stroke.js';
import Style from 'ol/style/Style.js';
import Text from 'ol/style/Text.js';
import View from 'ol/View.js';


const code = p => p?.codes?.unlocode?.[0] || p?.codes?.own?.[0] || '';
const format = new GeoJSON();
const read = f => format.readFeature(f, {featureProjection: 'EPSG:3857'});
const geometry = f =>
    format.writeGeometryObject(f.getGeometry(), {featureProjection: 'EPSG:3857', rightHanded: true});
const clone = value => JSON.parse(JSON.stringify(value));
const uuid = () => {
    const b = crypto.getRandomValues(new Uint8Array(16));
    b[6] = (b[6] & 15) | 64;
    b[8] = (b[8] & 63) | 128;
    const s = [...b].map(n => n.toString(16).padStart(2, '0')).join('');
    return `${s.slice(0, 8)}-${s.slice(8, 12)}-${s.slice(12, 16)}-${s.slice(16, 20)}-${s.slice(20)}`;
};
export function createPlaceEditor(host, options = {}) {
    host.classList.add('pe');
    const segments = Object.entries(BASEMAPS).map(([key, b]) =>
        `<button class="pe-segment" type="button" role="radio" aria-checked="false" data-basemap="${key}">${ICON[key]}${b.label}</button>`).join('');
    host.innerHTML = `
      <div class="pe-body place-workspace">
        <aside class="pe-sidebar place-sidebar">
          <div class="pe-sidebar__actions">
            <button class="pe-btn pe-btn--primary" type="button" data-do="new">${ICON.plus}New place</button>
            <details class="pe-menu place-menu"><summary class="pe-icon-btn pe-icon-btn--outline" aria-label="Import, export and other place actions" title="More actions">${ICON.dots}</summary>
              <div class="pe-menu__items place-menu-items">
                <button class="pe-menu__item" type="button" data-do="import">Import…</button>
                <button class="pe-menu__item" type="button" data-do="export">Export</button>
                <button class="pe-menu__item" type="button" data-do="view-json">View GeoJSON</button>
                <button class="pe-menu__item" type="button" data-do="reload">Reload</button>
              </div>
            </details>
          </div>
          <div class="pe-search">${ICON.search}<input class="pe-input" type="search" data-field="search" aria-label="Search places" placeholder="Search places" autocomplete="off"></div>
          <div class="pe-list place-list" role="listbox" aria-label="Saved places"></div>
          <span class="pe-sidebar__count" data-count></span>
        </aside>
        <section class="pe-editor place-detail">
          <div class="pe-empty" data-empty><strong>No place selected</strong><span>Pick a place from the list or on the map, or create a new one.</span></div>
          <fieldset class="pe-form place-fields" disabled hidden>
            <label class="pe-field" data-col="1"><span class="pe-label">Name</span><input class="pe-input" data-field="name" maxlength="120" placeholder="Place name"></label>
            <label class="pe-field" data-col="2"><span class="pe-label">Type</span><span class="pe-select-wrap"><select class="pe-select" data-field="place_type"><option value="port">Port</option><option value="berth">Berth</option><option value="anchorage">Anchorage</option><option value="section">Section</option><option value="terminal">Terminal</option><option value="mooring">Mooring</option><option value="marina">Marina</option><option value="area">Area</option></select>${ICON.chevron}</span></label>
            <label class="pe-field" data-col="3" data-row="unlocode"><span class="pe-label">UN/LOCODE</span><input class="pe-input pe-input--mono" data-field="unlocode" maxlength="5" placeholder="NLRTM" spellcheck="false" autocomplete="off"></label>
            <label class="pe-field" data-col="1" data-line="2" data-row="part_of"><span class="pe-label">Parent</span><span class="pe-select-wrap"><select class="pe-select" data-field="part_of"><option value="">Unknown</option><option value="none">No parent</option></select>${ICON.chevron}</span></label>
            <label class="pe-field" data-col="2" data-line="2" data-row="category"><span class="pe-label">Category</span><input class="pe-input" data-field="category" list="place-categories" maxlength="60" placeholder="VTS, restricted place…"><datalist id="place-categories"></datalist></label>
            <label class="pe-field" data-col="3" data-line="2" data-row="size"><span class="pe-label">Marker from zoom</span><span class="pe-select-wrap"><select class="pe-select" data-field="size"><option value="0">12 · very small</option><option value="1">11 · small</option><option value="2">9 · medium</option><option value="3">7 · large</option></select>${ICON.chevron}</span></label>
          </fieldset>
          <div class="pe-map place-map-wrap">
            <div class="pe-map__canvas place-map"></div>
            <div class="pe-toolbar pe-toolbar--tools" role="toolbar" aria-label="Shape tools">${tool('draw', 'Draw a boundary', 'pentagon', 'Draw')}${tool('point', 'Set a point', 'add_location_alt', 'Point')}${tool('delete-part', 'Delete a part', 'delete')}${sep}${tool('undo', 'Undo', 'undo')}${tool('redo', 'Redo', 'redo')}${sep}${tool('import-shape', 'Import shape', 'upload')}${tool('export-shape', 'Export shape', 'download')}${tool('paste-shape', 'Paste shape', 'content_paste')}</div>
            <div class="pe-toolbar pe-toolbar--view" role="toolbar" aria-label="Map view">${tool('fit', 'Fit to shape', 'fit_screen')}${tool('fullscreen', 'Full screen', 'fullscreen')}${sep}${tool('zoom-in', 'Zoom in', 'add')}${tool('zoom-out', 'Zoom out', 'remove')}</div>
            <div class="pe-paste place-paste" hidden><textarea class="pe-input" data-paste rows="7" spellcheck="false" placeholder="Paste GeoJSON: a geometry, a Feature, or a collection holding one"></textarea>
              <div class="pe-paste__actions"><button class="pe-btn" type="button" data-do="paste-close">Close</button><button class="pe-btn pe-btn--primary" type="button" data-do="paste-apply">Apply</button></div></div>
            <div class="pe-map__hint" aria-live="polite" hidden><strong><span data-points>0</span> <span data-points-word>points</span></strong><span class="pe-map__hint-sep" aria-hidden="true"></span><span data-hint></span></div>
            <div class="pe-toolbar pe-toolbar--layers" role="radiogroup" aria-label="Map style">${segments}</div>
            <div class="pe-attribution place-attribution"></div>
          </div>
        </section>
      </div>
      <footer class="pe-footer">
        <button class="pe-btn pe-btn--danger" type="button" data-do="delete" hidden>${ICON.trash}<span class="pe-btn__label">Delete place</span></button>
        <span class="pe-footer__spacer"></span>
        <span class="pe-feedback place-feedback" role="status" aria-live="polite"></span>
        <span class="pe-dirty" role="status" hidden>Unsaved changes</span>
        <button class="pe-btn" type="button" data-do="cancel" hidden>Cancel</button>
        <button class="pe-btn pe-btn--primary" type="button" data-do="edit" hidden>Edit place</button>
        <button class="pe-btn pe-btn--primary" type="button" data-do="save" hidden>Save place</button>
      </footer>
      <input type="file" data-file accept=".geojson,.json,application/geo+json,application/json" hidden>`;
    if (!toolTips)
        toolTips = tooltip.create({selector: '.pe-toolbar .pe-tool--icon', placement: 'below'});
    toolTips.prepare(host);
    const q = s => host.querySelector(s);
    const input = name => q(`[data-field="${name}"]`);
    let placeSettings = null, catalogueVersion = '';
    let records = [], draft = null, original = null, busy = false, dead = false, draw = null, undo = [],
        redo = [], unsaved = false, editingMode = false, deleteArmed = false, drawType = null;
    const editing = new VectorSource();
    // every other place nearby, in a quiet slate, so a new outline is drawn against its neighbours
    const context = new VectorSource();
    const markers = new VectorSource(); // a dot per catalogue entry; see refreshMarkers
    const shapes = new globalThis.Map(); // uuid:revision -> the full feature, fetched once (Map here is OpenLayers')
    // The shape's colours come from the stylesheet, so they follow day and night:
    // read them once here and again whenever the theme flips
    let S = null;
    function makeStyles() {
        const css = getComputedStyle(host), v = n => css.getPropertyValue(n).trim();
        const t = {stroke: v('--pe-shape-stroke') || '#2563c9', fill: v('--pe-shape-fill') || 'rgba(37,99,201,.16)',
                   vertexFill: v('--pe-vertex-fill') || '#f8f8fa', vertexStroke: v('--pe-vertex-stroke') || '#2563c9',
                   halo: v('--pe-vertex-active-halo') || 'rgba(37,99,201,.25)', midpoint: v('--pe-midpoint-fill') || 'rgba(28,30,33,.6)',
                   context: v('--pe-context-stroke') || '#64748b', contextFill: v('--pe-context-fill') || 'rgba(100,116,139,.12)',
                   marker: v('--pe-marker-fill') || 'rgba(100,116,139,.6)', danger: v('--color-danger') || '#c62828'};
        S = {
            t,
            // the outline; a point place is a filled dot in the same blue
            shape: new Style({stroke: new Stroke({color: t.stroke, width: 2.2, lineJoin: 'round'}), fill: new Fill({color: t.fill}),
                              image: new CircleStyle({radius: 7, fill: new Fill({color: t.stroke}), stroke: new Stroke({color: t.vertexFill, width: 2.5})}),
                              declutterMode: 'none'}),
            vertex: new CircleStyle({radius: 5, fill: new Fill({color: t.vertexFill}), stroke: new Stroke({color: t.vertexStroke, width: 2})}),
            midpoint: new CircleStyle({radius: 2.5, fill: new Fill({color: t.midpoint})}),
            // the corner under the pointer or being dragged
            handle: [new Style({image: new CircleStyle({radius: 12, fill: new Fill({color: t.halo})})}),
                     new Style({image: new CircleStyle({radius: 7, fill: new Fill({color: t.stroke}), stroke: new Stroke({color: t.vertexFill, width: 2.5})})})],
            context: new Style({image: new CircleStyle({radius: 4, fill: new Fill({color: t.context})}), stroke: new Stroke({color: t.context, width: 1.5}),
                                fill: new Fill({color: t.contextFill})}),
            markers: [3, 4.5, 6, 7.5].map(radius => new Style({image: new CircleStyle({radius, fill: new Fill({color: t.marker}),
                                                                                       stroke: new Stroke({color: '#fff', width: 1})})})),
            doomed: new Style({stroke: new Stroke({color: t.danger, width: 3}), fill: new Fill({color: 'rgba(220, 38, 38, 0.25)'})}),
        };
    }
    makeStyles();
    // Every corner of the shape being edited says where it is, white on the
    // outline's blue, so a label reads against the sea, the land and the fill.
    //
    // The layer declutters, which is what thins them: a hundred-point outline
    // zoomed out would otherwise stack its labels into an unreadable block, and
    // the corner handles must stay visible through it, so they decline to take
    // part. Zooming in gives every corner its own again.
    const coordinateLabel = (coordinate) => {
        const [lon, lat] = toLonLat(coordinate);
        return new Style({
            geometry: new Point(coordinate),
            text: new Text({
                text: lat.toFixed(5) + '\n' + lon.toFixed(5),
                font: '10px ui-monospace, SFMono-Regular, Menlo, monospace',
                textAlign: 'center',
                offsetY: -20,
                fill: new Fill({color: '#fff'}),
                backgroundFill: new Fill({color: S.t.stroke}),
                padding: [2, 4, 2, 4],
            }),
        });
    };
    const corners = (coordinates, out = []) => {
        if (typeof coordinates[0] === 'number') out.push(coordinates);
        else for (const part of coordinates) corners(part, out);
        return out;
    };
    // the rings of a polygon or multipolygon, in map coordinates
    const rings = g => g.getType() === 'Polygon' ? g.getCoordinates() : g.getType() === 'MultiPolygon' ? g.getCoordinates().flat() : [];
    const activeStyleFn = (feature) => {
        const geometry = feature.getGeometry();
        if (!geometry || typeof geometry.getCoordinates !== 'function') return S.shape;
        const styles = [S.shape];
        if (!editingMode || geometry.getType() === 'Point') return styles;
        // while editing, every corner is a handle and every edge has a midpoint to drag from
        const seen = new Set(), vertices = [], mids = [];
        for (const ring of rings(geometry))
            for (let i = 0; i + 1 < ring.length; i++) {
                const a = ring[i], b = ring[i + 1];
                mids.push([(a[0] + b[0]) / 2, (a[1] + b[1]) / 2]);
                const key = a[0] + ':' + a[1]; // a ring closes on its first point
                if (seen.has(key)) continue;
                seen.add(key);
                vertices.push(a);
            }
        styles.push(new Style({geometry: new MultiPoint(mids), image: S.midpoint}),
                    new Style({geometry: new MultiPoint(vertices), image: S.vertex}));
        for (const corner of vertices) styles.push(coordinateLabel(corner));
        return styles;
    };
    // the corners of the draft, for the count on the map
    function pointCount() {
        const g = draft?.geometry;
        if (!g) return 0;
        if (g.type === 'Point') return 1;
        const seen = new Set();
        for (const c of corners(g.coordinates)) seen.add(c[0] + ':' + c[1]);
        return seen.size;
    }
    // a dot per place, sized by the port's own size class - the 0..3 that decides
    // from which zoom the viewer shows its marker - so the big ports read first
    const markerStyle = f => S.markers[Math.min(3, Math.max(0, f.get('size') | 0))];
    const map = new Map({
        target: q('.place-map'),
        controls: [],
        layers: [
            new TileLayer({
                className: 'place-seamark', // its own canvas: sharing one with the base map left it unrendered
                source: new XYZ({
                    url: 'https://tiles.openseamap.org/seamark/{z}/{x}/{y}.png',
                    attributions: 'Map data: &copy; <a href="https://www.openseamap.org">OpenSeaMap</a>'
                }),
                opacity: 0.5
            }),
            new VectorLayer({className: 'place-markers', source: markers, style: markerStyle}),
            new VectorLayer({className: 'place-context', source: context, style: () => S.context}),
            new VectorLayer({
                source: editing,
                style: activeStyleFn,
                declutter: true
            })
        ],
        view: new View({center: fromLonLat([4.4, 51.95]), zoom: 10})
    });
    // the background: streets, a light grey canvas or satellite imagery, kept per browser
    const base = new TileLayer({className: 'place-base'});
    let basemap = 'streets';
    try { if (BASEMAPS[localStorage.getItem(BASEMAP_KEY)]) basemap = localStorage.getItem(BASEMAP_KEY); } catch { /* private mode */ }
    // the street map is dimmed a little so the outlines drawn over it stand out
    base.on('postrender', e => {
        const dim = BASEMAPS[basemap].dim;
        if (!dim) return;
        e.context.save();
        e.context.setTransform(1, 0, 0, 1, 0, 0);
        e.context.fillStyle = `rgba(0, 0, 0, ${dim})`;
        e.context.fillRect(0, 0, e.context.canvas.width, e.context.canvas.height);
        e.context.restore();
    });
    function setBasemap(key, remember = true) {
        if (!BASEMAPS[key]) return;
        basemap = key;
        const b = BASEMAPS[key];
        base.setSource(b.url ? new XYZ({url: b.url, maxZoom: b.maxZoom, crossOrigin: 'anonymous'}) : new OSM());
        for (const seg of host.querySelectorAll('.pe-segment')) {
            const on = seg.dataset.basemap === key;
            seg.setAttribute('aria-checked', String(on));
            seg.tabIndex = on ? 0 : -1;
        }
        q('.pe-attribution').innerHTML = b.credit + ' · Seamarks © <a href="https://www.openseamap.org" target="_blank" rel="noopener">OpenSeaMap</a>';
        if (remember) try { localStorage.setItem(BASEMAP_KEY, key); } catch { /* private mode */ }
    }
    setBasemap(basemap, false);
    map.getLayers().insertAt(0, base);
    const modify = new Modify({source: editing, style: () => S.handle});
    map.addInteraction(modify);
    const resize = new ResizeObserver(() => map.updateSize());
    resize.observe(q('.place-map-wrap'));
    // day and night: the hub flips a class on <html>; the shape follows
    const themeWatch = new MutationObserver(() => {
        makeStyles();
        for (const layer of map.getLayers().getArray()) layer.changed();
    });
    themeWatch.observe(document.documentElement, {attributes: true, attributeFilter: ['class', 'data-theme']});
    host.querySelector('.pe-toolbar--layers').addEventListener('keydown', e => {
        if (e.key !== 'ArrowLeft' && e.key !== 'ArrowRight') return;
        e.preventDefault();
        const keys = Object.keys(BASEMAPS), next = keys[(keys.indexOf(basemap) + (e.key === 'ArrowRight' ? 1 : keys.length - 1)) % keys.length];
        setBasemap(next);
        q(`.pe-segment[data-basemap="${next}"]`).focus();
    });
    for (const seg of host.querySelectorAll('.pe-segment'))
        seg.addEventListener('click', () => setBasemap(seg.dataset.basemap));
    function status(message, error = false) {
        q('.place-feedback').textContent = message;
        q('.place-feedback').dataset.error = error;
    }
    function dirty(value) {
        unsaved = value;
        buttons();
    }
    function buttons() {
        q('[data-empty]').hidden = !!draft;
        q('.place-fields').hidden = !draft;
        input('search').hidden = false;
        q('.place-list').hidden = false;
        for (const action of ['save', 'cancel'])
            q(`[data-do="${action}"]`).hidden = !editingMode;
        for (const action of ['export', 'view-json'])
            q(`[data-do="${action}"]`).hidden = !draft;
        q('[data-do="edit"]').hidden = !draft || editingMode;
        q('[data-do="delete"]').hidden = !original;
        for (const action of ['draw', 'point', 'delete-part', 'undo', 'redo', 'import-shape', 'paste-shape'])
            q(`[data-do="${action}"]`).hidden = !editingMode;
        q('[data-do="export-shape"]').hidden = !draft?.geometry;
        tidyTools();
        if (!editingMode) q('.place-paste').hidden = true;
        q('.place-fields').disabled = !editingMode || busy;
        q('.pe-dirty').hidden = !unsaved;
        q('[data-count]').textContent = records.length === 1 ? '1 saved place' : `${records.length.toLocaleString()} saved places`;
        q('.place-feedback').classList.toggle('place-busy', busy);
        for (const b of host.querySelectorAll('button[data-do]'))
            b.disabled = busy;
        for (const action of ['save', 'cancel', 'delete', 'draw', 'point', 'export', 'view-json'])
            q(`[data-do="${action}"]`).disabled = busy || !draft || (action === 'delete' && !original);
        q('[data-do="fit"]').disabled = busy || !draft?.geometry;
        q('[data-do="export-shape"]').disabled = busy || !draft?.geometry;
        q('[data-do="delete-part"]').disabled = busy || !draft?.geometry;
        q('[data-do="undo"]').disabled = busy || !undo.length;
        q('[data-do="redo"]').disabled = busy || !redo.length;
        if (busy || !editingMode || !draft?.geometry)
            armDelete(false);
        modify.setActive(!busy && editingMode && !!draft && !draw && !deleteArmed);
        hint();
    }
    // the line on the map: how many corners, and what a click does now
    function hint() {
        const box = q('.pe-map__hint');
        box.hidden = !editingMode || !draft;
        if (box.hidden) return;
        const n = pointCount();
        q('[data-points]').textContent = n.toLocaleString();
        q('[data-points-word]').textContent = n === 1 ? 'point' : 'points';
        q('[data-hint]').textContent =
            draw ? (drawType === 'Point' ? 'Click the map to place the point · Esc cancels' :
                                           'Click to add corners · double-click or the first corner finishes · Esc cancels') :
            deleteArmed ? 'Click a part to delete it · Esc cancels' :
            !draft.geometry ? 'Draw a boundary or set a point to begin' :
            draft.geometry.type === 'Point' ? 'Drag the point to move it' :
            'Drag a corner to move · drag an edge to add · Alt-click to remove';
    }
    // dividers only between groups that still show buttons; a toolbar with none left disappears
    function tidyTools() {
        for (const bar of host.querySelectorAll('.pe-toolbar--tools, .pe-toolbar--view')) {
            let seen = false, lastSep = null, any = false;
            for (const el of bar.children) {
                if (el.classList.contains('pe-toolbar__sep')) {
                    el.hidden = !seen;
                    if (seen) lastSep = el;
                    seen = false;
                    continue;
                }
                if (!el.hidden) seen = any = true;
            }
            if (!seen && lastSep)
                lastSep.hidden = true;
            bar.hidden = !any;
        }
        q('[data-do="draw"]').setAttribute('aria-pressed', String(!!draw && drawType !== 'Point'));
        q('[data-do="point"]').setAttribute('aria-pressed', String(!!draw && drawType === 'Point'));
        q('[data-do="delete-part"]').setAttribute('aria-pressed', String(deleteArmed));
    }
    function setTool(name, label, active, icon) {
        const b = q(`[data-do="${name}"]`);
        b.dataset.label = label;
        b.setAttribute('aria-label', label);
        b.setAttribute('aria-pressed', String(!!active));
        if (icon)
            b.querySelector('svg').outerHTML = toolSvg(icon);
    }
    function stopDraw() {
        if (draw) {
            map.removeInteraction(draw);
            draw = null;
        }
        drawType = null;
        setTool('draw', 'Draw a boundary', false);
        setTool('point', 'Set a point', false);
        modify.setActive(editingMode && !!draft && !busy);
        if (draft) buttons();
    }
    function fields() {
        const p = draft?.properties || {};
        input('name').value = p.name || '';
        input('place_type').value = p.place_type || 'port';
        input('unlocode').value = (p.codes?.unlocode || []).join(', ');
        datalists(p.part_of === null ? 'none' : p.part_of || '');
        input('category').value = p.attributes?.area_subtype || '';
        input('size').value = String(p.size || 0);
        typeFields();
        buttons();
    }
    function typeFields() {
        const type = input('place_type').value;
        q('[data-row="unlocode"]').hidden = type !== 'port';
        q('[data-row="category"]').hidden = type !== 'area';
    }
    const typeLabel = t => [...input('place_type').options].find(o => o.value === t)?.textContent || t || 'Place';
    // one row: an icon tile, the name over its type, the code at the end
    function row(name, meta, codeText, type, selected) {
        const b = document.createElement('button');
        b.type = 'button';
        b.role = 'option';
        b.className = 'pe-place';
        b.setAttribute('aria-selected', String(selected));
        if (selected) b.setAttribute('aria-current', 'true');
        b.innerHTML = `<span class="pe-place__icon">${AREA_TYPES.has(type) ? ICON.area : ICON.anchor}</span>` +
            '<span class="pe-place__text"><span class="pe-place__name"></span><span class="pe-place__meta"></span></span>' +
            '<span class="pe-place__code"></span>';
        b.querySelector('.pe-place__name').textContent = name;
        b.querySelector('.pe-place__meta').textContent = meta + (selected && editingMode ? ' · editing' : '');
        b.querySelector('.pe-place__code').textContent = codeText || '';
        return b;
    }
    function note(text) {
        const p = document.createElement('p');
        p.className = 'pe-list__note';
        p.textContent = text;
        return p;
    }
    function list() {
        const list = q('.place-list');
        list.replaceChildren();
        const search = input('search').value.toLowerCase();
        if (draft && !original)
            list.appendChild(row(draft.properties.name || 'New place', typeLabel(draft.properties.place_type), code(draft.properties),
                                 draft.properties.place_type, true));
        const matches = records.filter(f => (f.properties.name + ' ' + code(f.properties)).toLowerCase().includes(search));
        for (const f of matches.sort((a, b) => a.properties.name.localeCompare(b.properties.name)).slice(0, 200)) {
            const parent = records.find(p => p.id === f.properties.part_of);
            const meta = typeLabel(f.properties.place_type) + (parent ? ' · ' + parent.properties.name : '');
            const b = row(f.properties.name, meta, code(f.properties), f.properties.place_type, f.id === draft?.id);
            b.onclick = () => select(f);
            list.appendChild(b);
        }
        if (matches.length > 200)
            list.appendChild(note(`Showing 200 of ${matches.length.toLocaleString()}. Search to narrow the list.`));
        if (!list.childNodes.length)
            list.appendChild(note(records.length ? 'No matching places.' : 'No saved places yet.'));
    }
    // the port and category suggestions follow the records, not the search box
    function datalists(selected = input('part_of').value) {
        const parents = records.filter(f => f.id !== draft?.id && !f.properties.redirect_to)
            .map(f => new Option(f.properties.name, f.id));
        input('part_of').replaceChildren(new Option('Unknown', ''), new Option('No parent', 'none'), ...parents);
        if (selected && ![...input('part_of').options].some(o => o.value === selected))
            input('part_of').add(new Option('Not in this catalogue · ' + selected.slice(0, 8), selected)); // a parent held elsewhere
        input('part_of').value = selected;
        const subtypes = [...new Set(records.map(f => f.properties.attributes?.area_subtype).filter(Boolean))];
        q('#place-categories').replaceChildren(...subtypes.map(v => new Option(v, v)));
    }
    function renderGeometry() {
        editing.clear();
        hint();
        if (draft?.geometry)
            editing.addFeature(read(draft));
    }
    // A dot for every entry in the catalogue, from the summary rows: a port point
    // has no geometry to fetch, so with thousands of them this stays one pass
    // over a list that is already in hand. Rebuilt only when the catalogue moves.
    let markedVersion = null;
    function refreshMarkers() {
        if (markedVersion === catalogueVersion && markers.getFeatures().length === records.length)
            return;
        markedVersion = catalogueVersion;
        markers.clear();
        for (const r of records) {
            // the summary's geometry is the marker point, for a polygon too
            const at = r.geometry?.coordinates;
            if (r.geometry?.type === 'Point' && Number.isFinite(at?.[0]) && Number.isFinite(at?.[1]))
                markers.addFeature(read(r));
        }
    }

    // the other places that have a shape, minus the one being edited; each shape
    // is fetched once per revision and kept
    async function refreshContext() {
        refreshMarkers();
        const wanted = records.filter(r => r.has_geometry && Number.isInteger(r.runtime_id) && r.id !== draft?.id);
        const version = catalogueVersion;
        const features = await Promise.all(wanted.map(async r => {
            const key = r.id + ':' + r.properties.revision;
            if (!shapes.has(key)) {
                try {
                    const full = await request('/api/place.json?id=' + r.runtime_id + '&version=' + encodeURIComponent(version));
                    if (full?.geometry) shapes.set(key, full);
                } catch { /* a place that vanished meanwhile is simply not drawn */ }
            }
            return shapes.get(key);
        }));
        if (dead || version !== catalogueVersion)
            return;
        context.clear();
        for (const f of features)
            if (f) context.addFeature(read(f));
    }
    function discard() {
        return !unsaved || confirm('Discard unsaved place changes?');
    }
    async function select(f, force = false) {
        if (busy || (!force && !discard()))
            return;
        if (f && !placeSettings?.enabled && !await ensureConfigured())
            return;
        // leave edit mode before the fetch: a place that changed meanwhile must not
        // leave the panel editing a draft that is no longer current
        stopDraw();
        editingMode = false;
        if (f) {
            try {
                const full = await request('/api/place.json?id=' + f.runtime_id + '&version=' + encodeURIComponent(catalogueVersion));
                if (!full.geometry) throw Error('Place changed or removed elsewhere. Reload the list.');
                f = {...full, runtime_id:f.runtime_id};
            } catch (e) { buttons(); status(e.message, true); return; }
        }
        original = f ? clone(f) : null;
        draft = f ? clone(f) : null;
        undo = [];
        redo = [];
        renderGeometry();
        refreshContext();
        list();
        fields();
        dirty(false);
        status('');
        if (draft?.geometry)
            map.getView().fit(editing.getExtent(), {padding: [60, 60, 60, 60], maxZoom: 16});
    }
    function checkpoint(before) {
        undo.push(before);
        if (undo.length > 50)
            undo.shift();
        redo = [];
        dirty(true);
    }
    let beforeModify = null;
    modify.on('modifystart', () => {
        beforeModify = clone(draft);
    });
    modify.on('modifyend', () => {
        if (!draft)
            return;
        draft.geometry = geometry(editing.getFeatures()[0]);
        checkpoint(beforeModify);
        status('Boundary changed. Save to apply.');
    });
    const doomed = new VectorSource();
    let hoverPart = null;
    map.addLayer(new VectorLayer({
        className: 'place-doomed',
        source: doomed,
        style: () => S.doomed
    }));
    function armDelete(on) {
        if (deleteArmed === !!on)
            return;
        deleteArmed = !!on;
        setTool('delete-part', deleteArmed ? 'Cancel delete' : 'Delete a part', deleteArmed);
        q('.place-map-wrap').classList.toggle('is-deleting', deleteArmed);
        if (!deleteArmed) {
            doomed.clear();
            hoverPart = null;
        }
        modify.setActive(!deleteArmed && !busy && editingMode && !!draft && !draw);
        hint();
    }
    function parts() {
        const g = draft?.geometry;
        return g?.type === 'MultiPolygon' ? g.coordinates : g?.type === 'Polygon' ? [g.coordinates] : [];
    }
    // deleting the last part leaves the place without a shape, the state a new place starts in
    function removePart(index, ring = 0) {
        const before = clone(draft), list = parts();
        if (!list[index])
            return;
        if (ring)
            list[index].splice(ring, 1);
        else
            list.splice(index, 1);
        draft.geometry = !list.length ? null :
            list.length === 1 ? {type: 'Polygon', coordinates: list[0]} : {type: 'MultiPolygon', coordinates: list};
        renderGeometry();
        checkpoint(before);
        buttons();
        status(ring ? 'Hole removed. Save to apply.' :
               draft.geometry ? 'Part deleted. Save to apply.' :
                                'Shape cleared. Draw a new boundary, or press Cancel.');
    }
    function partAt(coordinate, pixel) {
        const g = draft?.geometry;
        if (g?.type === 'Point') {
            const p = map.getPixelFromCoordinate(fromLonLat(g.coordinates));
            return p && Math.hypot(p[0] - pixel[0], p[1] - pixel[1]) <= 16 ? 0 : -1;
        }
        const list = parts();
        for (let i = 0; i < list.length; i++)
            if (format.readGeometry({type: 'Polygon', coordinates: list[i]}, {featureProjection: 'EPSG:3857'})
                    .intersectsCoordinate(coordinate))
                return i;
        return -1;
    }
    // OpenLayers keeps a ring at three corners, so Alt-clicking one of those takes the ring itself
    function minimalRingAt(pixel) {
        const list = parts();
        for (let i = 0; i < list.length; i++)
            for (let r = 0; r < list[i].length; r++)
                if (list[i][r].length === 4)
                    for (const corner of list[i][r]) {
                        const p = map.getPixelFromCoordinate(fromLonLat(corner));
                        if (p && Math.hypot(p[0] - pixel[0], p[1] - pixel[1]) <= 12)
                            return [i, r];
                    }
        return null;
    }
    map.on('singleclick', e => {
        if (!draft || !editingMode) {
            // not editing: the map is a way to pick a place, the way its row is
            if (busy)
                return;
            const hit = map.forEachFeatureAtPixel(e.pixel, f => f.getId(),
                                                  {hitTolerance: 6, layerFilter: l => l.getClassName() === 'place-markers'});
            const found = hit && records.find(r => r.id === hit);
            if (found)
                select(found).then(() => { if (draft?.id === found.id) actions.edit(); });
            return;
        }
        if (busy || draw)
            return;
        if (deleteArmed) {
            const index = partAt(e.coordinate, e.pixel);
            armDelete(false);
            if (index < 0) {
                status('Nothing deleted: click inside a part.');
                return;
            }
            if (draft.geometry.type === 'Point') {
                const before = clone(draft);
                draft.geometry = null;
                renderGeometry();
                checkpoint(before);
                buttons();
                status('Point cleared. Set a new point, or press Cancel.');
                return;
            }
            removePart(index);
            return;
        }
        if (e.originalEvent.altKey) {
            const hit = minimalRingAt(e.pixel);
            if (hit)
                removePart(hit[0], hit[1]);
        }
    });
    map.on('pointermove', e => {
        if (!deleteArmed || e.dragging)
            return;
        const index = draft?.geometry?.type === 'Point' ? -1 : partAt(e.coordinate, e.pixel);
        if (index === hoverPart)
            return;
        hoverPart = index;
        doomed.clear();
        if (index >= 0)
            doomed.addFeature(read({type: 'Feature', properties: {}, geometry: {type: 'Polygon', coordinates: parts()[index]}}));
    });
    for (const key of ['name', 'place_type', 'unlocode', 'part_of', 'category', 'size'])
        input(key).addEventListener(['place_type', 'part_of', 'size'].includes(key) ? 'change' : 'input', () => {
            if (!draft)
                return;
            const before = clone(draft), p = draft.properties, value = input(key).value;
            if (key === 'unlocode') {
                p.codes ||= {};
                p.codes.unlocode = value.toUpperCase().split(/[,;\s]+/).filter(Boolean);
            } else if (key === 'part_of') {
                if (value) p.part_of = value === 'none' ? null : value;
                else delete p.part_of;
            } else if (key === 'category') {
                p.attributes ||= {};
                p.attributes.area_subtype = value;
            } else
                p[key] = key === 'size' ? Number(value) : value;
            if (key === 'place_type' && value !== 'port' && p.codes)
                delete p.codes.unlocode;
            checkpoint(before);
            if (key === 'place_type')
                fields();
        });
    input('search').addEventListener('input', list);
    async function request(path, body) {
        const r = await fetch(path, {
            cache: 'no-store',
            ...(body ? {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify(body)
            } :
                       {})
        });
        if (r.status === 401)
            window.hubAuthRequired();
        const data = await r.json();
        if (!r.ok)
            throw Error(data.error || 'Place request failed');
        return data;
    }
    async function load() {
        if (!discard())
            return;
        busy = true;
        buttons();
        status('Loading places…');
        try {
            const [data, settings] =
                await Promise.all([request('/api/places'), request('/api/places/settings')]);
            if (dead)
                return;
            placeSettings = settings;
            records = (data.objects || []).map(placeFeature);
            catalogueVersion = data.place_version || '';
            original = null;
            draft = null;
            editingMode = false;
            stopDraw();
            editing.clear();
            datalists();
            list();
            refreshContext();
            fields();
            dirty(false);
            fit();
            status('');
        } catch (e) {
            if (!dead)
                status(e.message, true);
        } finally {
            busy = false;
            if (!dead)
                buttons();
        }
    }
    async function ensureConfigured() {
        if (placeSettings?.enabled)
            return true;
        const dialog = document.createElement('dialog');
        dialog.className = 'place-setup';
        dialog.innerHTML =
            '<h3>Enable places for this viewer?</h3><p>Place definitions will be saved in:</p><code></code><p>Save this directory in the viewer configuration and continue to the editor. The map will load saved places automatically.</p><div class="place-top"><button class="btn" data-cancel>Cancel</button><button class="btn btn-primary" data-enable>Save and continue</button></div>';
        dialog.querySelector('code').textContent =
            placeSettings?.directory || 'the installation places directory';
        host.appendChild(dialog);
        busy = true;
        buttons();
        return new Promise(resolve => {
            let finished = false, saving = false;
            const finish = (ok, cancelled = false) => {
                if (finished)
                    return;
                finished = true;
                dialog.close();
                dialog.remove();
                busy = false;
                buttons();
                resolve(ok);
                if (cancelled)
                    options.onCancelSetup?.();
            };
            dialog.addEventListener('cancel', e => {
                e.preventDefault();
                e.stopPropagation();
                if (!saving)
                    finish(false, true);
            });
            dialog.querySelector('[data-cancel]').onclick = () => finish(false, true);
            dialog.querySelector('[data-enable]').onclick = async () => {
                saving = true;
                for (const b of dialog.querySelectorAll('button'))
                    b.disabled = true;
                try {
                    placeSettings = await request('/api/places/enable', {});
                    ConfigStore?.invalidate();
                    options.onEnabled?.();
                    finish(true);
                } catch (e) {
                    finish(false);
                    status(e.message, true);
                }
            };
            dialog.showModal();
        });
    }
    async function save(remove = false) {
        if (!draft || busy)
            return;
        if (!draft.geometry) {
            status('Draw a boundary before saving.', true);
            return;
        }
        if (remove && !confirm(`Delete “${original.properties.name}”?`))
            return;
        for (const key of ['name', 'unlocode', 'category'])
            if (typeof draft.properties[key] === 'string')
                draft.properties[key] = draft.properties[key].trim();
        stopDraw();
        busy = true;
        buttons();
        status(remove ? 'Deleting place…' : 'Saving place…');
        try {
            const data =
                await request('/api/places/' + (remove ? 'delete' : 'save'), remove ? original : draft);
            if (dead)
                return;
            records = (data.objects || []).map(placeFeature);
            catalogueVersion = data.place_version || '';
            datalists();
            dirty(false);
            busy = false;
            // the save is done whatever the re-fetch below says: a saved draft
            // is the record now, so the panel leaves edit mode either way
            editingMode = false;
            const saved = remove ? null : records.find(f => f.id === draft.id);
            if (!remove && !saved) { await load(); return; }
            await select(saved, true);
            if (!q('.place-feedback').dataset.error || q('.place-feedback').dataset.error === 'false')
                status(remove ? 'Place deleted.' : 'Saved. Viewer places update automatically.');
        } catch (e) {
            if (!dead)
                status(e.message, true);
        } finally {
            busy = false;
            if (!dead)
                buttons();
        }
    }
    function fit() {
        if (editing.getFeatures().length)
            map.getView().fit(editing.getExtent(), {padding: [60, 60, 60, 60], maxZoom: 15});
    }
    function cancel() {
        editingMode = false;
        stopDraw();
        draft = original ? clone(original) : null;
        undo = [];
        redo = [];
        renderGeometry();
        refreshContext();
        list();
        fields();
        dirty(false);
        editing.changed();
        status('');
    }
    function definitionText() {
        const feature = clone(draft);
        delete feature.runtime_id;
        delete feature.has_geometry;
        delete feature.properties.source;
        return JSON.stringify(feature, null, 2) + '\n';
    }
    function download() {
        const blob = new Blob([definitionText()], {type: 'application/geo+json'}),
              url = URL.createObjectURL(blob), a = document.createElement('a');
        a.href = url;
        a.download = draft.id + '.geojson';
        a.click();
        setTimeout(() => URL.revokeObjectURL(url), 1000);
    }
    function viewJSON() {
        if (!draft)
            return;
        const blob = new Blob([definitionText()], {type: 'text/plain;charset=utf-8'});
        const url = URL.createObjectURL(blob);
        window.open(url, '_blank', 'noopener,noreferrer');
        setTimeout(() => URL.revokeObjectURL(url), 60000);
    }
    const actions = {
        edit() {
            if (!draft || busy) return;
            editingMode = true;
            list();
            buttons();
            editing.changed();
            status('');
        },
        async new () {
            if (!discard() || !await ensureConfigured())
                return;
            await select(null, true);
            editingMode = true;
            draft = {
                type: 'Feature',
                id: uuid(),
                properties: {schema_version: 2, revision: 0, name: '', place_type: 'port'},
                geometry: null
            };
            list();
            fields();
            dirty(true);
            actions.draw();
        },
        point() { const was = draw && drawType === 'Point'; stopDraw(); if (!was) actions.draw('Point'); },
        draw(shape = 'Polygon') {
            if (!draft || !editingMode)
                return;
            armDelete(false);
            if (draw) {
                stopDraw();
                return;
            }
            draw = new Draw({type: shape, stopClick: true, style: [S.shape, ...S.handle]});
            drawType = shape;
            map.addInteraction(draw);
            modify.setActive(false);
            setTool(shape === 'Point' ? 'point' : 'draw', 'Cancel drawing', true);
            buttons();
            status('');
            draw.on('drawend', e => {
                const before = clone(draft);
                const next = geometry(e.feature);
                if (draft.geometry && draft.geometry.type !== "Point" && next.type !== "Point") {
                    const parts = draft.geometry.type === 'MultiPolygon' ? draft.geometry.coordinates :
                                                                           [draft.geometry.coordinates];
                    draft.geometry = {type: 'MultiPolygon', coordinates: [...parts, next.coordinates]};
                } else
                    draft.geometry = next;
                renderGeometry();
                checkpoint(before);
                setTimeout(stopDraw, 0);
                if (!draft.properties.name) {
                    status('Boundary ready. Name the place and save.');
                    input('name').focus();
                } else
                    status('Boundary ready. Save to apply.');
            });
        },
        undo() {
            if (!undo.length)
                return;
            stopDraw();
            redo.push(clone(draft));
            draft = undo.pop();
            renderGeometry();
            fields();
            dirty(true);
            status('Undone. Save to apply.');
        },
        redo() {
            if (!redo.length)
                return;
            stopDraw();
            undo.push(clone(draft));
            draft = redo.pop();
            renderGeometry();
            fields();
            dirty(true);
            status('Redone. Save to apply.');
        },
        save: () => save(),
        delete: () => save(true),
        cancel,
        export: download,
        'view-json': viewJSON,
        reload: load,
        fit,
        async import() {
            if (await ensureConfigured()) {
                fileMode = 'place';
                q('[data-file]').click();
            }
        },
        'delete-part': () => {
            if (!draft?.geometry || !editingMode)
                return;
            stopDraw();
            armDelete(!deleteArmed);
        },
        'import-shape': () => { fileMode = 'shape'; q('[data-file]').click(); },
        'paste-shape': () => {
            const panel = q('.place-paste');
            panel.hidden = !panel.hidden;
            if (!panel.hidden) q('[data-paste]').focus();
        },
        'paste-apply': () => {
            if (!draft || !editingMode)
                return;
            try {
                applyShape(shapeFromGeoJSON(q('[data-paste]').value), 'Shape pasted. Save to apply.');
                q('[data-paste]').value = '';
                q('.place-paste').hidden = true;
            } catch (err) {
                status(err.message, true);
            }
        },
        'paste-close': () => { q('.place-paste').hidden = true; },
        fullscreen: () => {
            const wrap = q('.place-map-wrap');
            if (document.fullscreenElement === wrap) document.exitFullscreen();
            else wrap.requestFullscreen?.();
        },
        'export-shape': () => {
            const blob = new Blob([JSON.stringify({type: 'Feature', properties: {}, geometry: draft.geometry}) + '\n'], {type: 'application/geo+json'}),
                  url = URL.createObjectURL(blob), a = document.createElement('a');
            a.href = url;
            a.download = draft.id + '-shape.geojson';
            a.click();
            setTimeout(() => URL.revokeObjectURL(url), 1000);
        },
        'zoom-in': () => map.getView().setZoom(map.getView().getZoom() + 1),
        'zoom-out': () => map.getView().setZoom(map.getView().getZoom() - 1)
    };
    for (const b of host.querySelectorAll('[data-do]'))
        b.addEventListener('click', () => {
            q('.place-menu').open = false;
            actions[b.dataset.do]();
        });
    // a shape from a file or the clipboard replaces the draft's geometry and nothing else
    function applyShape(next, message) {
        const before = clone(draft);
        draft.geometry = next;
        renderGeometry();
        checkpoint(before);
        fit();
        status(message);
    }
    let fileMode = 'place';
    const onFullscreen = () => {
        const full = document.fullscreenElement === q('.place-map-wrap');
        setTool('fullscreen', full ? 'Exit full screen' : 'Full screen', full, full ? 'fullscreen_exit' : 'fullscreen');
        map.updateSize();
    };
    document.addEventListener('fullscreenchange', onFullscreen);
    q('[data-file]').addEventListener('change', async e => {
        const file = e.target.files[0];
        e.target.value = '';
        if (!file)
            return;
        if (fileMode === 'shape') {
            if (!draft || !editingMode)
                return;
            try {
                if (file.size > 131072)
                    throw Error('Shape file exceeds 128 KiB');
                const text = await file.text();
                if (dead)
                    return;
                applyShape(shapeFromGeoJSON(text), 'Shape imported. Save to apply.');
            } catch (err) {
                status(err.message, true);
            }
            return;
        }
        if (!discard())
            return;
        try {
            if (file.size > 131072)
                throw Error('Place file exceeds 128 KiB');
            const f = JSON.parse(await file.text());
            if (dead)
                return;
            if (f.type !== 'Feature' || !['Point', 'Polygon', 'MultiPolygon'].includes(f.geometry?.type))
                throw Error('Import one place Feature.');
            if (![1, 2].includes(f.properties?.schema_version))
                throw Error('Import a place file of schema 1 or 2.');
            read(f);
            select(null, true);
            editingMode = true;
            draft = f;
            draft.id = typeof f.id === 'string' &&
                    /^[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}$/.test(f.id) ?
                f.id :
                uuid();
            // an import over an existing place edits that place: its full record
            // is the original, not the summary row (whose geometry is the centroid)
            const existing = records.find(r => r.id === draft.id);
            original = null;
            if (existing) {
                const full = await request('/api/place.json?id=' + existing.runtime_id + '&version=' + encodeURIComponent(catalogueVersion));
                if (dead)
                    return;
                if (!full.geometry) throw Error('Place changed or removed elsewhere. Reload the list.');
                original = {...full, runtime_id: existing.runtime_id};
            }
            draft.properties = {
                schema_version: 2,
                name: 'Imported place',
                place_type: 'area',
                attributes: {area_subtype:'imported'},
                ...draft.properties,
                revision: original?.properties.revision || 0
            };
            renderGeometry();
            list();
            fields();
            dirty(true);
            fit();
            status('Imported for review. Save to apply.');
        } catch (e) {
            status(e.message, true);
        }
    });
    const closeMenu = e => {
        const menu = q('.place-menu');
        if (menu.open && !menu.contains(e.target))
            menu.open = false;
    };
    document.addEventListener('pointerdown', closeMenu);
    const key = e => {
        if (e.key !== 'Escape')
            return;
        if (q('.place-menu').open) {
            e.stopPropagation();
            q('.place-menu').open = false;
        } else if (draw) {
            e.stopPropagation();
            stopDraw();
        } else if (deleteArmed) {
            e.stopPropagation();
            armDelete(false);
            status('');
        }
    };
    host.addEventListener('keydown', key);
    load();
    return {
        resize: () => map.updateSize(),
        cancel,
        save: () => save(),
        unsaved: () => unsaved,
        destroy() {
            dead = true;
            document.removeEventListener('pointerdown', closeMenu);
            document.removeEventListener('fullscreenchange', onFullscreen);
            resize.disconnect();
            themeWatch.disconnect();
            stopDraw();
            map.setTarget(null);
            map.dispose();
        }
    };
}
