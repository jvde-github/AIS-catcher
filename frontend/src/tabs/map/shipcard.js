import { settings } from '../../settings.js';
import { showNotification, showDialog } from '../../ui.js';
import {
    getFlagStyled, getCountryName, getShipName, getCallSign, getICAO,
    getStatusVal, getTypeVal, getShipTypeVal,
    getStringfromMsgType, getStringfromGroup, getStringfromChannels,
    getDeltaTimeVal, getDistanceVal, getDistanceUnit,
    getSpeedVal, getSpeedUnit, getDimVal, getDimUnit, getShipDimension,
    getLatValFormat, getLonValFormat, getEtaVal,
    getICAOfromHexIdent,
} from '../../format.js';
import { shipsDB, planesDB, binaryDB } from './state.js';

// ── Module state ──────────────────────────────────────────────────────────────

let deps = {};
export let card_mmsi = null;
export let card_type = null;

const shipcardIconMax = 3;
let shipcardIconCount  = { ship: 0, plane: 0 };
let shipcardIconOffset = { ship: 0, plane: 0 };
let selectCircleFeature = undefined;

// ── OL select-circle style ────────────────────────────────────────────────────

function selectCircleStyleFunction() {
    const iconScale   = settings.icon_scale   || 1.0;
    const circleScale = settings.circle_scale || 6.0;
    const radiusScale = 1 + (circleScale - 2.0) * 0.08;
    return new ol.style.Style({
        image: new ol.style.Circle({
            radius: 13 * iconScale * radiusScale,
            stroke: new ol.style.Stroke({
                color: settings.shipselection_color,
                width: circleScale * iconScale
            })
        })
    });
}

// ── Focus / selection marker ──────────────────────────────────────────────────

export function updateFocusMarker() {
    const extraVector = deps.extraVector;

    if (selectCircleFeature) {
        if (card_type === 'ship' && card_mmsi in shipsDB && shipsDB[card_mmsi].raw.lon && shipsDB[card_mmsi].raw.lat) {
            const c = ol.proj.fromLonLat([shipsDB[card_mmsi].raw.lon, shipsDB[card_mmsi].raw.lat]);
            selectCircleFeature.setGeometry(new ol.geom.Point(c));
            return;
        } else if (card_type === 'plane' && card_mmsi in planesDB && planesDB[card_mmsi].raw.lon && planesDB[card_mmsi].raw.lat) {
            const c = ol.proj.fromLonLat([planesDB[card_mmsi].raw.lon, planesDB[card_mmsi].raw.lat]);
            selectCircleFeature.setGeometry(new ol.geom.Point(c));
            return;
        } else {
            extraVector.removeFeature(selectCircleFeature);
            selectCircleFeature = undefined;
            return;
        }
    }

    if (card_type === 'ship' && card_mmsi in shipsDB && shipsDB[card_mmsi].raw.lon && shipsDB[card_mmsi].raw.lat) {
        const c = ol.proj.fromLonLat([shipsDB[card_mmsi].raw.lon, shipsDB[card_mmsi].raw.lat]);
        selectCircleFeature = new ol.Feature(new ol.geom.Point(c));
        selectCircleFeature.setStyle(selectCircleStyleFunction);
        selectCircleFeature.mmsi = card_mmsi;
        extraVector.addFeature(selectCircleFeature);
    } else if (card_type === 'plane' && card_mmsi in planesDB && planesDB[card_mmsi].raw.lon && planesDB[card_mmsi].raw.lat) {
        const c = ol.proj.fromLonLat([planesDB[card_mmsi].raw.lon, planesDB[card_mmsi].raw.lat]);
        selectCircleFeature = new ol.Feature(new ol.geom.Point(c));
        selectCircleFeature.setStyle(selectCircleStyleFunction);
        selectCircleFeature.mmsi = card_mmsi;
        extraVector.addFeature(selectCircleFeature);
    }
}

// ── Shipcard visibility / size ────────────────────────────────────────────────

export function shipcardVisible() {
    return document.getElementById("shipcard").classList.contains("visible");
}

export function measurecardVisible() {
    return document.getElementById("measurecard").classList.contains("visible");
}

export function toggleMeasurecard() {
    if (shipcardVisible() && !measurecardVisible()) showShipcard(null, null);
    document.getElementById("measurecard").classList.toggle("visible");
}

export function isShipcardMax() {
    return document.getElementById("shipcard").classList.contains("shipcard-ismax");
}

export function shipcardMinIfMaxonMobile() {
    if (shipcardVisible() && window.matchMedia("(max-height: 1000px) and (max-width: 500px)").matches && isShipcardMax()) {
        toggleShipcardSize();
    }
}

