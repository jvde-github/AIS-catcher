// Application messages on the viewer's map: the markers the server lumps
// (meteo stations, lock schedules, areas) and the badges on vessels that carry
// messages. What a message is and how it renders lives in the shared package;
// this module owns the layer, the settings and the fetching.

import OlFeature from 'ol/Feature';
import VectorLayer from 'ol/layer/Vector';
import VectorSource from 'ol/source/Vector';
import Point from 'ol/geom/Point';
import Polygon from 'ol/geom/Polygon';
import Style from 'ol/style/Style';
import Stroke from 'ol/style/Stroke';
import Fill from 'ol/style/Fill';
import CircleStyle from 'ol/style/Circle';
import Icon from 'ol/style/Icon';
import { fromLonLat } from 'ol/proj';

import { settings } from '../core/state.js';
import { hexToRgb } from '../../shared/color.js';
import { hasValidCoords } from '../../shared/core/geo.js';
import { sanitizeString } from '../../shared/core/text.js';
import * as render from '../../shared/binary.js';
import { KIND_CAT, AGE_FADE, LAYER_ALPHA, decodeBadge, BINARY_CATEGORIES, CAT_COLORS, decorate, tooltipSections, getBinaryMessageList, cardOpen,
    badgeCanvas, discCanvas, pillCanvas, hatchCanvas, areaRings, ageBucket, isDangerArea, markerHead, markerCaption } from '../../shared/binary.js';

export { BINARY_CATEGORIES };

// { getActiveReceiver, getShipsDB, showDialog, getDialogModal, saveSettings, redrawMap,
//   isHovered, rehover, isHoveringShip, rehoverShip }
let deps = null;

const HOVER_DWELL_MS = 500;

// markers the server lumped: id -> row, with a change-sequence cursor
let objectsDB = new Map();
let objectsSince = 0;
let serverTime = 0;
const hydrated = new Map();       // marker id -> { seq, messages }
const hydrating = new Map();      // marker id -> in-flight promise
const hydratedShips = new Map();  // mmsi -> { badge, messages }

const catOf = (o) => KIND_CAT[o.kind] || 'data';

export function init(d) {
    deps = d;
    render.init({ color: (cat) => badgeColor(cat), shipLabel, shipLink });
}

export function resetSince() { objectsSince = 0; }

// ─── names ───────────────────────────────────────────────────────────────────

const shipLabel = (mmsi) => deps.getShipsDB()[mmsi]?.raw?.shipname || `MMSI ${mmsi}`;

// a name the viewer can jump to; plain text when the ship is not in the store
const shipLink = (mmsi) => {
    const label = sanitizeString(String(shipLabel(mmsi)));
    if (!(mmsi in deps.getShipsDB())) return label;
    return `<a href="javascript:void(0)" style="color: inherit; text-decoration: underline;" onclick="closeDialog(); openFocus(${Number(mmsi)})">${label}</a>`;
};

// ─── styles ──────────────────────────────────────────────────────────────────

// one colour for all kinds unless the setting asks for the kind's own
const kindRgb = (cat) => hexToRgb((settings.binary_color_class && CAT_COLORS[cat]) || CAT_COLORS.data);
const badgeColor = (cat, alpha = 0.9) => { const [r, g, b] = kindRgb(cat); return `rgba(${r}, ${g}, ${b}, ${alpha})`; };
const tokenRgb = (prop, fallback) => hexToRgb(getComputedStyle(document.documentElement).getPropertyValue(prop).trim() || fallback);

const styleCache = new Map();
const areaCache = new Map();

const highlightRing = new Style({
    image: new CircleStyle({ radius: 12, fill: new Fill({ color: 'rgba(255, 255, 255, 0)' }), stroke: new Stroke({ color: 'rgba(255, 255, 255, 0.5)', width: 2 }) }),
    zIndex: 200
});

function areaStyle(danger) {
    let cached = areaCache.get(danger);
    if (cached) return cached;
    const rgb = danger ? tokenRgb('--overlay-danger', '#e11d48') : tokenRgb('--color-accent', '#0857b1');
    const ctx = document.createElement('canvas').getContext('2d');
    cached = [new Style({
        fill: new Fill({ color: ctx.createPattern(hatchCanvas(rgb), 'repeat') }),
        stroke: new Stroke({ color: `rgba(${rgb[0]}, ${rgb[1]}, ${rgb[2]}, 0.8)`, width: 1.5 }),
        zIndex: 1
    })];
    areaCache.set(danger, cached);
    return cached;
}

const iconOf = ({ canvas, size, width, height }) => new Icon({ img: canvas, width: width || size, height: height || size });

