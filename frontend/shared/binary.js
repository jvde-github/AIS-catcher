// Application-specific (binary) AIS messages, the part that is the same on the
// viewer and the site: which DAC/FI payload is what, and the HTML a message or
// a list of them renders to. The host says how a category is coloured and how a
// vessel is named and linked; everything else is data in, markup out.

import { sanitizeString, formatTime } from './core/text.js';
import { hasValidCoords } from './core/geo.js';
import { hexToRgb } from './color.js';
import { modal } from './components.js';

// server Item::Kind order, as packed into a ship row's badge
export const KIND_CAT = ['text', 'inland', 'data', 'aton', 'signal', 'zones', 'lock', 'safety', 'station'];

// the packed badge on a ship row: count (4b) | newest kind (3b) | age bucket (2b)
export const decodeBadge = (v) => ({ count: v & 15, kind: (v >> 4) & 7, age: (v >> 7) & 3 });

// fade per age bucket (fresh, past 15 min, past 30 min), the stations' steps
export const AGE_FADE = [1, 0.75, 0.5];
// the whole layer sits at the stations' opacity; the age steps are relative to it
export const LAYER_ALPHA = 0.9;

export const BINARY_CATEGORIES = ['data', 'lock', 'signal', 'inland', 'text', 'aton', 'zones', 'safety'];

// no red and no green: those mean invalid and validated on the map
// safety is the one kind that means danger, so it takes the red
export const CAT_COLORS = { data: '#0891b2', text: '#7c3aed', inland: '#0f766e', aton: '#d97706', zones: '#0857b1', lock: '#4338ca', signal: '#ea580c', safety: '#dc2626', station: '#008000' };

const host = {
    color: (cat) => CAT_COLORS[cat] || CAT_COLORS.data,
    shipLabel: (mmsi) => `MMSI ${mmsi}`,
    shipLink: null,
};

// { color(cat) -> css colour, shipLabel(mmsi) -> text, shipLink(mmsi) -> html }
export function init(h) { Object.assign(host, h); }

const shipLabel = (mmsi) => host.shipLabel(mmsi);
const shipLink = (mmsi) => (host.shipLink ? host.shipLink(mmsi) : sanitizeString(String(shipLabel(mmsi))));

// ─── the kinds ───────────────────────────────────────────────────────────────

// the decoder emits `fid` on some paths and `fi` on others
const fiOf = (m) => (m.fid != null ? m.fid : m.fi);
const has = (m, keys) => keys.some((k) => m[k] != null);

// 360 is the "not available" sentinel for every AIS direction field
const dirSuffix = (d) => (d != null && d !== 360 ? ` / ${d}&deg;` : '');
const periodSuffix = (p) => (p != null ? `, ${p}s` : '');
const pad2 = (n) => String(n).padStart(2, '0');
const text = (v) => sanitizeString(String(v));

// one row of a card; a row function returns a falsy value when its field is absent
export const meteoRow = (label, value) =>
    `<div style="display: flex; justify-content: space-between; padding: 1px 0; white-space: nowrap;">` +
    `<span style="font-size: 11px; opacity: 0.6; margin-right: 12px;">${label}</span>` +
    `<span style="font-size: 11px; font-weight: bold;">${value}</span></div>`;

const rowsOf = (m, table) => table.map(([label, format]) => { const v = format(m); return v ? meteoRow(label, v) : ''; }).join('');

const PRESSURE_TEND = ['steady', 'decreasing', 'increasing'];
const METEO_KEYS = ['wspeed', 'airtemp', 'pressure', 'waterlevel', 'watertemp', 'waveheight', 'swellheight', 'visibility', 'cspeed', 'water_flow', 'humidity'];
const HYDRO_KEYS = ['watercurrent', 'currentspeed', 'currentdir', 'watertemp', 'waterlevel'];
const METEO_ROWS = [
    ['Wind', (m) => m.wspeed != null && `${m.wspeed.toFixed(1)} kts${dirSuffix(m.wdir)}`],
    ['Air', (m) => m.airtemp != null && `${m.airtemp.toFixed(1)}&deg;C`],
    ['Gust', (m) => m.wgust != null && `${m.wgust.toFixed(1)} kts${dirSuffix(m.wgustdir)}`],
    ['Humidity', (m) => m.humidity != null && `${m.humidity}%`],
    ['Pressure', (m) => m.pressure != null && m.pressure > 799 &&
        `${m.pressure.toFixed(1)} hPa` + (m.pressuretend != null ? ` (${PRESSURE_TEND[m.pressuretend]})` : '')],
    ['Current', (m) => { const v = m.watercurrent || m.currentspeed; return v != null && `${v.toFixed(1)} kts${dirSuffix(m.currentdir || m.currentdirection)}`; }],
    ['Water Level', (m) => m.waterlevel != null && `${m.waterlevel.toFixed(2)} m`],
    ['Water', (m) => m.watertemp != null && `${m.watertemp.toFixed(1)}&deg;C`],
    ['Wave', (m) => m.waveheight != null && `${m.waveheight.toFixed(1)} m${dirSuffix(m.wavedir)}${periodSuffix(m.waveperiod)}`],
    ['Swell', (m) => m.swellheight != null && `${m.swellheight.toFixed(1)} m${dirSuffix(m.swelldir)}${periodSuffix(m.swellperiod)}`],
    ['Visibility', (m) => m.visibility != null && `${m.visibility.toFixed(1)} nm`],
    ['Flow', (m) => m.water_flow != null && `${m.water_flow} m&sup3;/s`],
    ['Station', (m) => m.station_id && text(m.station_id)],
];