export function toggleShipcardSize() {
    Array.from(document.getElementsByClassName("shipcard-min-only")).forEach(e => e.classList.toggle("visible"));
    Array.from(document.getElementsByClassName("shipcard-max-only")).forEach(e => e.classList.toggle("hide"));

    document.getElementById("shipcard").classList.toggle("shipcard-ismax");
    document.getElementById("shipcard_minmax_button").classList.toggle("keyboard_arrow_down_icon");
    document.getElementById("shipcard_minmax_button").classList.toggle("keyboard_arrow_up_icon");

    const e = document.getElementById("shipcard_content").children;

    if (isShipcardMax()) {
        for (let i = 0; i < e.length; i++) {
            if (
                (e[i].classList.contains("shipcard-max-only") && e[i].classList.contains("shipcard-row-selected")) ||
                (!e[i].classList.contains("shipcard-max-only") && !e[i].classList.contains("shipcard-row-selected"))
            ) e[i].classList.toggle("shipcard-row-selected");

            const aside = document.getElementById("shipcard");
            if (aside.style.top && aside.getBoundingClientRect().bottom > window.innerHeight) {
                if (card_mmsi in shipsDB && card_type === "ship") {
                    const pixel = deps.map.getPixelFromCoordinate(ol.proj.fromLonLat([shipsDB[card_mmsi].raw.lon, shipsDB[card_mmsi].raw.lat]));
                    positionAside(pixel, aside);
                } else if (card_mmsi in planesDB && card_type === "plane") {
                    const pixel = deps.map.getPixelFromCoordinate(ol.proj.fromLonLat([planesDB[card_mmsi].raw.lon, planesDB[card_mmsi].raw.lat]));
                    positionAside(pixel, aside);
                }
            }
        }
    } else {
        for (let i = 0; i < e.length; i++) {
            if (e[i].classList.contains("shipcard-row-selected")) e[i].classList.toggle("shipcard-row-selected");
        }
    }
}

export function shipcardselect() {
    if (isShipcardMax()) {
        this.classList.toggle("shipcard-max-only");
        this.classList.toggle("shipcard-row-selected");
    } else {
        toggleShipcardSize();
    }
    deps.saveSettings();
}

// ── Shipcard position ─────────────────────────────────────────────────────────

function adjustMapForShipcard(pixel) {
    let lat = null, lon = null;
    if (card_type === 'ship' && card_mmsi in shipsDB) {
        lat = shipsDB[card_mmsi].raw.lat;
        lon = shipsDB[card_mmsi].raw.lon;
    } else if (card_type === 'plane' && card_mmsi in planesDB) {
        lat = planesDB[card_mmsi].raw.lat;
        lon = planesDB[card_mmsi].raw.lon;
    }

    if (lat && lon) {
        const map = deps.map;
        const view = map.getView();
        const currentExtent = view.calculateExtent(map.getSize());
        const shipCoords = ol.proj.fromLonLat([lon, lat]);

        if (!ol.extent.containsCoordinate(currentExtent, shipCoords)) {
            view.animate({ center: shipCoords, duration: 1000 });
            return;
        }

        if (!pixel) pixel = map.getPixelFromCoordinate(shipCoords);

        const shipcardEl = document.getElementById("shipcard");
        const shipcardRect = shipcardEl.getBoundingClientRect();
        const mapRect = map.getTargetElement().getBoundingClientRect();

        const isUnderShipcard = (
            pixel[0] + mapRect.left >= shipcardRect.left &&
            pixel[0] + mapRect.left <= shipcardRect.right &&
            pixel[1] + mapRect.top  >= shipcardRect.top &&
            pixel[1] + mapRect.top  <= shipcardRect.bottom
        );

        if (isUnderShipcard) {
            deps.moveMapCenter(pixel);
        }
    }
}

