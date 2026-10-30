// Plane card: the same shell and tabs as the ship card, different fields.

import { planes, planesSince, cardMmsi, cardType } from '../../core/store.js';
import { getDistanceUnit, getDistanceVal, getLatValFormat, getLonValFormat, getSpeedUnit, getSpeedVal } from '../../core/units.js';
import { getCountryName, getDeltaTimeVal, getStringfromGroup, getStringfromMsgType, getICAO } from '@aiscatcher/core/text.js';
import { fieldRows } from '@aiscatcher/ui/components.js';
import { build as buildTabs, metrics } from '@aiscatcher/ui/card-tabs.js';
import { ui } from '../../ui.js';
import { fitTargetcard, openOnTab, setValidation, showOutOfRange, updateTrackOption } from '../targetcard/card.js';

const CATEGORY = {
    21: "Emergency", 23: "Service",
    31: "Glider", 32: "LTA", 33: "Parachutist", 34: "Ultralight", 36: "UAV", 37: "Space",
    41: "< 7 MT", 42: "7 - 34 MT", 43: "34 - 136 MT", 44: "High vortex", 45: "> 136 MT",
    46: "High perf", 47: "Rotorcraft",
};

const num = (v, digits, unit) => (v != null ? Number(v).toFixed(digits) + " " + unit : "");

// The card's fields, in the order they are shown: the first row above the tabs,
// the second on Summary, the next two on Flight, the rest on ADS-B. Each one
// knows its own label and how to read itself off a plane, so the markup is
// built from this list rather than repeated in index.html.
const ROWS = [
    {
        fields: [
            { key: "callsign", label: "Callsign", get: (p) => p.callsign },
            { key: "country", label: "Country", get: (p) => getCountryName(p.country) },
            { key: "hexident", label: "ICAO", get: (p) => getICAO(p) },
        ]
    },
    {
        fields: [
            { key: "category", label: "Category", get: (p) => CATEGORY[p.category] || p.category },
            { key: "squawk", label: "Squawk", get: (p) => p.squawk },
        ]
    },
    {
        fields: [
            { key: "speed", label: "Speed", get: (p) => (p.speed != null ? getSpeedVal(p.speed) + " " + getSpeedUnit() : "") },
            { key: "heading", label: "Heading", get: (p) => num(p.heading, 0, "°") },
            { key: "altitude", label: "Altitude", get: (p) => (p.airborne == 1 ? (p.altitude != null ? p.altitude + " ft" : "") : "on ground") },
        ]
    },
    {
        fields: [
            { key: "vertrate", label: "Vert Rate", get: (p) => (p.vertrate != null ? p.vertrate + " ft/min" : "") },
            { key: "lat", label: "Latitude", html: true, get: (p) => (p.lat != null ? getLatValFormat(p) : "") },
            { key: "lon", label: "Longitude", html: true, get: (p) => (p.lon != null ? getLonValFormat(p) : "") },
        ]
    },
    {
        // what the receiver saw, not what the aircraft said: distance and
        // bearing are measured from the station, as on the ship card
        fields: [
            { key: "distance", label: "Distance", get: (p) => (p.distance != null ? getDistanceVal(p.distance) + " " + getDistanceUnit() : "") },
            { key: "bearing", label: "Bearing", get: (p) => num(p.bearing, 0, "°") },
            { key: "level", label: "RSSI", get: (p) => num(p.level, 1, "dB") },
        ]
    },
    {
        fields: [
            { key: "last_signal", label: "Last Signal", get: (p) => getDeltaTimeVal(planesSince - p.last_signal) },
            { key: "messages", label: "Messages", get: (p) => p.nMessages },
            { key: "last_group", label: "Receiver", get: (p) => getStringfromGroup(p.last_group) },
        ]
    },
    {
        fields: [
            { key: "downlink", label: "Downlink", get: (p) => getStringfromMsgType(p.message_types) },
            { key: "TC", label: "TC", get: (p) => getStringfromMsgType(p.message_subtypes) },
        ]
    },
    {
        fields: [
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

let cells = null, layout = null, selectedTab = 'summary';

export function open(id, { fresh }) {
    openOnTab(layout, selectedTab, fresh);
}

export function build() {
    const mount = document.getElementById("targetcard_plane_rows");
    mount.dataset.contextType = 'plane';
    mount.style.display = 'none';
    layout = buildTabs(mount, 'targetcard_plane_', {
        tabs: [['summary', 'Summary'], ['flight', 'Flight'], ['adsb', 'ADS-B']],
        label: 'Aircraft details', tab: null,
        onTab: t => { if (t) selectedTab = t; fitTargetcard(); },
    });
    const rows = (parent, specs) => fieldRows(parent, specs, { rowClass: 'mapcard-content-row card-row sc-row', idPrefix: 'targetcard_plane_' });
    cells = {
        ...metrics(layout.head, ROWS[0].fields, 'targetcard_plane_'),
        ...rows(layout.panels.summary, [ROWS[1]]),
        ...rows(layout.panels.flight, ROWS.slice(2, 4)),
        ...rows(layout.panels.adsb, ROWS.slice(4)),
    };
}

export function populate() {

    if (cardType != 'plane') return;

    if (!(cardMmsi in planes)) {
        showOutOfRange();
        return;
    }

    const plane = planes[cardMmsi].raw;

    setValidation(plane.validated);

    ui.card.header.title().textContent = (plane.callsign || getICAO(plane));
    ui.card.header.flag(plane.country, getCountryName(plane.country));

    for (const row of ROWS) {
        for (const field of row.fields) {
            if (field.spacer) continue;
            const value = field.get(plane);
            const text = value == null ? "" : String(value);
            if (field.html) cells[field.key].innerHTML = text;
            else cells[field.key].textContent = text;
        }
    }

    updateTrackOption();
}
