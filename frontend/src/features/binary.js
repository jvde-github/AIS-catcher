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
import { hasValidCoords } from '../../shared/core/geo.js';
import { sanitizeString, formatTime } from '../../shared/core/text.js';

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
const METEO_KEYS = ['wspeed', 'airtemp', 'pressure', 'waterlevel', 'watertemp', 'waveheight', 'swellheight', 'visibility', 'cspeed', 'water_flow'];

export function isInlandMessage(msg) {
    if (!msg.message) return false;
    return msg.message.dac == 200 && fiOf(msg.message) == 55;
}

export function isTextMessage(msg) {
    if (!msg.message) return false;
    const fi = fiOf(msg.message);
    return msg.message.dac == 1 && (fi == 0 || fi == 29 || fi == 30);
}

export function isLockMessage(msg) {
    if (!msg.message) return false;
    return (msg.message.dac == 316 || msg.message.dac == 366) && fiOf(msg.message) == 2;
}

export function isMeteoMessage(msg) {
    const m = msg.message;
    if (!m) return false;
    if (m.dac == 1 && fiOf(m) == 31) return true;
    if ((m.dac == 316 || m.dac == 366) && fiOf(m) == 1) return true;
    return METEO_KEYS.some(k => m[k] != null);
}

export const BINARY_CATEGORIES = ['data', 'inland', 'text'];

function binaryIncluded(msg) {
    const ex = settings.binary_exclude;
    if (isTextMessage(msg)) return !ex.includes('text');
    if (isInlandMessage(msg)) return !ex.includes('inland');
    if (isLockMessage(msg)) return !ex.includes('data');
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
const meteoHeader = (time, label, extra = 0, count = 0) =>
    `<div style="font-size: 11px; color: #FFA500; padding: 4px 0 3px; margin-bottom: 2px; display: flex; justify-content: space-between; align-items: center;">` +
    `<span style="font-size: 11px;">${time} - ${label}</span>` +
    (count > 1 ? `<span style="font-size: 10px; opacity: 0.7;">&times;${count}</span>` : '') +
    (extra > 0 ? `<span style="font-size: 10px; opacity: 0.5; font-style: italic;">+${extra} more</span>` : '') +
    '</div>';

const seenLine = (m) => m.count > 1
    ? `<div style="font-size: 10px; opacity: 0.6;">seen ${m.count} times since ${formatTime(m.first)}</div>`
    : '';


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
    ['Flow', (m) => m.water_flow != null && `${m.water_flow} m&sup3;/s`],
    ['Station', (m) => m.station_id && sanitizeString(m.station_id)],
];

const HYDRO_FIELDS = ['watercurrent', 'currentspeed', 'currentdir', 'watertemp', 'waterlevel'];

export function getBinaryMessageContent(binary) {
    const messages = Array.isArray(binary) ? binary : [binary];
    if (messages.length === 0) return '';

    messages.sort((a, b) => b.timestamp - a.timestamp);

    const msg = messages[0].message;
    const hasHydroData = HYDRO_FIELDS.some(f => msg[f] != null);

    let content = '<div class="meteo-tooltip">';
    content += meteoHeader(messages[0].formattedTime,
        hasHydroData ? 'Meteo & Hydro' : 'Meteo', messages.length - 1, messages[0].count);

    for (const [label, format] of METEO_ROWS) {
        const value = format(msg);
        if (value) content += meteoRow(label, value);
    }

    content += '</div>';
    return content;
}

export function getTextMessageContent(binary) {
    const messages = Array.isArray(binary) ? binary : [binary];
    if (messages.length === 0) return '';
    messages.sort((a, b) => b.timestamp - a.timestamp);
    const msg = messages[0];
    const m = msg.message;
    const label = m.type == 6 || fiOf(m) == 30 ? 'Text Message (addressed)' : 'Text Message';
    let content = '<div class="meteo-tooltip">';
    content += meteoHeader(msg.formattedTime, label, messages.length - 1, msg.count);
    content += `<div style="font-size: 11px; font-weight: bold; white-space: pre-wrap;">${sanitizeString(m.text || '')}</div>`;
    if (m.dest_mmsi) content += meteoRow('To', sanitizeString(String(shipLabel(m.dest_mmsi))));
    if (m.ack_required) content += meteoRow('Ack', 'requested');
    content += seenLine(msg);
    content += '</div>';
    return content;
}

