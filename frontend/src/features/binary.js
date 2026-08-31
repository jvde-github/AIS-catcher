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
import Polygon from 'ol/geom/Polygon';
import Style from 'ol/style/Style';
import Stroke from 'ol/style/Stroke';
import Fill from 'ol/style/Fill';
import CircleStyle from 'ol/style/Circle';
import Text from 'ol/style/Text';
import { fromLonLat } from 'ol/proj';

import { settings } from '../core/state.js';
import { hexToRgb } from '../../shared/color.js';
import { hasValidCoords } from '../../shared/core/geo.js';
import { sanitizeString, formatTime } from '../../shared/core/text.js';

// { getActiveReceiver, getShipsDB, showDialog, getDialogModal, saveSettings, redrawMap }
let deps = null;

let binaryDB = {};
let binarySince = 0;
let binaryTimeout = 1800;
const binaryMaxPerShip = 250;

export function init(d) { deps = d; }

export function getDB() { return binaryDB; }
export function resetSince() { binarySince = 0; }

// ─── map layer ───────────────────────────────────────────────────────────────

const binaryVector = new VectorSource({ features: [] });

const binaryAssociatedOutline = new Style({
    image: new CircleStyle({
        radius: 12,
        fill: new Fill({ color: 'rgba(255, 255, 255, 0)' }),
        stroke: new Stroke({ color: 'rgba(255, 255, 255, 0.5)', width: 2 })
    }),
    zIndex: 200
});

const binaryStyleCache = new Map();

const tokenRgb = (prop, fallback) =>
    hexToRgb(getComputedStyle(document.documentElement).getPropertyValue(prop).trim() || fallback);

function badgeRed() {
    const [r, g, b] = tokenRgb('--overlay-danger', '#e11d48');
    return `rgba(${r}, ${g}, ${b}, 0.9)`;
}

const CAT_COLORS = { text: '#7c3aed', inland: '#0f766e', aton: '#d97706', zones: '#0857b1' };

function badgeColor(cat) {
    if (!settings.binary_color_class || !CAT_COLORS[cat]) return badgeRed();
    const [r, g, b] = hexToRgb(CAT_COLORS[cat]);
    return `rgba(${r}, ${g}, ${b}, 0.9)`;
}

const areaStyleCache = new Map();

function areaStyles(danger) {
    let cached = areaStyleCache.get(danger);
    if (cached) return cached;
    const [r, g, b] = danger ? tokenRgb('--overlay-danger', '#e11d48') : tokenRgb('--color-accent', '#0857b1');
    const canvas = document.createElement('canvas');
    canvas.width = canvas.height = 8;
    const ctx = canvas.getContext('2d');
    ctx.fillStyle = `rgba(${r}, ${g}, ${b}, 0.07)`;
    ctx.fillRect(0, 0, 8, 8);
    ctx.strokeStyle = `rgba(${r}, ${g}, ${b}, 0.3)`;
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    ctx.moveTo(-2, 10);
    ctx.lineTo(10, -2);
    ctx.moveTo(-6, 6);
    ctx.lineTo(6, -6);
    ctx.moveTo(2, 14);
    ctx.lineTo(14, 2);
    ctx.stroke();
    cached = [new Style({
        fill: new Fill({ color: ctx.createPattern(canvas, 'repeat') }),
        stroke: new Stroke({ color: `rgba(${r}, ${g}, ${b}, 0.8)`, width: 1.5 }),
        zIndex: 1
    })];
    areaStyleCache.set(danger, cached);
    return cached;
}

const countText = (count) => new Text({
    text: count.toString(),
    font: 'bold 9px Arial',
    fill: new Fill({ color: 'rgba(255, 255, 255, 0.85)' }),
    textAlign: 'center',
    textBaseline: 'middle'
});