function styleOf(feature) {
    if (feature.is_area) return areaStyle(!!feature.is_danger);
    const badge = !!feature.is_associated;
    const highlight = settings.binary_messages === 'highlight';
    const cat = settings.binary_color_class ? feature.binary_cat : 'data';
    const label = (!badge && settings.binary_id_labels && feature.binary_label) || '';
    const age = feature.binary_age || 0, fade = AGE_FADE[age];
    const key = `${cat}:${badge ? 'b' : 'm'}:${highlight}:${label}:${age}`;
    let cached = styleCache.get(key);
    if (cached) return cached;
    if (badge) {
        const mark = new Style({ image: iconOf(badgeCanvas(cat, kindRgb(cat), fade)), zIndex: 201 });
        cached = highlight ? [highlightRing, mark] : [mark];
    } else if (label) {
        const icon = iconOf(pillCanvas(label, badgeColor(cat)));
        icon.setOpacity(fade);
        cached = [new Style({ image: icon, zIndex: 100 })];
    } else {
        cached = [new Style({ image: iconOf(discCanvas(cat, kindRgb(cat), fade)), zIndex: 100 })];
    }
    styleCache.set(key, cached);
    return cached;
}

const vector = new VectorSource({ features: [] });
export const binaryLayer = new VectorLayer({ source: vector, style: styleOf, opacity: LAYER_ALPHA });

// ─── settings ────────────────────────────────────────────────────────────────

function shown(msg) {
    const ex = settings.binary_exclude;
    if (msg.cat !== 'data') return !ex.includes(msg.cat);
    return msg.message && msg.message.lat && msg.message.lon && !ex.includes('data');
}

export const binaryAnyShown = () => BINARY_CATEGORIES.some((c) => !settings.binary_exclude.includes(c));

function set(key, value) {
    settings[key] = value;
    styleCache.clear();
    deps.saveSettings();
    deps.redrawMap();
}
export const setBinaryDisplay = (v) => set('binary_messages', v);
export const setBinaryColorClass = (v) => set('binary_color_class', v);
export const setBinaryIdLabels = (v) => set('binary_id_labels', v);

export function setBinaryCategory(cat, on) {
    const excluded = settings.binary_exclude.filter((c) => c !== cat);
    if (!on) excluded.push(cat);
    settings.binary_exclude = excluded;
    deps.saveSettings();
    if (binaryLayer.isVisible() && binaryAnyShown()) fetchBinary().then(() => deps.redrawMap());
    else { objectsDB = new Map(); objectsSince = 0; deps.redrawMap(); }
}

// ─── fetching ────────────────────────────────────────────────────────────────

const api = (query) => fetch(`api/${query}&receiver=${deps.getActiveReceiver()}`).then((r) => r.json());
const items = (data) => (data.messages || []).filter((m) => m.message && m.message.mmsi).map(decorate);

// the markers that changed since the last poll; the client keeps the rest and ages them
export async function fetchBinary() {
    try {
        const data = await api(`mapobjects.json?since=${objectsSince}`);
        if (!objectsSince) { objectsDB = new Map(); hydrated.clear(); }
        for (const o of data.objects || []) objectsDB.set(o.id, o);
        for (const id of data.removed || []) { objectsDB.delete(id); hydrated.delete(id); }
        if (data.seq) objectsSince = data.seq;
        if (data.time) serverTime = data.time;
        for (const [id, o] of objectsDB) if (serverTime - o.t > (o.ttl || 1800)) { objectsDB.delete(id); hydrated.delete(id); }
        return true;
    } catch (error) {
        console.log("Failed loading map objects:", error);
        return false;
    }
}

// a marker's members, fetched once per (id, seq)
function hydrate(o) {
    const have = hydrated.get(o.id);
    if (have && have.seq === o.seq) return Promise.resolve(have.messages);
    if (hydrating.has(o.id)) return hydrating.get(o.id);
    const p = api(`binmsgs.json?marker=${o.id}`)
        .then((data) => { const messages = items(data); hydrated.set(o.id, { seq: o.seq, messages }); return messages; })
        .catch(() => [])
        .finally(() => hydrating.delete(o.id));
    hydrating.set(o.id, p);
    return p;
}

// a vessel's items, fetched once per badge value
function hydrateShip(ship) {
    const have = hydratedShips.get(ship.mmsi);
    if (have && have.badge === ship.binary) return Promise.resolve(have.messages);
    return api(`binmsgs.json?mmsi=${ship.mmsi}`)
        .then((data) => { const messages = items(data); hydratedShips.set(ship.mmsi, { badge: ship.binary, messages }); return messages; })
        .catch(() => []);
}

