// Receiving stations on the map, the part both hosts share: the marker, the
// badge on the vessel that carries a receiver, and the hover band. A station
// is {name, id, country, mmsi, status}, status online or offline.

import { sanitizeString } from './core/text.js';
import { hexToRgb } from './color.js';
import { discCanvas, badgeCanvas } from './binary.js';

const text = (v) => sanitizeString(String(v));

const STATION_COLORS = { online: '#008000', offline: '#ef4444' };
const stationColor = (status) => STATION_COLORS[status] || STATION_COLORS.online;

// a station standing on its own position: the filled disc with a white edge and
// the antenna, the edge blue when a GPS places it
export function stationCanvas(gps, status) {
    return discCanvas('station', hexToRgb(stationColor(status)), 1, gps ? 'rgba(46, 134, 255, 0.5)' : undefined);
}

// the badge on the vessel a station rides
export const stationBadgeCanvas = (status) => badgeCanvas('station', hexToRgb(stationColor(status)), 1);

// the hover band, one line: flag, name and number
export function stationBand(s) {
    const cc = String(s.country || '').trim().toLowerCase();
    const flag = /^[a-z]{2}$/.test(cc) ? `<span class="flag fi fi-${cc} flag-tooltip"></span>` : '';
    const chip = s.id ? `<span class="station-chip">${text(s.id)}</span>` : '';
    const pick = s.id ? ` data-station="${Number(s.id)}"` : '';
    return `<div class="tip-band"${pick} style="--band: ${stationColor(s.status)}"><div class="tooltip-card">${flag}` +
        `<div><span class="tooltip-name">${text(s.name || 'Receiving station')}</span>${chip}</div></div></div>`;
}
