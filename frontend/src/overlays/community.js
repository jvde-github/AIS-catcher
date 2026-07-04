// Community pane: opens aiscatcher.org/livemap in a popup and keeps the two
// map views in sync via postMessage; also owns the sharing-state indicator.

import { settings } from '../core/state.js';
import { fromLonLat, toLonLat } from 'ol/proj';

const COMMUNITY_NS = 'aiscatcher-mapsync';
const COMMUNITY_ORIGIN = 'https://www.aiscatcher.org';
const COMMUNITY_EPS_LATLON = 1e-4;
const COMMUNITY_EPS_ZOOM = 0.01;

let deps = null; // { config, getMap }
let communityPopup = null;
let communityLastApplied = null;

export function init(d) {
    deps = d;

    window.addEventListener('message', (ev) => {
        if (!communityPopup || ev.source !== communityPopup) return;
        if (ev.origin !== COMMUNITY_ORIGIN) return;
        const m = ev.data;
        if (!m || m.ns !== COMMUNITY_NS || m.type !== 'view') return;
        const v = { lat: +m.lat, lon: +m.lon, zoom: +m.zoom };
        if (!viewSane(v)) return;
        if (viewNear(v, communityLastApplied)) return;
        const view = deps.getMap().getView();
        view.setCenter(fromLonLat([v.lon, v.lat]));
        view.setZoom(v.zoom);
        communityLastApplied = v;
    });
}

function viewNear(a, b) {
    return a && b
        && Math.abs(a.lat  - b.lat)  < COMMUNITY_EPS_LATLON
        && Math.abs(a.lon  - b.lon)  < COMMUNITY_EPS_LATLON
        && Math.abs(a.zoom - b.zoom) < COMMUNITY_EPS_ZOOM;
}

function viewSane(v) {
    return Number.isFinite(v.lat)  && v.lat  >= -90  && v.lat  <= 90
        && Number.isFinite(v.lon)  && v.lon  >= -180 && v.lon  <= 180
        && Number.isFinite(v.zoom) && v.zoom >= 0    && v.zoom <= 28;
}

let liveSharing = null;

export function updateSharingState(sharing, sharing_uuid, engine_running) {
    liveSharing = {
        sharing: !!sharing,
        sharing_uuid: !!sharing_uuid,
        engine_running: engine_running !== false,
    };
    applySharingState();
}

export function sharingDisplay() {
    const f = liveSharing || deps.config.features || {};
    if (f.engine_running === false) return ["Receiver stopped", "gray"];
    if (!f.sharing)        return ["No", "red"];
    if (!f.sharing_uuid)   return ["Yes (anonymous)", "orange"];
    return ["Yes", "green"];
}

export function applySharingState() {
    const f = liveSharing || deps.config.features || {};
    const cls = f.engine_running === false ? "fill-gray"
              : !f.sharing ? "fill-red"
              : !f.sharing_uuid ? "fill-orange"
              : "fill-green";
    const btn = document.getElementById("xchange");
    if (btn) {
        btn.classList.remove("fill-red", "fill-orange", "fill-green", "fill-gray");
        btn.classList.add(cls);
    }
}

export function toggleCommunityPane() {
    if (communityPopup && !communityPopup.closed) {
        communityPopup.close();
        communityPopup = null;
        return;
    }
    const view = deps.getMap().getView();
    const [lon, lat] = toLonLat(view.getCenter());
    const zoom = view.getZoom();
    const url = `${COMMUNITY_ORIGIN}/livemap?lat=${lat.toFixed(4)}&lon=${lon.toFixed(4)}&zoom=${zoom.toFixed(2)}`;
    const w = Math.floor(screen.availWidth / 2);
    const h = screen.availHeight;
    communityPopup = window.open(url, 'aiscatcher-community',
        `popup=yes,width=${w},height=${h},left=${w},top=0`);
    if (communityPopup) {
        communityPopup.focus();
        communityLastApplied = { lat, lon, zoom };
    }
}

export function notifyCommunityPopupView() {
    if (!communityPopup || communityPopup.closed) return;
    const v = { lat: settings.lat, lon: settings.lon, zoom: settings.zoom };
    if (viewNear(v, communityLastApplied)) return;
    communityPopup.postMessage({
        ns: COMMUNITY_NS, v: 1, type: 'view',
        lat: v.lat, lon: v.lon, zoom: v.zoom
    }, COMMUNITY_ORIGIN);
}

export function pushVesselsToCommunityPopup(dynamic) {
    if (!communityPopup || communityPopup.closed) return;
    if (!Array.isArray(dynamic) || dynamic.length === 0) return;
    const updates = [];
    for (const v of dynamic) {
        const mmsi = v[0], lat = v[1], lon = v[2];
        if (mmsi == null || lat == null || lon == null) continue;
        updates.push([mmsi, lat, lon]);
    }
    if (updates.length === 0) return;
    communityPopup.postMessage(
        { ns: COMMUNITY_NS, v: 1, type: 'stations-vessels', updates },
        COMMUNITY_ORIGIN
    );
}
