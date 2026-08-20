// Sections, footer icons, popovers and field rendering for the shipcard.
// Placement and open/close stay with the map layout in script.js.

import { ships, cardMmsi, cardType, hoverMmsi, hoverType, markerTracks, clock } from '../core/store.js';
import { settings } from '../core/state.js';
import {
    CHANGE, getChangeVal, getChangeListHTML, getSpeedHistorySVG, getDraughtChartSVG,
    getShipDimensionSVG, getShipDimension, getCallSign, getCountryName, getDeltaTimeVal, CARD_FLAG_STYLE,
    getDimUnit, getDistanceUnit, getDistanceVal, getDraughtVal, getEtaVal, getFlagStyled,
    getLatValFormat, getLonValFormat, getMmsiTypeVal, getShipName, getShipTypeFull,
    getShipTypeShort, getSpeedUnit, getSpeedVal, getStatusVal, getStringfromChannels,
    getStringfromGroup, getStringfromMsgType,
} from '../core/format.js';
import * as binary from './binary.js';

// { fitShipcard, getReceiver, realtimeEnabled, registerAction, isFollowing,
//   updateFocusMarker, hoverTrackShown, selectTrackShown }
let deps = null;

export function init(d) { deps = d; }

// ─── sections ────────────────────────────────────────────────────────────────

const sectionOpen = {
    voyage: true, vessel: true, source: false,
    hull: false, speed: false, draught: false, changes: false,
};

let historyCache = { mmsi: 0, pts: null, changes: null };

export function resetHistory() {
    historyCache = { mmsi: 0, pts: null, changes: null };

    ["hull", "speed", "draught", "changes"].forEach((k) => {
        const body = document.getElementById("shipcard_" + k + "_body");
        if (body) body.innerHTML = "";
        sectionOpen[k] = false;
        setSectionAvailable(k, true);
    });

    sectionOpen.voyage = true;
    sectionOpen.vessel = true;
    sectionOpen.source = false;
    Object.keys(sectionOpen).forEach(applySection);
}

export function sectionState() { return sectionOpen; }

export function applySection(key) {
    const open = !!sectionOpen[key];

    document.querySelectorAll('#shipcard_content [data-section="' + key + '"]').forEach((el) => {
        if (el.classList.contains("shipcard-section")) {
            const caret = el.querySelector(".shipcard-section-caret");
            if (caret) {
                caret.classList.toggle("keyboard_arrow_up_icon", open);
                caret.classList.toggle("keyboard_arrow_down_icon", !open);
            }
            return;
        }
        el.classList.toggle("sec-collapsed", !open);
    });
}

export function toggleSection(key) {
    if (!key) return;

    sectionOpen[key] = !sectionOpen[key];
    applySection(key);
    if (sectionOpen[key]) fillSection(key);
    deps.fitShipcard();
}

export function openSection(key) {
    if (!sectionOpen[key]) toggleSection(key);
}

export function setSectionAvailable(key, available) {
    const head = document.getElementById("shipcard_" + key + "_head");
    const body = document.getElementById("shipcard_" + key + "_section");

    if (head) head.classList.toggle("sec-empty", !available);
    if (body && !available) body.classList.add("sec-collapsed");
}

export function renderHull(ship) {
    ship = ship || ships[cardMmsi]?.raw;
    const svg = ship ? getShipDimensionSVG(ship) : "";

    setSectionAvailable("hull", !!svg);
    const body = document.getElementById("shipcard_hull_body");
    if (body) body.innerHTML = svg;
}

