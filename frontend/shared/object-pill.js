// Each slice is its own OpenLayers image/hit target. The shared capsule outline
// has no internal borders, badges, hover effects or DOM overlays.
import { kindGlyph, MAP_MARKER_RADIUS, MAP_MARKER_FILL_ALPHA, MAP_MARKER_BORDER_ALPHA } from './binary.js';
import { ANCHOR_SVG } from './icons.js';

export const PILL_SLOT_WIDTH = 20;
let anchor;

export function objectPillCanvas(cat, status, rgb, fade, index, count, glyph = cat) {
    const scale = window.devicePixelRatio || 1;
    const width = PILL_SLOT_WIDTH + 4, height = 2 * (MAP_MARKER_RADIUS + 2);
    const canvas = document.createElement('canvas');
    canvas.width = width * scale;
    canvas.height = height * scale;
    const ctx = canvas.getContext('2d');
    ctx.scale(scale, scale);
    // Clip at each slot boundary so neighbouring features never cover its hit
    // target. The first/last slots retain space for the rounded outer stroke.
    ctx.beginPath();
    ctx.rect(index ? 2 : 0, 0, PILL_SLOT_WIDTH + (index ? 0 : 2) + (index === count - 1 ? 2 : 0), height);
    ctx.clip();
    const x = 2 - index * PILL_SLOT_WIDTH, y = 2, w = count * PILL_SLOT_WIDTH, h = 2 * MAP_MARKER_RADIUS, r = MAP_MARKER_RADIUS;
    ctx.beginPath();
    ctx.moveTo(x + r, y);
    ctx.arcTo(x + w, y, x + w, y + h, r);
    ctx.arcTo(x + w, y + h, x, y + h, r);
    ctx.arcTo(x, y + h, x, y, r);
    ctx.arcTo(x, y, x + w, y, r);
    ctx.closePath();
    const fill = cat === 'port' ? [7, 80, 120] : cat === 'station' ? (status === 'offline' ? [239, 68, 68] : [0, 128, 0]) : rgb;
    ctx.fillStyle = `rgba(${fill.join(',')},${MAP_MARKER_FILL_ALPHA * fade})`;
    ctx.fill();
    ctx.strokeStyle = `rgba(255,255,255,${MAP_MARKER_BORDER_ALPHA * fade})`;
    ctx.lineWidth = 1.5;
    ctx.stroke();
    ctx.globalAlpha = fade;
    if (cat === 'port') {
        anchor ||= new Path2D(ANCHOR_SVG.match(/ d="([^"]+)"/)[1]);
        ctx.translate(width / 2 - 6.5, height / 2 + 6.5);
        ctx.scale(13 / 960, 13 / 960);
        ctx.fillStyle = 'rgba(255,255,255,.95)';
        ctx.fill(anchor);
    } else {
        kindGlyph(ctx, glyph, width / 2, height / 2, MAP_MARKER_RADIUS * .52);
    }
    return { canvas, width, height };
}
