// Zones: each in its colour, as a chip, and the dialog that adds an input's or an engine's zones,
// suggesting those the configuration and the open pages already use.
import * as AISComponents from '@aiscatcher/ui/components.js';
import { el } from './dom.js';
import { ConfigStore, editors } from './config-store.js';

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
const getZoneCss = zone => `var(--${getZoneColor(zone)})`;

export function zoneChip(zone, onRemove) {
    return el('span', `chip ${getZoneColor(zone)}`, { style: `--z:${getZoneCss(zone)}` },
        zone,
        el('button', 'sys-dismiss', { type: 'button', onClick: onRemove }, '×')
    );
}

async function fetchAllZones() {
    try {
        const cfg = await ConfigStore.fetch();
        const zones = new Set();
        const walk = o => {
            if (Array.isArray(o)) { o.forEach(walk); return; }
            if (!o || typeof o !== 'object') return;
            if (Array.isArray(o.zone)) o.zone.forEach(z => zones.add(z));
            Object.values(o).forEach(walk);
        };
        walk(cfg);
        editors.forEach(ed => walk(ed.data));
        if (Array.isArray(cfg.sharing_zone)) cfg.sharing_zone.forEach(z => zones.add(z));
        return [...zones].sort();
    } catch { return []; }
}

let activeZoneEdit = { zones: [], onUpdate: null, allZones: [] };


export function openZoneModal(currentZones, onUpdate) {
    const modal = AISComponents.modal({
        id: 'cm-zone-modal',
        title: 'Manage Zones',
        cardClass: 'modal-sm',
        bodyClass: 'col col-loose modal-body',
        footerLabel: 'Done'
    });
    modal.body.id = 'cm-zone-body';
    activeZoneEdit.zones = [...currentZones];
    activeZoneEdit.onUpdate = onUpdate;
    activeZoneEdit.allZones = [];
    renderZoneModalBody();
    fetchAllZones().then(all => {
        activeZoneEdit.allZones = all;
        renderZoneModalBody();
    });
    modal.open();
}

function renderZoneModalBody() {
    const body = document.getElementById('cm-zone-body');
    if (!body) return;
    body.innerHTML = '';

    const chipsWrap = el('div', 'row row-wrap row-tight sys-minrow');
    if (activeZoneEdit.zones.length === 0) {
        chipsWrap.appendChild(el('span', 't-small t-subtle t-italic', {}, 'No zones assigned'));
    } else {
        activeZoneEdit.zones.forEach((zone, i) => {
            chipsWrap.appendChild(zoneChip(zone, () => {
                activeZoneEdit.zones.splice(i, 1);
                activeZoneEdit.onUpdate([...activeZoneEdit.zones]);
                renderZoneModalBody();
            }));
        });
    }
    body.appendChild(chipsWrap);

    const addSection = el('div', 'col col-tight');
    const inputRow = el('div', 'row');
    const input = el('input', 'input grow', {
        type: 'text',
        placeholder: 'Zone name (e.g. internal)',
        onKeydown: (e) => {
            if (e.key === 'Enter') { e.preventDefault(); addZoneFromInput(); }
        },
        onInput: () => {
            input.value = input.value.replace(/[^a-zA-Z0-9_-]/g, '');
            renderSuggestions(input.value.toLowerCase());
        }
    });
    inputRow.appendChild(input);
    inputRow.appendChild(el('button', 'btn', {
        type: 'button', onClick: addZoneFromInput
    }, 'Add'));
    addSection.appendChild(inputRow);

    const suggestionsDiv = el('div', 'row row-wrap row-tight', { id: 'cm-zone-suggestions' });
    addSection.appendChild(suggestionsDiv);
    body.appendChild(addSection);

    function addZoneFromInput() {
        const val = input.value.trim().replace(/[^a-zA-Z0-9_-]/g, '');
        if (!val) return;
        input.value = val;
        if (!activeZoneEdit.zones.includes(val)) {
            activeZoneEdit.zones.push(val);
            activeZoneEdit.onUpdate([...activeZoneEdit.zones]);
            if (!activeZoneEdit.allZones.includes(val)) activeZoneEdit.allZones.push(val);
        }
        input.value = '';
        renderZoneModalBody();
    }

    function renderSuggestions(query) {
        suggestionsDiv.innerHTML = '';
        const suggestions = activeZoneEdit.allZones.filter(z =>
            !activeZoneEdit.zones.includes(z) && (query === '' || z.toLowerCase().includes(query))
        );
        suggestions.slice(0, 8).forEach(z => {
            suggestionsDiv.appendChild(el('button', `chip sys-chip-btn ${getZoneColor(z)}`, {
                type: 'button',
                onClick: () => {
                    if (!activeZoneEdit.zones.includes(z)) {
                        activeZoneEdit.zones.push(z);
                        activeZoneEdit.onUpdate([...activeZoneEdit.zones]);
                    }
                    input.value = '';
                    renderZoneModalBody();
                }
            }, z));
        });
    }

    renderSuggestions('');
    setTimeout(() => input.focus(), 50);
}

// a zone's colour: its class, and the colour as CSS
export const ZoneColors = { badge: getZoneColor, css: getZoneCss };