function positionAside(pixel, aside) {
    deps.stopHover();

    if (settings.kiosk && settings.kiosk_pan_map && card_type === 'ship' && card_mmsi in shipsDB) {
        deps.moveMapCenter(pixel);
        const mapSize = deps.map.getSize();
        pixel = [mapSize[0] / 2, mapSize[1] / 2];
    }

    if (settings.shipcard_pinned && settings.shipcard_pinned_x !== null && settings.shipcard_pinned_y !== null) {
        aside.style.left = `${settings.shipcard_pinned_x}px`;
        aside.style.top  = `${settings.shipcard_pinned_y}px`;
        return;
    }

    aside.style.left = "";
    aside.style.top  = "";

    if (pixel) {
        const margin      = 35;
        const marginRight = document.getElementById("tableside").classList.contains("active") ? 592 : 30;
        const mapSize     = deps.map.getSize();
        const shipCardRect   = aside.getBoundingClientRect();
        const shipCardWidth  = shipCardRect.width;
        const shipCardHeight = shipCardRect.height;

        const rightSpace = mapSize[0] - (pixel[0] + shipCardWidth + margin + marginRight);
        const leftSpace  = pixel[0] - (shipCardWidth + margin);

        if ((rightSpace > 0 || leftSpace > 0) && mapSize[1] > shipCardHeight + 2 * margin) {
            let topPosition = pixel[1] - (shipCardHeight / 2);
            topPosition = Math.max(margin, Math.min(mapSize[1] - shipCardHeight - margin, topPosition));
            aside.style.top = `${topPosition}px`;

            if (rightSpace >= 0) {
                aside.style.left = `${pixel[0] + margin}px`;
            } else if (leftSpace >= 0) {
                aside.style.left = `${pixel[0] - shipCardWidth - margin}px`;
            } else {
                aside.style.left = `${(mapSize[0] - shipCardWidth) / 2}px`;
            }
        }
    }
    adjustMapForShipcard(pixel);
}

// ── Shipcard pin / unpin ──────────────────────────────────────────────────────

export function pinShipcard() {
    const shipcard = document.getElementById("shipcard");
    settings.shipcard_pinned   = true;
    settings.shipcard_pinned_x = parseInt(shipcard.style.left) || 0;
    settings.shipcard_pinned_y = parseInt(shipcard.style.top)  || 0;
    applyShipcardPinStyling();
    showNotification("Shipcard pinned to current position");
    deps.saveSettings();
}

export function unpinShipcard() {
    settings.shipcard_pinned   = false;
    settings.shipcard_pinned_x = null;
    settings.shipcard_pinned_y = null;
    applyShipcardPinStyling();
    showNotification("Shipcard unpinned");
    deps.saveSettings();
}

function applyShipcardPinStyling() {
    const shipcard = document.getElementById("shipcard");
    if (settings.shipcard_pinned) {
        shipcard.classList.add("pinned");
        document.getElementById("shipcard_drag_handle").classList.add("opacity-25");
    } else {
        shipcard.classList.remove("pinned");
        document.getElementById("shipcard_drag_handle").classList.remove("opacity-25");
    }
}

export function toggleShipcardPin() {
    if (settings.shipcard_pinned) unpinShipcard();
    else pinShipcard();
}

// ── Track options ─────────────────────────────────────────────────────────────

export function trackOptionString(mmsi) {
    const select_track = card_type === 'ship' && mmsi === card_mmsi && deps.isSelectTrackEnabled();
    const track_shown  = deps.trackIsShown(mmsi);
    if (select_track) return "Show Track";
    return track_shown ? "Hide Track" : "Show Track";
}

export function updateShipcardTrackOption() {
    const trackOptionElement = document.getElementById("shipcard_track_option");
    if (deps.isShowAllTracks() || card_type === 'plane') {
        trackOptionElement.style.opacity = "0.5";
        trackOptionElement.style.pointerEvents = "none";
    } else {
        trackOptionElement.style.opacity = "1";
        trackOptionElement.style.pointerEvents = "auto";
    }
    if (card_mmsi && card_type === 'ship') {
        document.getElementById("shipcard_track").innerText = trackOptionString(card_mmsi);
    }
}

// ── Shipcard icons ────────────────────────────────────────────────────────────

export function addShipcardItem(icon, txt, title, onclick, contextType = 'ship') {
    const div = document.createElement("div");
    div.title = title;
    div.setAttribute("data-context-type", contextType);
    if (icon.startsWith("fa")) icon = "question_mark";
    div.innerHTML = '<i class="' + icon + '_icon"></i><span>' + txt + "</span>";
    div.setAttribute("onclick", onclick);
    document.getElementById("shipcard_footer").appendChild(div);
}

export function displayShipcardIcons(type) {
    const icons = document.querySelectorAll('#shipcard_footer > div');
    let idx = 0;

    for (const icon of icons) {
        if (icon.dataset.contextType !== type && icon.dataset.contextType) {
            icon.style.display = "none";
            continue;
        }

        const isMoreButton = icon.querySelector('i')?.classList.contains('more_horiz_icon');
        if (isMoreButton) { icon.style.display = "flex"; continue; }

        const isRealtimeDisabled = icon.id === 'shipcard_realtime_option' &&
            (typeof realtime_enabled === "undefined" || realtime_enabled === false);

        if (isRealtimeDisabled) {
            icon.style.display = "none";
        } else {
            const isInRange = idx >= shipcardIconOffset[type] && idx < shipcardIconOffset[type] + shipcardIconMax;
            icon.style.display = isInRange ? "flex" : "none";
            idx++;
        }
    }
}

