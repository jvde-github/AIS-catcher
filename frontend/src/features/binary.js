// Application-specific (binary) AIS messages: the DAC/FI payloads carried by
// message types 6 and 8 — meteo/hydro (DAC 1, FI 31), inland persons-on-board
// (DAC 200, FI 55) and free text (DAC 1, FI 0/29/30).
//
// Owns the binary message store, its map layer and every renderer that turns a
// message into tooltip or dialog HTML.

import OlFeature from 'ol/Feature';
import VectorLayer from 'ol/layer/Vector';
import VectorSource from 'ol/source/Vector';
import Point from 'ol/geom/Point';
import Style from 'ol/style/Style';
import Stroke from 'ol/style/Stroke';
import Fill from 'ol/style/Fill';
import CircleStyle from 'ol/style/Circle';
import Text from 'ol/style/Text';
import { fromLonLat } from 'ol/proj';

import { settings } from '../core/state.js';
import { hasValidCoords } from '../core/geo.js';
import { sanitizeString, formatTime } from '../core/format.js';

// { getActiveReceiver, getShipsDB, showDialog, getDialogModal, saveSettings, redrawMap }
let deps = null;

let binaryDB = {};
let binarySince = 0;
let binaryTimeout = 1800;
const binaryMaxPerShip = 50;

export function init(d) { deps = d; }

export function getDB() { return binaryDB; }
export function resetSince() { binarySince = 0; }

// ─── map layer ───────────────────────────────────────────────────────────────

const binaryVector = new VectorSource({ features: [] });

const binaryAssociatedOutline = new Style({
    image: new CircleStyle({
        radius: 12,
        fill: new Fill({ color: 'rgba(255, 255, 255, 0)' }),
        stroke: new Stroke({ color: 'white', width: 2 })
    }),
    zIndex: 200
});

const binaryStyleCache = new Map();

const countText = (count) => new Text({
    text: count.toString(),
    font: 'bold 9px Arial',
    fill: new Fill({ color: 'white' }),
    textAlign: 'center',
    textBaseline: 'middle'
});

const binaryStyle = function (feature) {
    const count = feature.get('binary_count') || feature.binary_count || 1;
    const isAssociated = feature.get('is_associated') || feature.is_associated;
    const highlight = settings.binary_messages === 'highlight';
    const key = (isAssociated ? 'a:' : 'n:') + (highlight ? 'h:' : 'b:') + count;

    let cached = binaryStyleCache.get(key);
    if (cached) return cached;

    if (isAssociated) {
        // badge pinned to the ship marker's upper right
        const text = countText(count);
        text.setOffsetX(10);
        text.setOffsetY(-10);
        const badge = new Style({
            image: new CircleStyle({
                radius: 8,
                fill: new Fill({ color: 'rgba(220, 0, 0, 0.9)' }),
                stroke: new Stroke({ color: 'white', width: 1 }),
                displacement: [10, 10]
            }),
            text,
            zIndex: 201
        });
        cached = highlight ? [binaryAssociatedOutline, badge] : [badge];
    } else {
        cached = [new Style({
            image: new CircleStyle({
                radius: 10,
                fill: new Fill({ color: 'rgba(220, 0, 0, 0.9)' }),
                stroke: new Stroke({ color: 'white', width: 1.5 })
            }),
            text: countText(count),
            zIndex: 100
        })];
    }
    binaryStyleCache.set(key, cached);
    return cached;
};

export const binaryLayer = new VectorLayer({
    source: binaryVector,
    style: binaryStyle
});

// ─── classification ──────────────────────────────────────────────────────────

// the decoder emits `fid` on some paths and `fi` on others
const fiOf = (m) => (m.fid != null ? m.fid : m.fi);

export function isInlandMessage(msg) {
    if (!msg.message) return false;
    return msg.message.dac == 200 && fiOf(msg.message) == 55;
}

export function isTextMessage(msg) {
    if (!msg.message) return false;
    const fi = fiOf(msg.message);
    return msg.message.dac == 1 && (fi == 0 || fi == 29 || fi == 30);
}

export function isMeteoMessage(msg) {
    if (!msg.message) return false;
    return msg.message.dac == 1 && fiOf(msg.message) == 31;
}

