// Map objects on the viewer: the shared module keeps, draws, hovers and opens
// them; this host owns the settings, the since-feed transport and the badges
// that ride the viewer's own vessels.

import { settings } from '../core/state.js';
import { hasValidCoords } from '../../shared/core/geo.js';
import { sanitizeString } from '../../shared/core/text.js';
import { BINARY_CATEGORIES } from '../../shared/binary.js';
import * as mapobjects from '../../shared/mapobjects.js';
import { stationBand } from '../../shared/stations.js';

export { BINARY_CATEGORIES };

// { getActiveReceiver, getShipsDB, getStation, getStationName, saveSettings, redrawMap, openVessel, map,
//   isHovered, rehover, isHoveringShip, rehoverShip }
let deps = null;
let objectsSince = 0;

const shipLabel = (mmsi) => deps.getShipsDB()[mmsi]?.raw?.shipname || `MMSI ${mmsi}`;

// a name the viewer can jump to; plain text when the ship is not in the store
const shipLink = (mmsi) => {
    const label = sanitizeString(String(shipLabel(mmsi)));
    if (!(mmsi in deps.getShipsDB())) return label;
    return `<a href="javascript:void(0)" style="color: inherit; text-decoration: underline;" onclick="closeDialog(); openFocus(${Number(mmsi)})">${label}</a>`;
};

const api = (query) => fetch(`api/${query}&receiver=${deps.getActiveReceiver()}`).then((r) => r.json());

// the module is built once, at load, so the layer exists before the map does; every
// hook reaches for the host through `deps`, which init() fills in
const objects = mapobjects.create({
    fetchJSON: (url) => api(url),
    objectUrl: (key) => `object.json?key=${key}`,
    shipMessagesUrl: (mmsi) => `binmsgs.json?mmsi=${mmsi}`,
    eventsUrl: (since) => `events.json?since=${since}`,
    ship: (mmsi) => { const raw = deps.getShipsDB()[mmsi]?.raw; return raw ? { mmsi: raw.mmsi, lat: raw.lat, lon: raw.lon, name: raw.shipname, binary: raw.binary } : null; },
    shipLabel, shipLink,
    options: () => ({
        display: settings.binary_messages,
        colorClass: settings.binary_color_class,
        idLabels: settings.binary_id_labels,
        groupAreas: settings.binary_group_areas,
        hidden: (cat) => settings.binary_exclude.includes(cat),
    }),
    isHovered: (f) => deps.isHovered(f), rehover: (f) => deps.rehover(f),
    isHoveringShip: (m) => deps.isHoveringShip(m), rehoverShip: (m) => deps.rehoverShip(m),
    openVessel: (m) => deps.openVessel(m),
    map: () => deps.map(),
});

export const binaryLayer = objects.layer;

export function init(d) {
    deps = d;
    const map = d.map && d.map();
    if (map) viewChanged(map.getView().getZoom() || 0);
}

// ─── settings ────────────────────────────────────────────────────────────────

export const binaryAnyShown = () => BINARY_CATEGORIES.some((c) => !settings.binary_exclude.includes(c));

function set(key, value) {
    settings[key] = value;
    objects.restyle();
    deps.saveSettings();
    deps.redrawMap();
}
export const setBinaryDisplay = (v) => set('binary_messages', v);
export const setBinaryColorClass = (v) => set('binary_color_class', v);
export const setBinaryIdLabels = (v) => set('binary_id_labels', v);
export const setBinaryGroupAreas = (v) => set('binary_group_areas', v);

export function setBinaryCategory(cat, on) {
    const excluded = settings.binary_exclude.filter((c) => c !== cat);
    if (!on) excluded.push(cat);
    settings.binary_exclude = excluded;
    deps.saveSettings();
    if (binaryLayer.isVisible() && binaryAnyShown()) fetchBinary().then(() => deps.redrawMap());
    else { objects.clear(); objectsSince = 0; deps.redrawMap(); }
}

export function resetSince() { objectsSince = 0; objects.resetEvents(); }

// ─── transport ───────────────────────────────────────────────────────────────

// the objects that changed since the last poll; the client keeps the rest and ages them
export async function fetchBinary() {
    try {
        const data = await api(`mapobjects.json?since=${objectsSince}`);
        objects.applyDelta(data, !objectsSince);
        if (data.seq) objectsSince = data.seq;
        return true;
    } catch (error) {
        console.log("Failed loading map objects:", error);
        return false;
    }
}

// ─── drawing ─────────────────────────────────────────────────────────────────

// the module stacks what overlaps at the view's zoom, so it has to know it
let viewZoom = null;
export function viewChanged(zoom) {
    const z = Math.round(zoom);
    if (z === viewZoom) return;
    viewZoom = z;
    objects.setViewZoom(z);
    redrawBinaryMessages();
}

export function redrawBinaryMessages() {
    objects.redraw();
    if (settings.binary_messages === 'off') return;
    const shipsDB = deps.getShipsDB();
    for (const mmsi in shipsDB) objects.shipBadge(shipsDB[mmsi].raw);
    const own = ownVessel();
    if (own) objects.stationBadge(own, { name: deps.getStationName(), mmsi: own.mmsi, status: 'online' });
}

// the vessel the station's own MMSI names, when it is on the map with a position
export function ownVessel() {
    const st = deps.getStation();
    if (settings.show_station === false || !st || !st.mmsi) return null;
    const ship = deps.getShipsDB()[st.mmsi]?.raw;
    return ship && hasValidCoords(ship.lat, ship.lon) ? ship : null;
}

// ─── hover, card and dialog ──────────────────────────────────────────────────

export const { tooltip: markerTooltip, shipTooltip, shipKinds, click, pollEvents, noteSeen: eventSeen } = objects;
export const stationTooltip = (feature, vesselHtml) => stationBand(feature.station_info) + (vesselHtml || '');

// a badge feature or an MMSI opens the vessel's received and sent tabs
export function showBinaryMessageDialog(featureOrMmsi) {
    if (featureOrMmsi && typeof featureOrMmsi === 'object') return objects.click(featureOrMmsi);
    const ship = deps.getShipsDB()[featureOrMmsi]?.raw;
    objects.showVesselMessages(Number(featureOrMmsi), ship ? ship.binary : 0);
}
