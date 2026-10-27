// Receiving stations on the map, the part both hosts share: the marker, the
// badge on the vessel that carries a receiver, and the hover band. A station
// is {name, id, country, mmsi, status, heard}, status online (receiving), quiet (nothing
// heard for a while), down (its receivers are lost) or offline (not connected).

import { sanitizeString } from '@aiscatcher/core/text.js';
import { decodeHTMLEntities } from './components.js';
import { hexToRgb } from './color.js';
import { discCanvas, badgeCanvas } from './binary.js';

const text = (v) => sanitizeString(String(v));

// quiet and down are grey: a station that hears nothing has lost its colour; down has a red
// edge as well, as it wants the owner, and where the host animates it, a slow pulse
const STATION_COLORS = { online: '#008000', quiet: '#8a9099', down: '#8a9099', offline: '#ef4444' };
const stationColor = (status) => STATION_COLORS[status] || STATION_COLORS.online;
const DOWN_EDGE = 'rgba(220, 38, 38, 0.95)';

// the status of a station record or row: offline when not connected, else the state the
// host judged and sent with the row when it changed; a host that does not judge it shows
// a connected station as receiving. The row's `heard` is not compared with the clock here:
// a row is sent again only when it changes, so its `heard` stands still while the station
// keeps receiving.
export function stationStatus(o) {
    if (!o || o.online === false) return 'offline';
    if (o.state === 'down') return 'down';
    if (o.state === 'quiet') return 'quiet';
    return 'online';
}

// what a quiet or down station is doing, in a few words: since when it heard nothing
const clock = (t) => new Date(t * 1000).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
export function stationStateText(s) {
    if (s.status === 'down') return 'Receiver down';
    if (s.status === 'quiet') return s.heard > 0 ? 'Quiet since ' + clock(s.heard) : 'Quiet';
    return '';
}

// a station standing on its own position: the filled disc with a white edge and
// the antenna, the edge blue when a GPS places it
export function stationCanvas(gps, status) {
    return discCanvas('station', hexToRgb(stationColor(status)), 1, status === 'down' ? DOWN_EDGE : gps ? 'rgba(46, 134, 255, 0.5)' : undefined);
}

// the badge on the vessel a station rides
export const stationBadgeCanvas = (status) => badgeCanvas('station', hexToRgb(stationColor(status)), 1);

// the stations that heard a vessel as boxes, the latest first and marked: the
// first `max`, then a dotted box when there are more; `label` names a station by its id
export function stationBoxes(ids, label, max = 3) {
    if (!ids || !ids.length) return '';
    const more = ids.length > max ? '<span class="station-box station-etc">···</span>' : '';
    return ids.slice(0, max).map((id, i) => i
        ? `<span class="station-box">${text(label(id))}</span>`
        : `<span class="station-box station-last" title="Heard it last">${text(label(id))}</span>`).join('') + more;
}

// the hover band, one line: flag, name and number
export function stationBand(s) {
    const cc = String(s.country || '').trim().toLowerCase();
    const flag = /^[a-z]{2}$/.test(cc) ? `<span class="flag fi fi-${cc} flag-tooltip"></span>` : '';
    const chip = s.id ? `<span class="station-chip">${text(s.id)}</span>` : '';
    const pick = s.id ? ` data-station="${Number(s.id)}"` : '';
    const state = stationStateText(s);
    return `<div class="tip-band"${pick} style="--band: ${stationColor(s.status)}"><div class="tooltip-card">${flag}` +
        `<div><span class="tooltip-name">${text(decodeHTMLEntities(s.name) || 'Receiving station')}</span>${chip}` +
        (state ? `<div class="tooltip-dim">${text(state)}</div>` : '') + `</div></div></div>`;
}
