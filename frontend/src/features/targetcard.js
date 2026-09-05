
import { ships, cardMmsi, cardType, hoverMmsi, hoverType, markerTracks, clock } from '../core/store.js';
import { settings } from '../core/state.js';
import { getChangeVal, getShipDimension, getDimUnit, getDistanceUnit, getDistanceVal, getDraughtVal, getLatValFormat, getLonValFormat, getSpeedUnit, getSpeedVal, u } from '../core/units.js';
import * as shipcard from '../../shared/shipcard.js';
import * as classic from '../../shared/shipcard-classic.js';
import * as tabs from '../../shared/shipcard-tabs.js';
import { CHANGE, getCountryName, getDeltaTimeVal, getEtaVal, getMmsiTypeVal, getShipTypeFull, getShipTypeShort, getStatusVal, getStringfromChannels, getStringfromGroup, getStringfromMsgType } from '../../shared/core/text.js';
import { getChangeListHTML, getSpeedHistorySVG, getDraughtChartSVG, getShipDimensionSVG } from '../../shared/core/spark.js';
import { getCallSign, getShipName } from '../core/names.js';
import { regionName } from '../core/regions.js';
import { flagHTML } from '../../shared/components.js';
import { decodeBadge, glyphsHTML, KIND_CAT } from '../../shared/binary.js';
import * as binary from './binary.js';

// { fitTargetcard, getReceiver, realtimeEnabled, registerAction, isFollowing,
//   updateFocusMarker, hoverTrackShown, selectTrackShown }
let deps = null;
export let card = null;

export function init(d) {
    deps = d;
    card = d.card;
    if (!card) throw new Error("targetcard: no #targetcard to mount on");
    card.section.reset(SECTION_DEFAULTS);
    buildLayout(deps.getStyle ? deps.getStyle() : "classic");
    card.el.addEventListener("card:section", (e) => {
        if (e.detail.open) fillSection(e.detail.key);
        deps.fitTargetcard();
    });
}

// ─── sections ────────────────────────────────────────────────────────────────

export const SECTION_DEFAULTS = { ...classic.SECTIONS, source: true, aircraft: true, flight: true, adsb: true };
let cells = null, layout = null, style = "classic", slotHome = null;

let historyCache = { mmsi: 0, pts: null, changes: null };

// The card's shape: the classic fold of sections, or the remembered tabs. The
// receiver's own section rides along - after the body in classic, as the AIS
// tab otherwise - and the tabbed card stays open, so its chevron goes.
function buildLayout(s) {
    style = s === "tabs" ? "tabs" : "classic";
    const mount = document.getElementById("targetcard_common");
    const slot = document.getElementById("targetcard_slot_ais");
    const chevron = document.getElementById("targetcard_minmax_button");
    if (!slotHome && slot) slotHome = { parent: slot.parentNode, next: slot.nextSibling };
    mount.innerHTML = "";
    if (style === "tabs") {
        layout = tabs.build(mount, "targetcard_", {
            slot,
            tab: openTab(),
            onTab: (t) => { if (t && deps.setTab) deps.setTab(t); showHistory(t); deps.fitTargetcard(); },
            foldable: () => !!(deps.isCramped && deps.isCramped()),
        });
        if (chevron) chevron.style.display = "none";
        card.setMax(true);
    } else {
        if (slot && slotHome) slotHome.parent.insertBefore(slot, slotHome.next);
        mount.classList.remove("sc-host");
        if (chevron) chevron.style.display = "";
        const c = classic.build(card, mount, "targetcard_", { rowAttrs: { "data-action": "targetcardSelectSelf" } });
        layout = { cells: c, select: () => {}, active: () => null, update: () => {}, scrollTop: () => {} };
    }
    cells = layout.cells;
    document.getElementById("targetcard").classList.toggle("card-tabs", style === "tabs");
}

export function setStyle(s) {
    buildLayout(s);
    if (cardMmsi) {
        resetHistory();
        populate();
        loadVessel(cardMmsi);
    }
}

// a fresh card on a cramped pane opens folded, like the classic card opens
// compact there; otherwise the remembered tab
function openTab() {
    if (deps.foldOnOpen && deps.foldOnOpen()) return null;
    return deps.getTab ? deps.getTab() : tabs.DEFAULT_TAB;
}