export const BINARY_CATEGORIES = ['data', 'inland', 'text'];

function binaryIncluded(msg) {
    const ex = settings.binary_exclude;
    if (isTextMessage(msg)) return !ex.includes('text');
    if (isInlandMessage(msg)) return !ex.includes('inland');
    const hasLocation = msg.message && msg.message.lat && msg.message.lon;
    return hasLocation && !ex.includes('data');
}

export function binaryAnyShown() {
    return BINARY_CATEGORIES.some(c => !settings.binary_exclude.includes(c));
}

export function shipBinaryMessages(mmsi) {
    return (mmsi != null && binaryDB[mmsi]?.ship_messages) || [];
}

// ─── rendering ───────────────────────────────────────────────────────────────

const meteoRow = (label, value) =>
    `<div style="display: flex; justify-content: space-between; padding: 1px 0; white-space: nowrap;">` +
    `<span style="font-size: 11px; opacity: 0.6; margin-right: 12px;">${label}</span>` +
    `<span style="font-size: 11px; font-weight: bold;">${value}</span></div>`;

// orange caption line shared by the meteo, inland and text tooltips
const meteoHeader = (time, label, extra = 0) =>
    `<div style="font-size: 11px; color: #FFA500; padding: 4px 0 3px; margin-bottom: 2px; display: flex; justify-content: space-between; align-items: center;">` +
    `<span style="font-size: 11px;">${time} - ${label}</span>` +
    (extra > 0 ? `<span style="font-size: 10px; opacity: 0.5; font-style: italic;">+${extra} more</span>` : '') +
    '</div>';

const rawDetails = (obj) =>
    `<details class="binary-raw-data">
                      <summary>Show Raw Data</summary>
                      <pre>${sanitizeString(JSON.stringify(obj, null, 2))}</pre>
                    </details>`;

// 360 is the "not available" sentinel for every AIS direction field
const dirSuffix = (d) => (d != null && d !== 360 ? ` / ${d}&deg;` : '');
const periodSuffix = (p) => (p != null ? `, ${p}s` : '');

const PRESSURE_TEND = ['steady', 'decreasing', 'increasing'];

// Each entry returns the formatted value, or a falsy value when the field is
// absent. Order here is the order rows appear in the tooltip.
const METEO_ROWS = [
    ['Wind', (m) => m.wspeed != null && `${m.wspeed.toFixed(1)} kts${dirSuffix(m.wdir)}`],
    ['Air', (m) => m.airtemp != null && `${m.airtemp.toFixed(1)}&deg;C`],
    ['Pressure', (m) => m.pressure != null && m.pressure > 799 &&
        `${m.pressure.toFixed(1)} hPa` +
        (m.pressuretend != null ? ` (${PRESSURE_TEND[m.pressuretend]})` : '')],
    ['Current', (m) => {
        const speed = m.watercurrent || m.currentspeed;
        return speed != null &&
            `${speed.toFixed(1)} kts${dirSuffix(m.currentdir || m.currentdirection)}`;
    }],
    ['Water Level', (m) => m.waterlevel != null && `${m.waterlevel.toFixed(2)} m`],
    ['Water', (m) => m.watertemp != null && `${m.watertemp.toFixed(1)}&deg;C`],
    ['Wave', (m) => m.waveheight != null &&
        `${m.waveheight.toFixed(1)} m${dirSuffix(m.wavedir)}${periodSuffix(m.waveperiod)}`],
    ['Swell', (m) => m.swellheight != null &&
        `${m.swellheight.toFixed(1)} m${dirSuffix(m.swelldir)}${periodSuffix(m.swellperiod)}`],
    ['Visibility', (m) => m.visibility != null && `${m.visibility.toFixed(1)} nm`],
];

const HYDRO_FIELDS = ['watercurrent', 'currentspeed', 'currentdir', 'watertemp', 'waterlevel'];

