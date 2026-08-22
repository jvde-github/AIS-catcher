// Plane card: the same shell as the ship card, different fields.
// Mirrors features/targetcard.js so both entity types have one shape.

import { planes, planesSince, cardMmsi, cardType } from '../core/store.js';
import {
    CARD_FLAG_STYLE, getCountryName, getDeltaTimeVal, getDistanceUnit, getDistanceVal,
    getFlagStyled, getLatValFormat, getLonValFormat, getSpeedUnit, getSpeedVal,
    getStringfromGroup, getStringfromMsgType, getICAO,
} from '../../shared/core/format.js';
import * as targetcard from './targetcard.js';

let deps = null;

export function init(d) { deps = d; }

const CATEGORY = {
    21: "Emergency", 23: "Service",
    31: "Glider", 32: "LTA", 33: "Parachutist", 34: "Ultralight", 36: "UAV", 37: "Space",
    41: "< 7 MT", 42: "7 - 34 MT", 43: "34 - 136 MT", 44: "High vortex", 45: "> 136 MT",
    46: "High perf", 47: "Rotorcraft",
};

const num = (v, digits, unit) => (v != null ? Number(v).toFixed(digits) + " " + unit : "");

// The card's fields, in the order they are shown. Each one knows its own label
// and how to read itself off a plane, so the markup is built from this list
// rather than repeated in index.html.
const ROWS = [
    {
        cls: "targetcard-max-only", section: { key: "aircraft", label: "Aircraft", open: true }, fields: [
            { key: "callsign", label: "Callsign", get: (p) => p.callsign },
            { key: "country", label: "Country", get: (p) => getCountryName(p.country) },
            { key: "hexident", label: "ICAO", get: (p) => getICAO(p) },
        ]
    },
    {
        cls: "targetcard-max-only", fields: [
            { key: "category", label: "Category", get: (p) => CATEGORY[p.category] || p.category },
            { key: "squawk", label: "Squawk", get: (p) => p.squawk },
        ]
    },
    {
        cls: "targetcard-row-selected", section: { key: "flight", label: "Flight", open: true }, fields: [
            { key: "speed", label: "Speed", get: (p) => (p.speed ? getSpeedVal(p.speed) + " " + getSpeedUnit() : "") },
            { key: "heading", label: "Heading", get: (p) => num(p.heading, 0, "°") },
            { key: "altitude", label: "Altitude", get: (p) => (p.airborne == 1 ? (p.altitude ? p.altitude + " ft" : "") : "on ground") },
        ]
    },
    {
        cls: "targetcard-max-only", fields: [
            { key: "vertrate", label: "Vert Rate", get: (p) => (p.vertrate ? p.vertrate + " ft/min" : "") },
            { key: "lat", label: "Latitude", html: true, get: (p) => (p.lat ? getLatValFormat(p) : "") },
            { key: "lon", label: "Longitude", html: true, get: (p) => (p.lon ? getLonValFormat(p) : "") },
        ]
    },
    {
        // what the receiver saw, not what the aircraft said: distance and
        // bearing are measured from the station, as on the ship card
        cls: "targetcard-max-only", section: { key: "adsb", label: "ADS-B source", open: false }, fields: [
            { key: "distance", label: "Distance", get: (p) => (p.distance != null ? getDistanceVal(p.distance) + " " + getDistanceUnit() : "") },
            { key: "bearing", label: "Bearing", get: (p) => num(p.bearing, 0, "°") },
            { key: "level", label: "RSSI", get: (p) => num(p.level, 1, "dB") },
        ]
    },
    {
        cls: "targetcard-max-only", fields: [
            { key: "last_signal", label: "Last Signal", get: (p) => getDeltaTimeVal(planesSince - p.last_signal) },
            { key: "messages", label: "Messages", get: (p) => p.nMessages },
            { key: "last_group", label: "Receiver", get: (p) => getStringfromGroup(p.last_group) },
        ]
    },
    {
        cls: "targetcard-max-only", fields: [
            { key: "downlink", label: "Downlink", get: (p) => getStringfromMsgType(p.message_types) },
            { key: "TC", label: "TC", get: (p) => getStringfromMsgType(p.message_subtypes) },
        ]
    },
    {
        cls: "targetcard-max-only", fields: [
            { key: "sources", label: "Sources", get: (p) => getStringfromGroup(p.group_mask) },
            { key: "type", label: "Sender", get: () => "ADSB" },
        ]
    },
];

// every row is a three-column grid, so a short one is padded rather than
// letting two fields stretch and fall out of line with the rows around them
for (const row of ROWS) {
    while (row.fields.length < 3) row.fields.push({ spacer: true });
}

// a row group without a header of its own continues the section above it
const SECTIONS = [];
for (const row of ROWS) {
    if (row.section) SECTIONS.push(row.section);
    else if (SECTIONS.length) row.sectionKey = SECTIONS[SECTIONS.length - 1].key;
}

let cells = null;

function build() {
    if (cells) return cells;

    const mount = document.getElementById("targetcard_plane_rows");
    if (!mount) return null;

    cells = window.AISComponents.fieldRows(mount, ROWS, {
        rowClass: "mapcard-content-row targetcard-content-row",
        attrs: { "data-action": "targetcardSelectSelf", "data-context-type": "plane" },
        // the ids are how a pinned row is named in settings, so they outlive
        // the markup they used to be written in
        idPrefix: "targetcard_plane_",
        sectionClass: "targetcard-section targetcard-max-only",
        sectionAction: "toggleTargetcardSection",
        caretChipClass: "targetcard-caret-chip",
        caretClass: "targetcard-section-caret",
        headId: (key) => "targetcard_" + key + "_head",
    });

    // rows the user pinned in an earlier session: loadSettings could not apply
    // them, because these rows did not exist when it ran
    const pinned = new Set((deps && deps.pinnedRows && deps.pinnedRows()) || []);
    mount.querySelectorAll('[data-action="targetcardSelectSelf"]').forEach((row) => {
        const cell = row.querySelector("[id]");
        const on = !!cell && pinned.has(cell.id);
        row.classList.toggle("targetcard-max-only", !on);
        row.classList.toggle("targetcard-row-selected", on);
    });

    // both passes ran before these rows existed: the min/max one at startup and
    // the section one when the card was opened. The classes above are written
    // as a maximised card states them, so a minimised one is normalised here:
    // rows it drops are hidden, and the edge marking the rows it keeps belongs
    // to the maximised view only.
    if (deps && !deps.isMax()) {
        mount.querySelectorAll(".targetcard-max-only").forEach((r) => r.classList.add("hidden"));
        mount.querySelectorAll(".targetcard-row-selected")
            .forEach((r) => r.classList.remove("targetcard-row-selected"));
    }
    SECTIONS.forEach((s) => targetcard.applySection(s.key));
    return cells;
}

export function populate() {

    if (cardType != 'plane') return;

    if (!(cardMmsi in planes)) {
        targetcard.showOutOfRange();
        return;
    }

    const plane = planes[cardMmsi].raw;
    const cell = build();
    if (!cell) return;

    targetcard.setValidation(plane.validated);

    document.getElementById("targetcard_header_title").textContent = (plane.callsign || getICAO(plane));
    document.getElementById("targetcard_header_flag").innerHTML = getFlagStyled(plane.country, CARD_FLAG_STYLE);

    for (const row of ROWS) {
        for (const field of row.fields) {
            if (field.spacer) continue;
            const value = field.get(plane);
            const text = value == null ? "" : String(value);
            if (field.html) cell[field.key].innerHTML = text;
            else cell[field.key].textContent = text;
        }
    }

    targetcard.updateTrackOption();
}
