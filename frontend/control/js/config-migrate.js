// The configuration as the panel reads it: a file from before is brought to
// the layout of today here, every time it is fetched, so whatever a page saves
// is in that layout. Nothing is written until a page saves.
//   - the default station is station 0 of `stations`, not the viewer section's station keys
//   - the main server (beside the control panel) is the entry of `services` marked main, not
//     `control.viewer`; how ships are kept is `tracking`

import { on } from './dom.js';
import { num, validPos } from './stations.js';

const STATION_KEYS = ['station', 'lat', 'lon', 'alt', 'share_loc', 'use_gps', 'own_mmsi'];
const TRACKING_KEYS = ['file', 'backup', 'history', 'track_time', 'replay_time', 'track_memory', 'max_ships', 'cutoff',
    'expire', 'server_mode', 'places'];
// what the viewer section took that means nothing on a server of the site: its own port and
// address (the main server's are the control panel's), its per-receiver views and the receivers
// it showed, and switches that no longer did anything
const DROPPED_KEYS = ['port', 'port_min', 'port_max', 'ip_bind', 'split', 'zone', 'groups_in', 'cdn', 'msgs', 'zlib',
    'reuse_port', 'stats_on_close'];

// the default station as the viewer section said it: GPS unless use_gps said off, as the
// engine took it; 0, 0 is no position, as the panel once wrote it
function stationFromViewer(v) {
    const s = { id: 0 };
    if (String(v.station || '').trim()) s.name = String(v.station).trim();
    if (validPos(num(v.lat), num(v.lon)) && !(num(v.lat) === 0 && num(v.lon) === 0)) { s.lat = num(v.lat); s.lon = num(v.lon); }
    if (Number.isFinite(num(v.alt))) s.alt = num(v.alt);
    if (num(v.own_mmsi) > 0) s.mmsi = num(v.own_mmsi);
    if (v.use_gps === undefined || on(v.use_gps)) s.gps = true;
    s.share_loc = on(v.share_loc);
    return s;
}

export function migrateConfig(cfg) {
    if (!cfg || typeof cfg !== 'object') return cfg;
    const v = cfg.control?.viewer;
    if (v && typeof v === 'object') {
        // the station
        if (STATION_KEYS.some(k => k in v)) {
            const list = Array.isArray(cfg.stations) ? cfg.stations : (cfg.stations = []);
            if (!list.some(s => Number(s.id) === 0)) list.unshift(stationFromViewer(v));
            for (const k of STATION_KEYS) delete v[k];
        }
        // the main server and the tracking
        const list = Array.isArray(cfg.services) ? cfg.services : (cfg.services = []);
        let main = list.find(s => s && s.main === true);
        if (!main) list.unshift(main = { main: true });
        const tracking = cfg.tracking && typeof cfg.tracking === 'object' ? cfg.tracking : (cfg.tracking = {});
        for (const [k, value] of Object.entries(v)) {
            if (DROPPED_KEYS.includes(k)) continue;
            if (TRACKING_KEYS.includes(k)) { if (!(k in tracking)) tracking[k] = value; }
            else if (!(k in main)) main[k] = value;
        }
        delete cfg.control.viewer;
        if (!Object.keys(tracking).length) delete cfg.tracking;
    }
    // the main server first
    if (Array.isArray(cfg.services)) {
        const i = cfg.services.findIndex(s => s && s.main === true);
        if (i > 0) cfg.services.unshift(...cfg.services.splice(i, 1));
    }
    return cfg;
}
