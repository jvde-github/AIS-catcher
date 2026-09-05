// Receiving stations on the map, the part both hosts share: the marker, the
// badge on the vessel that carries a receiver, and the hover band. A station
// is {name, id, country, mmsi, status}, status online or offline.

import { sanitizeString } from './core/text.js';
import { hexToRgb } from './color.js';
import { kindGlyph, badgeCanvas } from './binary.js';

const text = (v) => sanitizeString(String(v));

const STATION_COLORS = { online: '#008000', offline: '#ef4444' };
const stationColor = (status) => STATION_COLORS[status] || STATION_COLORS.online;

// a station standing on its own position: the filled disc with a white edge and
// the antenna, the edge blue when a GPS places it
export function stationCanvas(gps, status) {
    const scale = window.devicePixelRatio || 1;
    const R = 8, SIZE = 2 * (R + 3);
    const canvas = document.createElement('canvas');
    canvas.width = canvas.height = SIZE * scale;
    const ctx = canvas.getContext('2d');
    ctx.scale(scale, scale);
    ctx.beginPath();
    ctx.arc(SIZE / 2, SIZE / 2, R, 0, 2 * Math.PI);
    ctx.fillStyle = stationColor(status);
    ctx.fill();
    ctx.strokeStyle = gps ? '#2e86ff' : 'rgba(255, 255, 255, 0.75)';
    ctx.lineWidth = 2;
    ctx.stroke();
    ctx.fillStyle = 'white';
    kindGlyph(ctx, 'station', SIZE / 2, SIZE / 2, R * 0.52);
    return { canvas, size: SIZE };
}

// the chip in a marker's corner saying how many objects share it; nothing below two
export function withCount(icon, count) {
    if (!(count > 1)) return icon;
    const size = icon.width || icon.size;
    const ctx = icon.canvas.getContext('2d');
    ctx.save();
    ctx.setTransform(icon.canvas.width / size, 0, 0, icon.canvas.width / size, 0, 0);
    const r = 6, cx = size - r - 1, cy = r + 1;
    ctx.beginPath();
    ctx.arc(cx, cy, r, 0, 2 * Math.PI);
    ctx.fillStyle = '#333';
    ctx.fill();
    ctx.strokeStyle = 'white';
    ctx.lineWidth = 1.5;
    ctx.stroke();
    ctx.fillStyle = 'white';
    ctx.font = 'bold 9px sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(count > 9 ? '9+' : String(count), cx, cy + 0.5);
    ctx.restore();
    return icon;
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
