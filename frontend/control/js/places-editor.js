import {placeFeature} from '../../shared/places.js';
import GeoJSON from 'ol/format/GeoJSON.js';
import Draw from 'ol/interaction/Draw.js';
import Modify from 'ol/interaction/Modify.js';
import { shapeFromGeoJSON } from './shape.js';
import * as tooltip from '../../shared/tooltip.js';
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
const toolSvg = icon => `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 -960 960 960" width="24" height="24" aria-hidden="true"><path d="${TOOL_ICONS[icon]}"/></svg>`;
const tool = (name, label, icon) => `<button type="button" class="map-button" data-do="${name}" title="${label}">${toolSvg(icon)}</button>`;
let toolTips = null;
import TileLayer from 'ol/layer/Tile.js';
import VectorLayer from 'ol/layer/Vector.js';
import Map from 'ol/Map.js';
import {fromLonLat} from 'ol/proj.js';
import OSM from 'ol/source/OSM.js';
import XYZ from 'ol/source/XYZ.js';
import VectorSource from 'ol/source/Vector.js';
import Fill from 'ol/style/Fill.js';
import CircleStyle from 'ol/style/Circle.js';
import Stroke from 'ol/style/Stroke.js';
import Style from 'ol/style/Style.js';
import View from 'ol/View.js';


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
    host.innerHTML = `
      <div class="place-top place-actions" role="group" aria-label="Place actions">
        <div class="place-toolbar-main">
          <button class="btn" data-do="new">New place</button>
          <details class="place-menu"><summary class="btn" aria-label="More actions" title="More actions">…</summary>
            <div class="place-menu-items">
              <button class="btn" data-do="import">Import…</button>
              <button class="btn" data-do="export">Export</button>
              <button class="btn" data-do="view-json">View GeoJSON</button>
              <button class="btn" data-do="reload">Reload</button>
            </div>
          </details>
        </div>
      </div>
      <div class="place-workspace"><aside class="place-sidebar">
        <input class="input" data-field="search" aria-label="Search places" placeholder="Search places…">
        <div class="place-list" role="listbox" aria-label="Saved places"></div>
      </aside><section class="place-detail">
        <div class="place-inspector">
          <div class="place-summary"><div><strong data-place-name>Select a place</strong><small data-place-description>Choose a place from the list.</small></div>
            <div class="place-toolbar-end">
              <button class="btn" data-do="delete" hidden>Delete</button>
              <button class="btn" data-do="edit" hidden>Edit</button>
              <button class="btn" data-do="cancel" hidden>Cancel</button>
              <button class="btn btn-primary" data-do="save" hidden>Save</button>
            </div>
          </div>
        <fieldset class="place-fields" disabled hidden>
          <label>Type<select class="input select" data-field="place_type"><option value="port">Port</option><option value="berth">Berth</option><option value="anchorage">Anchorage</option><option value="custom">Custom</option></select></label>
          <label>Name<input class="input" data-field="name" maxlength="120" placeholder="Place name"></label>
          <label data-row="unlocode">UN/LOCODE<input class="input" data-field="unlocode" maxlength="5" placeholder="NLRTM"></label>
          <label data-row="parent_unlocode">Parent port (optional)<input class="input" data-field="parent_unlocode" list="place-ports" maxlength="5" placeholder="NLRTM"></label>
          <label data-row="port_unlocode">Port UN/LOCODE<input class="input" data-field="port_unlocode" list="place-ports" maxlength="5" placeholder="NLRTM"><datalist id="place-ports"></datalist></label>
          <label data-row="category">Category<input class="input" data-field="category" list="place-categories" maxlength="60" placeholder="VTS, restricted place…"><datalist id="place-categories"></datalist></label>
          <label data-row="size">Marker from zoom<select class="input select" data-field="size"><option value="0">12 · very small</option><option value="1">11 · small</option><option value="2">9 · medium</option><option value="3">7 · large</option></select></label>
        </fieldset>
        <div class="place-feedback" role="status" aria-live="polite"></div>
        </div>
      <div class="place-map-wrap"><div class="place-map"></div>
        <div class="place-map-tools" role="toolbar" aria-label="Map tools">${tool('draw', 'Draw polygon', 'pentagon')}${tool('point', 'Set point', 'add_location_alt')}${tool('delete-part', 'Delete part', 'delete')}<span class="sep"></span>${tool('undo', 'Undo', 'undo')}${tool('redo', 'Redo', 'redo')}<span class="sep"></span>${tool('fit', 'Fit place', 'fit_screen')}<span class="sep"></span>${tool('import-shape', 'Import shape', 'upload')}${tool('export-shape', 'Export shape', 'download')}${tool('paste-shape', 'Paste shape', 'content_paste')}<span class="sep"></span>${tool('fullscreen', 'Full screen', 'fullscreen')}<span class="sep"></span>${tool('zoom-in', 'Zoom in', 'add')}${tool('zoom-out', 'Zoom out', 'remove')}</div>
        <div class="place-paste" hidden><textarea data-paste rows="7" spellcheck="false" placeholder="Paste GeoJSON: a geometry, a Feature, or a collection holding one"></textarea>
          <div class="place-paste-actions"><button class="btn" data-do="paste-apply">Apply</button><button class="btn" data-do="paste-close">Close</button></div></div>
        <div class="place-attribution">© <a href="https://www.openstreetmap.org/copyright" target="_blank" rel="noopener">OpenStreetMap</a> contributors</div>
      </div></section></div><input type="file" data-file accept=".geojson,.json,application/geo+json,application/json" hidden>`;
    if (!toolTips)
        toolTips = tooltip.create({selector: '.place-map-tools .map-button', placement: 'below'});
    toolTips.prepare(host);
    const q = s => host.querySelector(s);
    const input = name => q(`[data-field="${name}"]`);
    let placeSettings = null, catalogueVersion = '';
    let records = [], draft = null, original = null, busy = false, dead = false, draw = null, undo = [],
        redo = [], unsaved = false, editingMode = false, deleteArmed = false;
    const editing = new VectorSource();
    // every other place nearby, in the accent blue, so a new outline is drawn against its neighbours
    const context = new VectorSource();
    const shapes = new globalThis.Map(); // uuid:revision -> the full feature, fetched once (Map here is OpenLayers')
    const blue = getComputedStyle(host).getPropertyValue('--color-accent').trim() || '#0b5cad';
    const orange = '#f28c00'; // the shape being edited
    const handleStyle = new Style({image: new CircleStyle({radius: 5,
        fill: new Fill({color: orange}), stroke: new Stroke({color: '#fff', width: 2})})});
    const activeStyle = new Style({image:new CircleStyle({radius:6,fill:new Fill({color:orange})}),stroke: new Stroke({color: orange, width: 2.5}),
        fill: new Fill({color: 'rgba(242, 140, 0, 0.22)'})});
    const contextStyle = new Style({image:new CircleStyle({radius:4,fill:new Fill({color:blue})}),stroke: new Stroke({color: blue, width: 1.5}),
        fill: new Fill({color: 'rgba(11, 92, 173, 0.12)'})});
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
            new VectorLayer({className: 'place-context', source: context, style: contextStyle}),
            new VectorLayer({
                source: editing,
                style: activeStyle
            })
        ],
        view: new View({center: fromLonLat([4.4, 51.95]), zoom: 10})
    });
    // OpenFreeMap Positron, labelled in English where OpenStreetMap has the name and in the Latin
    // spelling otherwise; the plain OSM tiles stand in if the style cannot be loaded
    const basemapStyle = 'https://tiles.openfreemap.org/styles/positron';
    Promise.all([import('ol/layer/VectorTile.js'), import('ol-mapbox-style'), fetch(basemapStyle).then(r => r.json())])
        .then(([{default: VectorTileLayer}, {applyStyle}, style]) => {
            const english = ['coalesce', ['get', 'name:en'], ['get', 'name_int'], ['get', 'name:latin'], ['get', 'name']];
            for (const l of style.layers || []) {
                if (!l.layout?.['text-field'])
                    continue;
                l.layout['text-font'] = ['Arial'];
                if (JSON.stringify(l.layout['text-field']).includes('name'))
                    l.layout['text-field'] = english;
            }
            const background = style.layers?.find(l => l.type === 'background')?.paint?.['background-color'];
            const base = new VectorTileLayer({className: 'place-base', declutter: true});
            return applyStyle(base, style, {styleUrl: basemapStyle}).then(() => {
                if (background)
                    base.on('prerender', e => {
                        e.context.save();
                        e.context.fillStyle = background;
                        e.context.fillRect(0, 0, e.context.canvas.width, e.context.canvas.height);
                        e.context.restore();
                    });
                if (dead)
                    return;
                map.getLayers().insertAt(0, base);
                q('.place-attribution').innerHTML = '<a href="https://openfreemap.org" target="_blank" rel="noopener">OpenFreeMap</a> © <a href="https://www.openmaptiles.org/" target="_blank" rel="noopener">OpenMapTiles</a> · © <a href="https://www.openstreetmap.org/copyright" target="_blank" rel="noopener">OpenStreetMap</a> contributors';
            });
        })
        .catch(err => {
            console.error('Places map: OpenFreeMap failed, using OpenStreetMap tiles', err);
            if (!dead)
                map.getLayers().insertAt(0, new TileLayer({source: new OSM(), opacity: 0.6}));
        });
    const modify = new Modify({source: editing, style: handleStyle});
    map.addInteraction(modify);
    const resize = new ResizeObserver(() => map.updateSize());
    resize.observe(q('.place-map-wrap'));
    function status(message, error = false) {
        q('.place-feedback').textContent = message;
        q('.place-feedback').dataset.error = error;
    }
    function dirty(value) {
        unsaved = value;
        buttons();
    }
    function buttons() {
        q('.place-fields').hidden = !editingMode;
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
        const props = draft?.properties;
        q('[data-place-name]').textContent = props?.name || (draft ? 'New place' : 'Select a place');
        const typeLabel = props && [...input('place_type').options].find(o => o.value === props.place_type)?.textContent;
        const portCode = props?.unlocode || props?.port_unlocode;
        const portName = props?.port_unlocode && records.find(f => f.properties.place_type === 'port' && f.properties.unlocode === portCode)?.properties.name;
        const location = portCode ? [portCode, portName].filter(Boolean).join(' ') : props?.category;
        q('[data-place-description]').textContent = props ? [typeLabel, location].filter(Boolean).join(' · ') : 'Choose a place from the list.';
        q('.place-feedback').classList.toggle('place-busy', busy);
        for (const b of host.querySelectorAll('button[data-do]'))
            b.disabled = busy;
        for (const action of ['save', 'cancel', 'delete', 'draw', 'export', 'view-json'])
            q(`[data-do="${action}"]`).disabled = busy || !draft || (action === 'delete' && !original);
        q('[data-do="fit"]').disabled = busy || !draft?.geometry;
        q('[data-do="export-shape"]').disabled = busy || !draft?.geometry;
        q('[data-do="delete-part"]').disabled = busy || !draft?.geometry;
        q('[data-do="undo"]').disabled = busy || !undo.length;
        q('[data-do="redo"]').disabled = busy || !redo.length;
        if (busy || !editingMode || !draft?.geometry)
            armDelete(false);
        modify.setActive(!busy && editingMode && !!draft && !draw && !deleteArmed);
    }
    // dividers only between groups that still show buttons; a hairline only between two visible buttons
    function tidyTools() {
        let seen = false, prev = null, lastSep = null;
        for (const el of q('.place-map-tools').children) {
            if (el.classList.contains('sep')) {
                el.hidden = !seen;
                if (seen) { lastSep = el; prev = 'sep'; }
                seen = false;
                continue;
            }
            if (el.hidden)
                continue;
            el.classList.toggle('no-line', prev !== 'btn');
            seen = true; prev = 'btn';
        }
        if (!seen && lastSep)
            lastSep.hidden = true;
    }
    function setTool(name, label, active, icon) {
        const b = q(`[data-do="${name}"]`);
        b.dataset.label = label;
        b.setAttribute('aria-label', label);
        b.classList.toggle('is-active', !!active);
        if (icon)
            b.innerHTML = toolSvg(icon);
    }
    function stopDraw() {
        if (draw) {
            map.removeInteraction(draw);
            draw = null;
        }
        setTool('draw', 'Draw polygon', false);
        modify.setActive(editingMode && !!draft && !busy);
    }
    function fields() {
        const p = draft?.properties || {};
        for (const key of ['name', 'place_type', 'unlocode', 'parent_unlocode', 'port_unlocode', 'category'])
            input(key).value = p[key] || (key === 'place_type' ? 'port' : '');
        input('size').value = String(Number.isInteger(p.size) ? p.size : 0);
        typeFields();
        buttons();
    }
    function typeFields() {
        const type = input('place_type').value;
        for (const key of ['unlocode', 'parent_unlocode', 'port_unlocode', 'category'])
            q(`[data-row="${key}"]`).hidden = ['unlocode', 'parent_unlocode'].includes(key) ? type !== 'port' :
                key === 'category'                               ? type !== 'custom' :
                                                                   !['berth', 'anchorage'].includes(type);
    }
    function list() {
        const list = q('.place-list');
        list.replaceChildren();
        const search = input('search').value.toLowerCase();
        if (draft && !original) {
            const b = document.createElement('button');
            b.type = 'button';
            b.role = 'option';
            b.className = 'place-draft';
            b.setAttribute('aria-selected', 'true');
            b.textContent = draft.properties.name || 'New place';
            const small = document.createElement('small');
            small.textContent = draft.properties.place_type;
            b.appendChild(small);
            list.appendChild(b);
        }
        for (const f of records
                 .filter(
                     f => (f.properties.name + ' ' + (f.properties.unlocode || ''))
                              .toLowerCase()
                              .includes(search))
                 .sort((a, b) => a.properties.name.localeCompare(b.properties.name)).slice(0, 200)) {
            const b = document.createElement('button');
            b.type = 'button';
            b.role = 'option';
            b.setAttribute('aria-selected', String(f.id === draft?.id));
            b.textContent = f.properties.name;
            const small = document.createElement('small');
            const parent = records.find(p => f.properties.port_unlocode && p.properties.unlocode === f.properties.port_unlocode);
            small.textContent = parent ?
                `${parent.properties.name} › ${f.properties.place_type}` :
                `${f.properties.place_type}${(f.properties.unlocode || f.properties.port_unlocode) ? ' · ' + (f.properties.unlocode || f.properties.port_unlocode) : ''}`;
            b.appendChild(small);
            b.onclick = () => select(f);
            list.appendChild(b);
        }
        if (records.length > 200) {
            const hint = document.createElement('p'); hint.textContent = 'Showing up to 200 matches. Search to narrow the list.'; list.appendChild(hint);
        }
        if (!list.childNodes.length) {
            const text = document.createElement('p');
            text.className = 'place-help';
            text.style.padding = '10px';
            text.textContent = records.length ? 'No matching places.' : 'No saved places yet.';
            list.appendChild(text);
        }
    }
    // the port and category suggestions follow the records, not the search box
    function datalists() {
        q('#place-ports').replaceChildren(...records.filter(f => f.properties.place_type === 'port')
            .map(f => new Option(f.properties.name, f.properties.unlocode)));
        q('#place-categories')
            .replaceChildren(...[...new Set(records.filter(f => f.properties.place_type === 'custom')
                                                .map(f => f.properties.category))]
                                 .map(c => new Option(c, c)));
    }
    function renderGeometry() {
        editing.clear();
        if (draft?.geometry)
            editing.addFeature(read(draft));
    }
    // the other places that have a shape, minus the one being edited; each shape
    // is fetched once per revision and kept
    async function refreshContext() {
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
        style: new Style({stroke: new Stroke({color: '#dc2626', width: 3}), fill: new Fill({color: 'rgba(220, 38, 38, 0.25)'})})
    }));
    function armDelete(on) {
        if (deleteArmed === !!on)
            return;
        deleteArmed = !!on;
        setTool('delete-part', deleteArmed ? 'Cancel delete' : 'Delete part', deleteArmed);
        q('.place-map-wrap').classList.toggle('is-deleting', deleteArmed);
        if (!deleteArmed) {
            doomed.clear();
            hoverPart = null;
        }
        modify.setActive(!deleteArmed && !busy && editingMode && !!draft && !draw);
        if (deleteArmed)
            status('Click a part to delete it. Escape cancels.');
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
        if (!draft || !editingMode || busy || draw)
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
    for (const key of ['name', 'place_type', 'unlocode', 'parent_unlocode', 'port_unlocode', 'category', 'size'])
        input(key).addEventListener(['place_type', 'size'].includes(key) ? 'change' : 'input', () => {
            if (!draft)
                return;
            const before = clone(draft), p = draft.properties;
            p[key] = key === 'size' ? Number(input(key).value) :
                ['unlocode', 'parent_unlocode', 'port_unlocode'].includes(key) ? input(key).value.toUpperCase() : input(key).value;
            if (key === 'parent_unlocode' && !p[key]) delete p[key];
            if (key === 'place_type') {
                for (const k of ['unlocode', 'parent_unlocode', 'port_unlocode', 'category'])
                    delete p[k];
            }
            checkpoint(before);
            if (key === 'place_type')
                fields();
            const row = q('.place-list .place-draft');
            if (row && (key === 'name' || key === 'place_type')) {
                row.firstChild.textContent = p.name || 'New place';
                row.querySelector('small').textContent = p.place_type;
            }
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
            status(`${records.length} saved places`);
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
                    window.ConfigStore?.invalidate();
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
            buttons();
            status('Drag an edge to add a corner; Alt-click a corner removes it; the trash tool deletes a whole part.');
        },
        async new () {
            if (!discard() || !await ensureConfigured())
                return;
            await select(null, true);
            editingMode = true;
            draft = {
                type: 'Feature',
                id: uuid(),
                properties: {schema_version: 1, revision: 0, name: '', place_type: 'port'},
                geometry: null
            };
            list();
            fields();
            dirty(true);
            actions.draw();
        },
        point() { stopDraw(); actions.draw('Point'); },
        draw(shape = 'Polygon') {
            if (!draft || !editingMode)
                return;
            armDelete(false);
            if (draw) {
                stopDraw();
                return;
            }
            draw = new Draw({type: shape, stopClick: true, style: [activeStyle, handleStyle]});
            map.addInteraction(draw);
            modify.setActive(false);
            setTool('draw', 'Cancel drawing', true);
            status('Click corners on the map; double-click or the first corner finishes. Escape cancels.');
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
                schema_version: 1,
                name: 'Imported place',
                place_type: 'custom',
                category: 'Imported',
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
            stopDraw();
            map.setTarget(null);
            map.dispose();
        }
    };
}