const ATON_KEYS = ['asm_voltage_data', 'asm_light_status', 'asm_battery_status', 'asm_off_position_status',
    'asm_power_supply_type', 'asm_current_data', 'ana_int', 'ana_ext1', 'ana_ext2', 'racon', 'health', 'stat_ext', 'off_position'];
const RACON_STATES = ['no RACON installed', 'RACON not monitored', 'RACON operational', 'RACON error'];
const ATON_ROWS = [
    ['Station', (m) => { const v = m.name || m.station_name || (typeof m.station_id === 'string' && m.station_id); return v && text(v); }],
    ['Position', (m) => { const off = m.off_position != null ? m.off_position : m.asm_off_position_status; return off != null && (off ? 'off station' : 'on station'); }],
    ['Health', (m) => m.health != null && (m.health ? 'alarm' : 'good')],
    ['RACON', (m) => m.racon != null && (RACON_STATES[m.racon] || text(m.racon))],
    ['Light', (m) => m.asm_light_status != null && text(m.asm_light_status)],
    ['Battery', (m) => m.asm_battery_status != null && text(m.asm_battery_status)],
    ['Supply', (m) => m.asm_power_supply_type != null && text(m.asm_power_supply_type)],
    ['Voltage', (m) => m.asm_voltage_data != null && `${m.asm_voltage_data} V`],
    ['Current', (m) => m.asm_current_data != null && `${m.asm_current_data} A`],
    ['Analogue', (m) => m.ana_int != null && text(m.ana_int)],
    ['Ext 1', (m) => m.ana_ext1 != null && text(m.ana_ext1)],
    ['Ext 2', (m) => m.ana_ext2 != null && text(m.ana_ext2)],
    ['Status', (m) => m.stat_ext != null && text(m.stat_ext)],
];

const SIGNAL_STATES = ['not available', 'emergency: all vessels stop or divert', 'vessels shall not proceed',
    'vessels may proceed, one-way traffic', 'vessels may proceed, two-way traffic', 'proceed only on specific orders'];
const SIGNAL_ROWS = [
    ['Station', (m) => m.station_name && text(m.station_name)],
    ['Signal', (m) => m.traffic_signal != null && (SIGNAL_STATES[m.traffic_signal] || `signal ${m.traffic_signal}`)],
    ['Next', (m) => m.next_signal && (SIGNAL_STATES[m.next_signal] || `signal ${m.next_signal}`)],
    ['At', (m) => m.hour < 24 && m.minute < 60 && `${pad2(m.hour)}:${pad2(m.minute)} UTC`],
];

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
const AREA_ROWS = [
    ['Notice', (m) => m.area_notice_type != null && (NOTICE_TYPES[m.area_notice_type] || `type ${m.area_notice_type}`)],
    ['From', (m) => m.month && m.day && m.hour < 24 && m.minute < 60 && `${pad2(m.month)}-${pad2(m.day)} ${pad2(m.hour)}:${pad2(m.minute)} UTC`],
    ['Duration', (m) => m.area_notice_duration != null && durationLabel(m.area_notice_duration)],
];

// type 23: what a base station asks of the vessels in its rectangle
const STATION_TYPES = { 0: 'all mobiles', 1: 'Class A', 2: 'Class B', 3: 'SAR airborne', 4: 'Class B SO', 5: 'Class B CS', 6: 'inland waterways', 10: 'base station coverage' };
const TXRX_MODES = ['both channels', 'channel A only', 'channel B only'];
const INTERVALS = ['as autonomous', '10 min', '6 min', '3 min', '1 min', '30 s', '15 s', '10 s', '5 s', 'next shorter', 'next longer'];
const GROUP_ROWS = [
    ['Applies to', (m) => m.station_type != null && (STATION_TYPES[m.station_type] || `station type ${m.station_type}`)],
    ['Ship type', (m) => m.shiptype_text && m.shiptype_text !== 'Not available' && text(m.shiptype_text)],
    ['Channels', (m) => m.txrx != null && TXRX_MODES[m.txrx]],
    ['Report every', (m) => m.interval != null && m.interval > 0 && INTERVALS[m.interval]],
    ['Quiet time', (m) => m.quiet > 0 && `${m.quiet} min`],
    ['Area', (m) => m.ne_lon != null && `${m.sw_lat.toFixed(2)}, ${m.sw_lon.toFixed(2)} to ${m.ne_lat.toFixed(2)}, ${m.ne_lon.toFixed(2)}`],
];