export async function fillSection(key) {
    const mmsi = cardMmsi;
    if (!mmsi) return;

    if (key === "hull") return renderHull();
    if (!["speed", "draught", "changes"].includes(key)) return;

    const body = document.getElementById("shipcard_" + key + "_body");
    if (!body) return;

    if (historyCache.mmsi !== mmsi) {
        body.innerHTML = '<span class="dim-note">Loading…</span>';
        try {
            const receiver = deps.getReceiver();
            const [p, c] = await Promise.all([
                fetch("api/path.json?" + mmsi + "&receiver=" + receiver).then((r) => r.json()),
                fetch("api/changes.json?" + mmsi + "&receiver=" + receiver).then((r) => r.json()),
            ]);
            historyCache = { mmsi, pts: p[mmsi] || [], changes: Array.isArray(c) ? c : [] };
        } catch (err) {
            body.innerHTML = '<span class="dim-note">History unavailable</span>';
            return;
        }
    }

    // the card may have moved on while the history was in flight
    if (cardMmsi !== mmsi) return;

    const ship = ships[mmsi]?.raw;
    let html = "";

    if (key === "speed") html = getSpeedHistorySVG(historyCache.pts);
    else if (key === "draught") html = getDraughtChartSVG(historyCache.changes, ship ? ship.draught : null);
    else html = getChangeListHTML(historyCache.changes,
        [CHANGE.DESTINATION, CHANGE.ETA, CHANGE.SHIPNAME, CHANGE.CALLSIGN, CHANGE.STATUS],
        getChangeVal);

    body.innerHTML = html || '<span class="dim-note">Nothing recorded yet</span>';
    deps.fitShipcard();
}

// ─── popovers ────────────────────────────────────────────────────────────────

export function closePopovers() {
    document.querySelectorAll("#shipcard .tech-popover").forEach((el) => (el.style.display = "none"));
}

export function togglePopover(popoverId, iconId) {
    const popover = document.getElementById(popoverId);
    const icon = document.getElementById(iconId);

    const wasOpen = popover.style.display === "block";
    closePopovers();
    if (wasOpen) return;

    const onOutsideClick = (event) => {
        if (popover.contains(event.target)) return;
        event.stopPropagation();
        event.preventDefault();
        popover.style.display = "none";
        document.removeEventListener("click", onOutsideClick, true);
    };

    const iconRect = icon.getBoundingClientRect();
    const shipcardRect = document.getElementById("shipcard").getBoundingClientRect();

    popover.style.display = "block";

    let left = iconRect.left - shipcardRect.left + 20;
    let top = iconRect.bottom - shipcardRect.top + 5;

    const popoverRect = popover.getBoundingClientRect();

    if (iconRect.left + popoverRect.width + 20 > window.innerWidth)
        left = Math.max(5, iconRect.right - shipcardRect.left - popoverRect.width - 5);
    if (iconRect.bottom + popoverRect.height + 5 > window.innerHeight)
        top = Math.max(5, iconRect.top - shipcardRect.top - popoverRect.height - 5);

    left = Math.max(5, Math.min(left, shipcardRect.width - popoverRect.width - 5));
    top = Math.max(5, top);

    popover.style.left = left + "px";
    popover.style.top = top + "px";

    setTimeout(() => document.addEventListener("click", onOutsideClick, true), 0);
}

// ─── footer icons ────────────────────────────────────────────────────────────

const ICON_MAX = 3;
const ICON_UNAVAILABLE = 'shipcard-icon-unavailable';

let iconOffset = { ship: 0, plane: 0 };
let iconCount = { ship: 0, plane: 0 };

export function addItem(icon, txt, title, action, contextType = 'ship') {
    const div = document.createElement("div");
    div.title = title;
    div.setAttribute("data-context-type", contextType);

    if (icon.startsWith("fa")) icon = "question_mark";

    const i = document.createElement("i");
    i.className = icon + "_icon";
    const span = document.createElement("span");
    span.textContent = txt;
    div.appendChild(i);
    div.appendChild(span);

    const name = deps.registerAction(action);
    if (!name) return;

    div.dataset.action = name;
    document.getElementById("shipcard_footer").appendChild(div);
}

function isMoreIcon(icon) {
    return !!icon.querySelector('i')?.classList.contains('more_horiz_icon');
}

function iconAvailable(icon, type) {
    if (icon.dataset.contextType !== type) return false;
    if (icon.id === 'shipcard_realtime_option' && !deps.realtimeEnabled()) return false;
    return !icon.classList.contains(ICON_UNAVAILABLE);
}

