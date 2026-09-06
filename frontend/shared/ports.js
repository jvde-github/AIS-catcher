// Static landmarks use OpenLayers styles on the existing object layer.
import Style from 'ol/style/Style.js';
import Icon from 'ol/style/Icon.js';
import CircleStyle from 'ol/style/Circle.js';
import Fill from 'ol/style/Fill.js';
import Stroke from 'ol/style/Stroke.js';
import { MAP_MARKER_RADIUS, MAP_MARKER_FILL_ALPHA, MAP_MARKER_BORDER_ALPHA } from './binary.js';
import { hexToRgb } from './color.js';
import { ANCHOR_SVG } from './icons.js';
import { sanitizeString } from './core/text.js';

export const PORT_COLOR = '#075078';
const anchor = 'data:image/svg+xml,' + encodeURIComponent(ANCHOR_SVG);

export function portStyles() {
    const styles = [
        new Style({ image: new CircleStyle({ radius: MAP_MARKER_RADIUS, fill: new Fill({ color: [...hexToRgb(PORT_COLOR), MAP_MARKER_FILL_ALPHA] }), stroke: new Stroke({ color: [255, 255, 255, MAP_MARKER_BORDER_ALPHA], width: 1.5 }) }), zIndex: 100 }),
        new Style({ image: new Icon({ src: anchor, width: 13, height: 13, opacity: 0.95 }), zIndex: 101 }),
    ];
    return styles;
}

export function portBand(p) {
    const text = v => sanitizeString(String(v || ''));
    return `<div class="tip-band" style="--band: ${PORT_COLOR}"><div class="tooltip-card"><div>` +
        `<span class="tooltip-name">${text(p.label)}</span> · ${text(p.country)}</div></div></div>`;
}