export function rotateShipcardIcons() {
    shipcardIconOffset[card_type] += shipcardIconMax;
    if (shipcardIconOffset[card_type] >= shipcardIconCount[card_type]) {
        shipcardIconOffset[card_type] = 0;
    }
    displayShipcardIcons(card_type);
}

export function prepareShipcard() {
    shipcardIconCount.ship  = document.querySelectorAll('#shipcard_footer > div[data-context-type="ship"]').length;
    shipcardIconCount.plane = document.querySelectorAll('#shipcard_footer > div[data-context-type="plane"]').length;

    if (typeof realtime_enabled === "undefined" || realtime_enabled === false) {
        const realtimeOption = document.getElementById('shipcard_realtime_option');
        if (realtimeOption && realtimeOption.dataset.contextType === 'ship') {
            shipcardIconCount.ship--;
        }
    }

    if (shipcardIconCount.ship > shipcardIconMax) {
        addShipcardItem('more_horiz', 'More', 'More options', 'rotateShipcardIcons()', 'ship');
    }
    if (shipcardIconCount.plane > shipcardIconMax) {
        addShipcardItem('more_horiz', 'More', 'More options', 'rotateShipcardIcons()', 'plane');
    }

    displayShipcardIcons('ship');
}

// ── Shipcard validation & tech details ────────────────────────────────────────

function setShipcardValidation(v) {
    document.getElementById("shipcard_header").classList.remove("shipcard-validated", "shipcard-not-validated", "shipcard-dubious");
    if (v === 1) document.getElementById("shipcard_header").classList.add("shipcard-validated");
    else if (v === -1) document.getElementById("shipcard_header").classList.add("shipcard-dubious");
    else document.getElementById("shipcard_header").classList.add("shipcard-not-validated");
}

function updateMessageButton() {
    const messageButton = document.querySelector('#shipcard_footer [onclick="showBinaryMessageDialog(card_mmsi)"]');
    if (!messageButton) return;
    const iconElement = messageButton.querySelector('i.mail_icon');
    if (!iconElement) return;

    const hasBinaryMsgs = card_mmsi && binaryDB && card_mmsi in binaryDB &&
        binaryDB[card_mmsi].ship_messages && binaryDB[card_mmsi].ship_messages.length > 0;

    const existingBadge = iconElement.querySelector('.message-badge');
    if (existingBadge) existingBadge.remove();

    if (hasBinaryMsgs) {
        const count = binaryDB[card_mmsi].ship_messages.length;
        messageButton.style.display = '';
        const badge = document.createElement('span');
        badge.className = 'message-badge';
        badge.textContent = count;
        iconElement.appendChild(badge);
    } else {
        messageButton.style.display = 'none';
    }
}

function updateTechDetails(ship) {
    const formatFlag = (value, trueText = "Yes", falseText = "No", unknownText = "-") => {
        if (value === 0) return unknownText;
        if (value === 1) return falseText;
        if (value === 2) return trueText;
        return unknownText;
    };

    document.getElementById("tech_raim").textContent       = formatFlag(ship.raim);
    document.getElementById("tech_dte").textContent        = formatFlag(ship.dte, "Not Ready", "Ready");
    document.getElementById("tech_assigned").textContent   = formatFlag(ship.assigned, "Assigned", "Autonomous");
    document.getElementById("tech_display").textContent    = formatFlag(ship.display);
    document.getElementById("tech_dsc").textContent        = formatFlag(ship.dsc);
    document.getElementById("tech_band").textContent       = formatFlag(ship.band, "Dual", "Single");
    document.getElementById("tech_msg22").textContent      = formatFlag(ship.msg22);
    document.getElementById("tech_off_position").textContent = formatFlag(ship.off_position, "Off", "On");
    document.getElementById("tech_maneuver").textContent   = ship.maneuver === 0 ? "-" : ship.maneuver === 1 ? "None" : "Special";
}