const lockRows = (m) => {
    let rows = '';
    if (m.lock_id) rows += meteoRow('Lock', sanitizeString(m.lock_id));
    if (m.lock_schedule) {
        for (const entry of m.lock_schedule.split(';')) {
            const [vessel, dir, eta] = entry.split(',');
            rows += meteoRow(sanitizeString(vessel || ''), `${dir === '1' ? 'up' : 'down'} ${sanitizeString(eta || '')}`);
        }
    }
    if (m.vessel_name) rows += meteoRow('Vessel', sanitizeString(m.vessel_name));
    if (m.last_location) rows += meteoRow('Last', `${sanitizeString(m.last_location)} ${sanitizeString(m.last_ata || '')}`);
    if (m.first_lock) rows += meteoRow('Next', `${sanitizeString(m.first_lock)} ${sanitizeString(m.first_lock_eta || '')}`);
    if (m.second_lock) rows += meteoRow('Then', `${sanitizeString(m.second_lock)} ${sanitizeString(m.second_lock_eta || '')}`);
    if (m.delay_lock) rows += meteoRow('Delay at', sanitizeString(m.delay_lock));
    return rows;
};

export function getLockMessageContent(binary) {
    const messages = Array.isArray(binary) ? binary : [binary];
    if (messages.length === 0) return '';
    messages.sort((a, b) => b.timestamp - a.timestamp);
    const item = messages[0];
    const m = item.message;
    let content = '<div class="meteo-tooltip">';
    content += meteoHeader(item.formattedTime, m.message_id == 2 ? 'Estimated Lock Times' : 'Lockage Order', messages.length - 1, item.count);
    content += lockRows(m);
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
    content += meteoHeader(entry.formattedTime, 'Persons on Board', 0, entry.count);

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

    const locks = messages.filter(isLockMessage);
    if (locks.length > 0) content += getLockMessageContent(locks);

    if (includeText) {
        const text = messages.filter(isTextMessage);
        if (text.length > 0) content += getTextMessageContent(text);
    }
    return content;
}

const shipLabel = (mmsi) =>
    deps.getShipsDB()[mmsi]?.raw?.shipname || `MMSI ${mmsi}`;

function getBinaryMessageList(messages) {
    if (!messages || messages.length === 0) {
        return "<p>No messages available</p>";
    }

    const sorted = [...messages].sort((a, b) => b.timestamp - a.timestamp);
    let content = '<div class="binary-messages-list">';
    sorted.forEach((msg) => {
        let section;
        if (isMeteoMessage(msg)) section = getBinaryMessageContent(msg);
        else if (isInlandMessage(msg)) section = getInlandMessageContent(msg);
        else if (isLockMessage(msg)) section = getLockMessageContent(msg);
        else if (isTextMessage(msg)) section = getTextMessageContent(msg);
        else section = '<div class="meteo-tooltip">' + meteoHeader(msg.formattedTime, `DAC ${msg.message?.dac}, FI ${fiOf(msg.message || {})}`, 0, msg.count) + '</div>';
        if (msg.message && msg.message.mmsi) {
            const from = `<div style="font-size: 10px; opacity: 0.6; margin-top: 3px;">from ${sanitizeString(String(shipLabel(msg.message.mmsi)))}</div>`;
            section = section.slice(0, -6) + from + '</div>';
        }
        content += section;
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

                msg.formattedTime = formatTime(msg.timestamp);
                if (hasLocation) {
                    msg.message_lat = msg.message.lat;
                    msg.message_lon = msg.message.lon;
                }

                for (const other in binaryDB) {
                    const list = binaryDB[other].ship_messages;
                    const at = list.findIndex(m => m.key === msg.key);
                    if (at >= 0) list.splice(at, 1);
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
