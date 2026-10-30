// The vessel in the card: its tabs and rows, its details and history as fetched,
// the title with its message kinds, the footer's follow and message items.

import { ships, cardMmsi, cardType, clock } from '../../core/store.js';
import { settings } from '../../core/state.js';
import { getDistanceUnit, getDistanceVal, u } from '../../core/units.js';
import * as shipcard from '@aiscatcher/ui/shipcard.js';
import { beginLoading } from '@aiscatcher/ui/loading.js';
import * as tabs from '@aiscatcher/ui/shipcard-tabs.js';
import { getCountryName, getDeltaTimeVal, getShipTypeFull, getStringfromChannels, getStringfromGroup, getStringfromMsgType } from '@aiscatcher/core/text.js';
import { getCallSign, getShipName } from '../../core/names.js';
import { decodeHTMLEntities } from '@aiscatcher/ui/components.js';
import { decodeBadge, glyphsHTML, KIND_CAT, showsShipBadge } from '@aiscatcher/ui/binary.js';
import * as mapObjects from '../mapobjects/mapobjects.js';
import { escapeShipText, focus } from '../../data.js';
import { severalStations, stationLabel } from '../../core/config.js';
import { stationBoxes } from '@aiscatcher/ui/stations.js';
import { register } from '../../actions.js';
import { saveSettings } from '../../settings-store.js';
import { ui } from '../../ui.js';
import { fitTargetcard, addItem, openOnTab, setValidation, showOutOfRange, updateTrackOption } from './card.js';
import { isFollowing } from '../follow/follow.js';

let cells = null, layout = null;

let historyCache = { mmsi: 0, pts: null, changes: null };

// the vessel's tabs; the receiver's own rows ride along as the AIS tab
export function build() {
    layout = tabs.build(document.getElementById("targetcard_common"), "targetcard_", {
        slot: document.getElementById("targetcard_slot_ais"),
        tab: null,
        onTab: (t) => { if (t) { settings.shipcard_active_tab = t; saveSettings(); } showHistory(t); fitTargetcard(); },
    });
    cells = layout.cells;

    const nearby = addItem('near_me', 'Nearby', 'Ships, stations and ports nearest this vessel', () => {
        const raw = ships[cardMmsi]?.raw;
        if (!raw || !Number.isFinite(raw.lat) || !Number.isFinite(raw.lon)) return;
        mapObjects.openNearby({ lat: raw.lat, lon: raw.lon, mmsi: Number(cardMmsi),
                                title: decodeHTMLEntities(getShipName(raw)) || 'MMSI ' + cardMmsi,
                                country: raw.country, shiptype: raw.shiptype, status: raw.status });
    }, 'ship');
    document.getElementById('targetcard_track_option').after(nearby);
}

export function open(mmsi, { fresh }) {
    resetHistory();
    openOnTab(layout, settings.shipcard_active_tab, fresh);
    loadVessel(mmsi);
}

// the tab is the user's; history renders where it is looked at
function showHistory(tab) {
    if (tab === "history") { fillHistory("changes"); fillHistory("speed"); fillHistory("draught"); }
}

function resetHistory() {
    historyCache = { mmsi: 0, pts: null, changes: null };

    for (const k of ["hull", "speed", "draught", "changes"]) document.getElementById("targetcard_" + k + "_body").innerHTML = "";
}

// the antenna drawing, its block hidden when the vessel gives no dimensions
function renderHull(ship) {
    const svg = shipcard.hull(ship, u);
    const body = document.getElementById("targetcard_hull_body");
    body.parentElement.hidden = !svg;
    ui.card.keepScroll(() => { body.innerHTML = svg; });
}


async function fetchHistory(mmsi, kind) {
    if (historyCache.mmsi !== mmsi) historyCache = { mmsi, pts: null, changes: null };
    if (historyCache[kind] === null) {
        const url = (kind === "pts" ? "api/path.json?" : "api/changes.json?") + mmsi + "&stations=" + focus;
        const data = await fetch(url).then((r) => r.json());
        if (historyCache.mmsi !== mmsi) return null;
        historyCache[kind] = kind === "pts" ? (data[mmsi] || []) : (Array.isArray(data) ? data : []);
    }
    return historyCache[kind];
}

let vessel = { mmsi: 0, data: null, inflight: false };
let stopVesselLoading = () => {};

// The vessel's details and what it has reported changing, in one fetch: on
// opening, and again with each pull while the card stays on it.
async function loadVessel(mmsi) {
    stopVesselLoading();
    vessel = { mmsi, data: null, inflight: false };
    await refreshVessel();
}