export function getBinaryMessageContent(binary, includeRaw = false) {
    const messages = Array.isArray(binary) ? binary : [binary];
    if (messages.length === 0) return '';

    messages.sort((a, b) => b.timestamp - a.timestamp);

    const msg = messages[0].message;
    const hasHydroData = HYDRO_FIELDS.some(f => msg[f] != null);

    let content = '<div class="meteo-tooltip">';
    content += meteoHeader(messages[0].formattedTime,
        hasHydroData ? 'Meteo & Hydro' : 'Meteo', messages.length - 1);

    for (const [label, format] of METEO_ROWS) {
        const value = format(msg);
        if (value) content += meteoRow(label, value);
    }

    if (includeRaw) content += rawDetails(msg);

    content += '</div>';
    return content;
}

export function getTextMessageTooltip(messages) {
    const sorted = [...messages].sort((a, b) => b.timestamp - a.timestamp);
    const m = sorted[0];

    return '<div class="meteo-tooltip">' +
        meteoHeader(m.formattedTime, 'Text Message', sorted.length - 1) +
        `<div style="font-size: 11px; font-weight: bold; white-space: pre-wrap;">${sanitizeString(m.message.text || '')}</div>` +
        '</div>';
}

function getTextMessageContent(msg) {
    const m = msg.message;
    const kind = m.type == 6 || fiOf(m) == 30 ? "Text message (addressed)" : "Text message (broadcast)";

    let content = '<div class="binary-message-details">';
    content += `<div><strong>${kind}</strong></div>`;
    content += `<div style="font-size: var(--fs-lg); margin: 6px 0; white-space: pre-wrap;">${sanitizeString(m.text || '')}</div>`;

    if (m.dest_mmsi) content += `<div><strong>To:</strong> ${sanitizeString(String(shipLabel(m.dest_mmsi)))}</div>`;
    if (m.ack_required) content += '<div>Acknowledgement requested</div>';

    content += rawDetails(m);
    content += '</div>';
    return content;
}

export function getInlandMessageContent(binary) {
    const messages = Array.isArray(binary) ? binary : [binary];
    if (messages.length === 0) return '';

    // Keep only the latest persons-on-board message
    let entry = null;
    messages.forEach(m => {
        if (!isInlandMessage(m)) return;
        if (!entry || m.timestamp > entry.timestamp) entry = m;
    });
    if (!entry) return '';

    const msg = entry.message;

    let content = '<div class="meteo-tooltip">';
    content += meteoHeader(entry.formattedTime, 'Persons on Board');

    if (msg.crew_count != null) content += meteoRow('Crew', msg.crew_count);
    if (msg.passenger_count != null) content += meteoRow('Passengers', msg.passenger_count);
    if (msg.shipboard_personnel_count != null) content += meteoRow('Personnel', msg.shipboard_personnel_count);

    content += '</div>';
    return content;
}

// Meteo/inland/text sections appended to a hover tooltip. Standalone binary
// features carry no text messages, hence the flag.
export function tooltipSections(messages, includeText = false) {
    if (!messages || messages.length === 0) return '';

    let content = '';
    const meteo = messages.filter(isMeteoMessage);
    if (meteo.length > 0) content += getBinaryMessageContent(meteo);

    const inland = messages.filter(isInlandMessage);
    if (inland.length > 0) content += getInlandMessageContent(inland);

    if (includeText) {
        const text = messages.filter(isTextMessage);
        if (text.length > 0) content += getTextMessageTooltip(text);
    }
    return content;
}

const shipLabel = (mmsi) =>
    deps.getShipsDB()[mmsi]?.raw?.shipname || `MMSI ${mmsi}`;

function getBinaryMessageList(messages) {
    if (!messages || messages.length === 0) {
        return "<p>No messages available</p>";
    }

    const sortedMessages = [...messages].sort((a, b) => b.timestamp - a.timestamp);

    let content = '<div class="binary-messages-list">';
    content += `<div class="binary-message-count">${sortedMessages.length} message${sortedMessages.length > 1 ? 's' : ''} available</div>`;

    sortedMessages.forEach((msg, index) => {
        if (index > 0) {
            content += '<hr style="margin: 15px 0; border: 0; border-top: 1px solid rgba(0,0,0,0.1);">';
        }

        content += '<div class="binary-message-item">';
        content += `<div class="binary-message-header">
                      <span class="binary-message-time">${msg.formattedTime || new Date(msg.timestamp * 1000).toLocaleTimeString()}</span>`;

        if (msg.message && msg.message.mmsi) {
            content += `<span class="binary-message-source"> from ${shipLabel(msg.message.mmsi)}</span>`;
        }
        content += '</div>';

        if (isMeteoMessage(msg)) {
            content += getBinaryMessageContent(msg, true);
        } else if (isInlandMessage(msg)) {
            content += getInlandMessageContent(msg);
        } else if (isTextMessage(msg)) {
            content += getTextMessageContent(msg);
        } else {
            content += '<div class="binary-message-details">';
            content += `<div><strong>Message Type:</strong> ${msg.message ? `DAC ${msg.message.dac}, FI ${msg.message.fid || msg.message.fi}` : 'Unknown'}</div>`;
            content += rawDetails(msg.message);
            content += '</div>';
        }

        content += '</div>';
    });

    content += '</div>';
    return content;
}