const LOCK_STATUS = ['operational', 'limited operation', 'out of order'];
// 20-bit packed UTC used by berthing times: month-day-hour-minute
const fmtMDHM = (v) => `${pad2((v >> 16) & 15)}-${pad2((v >> 11) & 31)}T${pad2((v >> 6) & 31)}:${pad2(v & 63)}Z`;
const LOCK_ROWS = [
    ['Fairway', (m) => m.fairway_section && text(`${m.fairway_section} ${m.fairway_hectometre || ''}`.trim())],
    ['Location', (m) => (m.un_locode || m.terminal_code) && text(`${m.un_country || ''}${m.un_locode || ''} ${m.terminal_code || ''}`.trim())],
    ['RTA', (m) => m.rta && text(m.rta)],
    ['ETA', (m) => m.dac == 200 && m.eta && text(m.eta)],
    ['Status', (m) => m.lock_status != null && LOCK_STATUS[m.lock_status]],
    ['Tugs', (m) => m.tugboats != null && m.tugboats != 7 && m.tugboats],
    ['Air draught', (m) => m.air_draught && `${m.air_draught.toFixed(2)} m`],
    ['Berth', (m) => m.berth_name && text(`${m.berth_name}${m.berth_number ? ' #' + m.berth_number : ''}`)],
    ['Arrival', (m) => m.berth_arrival_time && fmtMDHM(m.berth_arrival_time)],
    ['Departure', (m) => m.berth_departure_time && fmtMDHM(m.berth_departure_time)],
    ['Position', (m) => m.berth_lat != null && m.berth_lon != null && `${m.berth_lat.toFixed(5)}, ${m.berth_lon.toFixed(5)}`],
    ['Lock', (m) => m.lock_id && text(m.lock_id)],
    ['Vessel', (m) => m.vessel_name && text(m.vessel_name)],
    ['Last', (m) => m.last_location && `${text(m.last_location)} ${text(m.last_ata || '')}`],
    ['Next', (m) => m.first_lock && `${text(m.first_lock)} ${text(m.first_lock_eta || '')}`],
    ['Then', (m) => m.second_lock && `${text(m.second_lock)} ${text(m.second_lock_eta || '')}`],
    ['Delay at', (m) => m.delay_lock && text(m.delay_lock)],
];
const lockExtras = (m) => {
    let rows = '';
    if (typeof m.tidal === 'string')
        m.tidal.split(';').forEach((seg, i) => {
            const [la, lo, dir, spd] = seg.split(',');
            rows += meteoRow(`Current ${i + 1}`, `${spd < 0 ? '?' : spd + ' kn'}${dir >= 0 ? ' / ' + dir + '&deg;' : ''} @ ${(+la).toFixed(3)}, ${(+lo).toFixed(3)}`);
        });
    if (typeof m.lock_schedule === 'string')
        for (const entry of m.lock_schedule.split(';')) {
            const [vessel, dir, eta] = entry.split(',');
            rows += meteoRow(text(vessel || ''), `${dir === '1' ? 'up' : 'down'} ${text(eta || '')}`);
        }
    return rows;
};
const lockLabel = (m) => m.rta != null ? 'Lock RTA' : m.dac == 200 && m.eta != null ? 'Lock ETA'
    : m.berth_name != null || m.berth_arrival_time != null ? 'Berthing' : m.tidal != null ? 'Tidal Currents'
    : m.message_id == 2 ? 'Estimated Lock Times' : 'Lockage Order';

const PERSONS_ROWS = [
    ['Crew', (m) => m.crew_count != null && m.crew_count],
    ['Passengers', (m) => m.passenger_count != null && m.passenger_count],
    ['Personnel', (m) => m.shipboard_personnel_count != null && m.shipboard_personnel_count],
];