export function toggleTechPopover() {
    const popover = document.getElementById("tech_popover");
    const icon    = document.getElementById("shipcard_tech_info");

    if (popover.style.display === "none" || !popover.style.display) {
        const iconRect     = icon.getBoundingClientRect();
        const shipcardRect = document.getElementById("shipcard").getBoundingClientRect();

        popover.style.display = "block";

        let left = iconRect.left - shipcardRect.left + 20;
        let top  = iconRect.bottom - shipcardRect.top + 5;

        const popoverRect   = popover.getBoundingClientRect();
        const viewportWidth  = window.innerWidth;
        const viewportHeight = window.innerHeight;

        if (iconRect.left + popoverRect.width + 20 > viewportWidth) {
            left = Math.max(5, iconRect.right - shipcardRect.left - popoverRect.width - 5);
        }
        if (iconRect.bottom + popoverRect.height + 5 > viewportHeight) {
            top = Math.max(5, iconRect.top - shipcardRect.top - popoverRect.height - 5);
        }

        left = Math.max(5, Math.min(left, shipcardRect.width - popoverRect.width - 5));
        top  = Math.max(5, top);

        popover.style.left = left + "px";
        popover.style.top  = top + "px";

        setTimeout(() => document.addEventListener("click", closeTechPopover, true), 0);
    } else {
        popover.style.display = "none";
        document.removeEventListener("click", closeTechPopover, true);
    }
}

function closeTechPopover(event) {
    const popover = document.getElementById("tech_popover");
    const icon    = document.getElementById("shipcard_tech_info");
    event.stopPropagation();
    event.preventDefault();
    if (!popover.contains(event.target) && event.target !== icon && !icon?.contains(event.target)) {
        popover.style.display = "none";
        document.removeEventListener("click", closeTechPopover, true);
    }
}

// ── Populate shipcard / planecard ─────────────────────────────────────────────

export function populateShipcard() {
    if (card_type !== 'ship') return;

    if (!(card_mmsi in shipsDB)) {
        document.getElementById("shipcard_content")
            .querySelectorAll("span:nth-child(2)")
            .forEach(e => { e.innerHTML = null; });
        document.getElementById("shipcard_header_title").innerHTML = "<b style='color:red;'>Out of range</b>";
        document.getElementById("shipcard_header_flag").innerHTML  = "";
        document.getElementById("shipcard_mmsi").innerHTML         = card_mmsi;
        updateFocusMarker();
        return;
    }

    const ship = shipsDB[card_mmsi].raw;

    document.getElementById("shipcard_header_flag").innerHTML  = getFlagStyled(ship.country, "padding: 0px; margin: 0px; margin-right: 5px; box-shadow: 2px 2px 3px rgba(0, 0, 0, 0.5); font-size: 26px;");
    document.getElementById("shipcard_header_title").innerHTML = (getShipName(ship) || ship.mmsi);

    setShipcardValidation(ship.validated);

    ["destination", "mmsi", "count", "imo", "received_stations"].forEach(e => {
        document.getElementById("shipcard_" + e).innerHTML = ship[e];
    });

    [
        { id: "cog",     u: "&deg", d: 0 },
        { id: "bearing", u: "&deg", d: 0 },
        { id: "heading", u: "&deg", d: 0 },
        { id: "level",   u: "dB",   d: 1 },
        { id: "ppm",     u: "ppm",  d: 1 },
    ].forEach(el => {
        document.getElementById("shipcard_" + el.id).innerHTML = ship[el.id] ? Number(ship[el.id]).toFixed(el.d) + " " + el.u : null;
    });

    document.getElementById("shipcard_country").innerHTML    = getCountryName(ship.country);
    document.getElementById("shipcard_callsign").innerHTML   = getCallSign(ship);
    document.getElementById("shipcard_msgtypes").innerHTML   = getStringfromMsgType(ship.msg_type);
    document.getElementById("shipcard_last_group").innerHTML = getStringfromGroup(ship.last_group);
    document.getElementById("shipcard_sources").innerHTML    = getStringfromGroup(ship.group_mask);
    document.getElementById("shipcard_channels").innerHTML   = getStringfromChannels(ship.channels);
    document.getElementById("shipcard_type").innerHTML       = getTypeVal(ship) + ' <i class="info_icon shipcard-tech-icon" id="shipcard_tech_info" onclick="event.stopPropagation(); toggleTechPopover()" title="Technical details"></i>';
    document.getElementById("shipcard_shiptype").innerHTML   = getShipTypeVal(ship.shiptype);
    document.getElementById("shipcard_status").innerHTML     = getStatusVal(ship);
    document.getElementById("shipcard_last_signal").innerHTML = getDeltaTimeVal(ship.last_signal);
    document.getElementById("shipcard_eta").innerHTML        = ship.eta_month != null && ship.eta_hour != null && ship.eta_day != null && ship.eta_minute != null ? getEtaVal(ship) : null;
    document.getElementById("shipcard_lat").innerHTML        = ship.lat ? getLatValFormat(ship) : null;
    document.getElementById("shipcard_lon").innerHTML        = ship.lon ? getLonValFormat(ship) : null;
    document.getElementById("shipcard_altitude").innerHTML   = ship.altitude ? ship.altitude + " m" : null;
    document.getElementById("shipcard_speed").innerHTML      = ship.speed ? getSpeedVal(ship.speed) + " " + getSpeedUnit() : null;
    document.getElementById("shipcard_distance").innerHTML   = ship.distance ? (getDistanceVal(ship.distance) + " " + getDistanceUnit() + (ship.repeat > 0 ? " (R)" : "")) : null;
    document.getElementById("shipcard_draught").innerHTML    = ship.draught ? getDimVal(ship.draught) + " " + getDimUnit() : null;
    document.getElementById("shipcard_dimension").innerHTML  = getShipDimension(ship);

    updateShipcardTrackOption();
    updateMessageButton();
    updateTechDetails(ship);
}