// the tab is the user's; history renders where it is looked at
function showHistory(tab) {
    if (tab === "voyage") fillSection("changes");
    else if (tab === "history") { fillSection("speed"); fillSection("draught"); }
}

export function resetHistory() {
    historyCache = { mmsi: 0, pts: null, changes: null };

    ["hull", "speed", "draught", "changes"].forEach((k) => {
        const body = document.getElementById("targetcard_" + k + "_body");
        if (body) body.innerHTML = "";
        card.section.available(k, true);
    });

    card.section.reset(SECTION_DEFAULTS);
    if (style === "tabs") {
        // switching ship with the card up on a cramped pane keeps its fold
        if (!(deps.isCramped && deps.isCramped()) || deps.foldOnOpen()) layout.select(openTab(), false);
        layout.scrollTop();
    }
}


export function renderHull(ship) {
    ship = ship || ships[cardMmsi]?.raw;
    const svg = ship ? shipcard.hull(ship, u) : "";

    card.section.available("hull", !!svg);
    const body = document.getElementById("targetcard_hull_body");
    if (style === "tabs" && body) body.parentElement.hidden = !svg;
    if (body) card.keepScroll(() => { body.innerHTML = svg; });
}


async function fetchHistory(mmsi, kind) {
    if (historyCache.mmsi !== mmsi) historyCache = { mmsi, pts: null, changes: null };
    if (historyCache[kind] === null) {
        const url = (kind === "pts" ? "api/path.json?" : "api/changes.json?") + mmsi + "&receiver=" + deps.getReceiver();
        const data = await fetch(url).then((r) => r.json());
        if (historyCache.mmsi !== mmsi) return null;
        historyCache[kind] = kind === "pts" ? (data[mmsi] || []) : (Array.isArray(data) ? data : []);
    }
    return historyCache[kind];
}

let vessel = { mmsi: 0, data: null, inflight: false };

// The vessel's details and what it has reported changing, in one fetch: on
// opening, and again with each pull while the card stays on it. The changes
// section opens by itself when there is anything to show.
export async function loadVessel(mmsi) {
    vessel = { mmsi, data: null, inflight: false };
    await refreshVessel();
}

async function refreshVessel() {
    const mmsi = cardMmsi;
    if (!mmsi || vessel.mmsi !== mmsi || vessel.inflight) return;
    vessel.inflight = true;
    let data;
    try {
        data = await fetch("api/ship.json?mmsi=" + mmsi + "&receiver=" + deps.getReceiver()).then((r) => r.json());
    } catch {
        vessel.inflight = false;
        return;
    }
    vessel.inflight = false;
    if (cardMmsi !== mmsi || vessel.mmsi !== mmsi || !data || data.mmsi == null) return;
    vessel.data = data;

    if (historyCache.mmsi !== mmsi) historyCache = { mmsi, pts: null, changes: null };
    historyCache.changes = Array.isArray(data.changes) ? data.changes : [];
    if (style === "tabs") {
        showHistory(layout.active());
    } else {
        const isOpen = (key) => !document.getElementById("targetcard_" + key + "_section")?.classList.contains("card-collapsed");
        if (isOpen("changes")) fillSection("changes");
        else if (shipcard.hasChanges(historyCache.changes)) card.section.open("changes");
        if (isOpen("draught")) fillSection("draught");
        else if (shipcard.hasDraughtChange(historyCache.changes)) card.section.open("draught");
    }

    card.keepScroll(populateCard);
}