// Every kind in one place: how a message is recognised (first match wins, in
// this order), what the card is called, and what its rows are. `label` and
// `rows` take the message payload; `label` also gets the whole item.
const KINDS = [
    { cat: 'safety', name: 'Safety message',
      // types 12 and 14
      test: (m, item) => item.type == 12 || item.type == 14,
      label: (m, item) => (item.type == 12 ? 'Safety Message (addressed)' : 'Safety Message') + (isUrgent(item) ? '' : ' (routine)'),
      rows: (m) => `<div style="font-size: 11px; font-weight: bold; white-space: pre-wrap;">${text(m.text || '')}</div>` },
    { cat: 'text', name: 'Text',
      // free text (DAC 1 FI 0/29/30)
      test: (m) => m.text != null,
      label: (m, item) => (item.type == 6 || fiOf(m) == 30 ? 'Text Message (addressed)' : 'Text Message'),
      rows: (m) => `<div style="font-size: 11px; font-weight: bold; white-space: pre-wrap;">${text(m.text || '')}</div>` +
          (m.ack_required ? meteoRow('Ack', 'requested') : '') },
    { cat: 'inland', name: 'Persons on board',
      test: (m) => m.dac == 200 && fiOf(m) == 55,
      label: () => 'Persons on Board', rows: (m) => rowsOf(m, PERSONS_ROWS) },
    { cat: 'zones', name: 'Area',
      // an area notice (DAC 1 FI 22) or a base station's group assignment rectangle (type 23)
      test: (m) => m.area_notice_type != null || m.area_shapes != null || m.ne_lon != null,
      label: (m) => (m.ne_lon != null && m.area_notice_type == null ? 'Group Assignment' : 'Area Notice'),
      rows: (m) => (m.area_notice_name ? `<div style="font-size: 11px; font-weight: bold;">${text(m.area_notice_name)}</div>` : '') +
          rowsOf(m, AREA_ROWS) + rowsOf(m, GROUP_ROWS) },
    { cat: 'aton', name: 'AtoN status',
      test: (m) => has(m, ATON_KEYS),
      label: () => 'AtoN Status', rows: (m) => rowsOf(m, ATON_ROWS) },
    { cat: 'lock', name: 'Lock',
      test: (m) => ((m.dac == 316 || m.dac == 366) && fiOf(m) == 2) || (m.dac == 200 && [21, 22].includes(fiOf(m))) || (m.dac == 1 && [20, 32].includes(fiOf(m))),
      label: lockLabel, rows: (m) => rowsOf(m, LOCK_ROWS) + lockExtras(m) },
    { cat: 'signal', name: 'Traffic signal',
      test: (m) => m.traffic_signal != null,
      label: () => 'Traffic Signal', rows: (m) => rowsOf(m, SIGNAL_ROWS) },
    { cat: 'data', name: 'Meteo',
      test: (m) => (m.dac == 1 && fiOf(m) == 31) || ((m.dac == 316 || m.dac == 366) && fiOf(m) == 1) || has(m, METEO_KEYS),
      label: (m) => (has(m, HYDRO_KEYS) ? 'Meteo & Hydro' : 'Meteo'), rows: (m) => rowsOf(m, METEO_ROWS) },
];
const kindOf = (msg) => (msg.message && KINDS.find((k) => k.test(msg.message, msg))) || null;

// ─── urgency ─────────────────────────────────────────────────────────────────

// A safety message has no priority field: a distress device sending it (AIS-SART
// 970, man-overboard 972, EPIRB 974) or the words used for something serious say it matters.
const DISTRESS_DEVICE = /^97[024]/;
const URGENT_WORDS = /\b(MAYDAY|PAN ?PAN|SECURITE|MOB|MAN ?OVER ?BOARD|OVERBOARD|SOS|DISTRESS|EMERGENCY|ACCIDENT|COLLISION|AGROUND|GROUND(ING)?|FIRE|SINKING|CAPSIZ|DANGER|WARNING|KEEP AWAY|STAY AWAY|KEEP CLEAR|NOT UNDER COMMAND|NUC)\b/i;
export const isStation = (mmsi) => mmsi >= 2000000 && mmsi <= 9999999;
export const isDistressDevice = (mmsi) => DISTRESS_DEVICE.test(String(mmsi));

export function isUrgent(msg) {
    if (!msg.message || (msg.type != 12 && msg.type != 14)) return false;
    const sender = msg.sender || msg.message.mmsi;
    const words = String(msg.message.text || '');
    if (isDistressDevice(sender)) return !/\bTEST\b/i.test(words);
    return URGENT_WORDS.test(words);
}


export const KIND_LABEL = Object.fromEntries(KINDS.map((k) => [k.cat, k.name]));

export function categoryOf(msg) {
    const k = kindOf(msg);
    return k ? k.cat : 'data';
}

// one decoded record, ready for the renderers
export function decorate(msg) {
    msg.cat = categoryOf(msg);
    msg.sender = msg.sender || msg.message.mmsi;
    msg.anchor = msg.anchor || msg.message.dest_mmsi || 0;
    msg.formattedTime = formatTime(msg.timestamp);
    const mlat = msg.message.lat != null ? msg.message.lat : msg.message.area_notice_lat;
    const mlon = msg.message.lon != null ? msg.message.lon : msg.message.area_notice_lon;
    if (mlat != null && mlon != null) {
        msg.message_lat = mlat;
        msg.message_lon = mlon;
    }
    return msg;
}

// ─── rendering ───────────────────────────────────────────────────────────────

// the bar's width is the host's: 3px in a list, the band width in a hover card
export const cardOpen = (cat) => `<div class="meteo-tooltip" style="border-left-color: ${host.color(cat)}">`;

// which way the message went, seen from the vessel the card is about: in when
// it was addressed to it, out when it sent it (to a station or a map point)
export const dirIcon = (m, ctx) => {
    if (!ctx || !ctx.mmsi) return '';
    if (m.anchor && m.anchor === ctx.mmsi) return '<span class="msg-dir" title="addressed to this vessel">&#8601;</span>';
    if ((m.sender || (m.message && m.message.mmsi)) === ctx.mmsi) return '<span class="msg-dir" title="sent by this vessel">&#8599;</span>';
    return '';
};