function getCategory(plane) {
    if (!plane || !plane.category) return "-";
    const categories = {
        41: "< 7 MT", 42: "7 - 34 MT", 43: "34 - 136 MT", 44: "High vortex",
        45: "> 136 MT", 46: "High perf", 47: "Rotorcraft",
        31: "Glider", 32: "LTA", 33: "Parachutist", 34: "Ultralight",
        36: "UAV", 37: "Space", 21: "Emergency", 23: "Service"
    };
    return categories[plane.category] || plane.category.toString() || "-";
}

export function populatePlanecard() {
    if (card_type !== 'plane') return;

    document.getElementById("shipcard_content")
        .querySelectorAll("span:nth-child(2)")
        .forEach(e => { e.innerHTML = null; });

    if (!(card_mmsi in planesDB)) {
        document.getElementById("shipcard_header_title").innerHTML = "<b style='color:red;'>Out of range</b>";
        document.getElementById("shipcard_header_flag").innerHTML  = "";
        document.getElementById("shipcard_mmsi").innerHTML         = card_mmsi;
        updateFocusMarker();
        return;
    }

    const plane = planesDB[card_mmsi].raw;

    setShipcardValidation(plane.validated);

    document.getElementById("shipcard_header_title").textContent     = (plane.callsign || getICAO(plane));
    document.getElementById("shipcard_header_flag").innerHTML        = getFlagStyled(plane.country, "padding: 0px; margin: 0px; margin-right: 5px; box-shadow: 2px 2px 3px rgba(0, 0, 0, 0.5); font-size: 26px;");
    document.getElementById("shipcard_plane_country").innerHTML      = getCountryName(plane.country);
    document.getElementById("shipcard_plane_type").innerHTML         = "ADSB";
    document.getElementById("shipcard_plane_callsign").textContent   = plane.callsign || "-";
    document.getElementById("shipcard_plane_hexident").textContent   = getICAO(plane);
    document.getElementById("shipcard_plane_category").textContent   = getCategory(plane);
    document.getElementById("shipcard_plane_squawk").textContent     = plane.squawk || "-";
    document.getElementById("shipcard_plane_speed").innerHTML        = plane.speed ? getSpeedVal(plane.speed) + " " + getSpeedUnit() : null;
    document.getElementById("shipcard_plane_altitude").textContent   = plane.airborne === 1 ? (plane.altitude ? `${plane.altitude} ft` : "-") : "on ground";
    document.getElementById("shipcard_plane_lat").innerHTML          = plane.lat ? getLatValFormat(plane) : null;
    document.getElementById("shipcard_plane_lon").innerHTML          = plane.lon ? getLonValFormat(plane) : null;
    document.getElementById("shipcard_plane_vertrate").textContent   = plane.vertrate ? `${plane.vertrate} ft/min` : "-";
    document.getElementById("shipcard_plane_last_signal").textContent = getDeltaTimeVal(plane.last_signal);
    document.getElementById("shipcard_plane_messages").textContent   = plane.nMessages || "-";
    document.getElementById("shipcard_plane_downlink").textContent   = getStringfromMsgType(plane.message_types);
    document.getElementById("shipcard_plane_TC").textContent         = getStringfromMsgType(plane.message_subtypes);
    document.getElementById("shipcard_plane_distance").innerHTML     = plane.distance ? (getDistanceVal(plane.distance) + " " + getDistanceUnit()) : null;
    document.getElementById("shipcard_plane_last_group").innerHTML   = getStringfromGroup(plane.last_group);
    document.getElementById("shipcard_plane_sources").innerHTML      = getStringfromGroup(plane.group_mask);

    [
        { id: "heading", u: "&deg", d: 0 },
        { id: "level",   u: "dB",   d: 1 },
        { id: "bearing", u: "&deg", d: 0 },
    ].forEach(el => {
        document.getElementById("shipcard_plane_" + el.id).innerHTML = plane[el.id] ? Number(plane[el.id]).toFixed(el.d) + " " + el.u : null;
    });

    updateShipcardTrackOption();
}