export function showBinaryMessageDialog(featureOrMmsi) {
    let content;

    if (typeof featureOrMmsi === 'object') {
        if (!featureOrMmsi.binary_messages || featureOrMmsi.binary_messages.length === 0) {
            deps.showDialog("Binary", "No message content available");
            return;
        }
        content = getBinaryMessageList(featureOrMmsi.binary_messages);

    } else if (typeof featureOrMmsi === 'number' || typeof featureOrMmsi === 'string') {
        const mmsi = Number(featureOrMmsi);
        if (shipBinaryMessages(mmsi).length === 0) {
            deps.showDialog("Binary", "No binary messages available for this vessel");
            return;
        }
        content = getBinaryMessageList(binaryDB[mmsi].ship_messages);
    }

    deps.showDialog("Binary", content);
    deps.getDialogModal().card.style.maxWidth = "500px";
}

// ─── settings ────────────────────────────────────────────────────────────────

export function setBinaryDisplay(v) {
    settings.binary_messages = v;
    binaryStyleCache.clear();
    deps.saveSettings();
    deps.redrawMap();
}

export function setBinaryCategory(cat, shown) {
    const excluded = settings.binary_exclude.filter(c => c !== cat);
    if (!shown) excluded.push(cat);
    settings.binary_exclude = excluded;
    deps.saveSettings();

    binarySince = 0;
    if (binaryLayer.isVisible() && binaryAnyShown()) {
        fetchBinary().then(() => deps.redrawMap());
    } else {
        binaryDB = {};
        deps.redrawMap();
    }
}

// ─── fetch ───────────────────────────────────────────────────────────────────

export async function fetchBinary() {
    const isIncremental = binarySince > 0;

    if (!isIncremental) binaryDB = {};

    try {
        const response = await fetch("api/binmsgs.json?receiver=" + deps.getActiveReceiver() +
            (isIncremental ? "&since=" + binarySince : ""));
        const data = await response.json();

        const messages = data.messages || [];
        const serverTime = data.time || 0;
        if (data.timeout) binaryTimeout = data.timeout;

        messages.forEach((msg) => {
            const hasLocation = msg.message && msg.message.lat && msg.message.lon;
            if (msg.message && msg.message.mmsi && binaryIncluded(msg)) {
                const mmsi = msg.message.mmsi;

                if (!binaryDB[mmsi]) {
                    binaryDB[mmsi] = {
                        ship_messages: [],
                        standalone_messages: []
                    };
                }

                const exists = binaryDB[mmsi].ship_messages.some(m =>
                    m.timestamp === msg.timestamp && m.message?.dac === msg.message.dac &&
                    (m.message?.fid ?? m.message?.fi) === (msg.message.fid ?? msg.message.fi));
                if (exists) return;

                msg.formattedTime = formatTime(msg.timestamp);
                if (hasLocation) {
                    msg.message_lat = msg.message.lat;
                    msg.message_lon = msg.message.lon;
                }

                binaryDB[mmsi].ship_messages.push(msg);
            }
        });

        if (serverTime > 0) {
            binarySince = serverTime - 1;
            const cutoff = serverTime - binaryTimeout;
            for (const mmsi in binaryDB) {
                let msgs = binaryDB[mmsi].ship_messages.filter(m => m.timestamp > cutoff);
                if (msgs.length > binaryMaxPerShip) {
                    msgs.sort((a, b) => b.timestamp - a.timestamp);
                    msgs.length = binaryMaxPerShip;
                }
                if (msgs.length === 0) delete binaryDB[mmsi];
                else binaryDB[mmsi].ship_messages = msgs;
            }
        }

        let idx = 0;
        for (const mmsi in binaryDB) {
            binaryDB[mmsi].ship_messages.forEach(m => { m.index = ++idx; });
        }

        return true;
    } catch (error) {

        console.log("Failed loading binary:", error);
        return false;
    }
}