// orange caption line of every card
// the caption row: the kind's glyph at text height, the time and label, and
// on a list card a pin to the message's place when it has one
export const meteoHeader = (time, label, icon = '', cat = null, pin = '') =>
    `<div class="msg-head" style="color: #FFA500;">${cat ? glyphImg(cat) : ''}` +
    `<span>${icon}${time} - ${label}</span>${pin}</div>`;

const glyphImg = (cat) => {
    try { return `<img class="msg-glyph" src="${glyphURL(cat)}" alt="">`; } catch { return ''; }
};

const PIN_SVG = '<svg viewBox="0 0 24 24"><path d="M12 2C8.1 2 5 5.1 5 9c0 5.2 7 13 7 13s7-7.8 7-13c0-3.9-3.1-7-7-7zm0 9.5a2.5 2.5 0 1 1 0-5 2.5 2.5 0 0 1 0 5z"/></svg>';

// where a message belongs: its own point when it carries one, else its sender's marker when that is on the map
export const messagePlace = (m, ctx) => {
    if (m.message_lat != null && m.message_lon != null) return [m.message_lat, m.message_lon];
    const sender = m.sender || (m.message && m.message.mmsi);
    return sender && ctx && ctx.locate ? ctx.locate(sender) : null;
};

const pinHTML = (m, ctx) => {
    const at = ctx && ctx.link ? messagePlace(m, ctx) : null;
    return at ? `<a href="javascript:void(0)" class="msg-pin" data-lat="${at[0]}" data-lon="${at[1]}" title="Show on the map">${PIN_SVG}</a>` : '';
};

// who sent it and, for an addressed message, to whom; a vessel's own
// broadcast in its own card says nothing. Links when the host wants them.
export const routeLine = (m, ctx) => {
    const name = (mmsi) => (ctx && ctx.link ? shipLink(mmsi) : text(shipLabel(mmsi)));
    const sender = m.sender || (m.message && m.message.mmsi);
    if (!sender) return '';
    let line;
    if (m.anchor) line = `${name(sender)} &rarr; ${name(m.anchor)}`;
    else if (!ctx || ctx.mmsi !== sender) line = `from ${name(sender)}`;
    else return '';
    return `<div style="font-size: 10px; opacity: 0.6; margin-top: 3px;">${line}</div>`;
};

const seenLine = (m) => m.count > 1
    ? `<div style="font-size: 10px; opacity: 0.6;">seen ${m.count} times since ${formatTime(m.first)}</div>`
    : '';

// One card for a kind: the newest of `messages` gives the caption and rows.
// `ctx.mmsi` names the vessel the card is about.
function card(kind, messages, ctx) {
    const sorted = [...messages].sort((a, b) => b.timestamp - a.timestamp);
    const item = sorted[0], m = item.message;
    const label = kind ? kind.label(m, item) : `DAC ${m.dac}, FI ${fiOf(m)}`;
    const cat = kind ? kind.cat : 'data';
    return cardOpen(cat) +
        meteoHeader(item.formattedTime, label, dirIcon(item, ctx), cat, pinHTML(item, ctx)) +
        (kind ? kind.rows(m) : '') + seenLine(item) + routeLine(item, ctx) + '</div>';
}

// The sections of a hover card: one per kind present, in KINDS order. Text
// only where asked for, since a map point never carries any.
export function tooltipSections(messages, includeText = false, ctx = null) {
    if (!messages || messages.length === 0) return '';
    let content = '';
    for (const kind of KINDS) {
        if (kind.cat === 'text' && !includeText) continue;
        const of = messages.filter((msg) => kindOf(msg) === kind);
        if (of.length) content += card(kind, of, ctx);
    }
    if (!content) content = card(null, messages, ctx);
    return content;
}

// every message as its own card, newest first; the route lines link
export function getBinaryMessageList(messages, ctx = null) {
    if (!messages || messages.length === 0) return '<p>No messages available</p>';
    const linked = { ...(ctx || {}), mmsi: 0, link: true };
    const sorted = [...messages].sort((a, b) => b.timestamp - a.timestamp);
    return '<div class="binary-messages-list">' + sorted.map((msg) => card(kindOf(msg), [msg], linked)).join('') + '</div>';
}

// which way a message went for the vessel a list is about
export const isSentBy = (m, mmsi) => (m.sender || (m.message && m.message.mmsi)) === mmsi;

// the dialog every message list opens in, level with the ship card below the header
export function messageDialog() {
    const dlg = modal({ id: 'binary-messages', cardClass: 'modal-messages' });
    const css = getComputedStyle(document.documentElement);
    const inset = parseFloat(css.getPropertyValue('--map-inset-top')) || 10;
    const gap = parseFloat(css.getPropertyValue('--size-map-gap')) || 0;
    const map = document.getElementById('map');
    dlg.root.style.setProperty('--msg-dialog-top', `${Math.round((map ? map.getBoundingClientRect().top : 0) + inset - gap)}px`);
    return dlg;
}