// ── Show / close shipcard ─────────────────────────────────────────────────────

export function showShipcard(type, m, pixel = undefined) {
    const aside   = document.getElementById("shipcard");
    const visible = shipcardVisible();

    const ship_old_mmsi  = card_mmsi;
    const ship_old_type  = card_type;

    if (deps.isSelectTrackEnabled() && (card_mmsi !== m || m == null)) {
        deps.setSelectTrackEnabled(false);
        if (!(card_mmsi === deps.getHoverMMSI() && deps.isHoverTrackEnabled() && deps.getHoverType() === 'ship')) {
            deps.hideTrack(card_mmsi);
        }
    }

    if (m != null && !visible) {
        if (measurecardVisible()) toggleMeasurecard();
        aside.classList.toggle("visible");
        deps.setSelectTrackEnabled(false);
    } else if (visible && m == null) {
        aside.classList.toggle("visible");
    }

    if (type !== card_type) {
        document.querySelectorAll('#shipcard_content [data-context-type]').forEach(element => {
            element.style.display = element.dataset.contextType === type ? '' : 'none';
        });
        displayShipcardIcons(type);
    }

    card_mmsi = m;
    card_type = type;

    if (shipcardVisible()) {
        if (settings.show_track_on_select && card_type === 'ship') {
            if (deps.getHoverMMSI() === m && deps.isHoverTrackEnabled() && deps.getHoverType() === 'ship') {
                deps.setHoverTrackEnabled(false);
                deps.setSelectTrackEnabled(true);
            } else if (!deps.trackIsShown(m)) {
                deps.setSelectTrackEnabled(true);
                deps.showTrack(m);
            }
        }

        if (isShipcardMax()) toggleShipcardSize();
        if (!visible) shipcardMinIfMaxonMobile();
        positionAside(pixel, aside);

        if (card_type === 'ship')  populateShipcard();
        else if (card_type === 'plane') populatePlanecard();

        aside.style.display = 'none';
        aside.offsetHeight;
        aside.style.display = '';
    }

    deps.trackLayer.changed();
    updateFocusMarker();
}

export function closeShipcard() { showShipcard(null, null); }

// ── Binary message dialogs ────────────────────────────────────────────────────

function getBinaryMessageContent(binary, showMultiple = false) {
    const messages = Array.isArray(binary) ? binary : [binary];
    if (messages.length === 0) return '';
    messages.sort((a, b) => b.timestamp - a.timestamp);
    const messagesToShow = showMultiple ? Math.min(messages.length, 3) : 1;

    let content = '<div class="meteo-tooltip" style="max-width: 320px;">';

    for (let i = 0; i < messagesToShow; i++) {
        const msg = messages[i].message;
        if (i > 0) content += '<hr style="margin: 10px 0; border: 0; border-top: 1px solid rgba(255,255,255,0.3);">';

        const hasHydroData = ['watercurrent','currentspeed','currentdir','watertemp','waterlevel'].some(k => k in msg && msg[k] != null);
        content += `<h4 style="margin: 5px 0; font-size: 12px; color: #eee;">${messages[i].formattedTime} - ${hasHydroData ? 'Meteo & Hydro' : 'Meteo'}`;
        if (messagesToShow > 1) content += ` (${i + 1}/${messagesToShow})`;
        content += '</h4><div style="display: grid; grid-template-columns: 1fr 1fr; grid-gap: 5px;">';

        const row = (label, value) => `<div style="display:flex;justify-content:space-between;padding:3px;"><div style="font-size:11px;color:#ccc;">${label}:</div><div style="font-size:11px;font-weight:bold;">${value}</div></div>`;

        if (msg.wspeed != null) {
            let v = msg.wspeed.toFixed(1) + ' knots';
            if (msg.wdir != null && msg.wdir !== 360) v += ' from ' + msg.wdir + '&deg';
            content += row('Wind', v);
        }
        if (msg.airtemp != null)   content += row('Air Temp', msg.airtemp.toFixed(1) + '&deg C');
        if (msg.pressure != null) {
            let v = msg.pressure.toFixed(1) + ' hPa';
            if (msg.pressuretend != null) v += ' (' + ['steady','decreasing','increasing'][msg.pressuretend] + ')';
            content += row('Pressure', v);
        }
        const currentSpeed = msg.watercurrent || msg.currentspeed;
        const currentDir   = msg.currentdir   || msg.currentdirection;
        if (currentSpeed != null) {
            let v = currentSpeed.toFixed(1) + ' knots';
            if (currentDir != null && currentDir !== 360) v += ' to ' + currentDir + '&deg';
            content += row('Current', v);
        }
        if (msg.waterlevel != null) content += row('Water Level', msg.waterlevel.toFixed(2) + ' m');
        if (msg.watertemp  != null) content += row('Water Temp',  msg.watertemp.toFixed(1)  + '&deg C');
        if (msg.waveheight != null) {
            let v = msg.waveheight.toFixed(1) + ' m';
            if (msg.wavedir != null && msg.wavedir !== 360) v += ' from ' + msg.wavedir + '&deg';
            if (msg.waveperiod != null) v += ', ' + msg.waveperiod + 's';
            content += row('Wave', v);
        }
        if (msg.swellheight != null) {
            let v = msg.swellheight.toFixed(1) + ' m';
            if (msg.swelldir != null && msg.swelldir !== 360) v += ' from ' + msg.swelldir + '&deg';
            if (msg.swellperiod != null) v += ', ' + msg.swellperiod + 's';
            content += row('Swell', v);
        }
        if (msg.visibility != null) content += row('Visibility', msg.visibility.toFixed(1) + ' nm');

        content += '</div>';
    }
    content += '</div>';
    return content;
}