const binaryStyle = function (feature) {
    if (feature.is_area) return areaStyles(!!feature.is_danger);

    const count = feature.get('binary_count') || feature.binary_count || 1;
    const isAssociated = feature.get('is_associated') || feature.is_associated;
    const highlight = settings.binary_messages === 'highlight';
    const cat = settings.binary_color_class ? (feature.binary_cat || 'data') : 'data';
    const key = cat + (isAssociated ? ':a:' : ':n:') + (highlight ? 'h:' : 'b:') + count;

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
                fill: new Fill({ color: badgeColor(cat) }),
                stroke: new Stroke({ color: 'rgba(255, 255, 255, 0.5)', width: 1 }),
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
                fill: new Fill({ color: badgeColor(cat) }),
                stroke: new Stroke({ color: 'rgba(255, 255, 255, 0.5)', width: 1.5 })
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
const METEO_KEYS = ['wspeed', 'airtemp', 'pressure', 'waterlevel', 'watertemp', 'waveheight', 'swellheight', 'visibility', 'cspeed', 'water_flow', 'humidity'];

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

const ATON_KEYS = ['asm_voltage_data', 'asm_light_status', 'asm_battery_status', 'asm_off_position_status',
    'asm_power_supply_type', 'asm_current_data', 'ana_int', 'ana_ext1', 'ana_ext2',
    'racon', 'health', 'stat_ext', 'off_position'];

export function isAtonMessage(msg) {
    const m = msg.message;
    return !!m && ATON_KEYS.some(k => m[k] != null);
}

export function isSignalMessage(msg) {
    return !!msg.message && msg.message.traffic_signal != null;
}

export function isAreaMessage(msg) {
    const m = msg.message;
    return !!m && (m.area_notice_type != null || m.area_shapes != null);
}

export function isMeteoMessage(msg) {
    const m = msg.message;
    if (!m) return false;
    if (m.dac == 1 && fiOf(m) == 31) return true;
    if ((m.dac == 316 || m.dac == 366) && fiOf(m) == 1) return true;
    return METEO_KEYS.some(k => m[k] != null);
}

export const BINARY_CATEGORIES = ['data', 'inland', 'text', 'aton', 'zones'];

function categoryOf(msg) {
    if (isTextMessage(msg)) return 'text';
    if (isInlandMessage(msg)) return 'inland';
    if (isAreaMessage(msg)) return 'zones';
    if (isAtonMessage(msg)) return 'aton';
    return 'data';
}

function binaryIncluded(msg) {
    const ex = settings.binary_exclude;
    const cat = categoryOf(msg);
    if (cat !== 'data') return !ex.includes(cat);
    if (isLockMessage(msg) || isSignalMessage(msg)) return !ex.includes('data');
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
    ['Gust', (m) => m.wgust != null && `${m.wgust.toFixed(1)} kts${dirSuffix(m.wgustdir)}`],
    ['Humidity', (m) => m.humidity != null && `${m.humidity}%`],
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

const RACON_STATES = ['no RACON installed', 'RACON not monitored', 'RACON operational', 'RACON error'];

const atonValue = (v) => sanitizeString(String(v));

const ATON_ROWS = [
    ['Position', (m) => {
        const off = m.off_position != null ? m.off_position : m.asm_off_position_status;
        return off != null && (off ? 'off station' : 'on station');
    }],
    ['Health', (m) => m.health != null && (m.health ? 'alarm' : 'good')],
    ['RACON', (m) => m.racon != null && (RACON_STATES[m.racon] || atonValue(m.racon))],
    ['Light', (m) => m.asm_light_status != null && atonValue(m.asm_light_status)],
    ['Battery', (m) => m.asm_battery_status != null && atonValue(m.asm_battery_status)],
    ['Supply', (m) => m.asm_power_supply_type != null && atonValue(m.asm_power_supply_type)],
    ['Voltage', (m) => m.asm_voltage_data != null && `${m.asm_voltage_data} V`],
    ['Current', (m) => m.asm_current_data != null && `${m.asm_current_data} A`],
    ['Analogue', (m) => m.ana_int != null && atonValue(m.ana_int)],
    ['Ext 1', (m) => m.ana_ext1 != null && atonValue(m.ana_ext1)],
    ['Ext 2', (m) => m.ana_ext2 != null && atonValue(m.ana_ext2)],
    ['Status', (m) => m.stat_ext != null && atonValue(m.stat_ext)],
];

const binaryCard = (binary, label, rows) => {
    const messages = Array.isArray(binary) ? binary : [binary];
    if (messages.length === 0) return '';
    messages.sort((a, b) => b.timestamp - a.timestamp);
    const item = messages[0];
    return '<div class="meteo-tooltip">' +
        meteoHeader(item.formattedTime, label, messages.length - 1, item.count) +
        rows(item.message) + seenLine(item) + '</div>';
};

const pad2 = (n) => String(n).padStart(2, '0');

export const getAtonMessageContent = (binary) => binaryCard(binary, 'AtoN Status', (m) => {
    const label = m.name || m.station_name || (typeof m.station_id === 'string' && m.station_id);
    let rows = label ? meteoRow('Station', sanitizeString(String(label))) : '';
    for (const [rowLabel, format] of ATON_ROWS) {
        const value = format(m);
        if (value) rows += meteoRow(rowLabel, value);
    }
    return rows;
});

const SIGNAL_STATES = ['not available', 'emergency: all vessels stop or divert', 'vessels shall not proceed',
    'vessels may proceed, one-way traffic', 'vessels may proceed, two-way traffic',
    'proceed only on specific orders'];

export const getSignalMessageContent = (binary) => binaryCard(binary, 'Traffic Signal', (m) => {
    let rows = '';
    if (m.station_name) rows += meteoRow('Station', sanitizeString(m.station_name));
    if (m.traffic_signal != null) rows += meteoRow('Signal', SIGNAL_STATES[m.traffic_signal] || `signal ${m.traffic_signal}`);
    if (m.next_signal) rows += meteoRow('Next', SIGNAL_STATES[m.next_signal] || `signal ${m.next_signal}`);
    if (m.hour < 24 && m.minute < 60) rows += meteoRow('At', `${pad2(m.hour)}:${pad2(m.minute)} UTC`);
    return rows;
});

const NOTICE_TYPES = {
    0: 'Caution: marine mammals habitat', 1: 'Caution: marine mammals, reduce speed',
    2: 'Caution: marine mammals, report sightings', 3: 'Caution: marine mammals, avoid area',
    4: 'Caution: protected habitat, reduce speed', 5: 'Caution: protected habitat, report sightings',
    6: 'Caution: protected habitat, avoid area', 7: 'Caution: derelicts', 8: 'Caution: traffic congestion',
    9: 'Caution: marine event', 10: 'Caution: divers down', 11: 'Caution: swim area', 12: 'Caution: dredge operations',
    13: 'Caution: survey operations', 14: 'Caution: underwater operation', 15: 'Caution: seaplane operations',
    16: 'Caution: nets in water', 17: 'Caution: cluster of fishing vessels', 18: 'Caution: fairway closed',
    19: 'Caution: harbour closed', 20: 'Caution: risk, see text', 21: 'Caution: underwater vehicle operation',
    23: 'Storm front (line squall)', 24: 'Hazardous sea ice', 25: 'Storm warning', 26: 'High wind',
    27: 'High waves', 28: 'Restricted visibility', 29: 'Strong currents', 30: 'Heavy icing',
    32: 'Fishing prohibited', 33: 'No anchoring', 34: 'Entry approval required', 35: 'Entry prohibited',
    36: 'Active military OPAREA', 37: 'Firing danger area', 38: 'Drifting mines',
    40: 'Anchorage open', 41: 'Anchorage closed', 42: 'Anchoring prohibited',
    56: 'Security alert level 1', 57: 'Security alert level 2', 58: 'Security alert level 3',
    64: 'Distress: vessel disabled and adrift', 65: 'Distress: sinking', 66: 'Distress: abandoning ship',
    67: 'Distress: person overboard', 68: 'Distress: ship aground',
    80: 'Instruction: contact VTS here', 81: 'Instruction: contact port administration',
    82: 'Instruction: do not proceed beyond this point', 83: 'Instruction: await instructions before proceeding',
    88: 'Information: pilot boarding position', 89: 'Information: icebreaker waiting area',
    90: 'Information: place of refuge', 91: 'Information: position of icebreakers',
    96: 'Chart feature: sunken vessel', 97: 'Chart feature: submerged object', 99: 'Chart feature: shoal area',
    120: 'Route: recommended route', 121: 'Route: alternative route', 122: 'Route: recommended route through ice',
    125: 'Other', 126: 'Cancellation'
};

const durationLabel = (d) => (d >= 60 ? `${Math.floor(d / 60)}h${d % 60 ? ` ${d % 60}m` : ''}` : `${d} min`);

export const getAreaMessageContent = (binary) => binaryCard(binary, 'Area Notice', (m) => {
    let rows = '';
    if (m.area_notice_name)
        rows += `<div style="font-size: 11px; font-weight: bold;">${sanitizeString(m.area_notice_name)}</div>`;
    if (m.area_notice_type != null) rows += meteoRow('Notice', NOTICE_TYPES[m.area_notice_type] || `type ${m.area_notice_type}`);
    if (m.month && m.day && m.hour < 24 && m.minute < 60)
        rows += meteoRow('From', `${pad2(m.month)}-${pad2(m.day)} ${pad2(m.hour)}:${pad2(m.minute)} UTC`);
    if (m.area_notice_duration != null) rows += meteoRow('Duration', durationLabel(m.area_notice_duration));
    return rows;
});

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

    const atons = messages.filter(isAtonMessage);
    if (atons.length > 0) content += getAtonMessageContent(atons);

    const signals = messages.filter(isSignalMessage);
    if (signals.length > 0) content += getSignalMessageContent(signals);

    const areas = messages.filter(isAreaMessage);
    if (areas.length > 0) content += getAreaMessageContent(areas);

    if (!content && messages.length > 0) content = genericCard(messages[0]);

    if (includeText) {
        const text = messages.filter(isTextMessage);
        if (text.length > 0) content += getTextMessageContent(text);
    }
    return content;
}

const genericCard = (msg) => '<div class="meteo-tooltip">' +
    meteoHeader(msg.formattedTime, `DAC ${msg.message?.dac}, FI ${fiOf(msg.message || {})}`, 0, msg.count) + '</div>';

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
        if (isAreaMessage(msg)) section = getAreaMessageContent(msg);
        else if (isSignalMessage(msg)) section = getSignalMessageContent(msg);
        else if (isAtonMessage(msg)) section = getAtonMessageContent(msg);
        else if (isMeteoMessage(msg)) section = getBinaryMessageContent(msg);
        else if (isInlandMessage(msg)) section = getInlandMessageContent(msg);
        else if (isLockMessage(msg)) section = getLockMessageContent(msg);
        else if (isTextMessage(msg)) section = getTextMessageContent(msg);
        else section = genericCard(msg);
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

export function setBinaryColorClass(v) {
    settings.binary_color_class = v;
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
            if (msg.message && msg.message.mmsi && binaryIncluded(msg)) {
                const mmsi = msg.message.mmsi;

                if (!binaryDB[mmsi]) {
                    binaryDB[mmsi] = {
                        ship_messages: [],
                        standalone_messages: []
                    };
                }

                msg.formattedTime = formatTime(msg.timestamp);
                const mlat = msg.message.lat != null ? msg.message.lat : msg.message.area_notice_lat;
                const mlon = msg.message.lon != null ? msg.message.lon : msg.message.area_notice_lon;
                if (mlat != null && mlon != null) {
                    msg.message_lat = mlat;
                    msg.message_lon = mlon;
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

const DEG = Math.PI / 180;

function offsetLonLat(lon, lat, east, north) {
    return [lon + east / (111320 * Math.cos(lat * DEG)), lat + north / 111320];
}

function shapeRing(rec) {
    const p = rec.split(',');
    const lon = +p[1], lat = +p[2];
    if (!hasValidCoords(lat, lon)) return null;
    if (p[0] === 'c') {
        const r = +p[3];
        if (!(r > 0)) return null;
        const ring = [];
        for (let d = 0; d <= 360; d += 10) ring.push(offsetLonLat(lon, lat, r * Math.sin(d * DEG), r * Math.cos(d * DEG)));
        return ring;
    }
    if (p[0] === 'r') {
        const e = +p[3], n = +p[4], o = +p[5] * DEG;
        const ex = [Math.cos(o), -Math.sin(o)], nx = [Math.sin(o), Math.cos(o)];
        return [[0, 0], [e, 0], [e, n], [0, n], [0, 0]].map(([x, y]) =>
            offsetLonLat(lon, lat, x * ex[0] + y * nx[0], x * ex[1] + y * nx[1]));
    }
    if (p[0] === 's') {
        const r = +p[3];
        let a = +p[4];
        const b = +p[5] > a ? +p[5] : +p[5] + 360;
        if (!(r > 0)) return null;
        const ring = [[lon, lat]];
        for (; a < b; a += 5) ring.push(offsetLonLat(lon, lat, r * Math.sin(a * DEG), r * Math.cos(a * DEG)));
        ring.push(offsetLonLat(lon, lat, r * Math.sin(b * DEG), r * Math.cos(b * DEG)));
        ring.push([lon, lat]);
        return ring;
    }
    return null;
}

function addAreaFeatures(msg) {
    msg.message.area_shapes.split(';').forEach((rec, i) => {
        const ring = shapeRing(rec);
        if (!ring) return;
        const feature = new OlFeature({ geometry: new Polygon([ring.map((ll) => fromLonLat(ll))]) });
        feature.binary = true;
        feature.is_area = true;
        feature.is_danger = msg.message.area_notice_type != null && msg.message.area_notice_type < 80;
        feature.binary_count = msg.count;
        feature.binary_messages = [msg];
        feature.setId(`binary-area-${msg.key}-${i}`);
        binaryVector.addFeature(feature);
    });
}

const newestCat = (msgs) => categoryOf(msgs.reduce((a, b) => (b.timestamp > a.timestamp ? b : a)));

const SHIP_PROXIMITY = 0.05;    // messages nearer than this badge on the ship

function addStandaloneFeature(msg, mmsi) {
    if (!hasValidCoords(msg.message_lat, msg.message_lon)) return;

    const feature = new OlFeature({ geometry: new Point(fromLonLat([msg.message_lon, msg.message_lat])) });

    feature.binary = true;
    feature.binary_count = msg.count || 1;
    feature.binary_messages = [msg];
    feature.binary_mmsi_counts = { [mmsi]: msg.count || 1 };
    feature.is_associated = false;
    feature.binary_cat = categoryOf(msg);
    feature.setId(`binary-item-${msg.key}`);

    binaryVector.addFeature(feature);
}

export function redrawBinaryMessages() {
    binaryVector.clear();

    if (settings.binary_messages === 'off') return;

    const shipsDB = deps.getShipsDB();

    for (const [mmsi, msgData] of Object.entries(binaryDB)) {
        if (!msgData.ship_messages || msgData.ship_messages.length === 0) continue;

        const allMessages = msgData.ship_messages;
        const ship = mmsi in shipsDB ? shipsDB[mmsi].raw : null;

        allMessages.forEach(msg => {
            if (msg.message && msg.message.area_shapes) addAreaFeatures(msg);
        });

        if (!ship || !hasValidCoords(ship.lat, ship.lon)) {
            // no ship to badge: every located message stands on its own position
            allMessages.forEach(msg => addStandaloneFeature(msg, mmsi));
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
            else addStandaloneFeature(msg, mmsi);
        });

        if (shipMessages.length > 0) {
            const feature = new OlFeature({ geometry: new Point(fromLonLat([ship.lon, ship.lat])) });

            feature.binary = true;
            feature.ship = ship;
            feature.binary_mmsi = mmsi;
            feature.binary_count = shipMessages.length;
            feature.binary_messages = shipMessages;
            feature.is_associated = true;
            feature.binary_cat = newestCat(shipMessages);
            feature.setId(`binary-ship-${mmsi}`);

            binaryVector.addFeature(feature);
        }
    }
}