// the tabs switch through one listener on the document: the viewer's content
// security policy allows no inline handlers, and no host has to wire anything
let tabsBound = false;
function bindTabs() {
    if (tabsBound || typeof document === 'undefined') return;
    tabsBound = true;
    document.addEventListener('click', (e) => {
        const t = e.target.closest ? e.target.closest('.msg-tabs .tab') : null;
        if (!t) return;
        const r = t.closest('.msg-tabs');
        r.querySelectorAll('.tab').forEach((x) => x.classList.toggle('active', x === t));
        r.querySelectorAll('.msg-pane').forEach((p) => { p.style.display = p.dataset.pane === t.dataset.tab ? '' : 'none'; });
    });
}

// A vessel's dialog: what it received on one tab, what it sent on the other,
// the side with anything in front, an empty side simply empty.
export function getBinaryMessageTabs(messages, mmsi, ctx = null) {
    bindTabs();
    const sent = (messages || []).filter((m) => isSentBy(m, mmsi));
    const received = (messages || []).filter((m) => !isSentBy(m, mmsi));
    const front = !received.length && sent.length ? 'sent' : 'received';
    const tab = (id, label, n) =>
        `<div class="tab${id === front ? ' active' : ''}" data-tab="${id}">${label} <span class="msg-tab-n">${n}</span></div>`;
    const pane = (id, list) =>
        `<div class="msg-pane" data-pane="${id}"${id === front ? '' : ' style="display: none"'}>` +
        (list.length ? getBinaryMessageList(list, ctx) : '') + '</div>';
    return '<div class="msg-tabs"><div class="tabs msg-tabbar">' +
        tab('received', 'Received', received.length) + tab('sent', 'Sent', sent.length) + '</div>' +
        pane('received', received) + pane('sent', sent) + '</div>';
}

// ─── marks on the map ────────────────────────────────────────────────────────

// What a message is, as a small white glyph: waves for a sensor, a padlock for
// a lock, the AtoN diamond, three lights for a signal, a frame for an area, a
// figure for persons on board, a bubble for text. `s` is the half size.
const ANTENNA_D = 'M198-278q-60-58-89-133T80-560q0-74 29-149t89-133l35 35q-50 49-76.5 116.5T130-560q0 63 26.5 130.5T233-313l-35 35Zm92-92q-40-37-59-89.5T212-560q0-48 19-100.5t59-89.5l35 35q-29 29-46 72.5T262-560q0 35 17.5 79.5T325-405l-35 35Zm4 290 133-405q-17-12-27.5-31T389-560q0-38 26.5-64.5T480-651q38 0 64.5 26.5T571-560q0 25-10.5 44T533-485L666-80h-59l-29-90H383l-30 90h-59Zm108-150h156l-78-238-78 238Zm268-140-35-35q29-29 46-72.5t17-82.5q0-35-17.5-79.5T635-715l35-35q39 37 58.5 89.5T748-560q0 47-19.5 100T670-370Zm92 92-35-35q49-49 76-116.5T830-560q0-63-27-130.5T727-807l35-35q60 58 89 133t29 149q0 75-27.5 149.5T762-278Z';
let antenna = null;

export function kindGlyph(ctx, cat, x, y, s) {
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.95)';
    ctx.fillStyle = 'rgba(255, 255, 255, 0.95)';
    ctx.lineWidth = Math.max(1.2, s * 0.32);
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';
    switch (cat) {
    case 'lock': {
        ctx.beginPath();
        ctx.arc(x, y - s * 0.25, s * 0.55, Math.PI, 0);
        ctx.stroke();
        ctx.fillRect(x - s * 0.8, y - s * 0.25, s * 1.6, s * 1.15);
        break;
    }
    case 'aton': {
        ctx.beginPath();
        ctx.moveTo(x, y - s);
        ctx.lineTo(x + s * 0.8, y);
        ctx.lineTo(x, y + s);
        ctx.lineTo(x - s * 0.8, y);
        ctx.closePath();
        ctx.fill();
        break;
    }
    case 'signal': {
        for (const dy of [-1, 0, 1]) {
            ctx.beginPath();
            ctx.arc(x, y + dy * s * 0.72, s * 0.3, 0, 2 * Math.PI);
            ctx.fill();
        }
        break;
    }
    case 'zones': {
        ctx.strokeRect(x - s * 0.8, y - s * 0.8, s * 1.6, s * 1.6);
        break;
    }
    case 'inland': {
        ctx.beginPath();
        ctx.arc(x, y - s * 0.5, s * 0.36, 0, 2 * Math.PI);
        ctx.fill();
        ctx.beginPath();
        ctx.arc(x, y + s * 0.95, s * 0.85, Math.PI, 0);
        ctx.fill();
        break;
    }
    case 'text': {
        ctx.strokeRect(x - s * 0.9, y - s * 0.65, s * 1.8, s * 1.2);
        break;
    }
    case 'station': {
        // the antenna the viewer has always used, drawn from its path
        const k = (2.7 * s) / 960;
        ctx.save();
        ctx.translate(x - 480 * k, y + 480 * k);
        ctx.scale(k, k);
        ctx.fill(antenna || (antenna = new Path2D(ANTENNA_D)));
        ctx.restore();
        break;
    }
    case 'safety': {
        // a warning triangle with its mark
        ctx.beginPath();
        ctx.moveTo(x, y - s);
        ctx.lineTo(x + s * 1.05, y + s * 0.8);
        ctx.lineTo(x - s * 1.05, y + s * 0.8);
        ctx.closePath();
        ctx.stroke();
        ctx.fillRect(x - s * 0.12, y - s * 0.35, s * 0.24, s * 0.7);
        ctx.fillRect(x - s * 0.12, y + s * 0.45, s * 0.24, s * 0.22);
        break;
    }
    default: {
        for (const dy of [-0.4, 0.4]) {
            const yy = y + dy * s;
            ctx.beginPath();
            ctx.moveTo(x - s, yy);
            ctx.quadraticCurveTo(x - s / 2, yy - s * 0.6, x, yy);
            ctx.quadraticCurveTo(x + s / 2, yy + s * 0.6, x + s, yy);
            ctx.stroke();
        }
    }
    }
}

