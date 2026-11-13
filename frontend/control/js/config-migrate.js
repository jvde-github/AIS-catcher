// An older file is brought to the current layout each time it is fetched, so whatever a page
// saves is in that layout; nothing is written until a page saves.
//   - the default station is station 0 of `stations`, not the viewer section's station keys
//   - the main server (beside the control panel) is the entry of `services` marked main, not
//     `control.viewer`; how ships are kept is `tracking`
//   - a web viewer of the `server` section is an entry of `services`: on the site's ships, or on
//     ships of its own behind its message filter, which goes under its `tracking`
//   - a receiver's settings at the top of the file are a receiver of the list; its kind of device
//     is in capitals, its model is its engines; the sensitivity switch is gone
//   - an output's json and json_full switches are its msgformat; a server's message switch is msg;
//     replay_time is track_time
//   - whether a server shows where its stations are is its own share_loc, not each station's

import { isActive, on } from './dom.js';
import { num, validPos } from './stations.js';
import { DEVICE_BLOCKS } from './inputs.js';

// a receiver's settings, before the receivers were a list
const RECEIVER_KEYS = ['serial', 'input', 'verbose', 'model', 'engines', 'meta', 'own_mmsi', ...DEVICE_BLOCKS];

const STATION_KEYS = ['station', 'lat', 'lon', 'alt', 'share_loc', 'use_gps', 'own_mmsi'];
const TRACKING_KEYS = ['file', 'backup', 'history', 'track_time', 'replay_time', 'track_memory', 'max_ships', 'cutoff',
    'expire', 'server_mode', 'places'];
// what the viewer section took that means nothing on the main server: its own port and address
// (the main server's are the control panel's), its per-receiver views and the receivers it
// showed, and switches that did nothing
const DROPPED_KEYS = ['port', 'port_min', 'port_max', 'ip_bind', 'split', 'zone', 'groups_in', 'cdn', 'msgs', 'zlib',
    'reuse_port', 'stats_on_close'];
// the terms of a message filter, older names too; a web viewer's, and what picked its receivers: a tracker of its own
export const FILTER_KEYS = ['filter', 'allow_type', 'block_type', 'allow_mmsi', 'block_mmsi', 'allow_channel', 'allow_repeat',
    'block_repeat', 'remove_empty', 'unique', 'position_interval', 'own_interval', 'exclude_errors', 'exclude_quality',
    'only_errors', 'stations', 'id', 'gps', 'ais', 'select_mmsi', 'select_channel', 'select_repeat', 'select_id', 'downsample'];
const ROUTE_KEYS = ['zone', 'groups_in'];
// what the viewer of the `server` section took that a server has no use for
const VIEWER_DROPPED = ['port_min', 'port_max', 'split', 'cdn', 'msgs', 'zlib', 'reuse_port', 'stats_on_close'];