export function displayIcons(type) {
    const icons = [...document.querySelectorAll('#shipcard_footer > div')];
    const more = icons.filter((icon) => isMoreIcon(icon) && icon.dataset.contextType === type);
    const available = icons.filter((icon) => !isMoreIcon(icon) && iconAvailable(icon, type));

    iconCount[type] = available.length;
    if (iconOffset[type] >= available.length) iconOffset[type] = 0;

    const paged = available.length > ICON_MAX;
    for (const icon of icons) icon.style.display = more.includes(icon) && paged ? "flex" : "none";

    available
        .slice(iconOffset[type], iconOffset[type] + ICON_MAX)
        .forEach((icon) => (icon.style.display = "flex"));
}

export function rotateIcons() {
    iconOffset[cardType] += ICON_MAX;
    if (iconOffset[cardType] >= iconCount[cardType]) iconOffset[cardType] = 0;

    displayIcons(cardType);
}

export function prepare() {
    iconOffset = { ship: 0, plane: 0 };
    iconCount = { ship: 0, plane: 0 };

    addItem('more_horiz', 'More', 'More options', 'rotateShipcardIcons', 'ship');
    addItem('more_horiz', 'More', 'More options', 'rotateShipcardIcons', 'plane');

    displayIcons('ship');
}

export function updateMessageButton() {
    const button = document.getElementById("shipcard_message_option");
    if (!button) return;

    const iconElement = button.querySelector('i.mail_icon');
    if (!iconElement) return;

    const count = binary.shipBinaryMessages(cardMmsi).length;
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
    if (wasAvailable !== (count > 0)) displayIcons(cardType);
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
    const trackOptionElement = document.getElementById("shipcard_track_option");

    if (settings.show_all_tracks || cardType == 'plane') {
        trackOptionElement.style.opacity = "0.5";
        trackOptionElement.style.pointerEvents = "none";
    } else {
        trackOptionElement.style.opacity = "1";
        trackOptionElement.style.pointerEvents = "auto";
    }

    if (cardMmsi && cardType == 'ship') {
        document.getElementById("shipcard_track").innerText = trackOptionString(cardMmsi);
    }
}

export function updateFollowOption() {
    const option = document.getElementById("shipcard_follow_option");
    const following = deps.isFollowing(cardMmsi);

    option.querySelector("#shipcard_follow").innerText = following ? "Unfollow" : "Follow";
    option.title = following ? "Stop centring the map on this vessel" : "Keep the map centred on this vessel";
    option.classList.toggle("shipcard-icon-active", following);
}

export function setValidation(v) {
    document.getElementById("shipcard_header").classList.remove("shipcard-validated", "shipcard-not-validated", "shipcard-dubious");

    switch (v) {
        case 1:
            document.getElementById("shipcard_header").classList.add("shipcard-validated");
            break;
        case -1:
            document.getElementById("shipcard_header").classList.add("shipcard-dubious");
            break;
        default:
            document.getElementById("shipcard_header").classList.add("shipcard-not-validated");
    }
}

