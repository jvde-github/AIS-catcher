
import { ships, cardMmsi, cardType, hoverMmsi, hoverType, markerTracks, clock } from '../core/store.js';
import { settings } from '../core/state.js';
import { getChangeVal, getShipDimension, getDimUnit, getDistanceUnit, getDistanceVal, getDraughtVal, getLatValFormat, getLonValFormat, getSpeedUnit, getSpeedVal, u } from '../core/units.js';
import { CHANGE, getCountryName, getDeltaTimeVal, getEtaVal, getMmsiTypeVal, getShipTypeFull, getShipTypeShort, getStatusVal, getStringfromChannels, getStringfromGroup, getStringfromMsgType } from '../../shared/core/text.js';
import { getChangeListHTML, getSpeedHistorySVG, getDraughtChartSVG, getShipDimensionSVG } from '../../shared/core/spark.js';
import { getCallSign, getShipName } from '../core/names.js';
import { flagHTML } from '../../shared/components.js';
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
    card.el.addEventListener("card:section", (e) => {
        if (e.detail.open) fillSection(e.detail.key);
        deps.fitTargetcard();
    });
}

// ─── sections ────────────────────────────────────────────────────────────────

export const SECTION_DEFAULTS = {
    voyage: true, vessel: true, source: true,
    hull: true, speed: false, draught: false, changes: false,
    aircraft: true, flight: true, adsb: true,
};

let historyCache = { mmsi: 0, pts: null, changes: null };

export function resetHistory() {
    historyCache = { mmsi: 0, pts: null, changes: null };

    ["hull", "speed", "draught", "changes"].forEach((k) => {
        const body = document.getElementById("targetcard_" + k + "_body");
        if (body) body.innerHTML = "";
        card.section.available(k, true);
    });

    card.section.reset(SECTION_DEFAULTS);
}


export function renderHull(ship) {
    ship = ship || ships[cardMmsi]?.raw;
    const svg = ship ? getShipDimensionSVG(ship, u) : "";

    card.section.available("hull", !!svg);
    const body = document.getElementById("targetcard_hull_body");
    if (body) body.innerHTML = svg;
}

export async function fillSection(key) {
    const mmsi = cardMmsi;
    if (!mmsi) return;

    if (key === "hull") return renderHull();
    if (!["speed", "draught", "changes"].includes(key)) return;

    const body = document.getElementById("targetcard_" + key + "_body");
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

    if (key === "speed") html = getSpeedHistorySVG(historyCache.pts, 10, u);
    else if (key === "draught") html = getDraughtChartSVG(historyCache.changes, ship ? ship.draught : null, u);
    else html = getChangeListHTML(historyCache.changes,
        [CHANGE.DESTINATION, CHANGE.ETA, CHANGE.SHIPNAME, CHANGE.CALLSIGN, CHANGE.STATUS], u);

    body.innerHTML = html || '<span class="dim-note">Nothing recorded yet</span>';
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

export function updateMessageButton() {
    const button = document.getElementById("targetcard_message_option");
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

export function populate() {

    if (cardType != 'ship') return;

    if (!(cardMmsi in ships)) {
        showOutOfRange();
        return;
    }

    let ship = ships[cardMmsi].raw;

    document.getElementById("targetcard_header_flag").innerHTML = flagHTML(ship.country, 'flag-card', getCountryName(ship.country));
    document.getElementById("targetcard_header_title").innerHTML = (getShipName(ship) || ship.mmsi);

    setValidation(ship.validated);

    ["destination", "mmsi", "count", "received_stations"].forEach((e) => (document.getElementById("targetcard_" + e).innerHTML = ship[e] != null ? ship[e] : "-"));

    if (ship.imo != null) {
        document.getElementById("targetcard_imo_label").innerHTML = "IMO";
        document.getElementById("targetcard_imo").innerHTML = ship.imo;
    } else {
        document.getElementById("targetcard_imo_label").innerHTML = ship.eni ? "ENI" : "IMO";
        document.getElementById("targetcard_imo").innerHTML = ship.eni || "-";
    }

    [
        { id: "cog", u: "&deg", d: 1 },
        { id: "bearing", u: "&deg", d: 0 },
        { id: "heading", u: "&deg", d: 0 },
        { id: "level", u: "dB", d: 1 },
        { id: "ppm", u: "ppm", d: 1 },
    ].forEach((el) => (document.getElementById("targetcard_" + el.id).innerHTML = ship[el.id] != null ? Number(ship[el.id]).toFixed(el.d) + " " + el.u : null));

    document.getElementById("targetcard_country").innerHTML = getCountryName(ship.country);
    document.getElementById("targetcard_callsign").innerHTML = getCallSign(ship) || null;
    document.getElementById("targetcard_msgtypes").innerHTML = getStringfromMsgType(ship.msg_type);

    document.getElementById("targetcard_last_group").innerHTML = getStringfromGroup(ship.last_group);
    document.getElementById("targetcard_sources").innerHTML = getStringfromGroup(ship.group_mask);

    document.getElementById("targetcard_channels").innerHTML = getStringfromChannels(ship.channels);
    document.getElementById("targetcard_type").innerHTML = getMmsiTypeVal(ship) + ' <i class="info_icon card-tech-icon" id="targetcard_tech_info" data-action="techInfo" title="Technical details"></i>';
    const shiptype = ship.shiptype ?? null;
    document.getElementById("targetcard_shiptype").innerHTML = shiptype != null
        ? getShipTypeShort(shiptype) + ' <i class="info_icon card-tech-icon" id="targetcard_shiptype_info" data-action="shiptypeInfo" title="Ship type details"></i>'
        : getShipTypeShort(shiptype);
    document.getElementById("shiptype_code").textContent = shiptype != null ? shiptype : "-";
    document.getElementById("shiptype_desc").textContent = shiptype != null
        ? getShipTypeFull(shiptype)
        : "-";
    document.getElementById("targetcard_status").innerHTML = getStatusVal(ship);
    document.getElementById("targetcard_last_signal").innerHTML = getDeltaTimeVal(clock - ship.last_signal);
    document.getElementById("targetcard_eta").innerHTML = ship.eta_month != null && ship.eta_hour != null && ship.eta_day != null && ship.eta_minute != null ? getEtaVal(ship) : null;
    document.getElementById("targetcard_lat").innerHTML = ship.lat != null ? getLatValFormat(ship) : null;
    document.getElementById("targetcard_lon").innerHTML = ship.lon != null ? getLonValFormat(ship) : null;
    document.getElementById("targetcard_altitude").innerHTML = ship.altitude != null ? ship.altitude + " m" : null;

    document.getElementById("targetcard_speed").innerHTML = ship.speed != null ? getSpeedVal(ship.speed) + " " + getSpeedUnit() : null;
    document.getElementById("targetcard_distance").innerHTML = ship.distance != null ? getDistanceVal(ship.distance) + " " + getDistanceUnit() : null;
    document.getElementById("targetcard_repeated").innerHTML = ship.repeat != null ? (ship.repeat > 0 ? "Yes" : "No") : null;
    document.getElementById("targetcard_draught").innerHTML = ship.draught ? getDraughtVal(ship.draught) + " " + getDimUnit() : null;
    document.getElementById("targetcard_dimension").innerHTML = getShipDimension(ship);
    document.getElementById("targetcard_bluesign").innerHTML = ship.maneuver === 2 ? "Set" : (ship.maneuver === 1 ? "Not set" : null);

    updateTrackOption();
    updateFollowOption();
    updateMessageButton();
    updateTechDetails(ship);
    renderHull(ship);

}