function disc(ctx, cat, cx, cy, R, [r, g, b], fade, ring = `rgba(255, 255, 255, ${0.5 * fade})`) {
    ctx.beginPath();
    ctx.arc(cx, cy, R, 0, 2 * Math.PI);
    ctx.fillStyle = `rgba(${r}, ${g}, ${b}, ${0.9 * fade})`;
    ctx.fill();
    ctx.strokeStyle = ring;
    ctx.lineWidth = 1.5;
    ctx.stroke();
    ctx.globalAlpha = fade;
    kindGlyph(ctx, cat, cx, cy, R * 0.52);
    ctx.globalAlpha = 1;
}

// A nameless marker standing on its own: the disc with its glyph, centred.
// `ring` overrides the white edge where the disc sits on a light ground.
export function discCanvas(cat, rgb, fade = 1, ring) {
    const scale = window.devicePixelRatio || 1;
    const R = 9, SIZE = 2 * (R + 2);
    const canvas = document.createElement('canvas');
    canvas.width = canvas.height = SIZE * scale;
    const ctx = canvas.getContext('2d');
    ctx.scale(scale, scale);
    disc(ctx, cat, SIZE / 2, SIZE / 2, R, rgb, fade, ring);
    return { canvas, size: SIZE };
}

// The badge on a vessel marker: the disc at the upper right with a hairline to the
// vessel's centre. The canvas is centred on the vessel; the host wraps it in an icon.
export function badgeCanvas(cat, rgb, fade = 1) {
    const scale = window.devicePixelRatio || 1;
    const R = 8, OFF = 10, SIZE = 2 * (OFF + R + 2);
    const canvas = document.createElement('canvas');
    canvas.width = canvas.height = SIZE * scale;
    const ctx = canvas.getContext('2d');
    ctx.scale(scale, scale);
    const c = SIZE / 2, cx = c + OFF, cy = c - OFF;
    const [r, g, b] = rgb;
    ctx.strokeStyle = `rgba(${r}, ${g}, ${b}, ${0.9 * fade})`;
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    ctx.moveTo(c, c);
    ctx.lineTo(cx, cy);
    ctx.stroke();
    disc(ctx, cat, cx, cy, R, rgb, fade);
    return { canvas, size: SIZE };
}

// ─── map objects: what both maps derive from a marker row ────────────────────

const FRESH_S = 900, STALE_S = 1800;

// the age bucket of a mark, from its timestamp and the server's clock
export const ageBucket = (t, ref) => { const age = ref - t; return age > STALE_S ? 2 : age > FRESH_S ? 1 : 0; };

export const ageText = (t, ref) => { const min = Math.max(0, Math.round((ref - t) / 60)); return min < 1 ? 'just now' : `${min} min ago`; };

// notice types below 80 are cautions and warnings; -1 is no notice at all
export const isDangerArea = (o) => o.atype != null && o.atype >= 0 && o.atype < 80;
// a group assignment's rectangle: an area with no notice type
export const isGroupArea = (o) => !!o.shapes && (o.atype == null || o.atype < 0);

// the head of a marker's hover card: its label or kind, and who sent it
// the vessel band's colour: its validity, grey when nothing is known
export const validityBand = (v) => (v === 1 ? '#10b981' : v === -1 ? '#ef4444' : 'lightgrey');

export function markerHead(o, shipLabel, color) {
    const cat = KIND_CAT[o.kind] || 'data';
    let html = `<div class="tip-band" style="--band: ${color}"><div class="tooltip-card">` +
        `<div>${text(o.label || KIND_LABEL[cat] || cat)}</div></div>`;
    if (o.senders && o.senders.length) {
        html += '<div style="margin-top: 5px; font-size: 0.9em;"><span class="tooltip-dim">From</span> ' +
            o.senders.slice(0, 3).map(([mmsi, n]) => `${text(shipLabel(mmsi))} (${n})`).join(', ') +
            (o.senders.length > 3 ? ` and ${o.senders.length - 3} more` : '') + '</div>';
    }
    return html + '</div>';
}