export async function fillSection(key) {
    const mmsi = cardMmsi;
    if (!mmsi) return;

    if (key === "hull") return renderHull();
    if (!["speed", "draught", "changes"].includes(key)) return;

    const body = document.getElementById("targetcard_" + key + "_body");
    if (!body) return;

    const kind = key === "speed" ? "pts" : "changes";
    let data;
    if (historyCache.mmsi !== mmsi || historyCache[kind] === null) {
        body.innerHTML = '<span class="dim-note">Loading…</span>';
        try {
            data = await fetchHistory(mmsi, kind);
        } catch (err) {
            body.innerHTML = '<span class="dim-note">History unavailable</span>';
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

    card.keepScroll(() => { body.innerHTML = html || '<span class="dim-note">N/A</span>'; });
    deps.fitTargetcard();
}

// ─── popovers ────────────────────────────────────────────────────────────────

// ─── footer icons ────────────────────────────────────────────────────────────

const ICON_UNAVAILABLE = 'card-icon-unavailable';

export function addItem(icon, txt, title, action, contextType = 'ship') {
    const name = deps.registerAction(action);
    if (!name) return;
    if (icon.startsWith("fa")) icon = "question_mark";
    card.footer.add({ icon, label: txt, title, action: name, group: contextType });
}

export function prepare() {
    card.footer.reset();
    card.footer.add({ icon: 'more_horiz', label: 'More', title: 'More options', action: 'rotateTargetcardIcons', group: 'ship', more: true });
    card.footer.add({ icon: 'more_horiz', label: 'More', title: 'More options', action: 'rotateTargetcardIcons', group: 'plane', more: true });
    card.footer.show('ship');
}

// the name, with one glyph per kind of message the vessel carries: the badge's
// newest kind at once, the rest once fetched; the glyphs open the messages
function setTitle(ship) {
    const title = document.getElementById("targetcard_header_title");
    const glyphs = (cats) => glyphsHTML(cats, 'data-action="showBinaryMessageDialogCard"');
    const name = getShipName(ship) || ship.mmsi;
    // the station the vessel carries, after the message kinds
    const station = ship.station ? glyphsHTML(['station'], '') : '';
    title.innerHTML = name + station;
    if (!ship.binary) return;
    const mmsi = cardMmsi;
    title.innerHTML = name + glyphs([KIND_CAT[decodeBadge(ship.binary).kind] || 'data']) + station;
    binary.shipKinds(ship).then((cats) => { if (cardMmsi === mmsi && cats.length) title.innerHTML = name + glyphs(cats) + station; });
}

export function updateMessageButton() {
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
    if (wasAvailable !== (count > 0)) card.footer.show(cardType);
}

// ─── content ─────────────────────────────────────────────────────────────────


export function trackOptionString(mmsi) {
    const hover_track = hoverType == 'ship' && mmsi == hoverMmsi && deps.hoverTrackShown();
    const select_track = cardType == 'ship' && mmsi == cardMmsi && deps.selectTrackShown();
    const track_shown = markerTracks.has(Number(mmsi));

    if (hover_track || select_track) return "Show Track";
    return track_shown ? "Hide Track" : "Show Track";
}

export function updateTrackOption() {
    const trackOptionElement = document.getElementById("targetcard_track_option");

    if (settings.show_all_tracks || cardType == 'plane') {
        trackOptionElement.style.opacity = "0.5";
        trackOptionElement.style.pointerEvents = "none";
    } else {
        trackOptionElement.style.opacity = "1";
        trackOptionElement.style.pointerEvents = "auto";
    }

    if (cardMmsi && cardType == 'ship') {
        document.getElementById("targetcard_track").innerText = trackOptionString(cardMmsi);
    }
}

export function updateFollowOption() {
    const option = document.getElementById("targetcard_follow_option");
    const following = deps.isFollowing(cardMmsi);

    option.querySelector("#targetcard_follow").innerText = following ? "Unfollow" : "Follow";
    option.title = following ? "Stop centring the map on this vessel" : "Keep the map centred on this vessel";
    option.classList.toggle("card-icon-active", following);
}

export function setValidation(v) {
    document.getElementById("targetcard_header").classList.remove("card-validated", "card-not-validated", "card-dubious");

    switch (v) {
        case 1:
            document.getElementById("targetcard_header").classList.add("card-validated");
            break;
        case -1:
            document.getElementById("targetcard_header").classList.add("card-dubious");
            break;
        default:
            document.getElementById("targetcard_header").classList.add("card-not-validated");
    }
}

export function showOutOfRange() {
    document
        .getElementById("targetcard_content")
        .querySelectorAll("span:nth-child(2)")
        .forEach((e) => (e.innerHTML = null));
    document.getElementById("targetcard_header_title").innerHTML = "<b style='color:red;'>Out of range</b>";
    document.getElementById("targetcard_header_flag").innerHTML = "";
    document.getElementById("targetcard_mmsi").innerHTML = cardMmsi;

    deps.updateFocusMarker();
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

export function populate() { card.keepScroll(populateCard); refreshVessel(); }

const cardHelpers = {
    units: u,
    regionName,
    callsign: getCallSign,
    age: (s) => getDeltaTimeVal(clock - s.last_signal),
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

    document.getElementById("targetcard_header_flag").innerHTML = flagHTML(ship.country, 'flag-card', getCountryName(ship.country));
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