function getBinaryMessageList(messages) {
    if (!messages || messages.length === 0) return "<p>No messages available</p>";

    const sortedMessages = [...messages].sort((a, b) => b.timestamp - a.timestamp);
    let content = '<div class="binary-messages-list">';
    content += `<div class="binary-message-count">${sortedMessages.length} message${sortedMessages.length > 1 ? 's' : ''} available</div>`;

    sortedMessages.forEach((msg, index) => {
        if (index > 0) content += '<hr style="margin: 15px 0; border: 0; border-top: 1px solid rgba(0,0,0,0.1);">';
        content += '<div class="binary-message-item">';
        content += `<div class="binary-message-header"><span class="binary-message-time">${msg.formattedTime || new Date(msg.timestamp * 1000).toLocaleTimeString()}</span>`;

        if (msg.message && msg.message.mmsi) {
            const shipName = msg.message.mmsi in shipsDB
                ? (shipsDB[msg.message.mmsi].raw.shipname || `MMSI ${msg.message.mmsi}`)
                : `MMSI ${msg.message.mmsi}`;
            content += `<span class="binary-message-source"> from ${shipName}</span>`;
        }
        content += '</div>';

        if (msg.message && msg.message.dac === 1 && (msg.message.fid === 31 || msg.message.fi === 31)) {
            content += getBinaryMessageContent(msg, false);
        } else {
            content += '<div class="binary-message-details">';
            content += `<div><strong>Message Type:</strong> ${msg.message ? `DAC ${msg.message.dac}, FI ${msg.message.fid || msg.message.fi}` : 'Unknown'}</div>`;
            content += `<details class="binary-raw-data"><summary>Show Raw Data</summary><pre>${JSON.stringify(msg.message, null, 2)}</pre></details>`;
            content += '</div>';
        }
        content += '</div>';
    });

    content += '</div>';
    return content;
}

export function showBinaryMessageDialog(featureOrMmsi) {
    let title, content;

    if (typeof featureOrMmsi === 'object') {
        if (!featureOrMmsi.binary_messages || featureOrMmsi.binary_messages.length === 0) {
            showDialog("Binary Message", "No message content available");
            return;
        }
        title   = "Binary Messages";
        content = getBinaryMessageList(featureOrMmsi.binary_messages);
    } else if (typeof featureOrMmsi === 'number' || typeof featureOrMmsi === 'string') {
        const mmsi = Number(featureOrMmsi);
        if (!binaryDB[mmsi] || !binaryDB[mmsi].ship_messages || binaryDB[mmsi].ship_messages.length === 0) {
            showDialog("Binary Messages", "No binary messages available for this vessel");
            return;
        }
        const shipName = mmsi in shipsDB ? (shipsDB[mmsi].raw.shipname || `MMSI ${mmsi}`) : `MMSI ${mmsi}`;
        title   = `Binary Messages for ${shipName}`;
        content = getBinaryMessageList(binaryDB[mmsi].ship_messages);
    }

    showDialog(title, content);
}

// ── Public API ────────────────────────────────────────────────────────────────

export function init(dependencies) {
    deps = dependencies;

    // Expose functions that are still referenced via HTML onclick="" attributes
    window.rotateShipcardIcons = rotateShipcardIcons;
    window.toggleShipcardSize  = toggleShipcardSize;
    window.toggleTechPopover   = toggleTechPopover;
    window.shipcardselect      = shipcardselect;
    window.showBinaryMessageDialog = showBinaryMessageDialog;

    applyShipcardPinStyling();
    prepareShipcard();
}