// ─── drawing ─────────────────────────────────────────────────────────────────

const GRID_SIZE = 0.01;         // clustering cell, degrees (approx. 1km)
const SHIP_PROXIMITY = 0.05;    // messages nearer than this badge on the ship

function addMessageToGridCell(msg, mmsi, gridCells) {
    const msgLat = msg.message_lat;
    const msgLon = msg.message_lon;

    if (!hasValidCoords(msgLat, msgLon)) return;

    const gridKey = `${Math.floor(msgLon / GRID_SIZE)},${Math.floor(msgLat / GRID_SIZE)}`;

    if (!gridCells[gridKey]) {
        gridCells[gridKey] = { messages: [], totalLat: 0, totalLon: 0, mmsiCounts: {} };
    }

    const cell = gridCells[gridKey];
    cell.messages.push(msg);
    cell.totalLat += msgLat;
    cell.totalLon += msgLon;
    cell.mmsiCounts[mmsi] = (cell.mmsiCounts[mmsi] || 0) + 1;
}

function createGridCellFeatures(gridCells) {
    for (const [gridKey, gridData] of Object.entries(gridCells)) {
        if (gridData.messages.length === 0) continue;

        const avgLat = gridData.totalLat / gridData.messages.length;
        const avgLon = gridData.totalLon / gridData.messages.length;

        const feature = new OlFeature({ geometry: new Point(fromLonLat([avgLon, avgLat])) });

        feature.binary = true;
        feature.binary_count = gridData.messages.length;
        feature.binary_messages = gridData.messages;
        feature.binary_mmsi_counts = gridData.mmsiCounts;
        feature.is_associated = false;
        feature.tooltip = `${gridData.messages.length} binary messages`;
        feature.setId(`binary-standalone-${gridKey}`);

        binaryVector.addFeature(feature);
    }
}

export function redrawBinaryMessages() {
    binaryVector.clear();

    if (settings.binary_messages === 'off') return;

    const shipsDB = deps.getShipsDB();
    const gridCells = {};

    for (const [mmsi, msgData] of Object.entries(binaryDB)) {
        if (!msgData.ship_messages || msgData.ship_messages.length === 0) continue;

        const allMessages = msgData.ship_messages;
        const ship = mmsi in shipsDB ? shipsDB[mmsi].raw : null;

        if (!ship || !hasValidCoords(ship.lat, ship.lon)) {
            // no ship to badge: every located message clusters on its own
            allMessages.forEach(msg => {
                if (!msg.message_lat || !msg.message_lon) return;
                addMessageToGridCell(msg, mmsi, gridCells);
            });
            continue;
        }

        const shipMessages = [];

        allMessages.forEach(msg => {
            // Messages without their own location (e.g. inland FID 10/55) badge on the ship
            if (!msg.message_lat || !msg.message_lon) {
                shipMessages.push(msg);
                return;
            }
            if (!hasValidCoords(msg.message_lat, msg.message_lon)) return;

            const distance = Math.hypot(msg.message_lat - ship.lat, msg.message_lon - ship.lon);
            if (distance <= SHIP_PROXIMITY) shipMessages.push(msg);
            else addMessageToGridCell(msg, mmsi, gridCells);
        });

        if (shipMessages.length > 0) {
            const feature = new OlFeature({ geometry: new Point(fromLonLat([ship.lon, ship.lat])) });

            feature.binary = true;
            feature.ship = ship;
            feature.binary_mmsi = mmsi;
            feature.binary_count = shipMessages.length;
            feature.binary_messages = shipMessages;
            feature.is_associated = true;
            feature.setId(`binary-ship-${mmsi}`);

            binaryVector.addFeature(feature);
        }
    }

    createGridCellFeatures(gridCells);
}