// A web viewer of the `server` section as an entry of `services`. One behind a filter, or on
// some receivers, keeps ships of its own: its tracking goes with it. Any other reads the site's
// ships: its tracking keys are the site's `tracking`, its station keys station 0's, where
// nothing says otherwise yet.
function serverFromViewer(v, cfg) {
    const entry = {};
    const own = {};
    // a viewer switched off stays a server switched off, with its settings, and changes nothing of the site's
    const off = v.active !== undefined && !on(v.active);
    // filtered as the station reads it: a filter with a term, or receivers picked; `filter` alone lets every message through
    const terms = FILTER_KEYS.some(k => k !== 'filter' && k in v);
    const filtered = (on(v.filter) && terms) || ('zone' in v && String(v.zone).length) || ('groups_in' in v);
    if (!filtered && !off && STATION_KEYS.some(k => k in v)) {
        const list = Array.isArray(cfg.stations) ? cfg.stations : (cfg.stations = []);
        if (!list.some(s => Number(s.id) === 0)) list.unshift(stationFromViewer(v));
    }
    for (const [k, value] of Object.entries(v)) {
        if (VIEWER_DROPPED.includes(k) || k === 'active') continue;
        if (k === 'ip_bind') continue; // below: an old viewer listened on every interface unless it said otherwise
        if (k === 'share_loc') continue; // below: each viewer's own, off unless it said so
        if (k === 'prome') { if (on(value)) entry.metrics = true; continue; }
        if (FILTER_KEYS.includes(k) || ROUTE_KEYS.includes(k)) { if (filtered && k !== 'filter') own[k] = value; continue; }
        if (TRACKING_KEYS.includes(k)) {
            // the places are the site's; a tracker of a server's own keeps its own backup
            if (filtered) { if (k !== 'places') own[k] = value; }
            else if (off) entry[k] = value;
            else { const t = cfg.tracking && typeof cfg.tracking === 'object' ? cfg.tracking : (cfg.tracking = {}); if (!(k in t)) t[k] = value; }
            continue;
        }
        if (!filtered && !off && STATION_KEYS.includes(k) && k !== 'station') continue;
        entry[k] = value;
    }
    const bind = v.ip_bind ? String(v.ip_bind) : '0.0.0.0';
    if (bind !== '127.0.0.1') entry.bind = bind;
    entry.share_loc = on(v.share_loc);
    if (filtered) entry.tracking = own;
    if (off) entry.active = false;
    // the ship list as v0.70 served it stays, for the scripts that read it
    if (!('api_v0' in entry)) entry.api_v0 = true;
    return entry;
}

// the default station as the viewer section said it: GPS unless use_gps said off, as the
// engine took it; 0, 0 is no position, as the panel once wrote it
function stationFromViewer(v) {
    const s = { id: 0 };
    if (String(v.station || '').trim()) s.name = String(v.station).trim();
    if (validPos(num(v.lat), num(v.lon)) && !(num(v.lat) === 0 && num(v.lon) === 0)) { s.lat = num(v.lat); s.lon = num(v.lon); }
    if (Number.isFinite(num(v.alt))) s.alt = num(v.alt);
    if (num(v.own_mmsi) > 0) s.mmsi = num(v.own_mmsi);
    if (v.use_gps === undefined || on(v.use_gps)) s.gps = true;
    return s;
}

function migrateReceivers(cfg) {
    const top = {};
    for (const key of RECEIVER_KEYS)
        if (Object.prototype.hasOwnProperty.call(cfg, key)) {
            top[key] = cfg[key];
            delete cfg[key];
        }
    if (Object.keys(top).length) {
        if (!Array.isArray(cfg.receiver)) cfg.receiver = [];
        const named = String(top.input ?? '') || String(top.serial ?? '');
        const i = named ? -1 : cfg.receiver.findIndex(r => r && typeof r === 'object' && isActive(r));
        if (i >= 0) cfg.receiver[i] = { ...top, ...cfg.receiver[i] };
        else cfg.receiver.unshift(named ? top : { input: 'NONE', ...top });
    }
    for (const r of Array.isArray(cfg.receiver) ? cfg.receiver : []) {
        if (!r || typeof r !== 'object') continue;
        if (typeof r.input === 'string') r.input = r.input.toUpperCase();
        delete r.sensitivity_high;
        if (typeof r.model === 'string') {
            const model = r.model.toLowerCase();
            if (!('engines' in r)) r.engines = [model === 'auto' ? {} : { type: model }];
            delete r.model;
        }
        if (r.model && typeof r.model === 'object' && !Array.isArray(r.model)) {
            if (on(r.model.active, true)) {
                const { active, ...settings } = r.model;
                if (!Array.isArray(r.engines)) r.engines = [Object.keys(settings).length ? { settings } : {}];
            }
            delete r.model;
        }
    }
}

function migrateMsgFormat(ch) {
    if (!('json' in ch) && !('json_full' in ch)) return;
    if (!('msgformat' in ch))
        ch.msgformat = on(ch.json_full) ? 'JSON_FULL' : on(ch.json) ? 'JSON_NMEA' : 'NMEA';
    delete ch.json;
    delete ch.json_full;
}

// the old key goes, or the server-side alias fights the new one
function migrateTracking(t) {
    if (!t || typeof t !== 'object' || t.replay_time === undefined) return;
    t.track_time ??= t.replay_time;
    delete t.replay_time;
}

