// Plane card: the same shell as the ship card, different fields.
// Mirrors features/targetcard.js so both entity types have one shape.

import { planes, planesSince, cardMmsi, cardType } from '../core/store.js';
import { getDistanceUnit, getDistanceVal, getLatValFormat, getLonValFormat, getSpeedUnit, getSpeedVal } from '../core/units.js';
import { getCountryName, getDeltaTimeVal, getStringfromGroup, getStringfromMsgType, getICAO } from '../../shared/core/text.js';
import { flagHTML, fieldRows } from '../../shared/components.js';
import { build as buildTabs, metrics } from '../../shared/card-tabs.js';
import { paneCramped } from '../../shared/mapui.js';
import { settings } from '../core/state.js';
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
            { key: "speed", label: "Speed", get: (p) => (p.speed != null ? getSpeedVal(p.speed) + " " + getSpeedUnit() : "") },
            { key: "heading", label: "Heading", get: (p) => num(p.heading, 0, "°") },
            { key: "altitude", label: "Altitude", get: (p) => (p.airborne == 1 ? (p.altitude != null ? p.altitude + " ft" : "") : "on ground") },
        ]
    },
    {
        cls: "card-max-only", fields: [
            { key: "vertrate", label: "Vert Rate", get: (p) => (p.vertrate != null ? p.vertrate + " ft/min" : "") },
            { key: "lat", label: "Latitude", html: true, get: (p) => (p.lat != null ? getLatValFormat(p) : "") },
            { key: "lon", label: "Longitude", html: true, get: (p) => (p.lon != null ? getLonValFormat(p) : "") },
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

let cells = null, layout = null, style = null, selectedTab = 'summary';

export function setStyle(value) {
    layout?.destroy();
    layout = null;
    cells = null;
    style = value === 'tabs' ? 'tabs' : 'classic';
    build();
    populate();
}

export function open(fresh) {
    if (!layout) return;
    if (fresh || !paneCramped()) layout.select(paneCramped() ? null : selectedTab, false);
    layout.scrollTop();
}

function build() {
    if (cells) return cells;

    const mount = document.getElementById("targetcard_plane_rows");
    if (!mount) return null;
    mount.dataset.contextType = 'plane';
    mount.style.display = cardType === 'plane' ? '' : 'none';
    mount.innerHTML = '';
    style = style || (settings.shipcard_style === 'tabs' ? 'tabs' : 'classic');
    if (style === 'tabs') {
        layout = buildTabs(mount, 'targetcard_plane_', {
            tabs: [['summary', 'Summary'], ['flight', 'Flight'], ['adsb', 'ADS-B']],
            label: 'Aircraft details', tab: paneCramped() ? null : selectedTab,
            foldable: paneCramped,
            onTab: t => { if (t) selectedTab = t; deps.fit?.(); },
        });
        cells = {};
        const rows = (parent, specs) => {
            Object.assign(cells, fieldRows(parent, specs.map(r => ({ fields: r.fields })), {
                rowClass: 'mapcard-content-row card-row sc-row', idPrefix: 'targetcard_plane_',
            }));
        };
        Object.assign(cells, metrics(layout.head, ROWS[0].fields, 'targetcard_plane_'));
        rows(layout.panels.summary, [ROWS[1]]);
        rows(layout.panels.flight, ROWS.slice(2, 4));
        rows(layout.panels.adsb, ROWS.slice(4));
        return cells;
    }
    mount.classList.remove('sc-host', 'sc-folded');

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