// ─── hover and dialog ────────────────────────────────────────────────────────

// the row draws the head at once; the members follow after a dwell
export function markerTooltip(feature) {
    const o = feature.binary_object;
    const head = markerHead(o, shipLabel, badgeColor(catOf(o)));
    const have = hydrated.get(o.id);
    if (have && have.seq === o.seq) return head + tooltipSections(have.messages);
    setTimeout(() => { if (deps.isHovered(feature)) hydrate(o).then(() => deps.rehover(feature)); }, HOVER_DWELL_MS);
    return head + markerCaption(o, serverTime);
}

// the vessel tooltip's section: the badge word draws the first line, the items follow after a dwell
export function shipTooltip(ship) {
    if (!ship.binary) return '';
    const b = decodeBadge(ship.binary);
    const cat = KIND_CAT[b.kind] || 'data';
    if (settings.binary_exclude.includes(cat)) return '';
    const line = (note) => cardOpen(cat) +
        `<div style="font-size: 11px; color: #FFA500; padding: 4px 0 3px;">${b.count} ${cat} message${b.count > 1 ? 's' : ''}${note}</div></div>`;
    const have = hydratedShips.get(ship.mmsi);
    if (have && have.badge === ship.binary) {
        const list = have.messages.filter(shown);
        if (!list.length) return have.messages.length ? '' : line(' <span class="tooltip-dim">(no longer retained)</span>');
        return tooltipSections(list, true, { mmsi: Number(ship.mmsi) }) || line('');
    }
    setTimeout(() => { if (deps.isHoveringShip(ship.mmsi)) hydrateShip(ship).then(() => deps.rehoverShip(ship.mmsi)); }, HOVER_DWELL_MS);
    return line('');
}

// a marker feature opens its members; a badge feature or an MMSI opens the vessel's
export function showBinaryMessageDialog(featureOrMmsi) {
    const show = (html) => {
        deps.showDialog("Binary", html || "No message content available");
        deps.getDialogModal().card.style.maxWidth = "500px";
    };
    if (typeof featureOrMmsi === 'object' && featureOrMmsi.binary_object) {
        hydrate(featureOrMmsi.binary_object).then((messages) => show(messages.length && getBinaryMessageList(messages)));
        return;
    }
    const mmsi = Number(typeof featureOrMmsi === 'object' ? featureOrMmsi.binary_mmsi : featureOrMmsi);
    const ship = deps.getShipsDB()[mmsi]?.raw;
    if (!ship || !ship.binary) return show('');
    hydrateShip(ship).then((messages) => {
        const list = messages.filter(shown);
        show(list.length && getBinaryMessageList(list, { mmsi }));
    });
}

// ─── drawing ─────────────────────────────────────────────────────────────────

function add(feature, id, props) {
    Object.assign(feature, props);
    feature.setId(id);
    vector.addFeature(feature);
}

export function redrawBinaryMessages() {
    vector.clear();
    if (settings.binary_messages === 'off') return;

    for (const o of objectsDB.values()) {
        if (settings.binary_exclude.includes(catOf(o))) continue;
        // areas are drawn, not hovered: the pointer answers to the marker at the centre
        if (o.shapes) areaRings(o.shapes).forEach((ring, i) =>
            add(new OlFeature({ geometry: new Polygon([ring.map((ll) => fromLonLat(ll))]) }), `binary-area-${o.id}-${i}`,
                { is_area: true, is_danger: isDangerArea(o), binary_object: o }));
        if (hasValidCoords(o.lat, o.lon))
            add(new OlFeature({ geometry: new Point(fromLonLat([o.lon, o.lat])) }), `binary-item-${o.id}`,
                { binary: true, is_associated: false, binary_object: o, binary_cat: catOf(o), binary_label: o.label || '', binary_age: ageBucket(o.t, serverTime) });
    }

    const shipsDB = deps.getShipsDB();
    for (const mmsi in shipsDB) {
        const ship = shipsDB[mmsi].raw;
        if (!ship.binary || !hasValidCoords(ship.lat, ship.lon)) continue;
        const b = decodeBadge(ship.binary);
        const cat = KIND_CAT[b.kind] || 'data';
        if (settings.binary_exclude.includes(cat)) continue;
        add(new OlFeature({ geometry: new Point(fromLonLat([ship.lon, ship.lat])) }), `binary-ship-${ship.mmsi}`,
            { binary: true, is_associated: true, ship, binary_mmsi: ship.mmsi, binary_cat: cat, binary_age: b.age });
    }
}
