// Plane card: the same shell as the ship card, different fields.
// Mirrors features/targetcard.js so both entity types have one shape.

import { planes, planesSince, cardMmsi, cardType } from '../core/store.js';
import { getDistanceUnit, getDistanceVal, getLatValFormat, getLonValFormat, getSpeedUnit, getSpeedVal } from '../core/units.js';
import { getCountryName, getDeltaTimeVal, getStringfromGroup, getStringfromMsgType, getICAO } from '../../shared/core/text.js';
import { flagHTML } from '../../shared/components.js';
import * as targetcard from './targetcard.js';

let deps = null;

export function init(d) {
    deps = d;
    build();
}

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
        cls: "card-max-only", section: { key: "aircraft", label: "Aircraft", open: targetcard.SECTION_DEFAULTS.aircraft }, fields: [
            { key: "callsign", label: "Callsign", get: (p) => p.callsign },
            { key: "country", label: "Country", get: (p) => getCountryName(p.country) },
            { key: "hexident", label: "ICAO", get: (p) => getICAO(p) },
        ]
    },
    {
        cls: "card-max-only", fields: [
            { key: "category", label: "Category", get: (p) => CATEGORY[p.category] || p.category },
            { key: "squawk", label: "Squawk", get: (p) => p.squawk },
        ]
    },
    {
        cls: "card-row-selected", section: { key: "flight", label: "Flight", open: targetcard.SECTION_DEFAULTS.flight }, fields: [
            { key: "speed", label: "Speed", get: (p) => (p.speed ? getSpeedVal(p.speed) + " " + getSpeedUnit() : "") },
            { key: "heading", label: "Heading", get: (p) => num(p.heading, 0, "°") },
            { key: "altitude", label: "Altitude", get: (p) => (p.airborne == 1 ? (p.altitude ? p.altitude + " ft" : "") : "on ground") },
        ]
    },
    {
        cls: "card-max-only", fields: [
            { key: "vertrate", label: "Vert Rate", get: (p) => (p.vertrate ? p.vertrate + " ft/min" : "") },
            { key: "lat", label: "Latitude", html: true, get: (p) => (p.lat ? getLatValFormat(p) : "") },
            { key: "lon", label: "Longitude", html: true, get: (p) => (p.lon ? getLonValFormat(p) : "") },
        ]
    },
    {
        // what the receiver saw, not what the aircraft said: distance and
        // bearing are measured from the station, as on the ship card
        cls: "card-max-only", section: { key: "adsb", label: "ADS-B source", open: targetcard.SECTION_DEFAULTS.adsb }, fields: [
            { key: "distance", label: "Distance", get: (p) => (p.distance != null ? getDistanceVal(p.distance) + " " + getDistanceUnit() : "") },
            { key: "bearing", label: "Bearing", get: (p) => num(p.bearing, 0, "°") },
            { key: "level", label: "RSSI", get: (p) => num(p.level, 1, "dB") },
        ]
    },
    {
        cls: "card-max-only", fields: [
            { key: "last_signal", label: "Last Signal", get: (p) => getDeltaTimeVal(planesSince - p.last_signal) },
            { key: "messages", label: "Messages", get: (p) => p.nMessages },
            { key: "last_group", label: "Receiver", get: (p) => getStringfromGroup(p.last_group) },
        ]
    },
    {
        cls: "card-max-only", fields: [
            { key: "downlink", label: "Downlink", get: (p) => getStringfromMsgType(p.message_types) },
            { key: "TC", label: "TC", get: (p) => getStringfromMsgType(p.message_subtypes) },
        ]
    },
    {
        cls: "card-max-only", fields: [
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

let lastSection = null;
for (const row of ROWS) {
    if (row.section) lastSection = row.section.key;
    else if (lastSection) row.sectionKey = lastSection;
}

let cells = null;

function build() {
    if (cells) return cells;

    const mount = document.getElementById("targetcard_plane_rows");
    if (!mount) return null;

    cells = targetcard.card.rows(mount, ROWS, {
        attrs: { "data-action": "targetcardSelectSelf" },
        group: "plane",
        idPrefix: "targetcard_plane_",
        headId: (key) => "targetcard_" + key + "_head",
    });
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
    document.getElementById("targetcard_header_flag").innerHTML = flagHTML(plane.country, 'flag-card', getCountryName(plane.country));

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