// the first line of a marker's hover card, before its members are fetched
export function markerCaption(o, ref) {
    const cat = KIND_CAT[o.kind] || 'data';
    return cardOpen(cat) + meteoHeader(formatTime(o.t), `${text(KIND_LABEL[cat] || cat)} &middot; ${ageText(o.t, ref)}`) + '</div>';
}

// ─── area geometry ───────────────────────────────────────────────────────────

const DEG = Math.PI / 180;
const offsetLonLat = (lon, lat, east, north) => [lon + east / (111320 * Math.cos(lat * DEG)), lat + north / 111320];

// one shape record - c,lon,lat,radius | r,lon,lat,east,north,orient | s,lon,lat,radius,from,to - as a lon/lat ring
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
        return [[0, 0], [e, 0], [e, n], [0, n], [0, 0]].map(([x, y]) => offsetLonLat(lon, lat, x * ex[0] + y * nx[0], x * ex[1] + y * nx[1]));
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

// every ring a marker's shapes describe, in lon/lat
export const areaRings = (shapes) => shapes.split(';').map(shapeRing).filter(Boolean);

// ─── more marks ──────────────────────────────────────────────────────────────

// a hatched 8 px tile in a colour, for an area's fill pattern
export function hatchCanvas([r, g, b]) {
    const canvas = document.createElement('canvas');
    canvas.width = canvas.height = 8;
    const ctx = canvas.getContext('2d');
    ctx.fillStyle = `rgba(${r}, ${g}, ${b}, 0.07)`;
    ctx.fillRect(0, 0, 8, 8);
    ctx.strokeStyle = `rgba(${r}, ${g}, ${b}, 0.3)`;
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    ctx.moveTo(-2, 10); ctx.lineTo(10, -2);
    ctx.moveTo(-6, 6); ctx.lineTo(6, -6);
    ctx.moveTo(2, 14); ctx.lineTo(14, 2);
    ctx.stroke();
    return canvas;
}

// a rounded pill with a label: map text backgrounds are sharp rectangles
export function pillCanvas(label, color) {
    const scale = window.devicePixelRatio || 1;
    const font = 'bold 9px Arial';
    const meas = document.createElement('canvas').getContext('2d');
    meas.font = font;
    const w = Math.ceil(meas.measureText(label).width) + 14, h = 17;
    const canvas = document.createElement('canvas');
    canvas.width = (w + 2) * scale;
    canvas.height = (h + 2) * scale;
    const ctx = canvas.getContext('2d');
    ctx.scale(scale, scale);
    ctx.beginPath();
    if (ctx.roundRect) ctx.roundRect(1, 1, w, h, h / 2); else ctx.rect(1, 1, w, h);   // roundRect needs Safari 16.4+
    ctx.fillStyle = color;
    ctx.fill();
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.6)';
    ctx.lineWidth = 1.2;
    ctx.stroke();
    ctx.font = font;
    ctx.fillStyle = 'rgba(255, 255, 255, 0.92)';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(label, 1 + w / 2, 1 + h / 2 + 0.5);
    return { canvas, width: w + 2, height: h + 2 };
}

// ─── glyphs in HTML ──────────────────────────────────────────────────────────

const glyphUrls = new Map();

// a kind's disc as a data URL, for a card header or a cell
// the disc as an image for a card header; its edge is grey on the light theme, where white would vanish
export function glyphURL(cat) {
    const dark = document.documentElement.classList.contains('dark');
    const key = cat + (dark ? ':dark' : ':light');
    let url = glyphUrls.get(key);
    if (!url) {
        const [r, g, b] = hexToRgb(CAT_COLORS[cat] || CAT_COLORS.data);
        url = discCanvas(cat, [r, g, b], 1, dark ? undefined : 'rgba(0, 0, 0, 0.15)').canvas.toDataURL();
        glyphUrls.set(key, url);
    }
    return url;
}

// one disc per kind, as a link the host wires up with `attrs` (an onclick or a data-action)
export function glyphsHTML(cats, attrs = '') {
    if (!cats.length) return '';
    const title = cats.map((c) => KIND_LABEL[c] || (c === 'station' ? 'Receiving station' : c)).join(', ') + ' \u00b7 show';
    return `<a href="javascript:void(0)" class="msg-count" title="${text(title)}" ${attrs}>` +
        cats.map((c) => `<img src="${glyphURL(c)}" alt="${text(KIND_LABEL[c] || c)}">`).join('') + '</a>';
}

// the kinds behind a badge word and, once fetched, behind a vessel's messages: newest first, no repeats
export function kindsOf(messages) {
    const cats = [];
    for (const m of [...messages].sort((x, y) => y.timestamp - x.timestamp)) if (!cats.includes(m.cat)) cats.push(m.cat);
    return cats;
}