export function showOutOfRange() {
    document
        .getElementById("shipcard_content")
        .querySelectorAll("span:nth-child(2)")
        .forEach((e) => (e.innerHTML = null));
    document.getElementById("shipcard_header_title").innerHTML = "<b style='color:red;'>Out of range</b>";
    document.getElementById("shipcard_header_flag").innerHTML = "";
    document.getElementById("shipcard_mmsi").innerHTML = cardMmsi;

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

export function populate() {

    if (cardType != 'ship') return;

    if (!(cardMmsi in ships)) {
        showOutOfRange();
        return;
    }

    let ship = ships[cardMmsi].raw;

    document.getElementById("shipcard_header_flag").innerHTML = getFlagStyled(ship.country, CARD_FLAG_STYLE);
    document.getElementById("shipcard_header_title").innerHTML = (getShipName(ship) || ship.mmsi);

    setValidation(ship.validated);

    ["destination", "mmsi", "count", "received_stations"].forEach((e) => (document.getElementById("shipcard_" + e).innerHTML = ship[e] != null ? ship[e] : "-"));

    if (ship.imo != null) {
        document.getElementById("shipcard_imo_label").innerHTML = "IMO";
        document.getElementById("shipcard_imo").innerHTML = ship.imo;
    } else {
        document.getElementById("shipcard_imo_label").innerHTML = ship.eni ? "ENI" : "IMO";
        document.getElementById("shipcard_imo").innerHTML = ship.eni || "-";
    }

    [
        { id: "cog", u: "&deg", d: 1 },
        { id: "bearing", u: "&deg", d: 0 },
        { id: "heading", u: "&deg", d: 0 },
        { id: "level", u: "dB", d: 1 },
        { id: "ppm", u: "ppm", d: 1 },
    ].forEach((el) => (document.getElementById("shipcard_" + el.id).innerHTML = ship[el.id] != null ? Number(ship[el.id]).toFixed(el.d) + " " + el.u : null));

    document.getElementById("shipcard_country").innerHTML = getCountryName(ship.country);
    document.getElementById("shipcard_callsign").innerHTML = getCallSign(ship) || null;
    document.getElementById("shipcard_msgtypes").innerHTML = getStringfromMsgType(ship.msg_type);

    document.getElementById("shipcard_last_group").innerHTML = getStringfromGroup(ship.last_group);
    document.getElementById("shipcard_sources").innerHTML = getStringfromGroup(ship.group_mask);

    document.getElementById("shipcard_channels").innerHTML = getStringfromChannels(ship.channels);
    document.getElementById("shipcard_type").innerHTML = getMmsiTypeVal(ship) + ' <i class="info_icon shipcard-tech-icon" id="shipcard_tech_info" data-action="techInfo" title="Technical details"></i>';
    const shiptype = ship.shiptype ?? null;
    document.getElementById("shipcard_shiptype").innerHTML = shiptype != null
        ? getShipTypeShort(shiptype) + ' <i class="info_icon shipcard-tech-icon" id="shipcard_shiptype_info" data-action="shiptypeInfo" title="Ship type details"></i>'
        : getShipTypeShort(shiptype);
    document.getElementById("shiptype_code").textContent = shiptype != null ? shiptype : "-";
    document.getElementById("shiptype_desc").textContent = shiptype != null
        ? getShipTypeFull(shiptype)
        : "-";
    document.getElementById("shipcard_status").innerHTML = getStatusVal(ship);
    document.getElementById("shipcard_last_signal").innerHTML = getDeltaTimeVal(clock - ship.last_signal);
    document.getElementById("shipcard_eta").innerHTML = ship.eta_month != null && ship.eta_hour != null && ship.eta_day != null && ship.eta_minute != null ? getEtaVal(ship) : null;
    document.getElementById("shipcard_lat").innerHTML = ship.lat != null ? getLatValFormat(ship) : null;
    document.getElementById("shipcard_lon").innerHTML = ship.lon != null ? getLonValFormat(ship) : null;
    document.getElementById("shipcard_altitude").innerHTML = ship.altitude != null ? ship.altitude + " m" : null;

    document.getElementById("shipcard_speed").innerHTML = ship.speed != null ? getSpeedVal(ship.speed) + " " + getSpeedUnit() : null;
    document.getElementById("shipcard_distance").innerHTML = ship.distance != null ? getDistanceVal(ship.distance) + " " + getDistanceUnit() : null;
    document.getElementById("shipcard_repeated").innerHTML = ship.repeat != null ? (ship.repeat > 0 ? "Yes" : "No") : null;
    document.getElementById("shipcard_draught").innerHTML = ship.draught ? getDraughtVal(ship.draught) + " " + getDimUnit() : null;
    document.getElementById("shipcard_dimension").innerHTML = getShipDimension(ship);
    document.getElementById("shipcard_bluesign").innerHTML = ship.maneuver === 2 ? "Set" : (ship.maneuver === 1 ? "Not set" : null);

    updateTrackOption();
    updateFollowOption();
    updateMessageButton();
    updateTechDetails(ship);
    renderHull(ship);

}

