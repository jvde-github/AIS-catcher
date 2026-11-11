import { el } from './dom.js';

const ZONE_COLORS = ['zone-1', 'zone-2', 'zone-3', 'zone-4',
    'zone-5', 'zone-6', 'zone-7', 'zone-8'];
function zoneHash(zone) {
    let hash = 0;
    for (let i = 0; i < zone.length; i++) hash = (hash * 31 + zone.charCodeAt(i)) & 0xFFFF;
    return hash;
}
const zoneColorIndex = new Map();
function zoneIndex(zone) {
    if (zoneColorIndex.has(zone)) return zoneColorIndex.get(zone);
    let i = zoneHash(zone) % ZONE_COLORS.length;
    if (zoneColorIndex.size < ZONE_COLORS.length) {
        const used = new Set(zoneColorIndex.values());
        while (used.has(i)) i = (i + 1) % ZONE_COLORS.length;
    }
    zoneColorIndex.set(zone, i);
    return i;
}
const getZoneColor = zone => ZONE_COLORS[zoneIndex(zone)];
export const zoneCss = zone => `var(--${getZoneColor(zone)})`;

export function zoneChip(zone, onRemove) {
    return el('span', `chip ${getZoneColor(zone)}`, { style: `--z:${zoneCss(zone)}` },
        zone,
        el('button', 'sys-dismiss', { type: 'button', onClick: onRemove }, '×')
    );
}