async function refreshVessel() {
    const mmsi = cardMmsi;
    if (!mmsi || vessel.mmsi !== mmsi || vessel.inflight) return;
    const request = vessel;
    request.inflight = true;
    const finishLoading = request.data ? () => {} : beginLoading(ui.card.el, 'Loading vessel', document.getElementById('targetcard_header'));
    stopVesselLoading = finishLoading;
    let data;
    try {
        data = await fetch("api/ship.json?mmsi=" + mmsi + "&stations=" + focus).then((r) => r.json());
    } catch {
        return;
    } finally {
        request.inflight = false;
        finishLoading();
    }
    if (cardMmsi !== mmsi || vessel !== request || !data || data.mmsi == null) return;
    vessel.data = escapeShipText(data);

    if (historyCache.mmsi !== mmsi) historyCache = { mmsi, pts: null, changes: null };
    historyCache.changes = Array.isArray(data.changes) ? data.changes : [];
    showHistory(layout.active());

    ui.card.keepScroll(populateCard);
}

// the History tab's lists and charts: "changes", "speed" or "draught"
async function fillHistory(key) {
    const mmsi = cardMmsi;
    if (!mmsi) return;

    const body = document.getElementById("targetcard_" + key + "_body");

    const kind = key === "speed" ? "pts" : "changes";
    let data;
    if (historyCache.mmsi !== mmsi || historyCache[kind] === null) {
        body.innerHTML = '<span class="dim-note">Loading…</span>';
        try {
            data = await fetchHistory(mmsi, kind);
        } catch (err) {
            body.innerHTML = '<span class="dim-note">' + (key === "speed" ? 'N/A' : 'History unavailable') + '</span>';
            return;
        }
    } else data = historyCache[kind];

    // the card may have moved on while the history was in flight
    if (cardMmsi !== mmsi || !data) return;

    const ship = ships[mmsi]?.raw;
    let html = "";

    if (key === "speed") html = shipcard.speedChart(data, u);
    else if (key === "draught") html = shipcard.draughtChart(data, ship ? ship.draught : null, u);
    else html = shipcard.changeList(data, u);

    ui.card.keepScroll(() => { body.innerHTML = html || '<span class="dim-note">N/A</span>'; });
    fitTargetcard();
}

// ─── footer icons ────────────────────────────────────────────────────────────

const ICON_UNAVAILABLE = 'card-icon-unavailable';

// the name, with one glyph per kind of message the vessel carries: the badge's
// newest kind at once, the rest once fetched; the glyphs open the messages
function setTitle(ship) {
    const title = ui.card.header.title();
    const glyphs = (cats) => glyphsHTML(cats, 'data-action="showBinaryMessageDialogCard"');
    const name = getShipName(ship) || ship.mmsi;
    // the station the vessel carries, after the message kinds
    const station = ship.station != null ? glyphsHTML(['station'], '') : '';
    title.innerHTML = name + station;
    if (!ship.binary) return;
    const mmsi = cardMmsi;
    const first = showsShipBadge(ship.binary) ? [KIND_CAT[decodeBadge(ship.binary).kind] || 'data'] : [];
    title.innerHTML = name + glyphs(first) + station;
    mapObjects.shipKinds(ship).then((cats) => { if (cardMmsi === mmsi && cats.length) title.innerHTML = name + glyphs(cats) + station; });
}

function updateMessageButton() {
    const button = document.getElementById("targetcard_message_option");
    if (!button) return;

    const iconElement = button.querySelector('i.mail_icon');
    if (!iconElement) return;

    // the row's packed badge says how many, before anything is fetched
    const word = ships[cardMmsi]?.raw?.binary;
    const count = word ? decodeBadge(word).count : 0;
    iconElement.querySelector('.message-badge')?.remove();

    if (count > 0) {
        const badge = document.createElement('span');
        badge.className = 'message-badge';
        badge.textContent = count;
        iconElement.appendChild(badge);
    }

    const wasAvailable = !button.classList.contains(ICON_UNAVAILABLE);
    button.classList.toggle(ICON_UNAVAILABLE, count === 0);

    // a button without messages must give up its slot, not sit there hidden
    if (wasAvailable !== (count > 0)) ui.card.footer.show(cardType);
}

// ─── content ─────────────────────────────────────────────────────────────────


export function updateFollowOption() {
    const option = document.getElementById("targetcard_follow_option");
    const following = isFollowing(cardMmsi);

    option.querySelector("#targetcard_follow").innerText = following ? "Unfollow" : "Follow";
    option.title = following ? "Stop centring the map on this vessel" : "Keep the map centred on this vessel";
    option.classList.toggle("card-icon-active", following);
}