// the stations a server's filter names, beside it with `filter` on or in its `tracking`; none for all
export function filterStations(sv) {
    const ids = (o) => ['id', 'select_id', 'stations'].flatMap(k => (Array.isArray(o[k]) ? o[k] : o[k] === undefined || o[k] === null ? [] : String(o[k]).split(',')));
    const t = sv.tracking && typeof sv.tracking === 'object' ? sv.tracking : {};
    return [...(on(sv.filter) ? ids(sv) : []), ...ids(t)].map(x => String(x).trim()).filter(Boolean).map(Number);
}

// A server without share_loc of its own goes by each station's; where its stations agree it gets
// its own, where they differ it stays until its switch is set.
function migrateShareLoc(cfg) {
    const stations = Array.isArray(cfg.stations) ? cfg.stations.filter(s => s && typeof s === 'object') : [];
    const shares = id => on(stations.find(s => Number(s.id) === id)?.share_loc);
    const every = [...new Set([0, ...stations.map(s => Number(s.id))])];
    for (const sv of Array.isArray(cfg.services) ? cfg.services : []) {
        if (!sv || typeof sv !== 'object' || 'share_loc' in sv) continue;
        const named = filterStations(sv);
        const seen = (named.length ? named : every).map(shares);
        if (seen.every(Boolean)) sv.share_loc = true;
        else if (!seen.some(Boolean)) sv.share_loc = false;
    }
}

export function migrateConfig(cfg) {
    if (!cfg || typeof cfg !== 'object') return cfg;
    migrateReceivers(cfg);
    for (const key of ['udp', 'tcp_listener', 'tcp'])
        if (Array.isArray(cfg[key])) cfg[key].forEach(ch => { if (ch && typeof ch === 'object') migrateMsgFormat(ch); });
    const v = cfg.control?.viewer;
    if (v && typeof v === 'object') {
        const located = on(v.share_loc);
        if (STATION_KEYS.some(k => k in v)) {
            const list = Array.isArray(cfg.stations) ? cfg.stations : (cfg.stations = []);
            if (!list.some(s => Number(s.id) === 0)) list.unshift(stationFromViewer(v));
            for (const k of STATION_KEYS) delete v[k];
        }
        const list = Array.isArray(cfg.services) ? cfg.services : (cfg.services = []);
        let main = list.find(s => s && s.main === true);
        if (!main) list.unshift(main = { main: true });
        if (!('api_v0' in main)) main.api_v0 = true;
        if (!('share_loc' in main)) main.share_loc = located;
        const tracking = cfg.tracking && typeof cfg.tracking === 'object' ? cfg.tracking : (cfg.tracking = {});
        for (const [k, value] of Object.entries(v)) {
            if (DROPPED_KEYS.includes(k)) continue;
            if (TRACKING_KEYS.includes(k)) { if (!(k in tracking)) tracking[k] = value; }
            else if (!(k in main)) main[k] = value;
        }
        delete cfg.control.viewer;
        if (!Object.keys(tracking).length) delete cfg.tracking;
    }
    if (cfg.server) {
        const list = Array.isArray(cfg.services) ? cfg.services : (cfg.services = []);
        for (const v of Array.isArray(cfg.server) ? cfg.server : [cfg.server])
            if (v && typeof v === 'object') list.push(serverFromViewer(v, cfg));
        delete cfg.server;
    }
    // a server's message switch is msg; msgs was briefly seeded by a defaults bug
    for (const s of Array.isArray(cfg.services) ? cfg.services : []) {
        if (!s || typeof s !== 'object') continue;
        if ('message' in s) {
            if (!('msg' in s)) s.msg = s.message;
            delete s.message;
        }
        delete s.msgs;
        migrateTracking(s.tracking);
    }
    migrateTracking(cfg.tracking);
    migrateShareLoc(cfg);
    if (Array.isArray(cfg.services)) {
        const i = cfg.services.findIndex(s => s && s.main === true);
        if (i > 0) cfg.services.unshift(...cfg.services.splice(i, 1));
    }
    return cfg;
}
