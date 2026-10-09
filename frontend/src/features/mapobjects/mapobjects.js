// Map objects on the viewer: the shared module keeps, draws, hovers and opens
// them; this host owns the settings, the since-feed transport and the badges
// that ride the viewer's own vessels.

import { settings } from '../../core/state.js';
import { hasValidCoords } from '@aiscatcher/core/geo.js';
import { sanitizeString } from '@aiscatcher/core/text.js';
import { BINARY_CATEGORIES } from '@aiscatcher/ui/binary.js';
import * as mapobjects from '@aiscatcher/map/mapobjects.js';
import { createEventHistory } from '@aiscatcher/ui/events-history.js';
import { map, redraw } from '../../map.js';
import { receiver } from '../../data.js';
import { ships, station } from '../../core/store.js';
import { config } from '../../core/config.js';
import { saveSettings, closeSettings } from '../settings/settings.js';
import { closeDialog } from '../../dialog.js';
import { showTargetcard } from '../targetcard/card.js';
import { register } from '../../actions.js';
import { hoverVessel, unhoverVessel, isHovered, rehover, isHoveringShip, rehoverShip } from '../hover/hover.js';
import { needsMapObjects, objectKindVisible } from '@aiscatcher/core/object-visibility.js';
import { stationBand } from '@aiscatcher/ui/stations.js';

export { BINARY_CATEGORIES };

// { navigate, setTableOpen }
let deps = null;
let objectsSince = 0;

const shipLabel = (mmsi) => ships[mmsi]?.raw?.shipname || `MMSI ${mmsi}`;

// a name the viewer can jump to; plain text when the ship is not in the store
const shipLink = (mmsi) => {
    const label = sanitizeString(String(shipLabel(mmsi)));
    if (!(mmsi in ships)) return label;
    return `<a href="javascript:void(0)" style="color: inherit; text-decoration: underline;" onclick="closeDialog(); openFocus(${Number(mmsi)})">${label}</a>`;
};

const api = (query) => fetch(`api/${query}&receiver=${receiver}`).then((r) => r.json());

// the module is built once, at load, so the layer exists before the map does; every
// hook reaches for the host through `deps`, which init() fills in
const objects = mapobjects.create({
    fetchJSON: (url) => api(url),
    objectUrl: (key) => `object.json?key=${key}`,
    shipMessagesUrl: (mmsi) => `binmsgs.json?mmsi=${mmsi}`,
    eventsUrl: (since) => `events.json?since=${since}`,
    ship: (mmsi) => { const raw = ships[mmsi]?.raw; return raw ? { mmsi: raw.mmsi, lat: raw.lat, lon: raw.lon, name: raw.shipname, binary: raw.binary } : null; },
    shipLabel, shipLink,
    options: () => ({
        display: settings.binary_messages,
        colorClass: settings.binary_color_class,
        idLabels: settings.binary_id_labels,
        groupAreas: settings.binary_group_areas,
        places: objectKindVisible(settings, 'place'),
        hidden: cat => !objectKindVisible(settings, cat),
    }),
    isHovered, rehover, isHoveringShip, rehoverShip,
    openVessel,
    hoverVessel, unhoverVessel,
    setTableOpen: (on) => deps.setTableOpen(on),
    map: () => map,
});

export const objectLayer = objects.layer;
export const setReceiverMarker = objects.setReceiverMarker;
export const openPorts = objects.openPorts;
export const openNearby = objects.openNearby;

// the ticker's history: the same side table, a page of the rings at a time
export const openEventHistory = createEventHistory({
    fetchJSON: (url) => api(url),
    historyUrl: (before, level, limit) => `events.json?before=${before}&level=${level}&limit=${limit}`,
    openVessel,
    navigate: (pos) => deps.navigate(pos),
    setTableOpen: (on) => deps.setTableOpen(on),
    map: () => map,
});

export async function openPlace(ref) {
    // Current visits remain clickable when place markers are switched off.
    await fetchObjects();
    objects.openPlace(ref);
}

// a vessel named in a dialog or the side table opens on its card
function openVessel(mmsi) {
    closeDialog();
    closeSettings();
    showTargetcard('ship', mmsi);
}

export function init(d) {
    deps = d;
    register({
        setBinaryCategory: (e, ds, el) => setBinaryCategory(ds.cat, el.checked),
        openEventHistory: () => { closeSettings(); openEventHistory(); },
    });
    if (map) viewChanged(map.getView().getZoom() || 0);
}

// ─── settings ────────────────────────────────────────────────────────────────

export const binaryAnyShown = () => BINARY_CATEGORIES.some((c) => !settings.binary_exclude.includes(c));
export const objectsAnyShown = () => needsMapObjects(settings, binaryAnyShown());

// a display setting changed: the badges and markers take it
export const restyle = () => objects.restyle();

export function setBinaryCategory(cat, on) {
    const excluded = settings.binary_exclude.filter((c) => c !== cat);
    if (!on) excluded.push(cat);
    settings.binary_exclude = excluded;
    saveSettings();
    if (objectLayer.isVisible() && objectsAnyShown()) fetchObjects().then(() => redraw());
    else { objects.clear(); objectsSince = 0; redraw(); }
}

export function resetSince() { objectsSince = 0; objects.resetEvents(); }

// ─── transport ───────────────────────────────────────────────────────────────

// the objects that changed since the last poll; the client keeps the rest and ages them
export async function fetchObjects() {
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
    for (const mmsi in ships) objects.shipBadge(ships[mmsi].raw);
    const own = ownVessel();
    if (own) objects.stationBadge(own, { name: config.station, mmsi: own.mmsi, status: 'online' });
}

// the vessel the station's own MMSI names, when it is on the map with a position
export function ownVessel() {
    const st = station;
    if (settings.show_station === false || !st || !st.mmsi) return null;
    const ship = ships[st.mmsi]?.raw;
    return ship && hasValidCoords(ship.lat, ship.lon) ? ship : null;
}

// ─── hover, card and dialog ──────────────────────────────────────────────────

export const { tooltip: markerTooltip, shipTooltip, shipKinds, click, pollEvents, noteSeen: eventSeen } = objects;
export const stationTooltip = (feature, vesselHtml) => stationBand(feature.station_info) + (vesselHtml || '');

// a badge feature or an MMSI opens the vessel's received and sent tabs
export function showBinaryMessageDialog(featureOrMmsi) {
    if (featureOrMmsi && typeof featureOrMmsi === 'object') return objects.click(featureOrMmsi);
    const ship = ships[featureOrMmsi]?.raw;
    objects.showVesselMessages(Number(featureOrMmsi), ship ? ship.binary : 0);
}