function updateTechDetails(ship) {
    const formatFlag = (value, trueText = "Yes", falseText = "No", unknownText = "-") => {
        if (value === 0) return unknownText;
        if (value === 1) return falseText;
        if (value === 2) return trueText;
        return unknownText;
    };

    document.getElementById("tech_raim").textContent = formatFlag(ship.raim);

    document.getElementById("tech_dte").textContent = formatFlag(ship.dte, "Not Ready", "Ready");

    document.getElementById("tech_assigned").textContent = formatFlag(ship.assigned, "Assigned", "Autonomous");

    document.getElementById("tech_display").textContent = formatFlag(ship.display);

    document.getElementById("tech_dsc").textContent = formatFlag(ship.dsc);

    document.getElementById("tech_band").textContent = formatFlag(ship.band, "Dual", "Single");

    document.getElementById("tech_msg22").textContent = formatFlag(ship.msg22);

    document.getElementById("tech_off_position").textContent = formatFlag(ship.off_position, "Off", "On");

    const maneuverText = ship.maneuver === 0 ? "-" : ship.maneuver === 1 ? "None" : "Special";
    document.getElementById("tech_maneuver").textContent = maneuverText;

    // Transponder vendor info (type 24 part B)
    document.getElementById("tech_vendor").textContent = ship.vendorid || "-";
    document.getElementById("tech_model").textContent = ship.model != null ? ship.model : "-";
    document.getElementById("tech_serial").textContent = ship.serial != null ? ship.serial : "-";
}

export function populate() { ui.card.keepScroll(populateCard); refreshVessel(); }

const cardHelpers = {
    units: u,
    callsign: getCallSign,
    age: (s) => getDeltaTimeVal(clock - s.last_signal),
    ageSeconds: (s) => (s.last_signal != null ? clock - s.last_signal : null),
    // the ship type's colour: the one its tracks are drawn in
    typeColor: (s) => (settings.track_class_colors || {})[s.shipclass] || null,
    goTo: (lat, lon) => ui.reveal([lon, lat], undefined, { minZoom: 12, center: true }),
    openPort: (port) => mapObjects.openPlace(port),
    infoIcon: (k) => k === "tech"
        ? ' <i class="info_icon card-tech-icon" id="targetcard_tech_info" data-action="techInfo" title="Technical details"></i>'
        : ' <i class="info_icon card-tech-icon" id="targetcard_shiptype_info" data-action="shiptypeInfo" title="Ship type details"></i>',
};

function populateCard() {

    if (cardType != 'ship') return;

    if (!(cardMmsi in ships)) {
        showOutOfRange();
        return;
    }

    let ship = { ...(vessel.mmsi === cardMmsi && vessel.data ? vessel.data : {}), ...ships[cardMmsi].raw };

    ui.card.header.flag(ship.country, getCountryName(ship.country));
    setTitle(ship);
    setValidation(ship.validated);
    shipcard.populate(cells, ship, cardHelpers);
    layout.update(ship, cardHelpers);

    ["count", "received_stations"].forEach((e) => (document.getElementById("targetcard_" + e).innerHTML = ship[e] != null ? ship[e] : "-"));
    [
        { id: "bearing", u: "&deg", d: 0 },
        { id: "level", u: "dB", d: 1 },
        { id: "ppm", u: "ppm", d: 1 },
    ].forEach((el) => (document.getElementById("targetcard_" + el.id).innerHTML = ship[el.id] != null ? Number(ship[el.id]).toFixed(el.d) + " " + el.u : null));
    document.getElementById("targetcard_msgtypes").innerHTML = getStringfromMsgType(ship.msg_type);
    document.getElementById("targetcard_last_group").innerHTML = getStringfromGroup(ship.last_group);
    document.getElementById("targetcard_sources").innerHTML = getStringfromGroup(ship.group_mask);
    document.getElementById("targetcard_channels").innerHTML = getStringfromChannels(ship.channels);
    // the stations that heard it, where the page has several; a viewer that keeps
    // no stations sends none
    const stations = ship.stations || [];
    document.getElementById("targetcard_stations_row").hidden = !severalStations() || !stations.length;
    document.getElementById("targetcard_stations").innerHTML = stationBoxes(stations, stationLabel, stations.length);
    const shiptype = ship.shiptype ?? null;
    document.getElementById("shiptype_code").textContent = shiptype != null ? shiptype : "-";
    document.getElementById("shiptype_desc").textContent = shiptype != null ? getShipTypeFull(shiptype) : "-";
    document.getElementById("targetcard_last_signal").innerHTML = getDeltaTimeVal(clock - ship.last_signal);
    document.getElementById("targetcard_distance").innerHTML = ship.distance != null ? getDistanceVal(ship.distance) + " " + getDistanceUnit() : null;
    document.getElementById("targetcard_repeated").innerHTML = ship.repeat != null ? (ship.repeat > 0 ? "Yes" : "No") : null;

    updateTrackOption();
    updateFollowOption();
    updateMessageButton();
    updateTechDetails(ship);
    renderHull(ship);

}

register({
    techInfo: (e) => { e.stopPropagation(); ui.card.popover.toggle(document.getElementById("tech_popover"), document.getElementById("targetcard_tech_info")); },
    shiptypeInfo: (e) => { e.stopPropagation(); ui.card.popover.toggle(document.getElementById("shiptype_popover"), document.getElementById("targetcard_shiptype_info")); },
});
