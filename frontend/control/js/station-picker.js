// An input's station: the default station, 0, where an input stands that names
// none; one of the stations listed; or a new one made on the spot. A number the
// configuration names but Stations does not have is shown as such, with a way to
// create it: the engine does not start while an input names a station that is not there.

import { el } from './dom.js';
import { App, ConfigStore } from './config-manager.js';
import { ICONS } from './icons.js';
import { TAGS, modeOf } from './stations-editor.js';
import { isDefaultStation, stationLabel } from './stations.js';
import { createPicker } from '@aiscatcher/ui/picker.js';

const fmt = v => Number(v).toFixed(4);
const placed = s => [s.lat, s.lon].every(v => v !== undefined && v !== null && v !== '' && Number.isFinite(Number(v)));

const WARN = '<svg class="st-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 3l10 18H2z"/><path d="M12 10v5M12 18v.5"/></svg>';

// where a station's position comes from, as the configuration says
function where(s) {
    const mode = modeOf(s);
    if (mode === 'vessel') return `On vessel MMSI ${s.mmsi}`;
    const pos = placed(s) ? `${fmt(s.lat)}, ${fmt(s.lon)}${s.alt !== undefined && s.alt !== '' ? ` · ${s.alt} m` : ''}` : '';
    if (mode === 'gps') return pos ? `From GPS · starts at ${pos}` : 'From GPS';
    return pos || 'No position yet';
}

// A new station under the next free number, or under the one given; saved at once, so the input
// can name it. The number it got, or null when the save failed.
async function createStation(id, name) {
    try {
        await ConfigStore.save(cfg => {
            const list = Array.isArray(cfg.stations) ? cfg.stations : (cfg.stations = []);
            if (id === null) id = list.reduce((m, s) => Math.max(m, Number(s.id) || 0), 0) + 1;
            if (!list.some(s => Number(s.id) === id)) list.push({ id, ...(name ? { name } : {}), share_loc: false });
        });
    } catch (e) {
        App.notify('error', `Could not create ${id === null ? 'the station' : 'station ' + id}: ${e.message}`);
        return null;
    }
    return id;
}

// The picker: which station, the default one first and the others by number, each with where its
// position comes from. `stations()` gives the stations, the default one first. onPick(id) with a
// number, or undefined for the default station; onCreated after a station is made, so the pages
// that list the stations read them again.
export function createStationPicker(value, { stations, onPick, onCreated } = {}) {
    let current = value === undefined || value === null || value === '' ? 0 : Number(value);
    const warn = el('div', 'st-stpick__warn', { hidden: true });

    const option = (s, several, group) => ({ value: Number(s.id), badge: Number(s.id), name: stationLabel(s, several), sub: where(s),
        tag: isDefaultStation(s) ? null : TAGS[modeOf(s)], group });
    const picker = createPicker({
        label: 'Stations',
        value: current,
        options: () => {
            const [own, ...rest] = stations();
            return [option(own, rest.length > 0, 'own'),
                ...[...rest].sort((a, b) => Number(a.id) - Number(b.id)).map(s => option(s, true, 'listed'))];
        },
        shown: id => ({ badge: id, name: `Station ${id}`, sub: 'Not under Stations', warn: true }),
        footer: () => newStation(),
        onPick: id => {
            current = id;
            paintWarn();
            onPick?.(id === 0 ? undefined : id);
        }
    });

    function paintWarn() {
        warn.hidden = stations().some(s => Number(s.id) === current);
        if (warn.hidden) return warn.replaceChildren();
        const id = current;
        warn.replaceChildren(el('span', 'st-stpick__warnicon', { html: WARN }),
            el('span', 'st-stpick__warntext', { text: `Station ${id} is not under Stations: the inputs will not start.` }),
            el('button', 'st-btn st-btn--sm', { type: 'button', text: `Create station ${id}`, onClick: async () => {
                if (await createStation(id, '') === null) return;
                onCreated?.(id);
                picker.refresh();
                paintWarn();
            } }));
    }

    // "+ New station…": a name, then Create; the number is the next free one
    function newStation() {
        const row = el('div', 'st-stpick__new');
        const start = el('button', 'st-stpick__add', { type: 'button', html: ICONS.plus + 'New station…', onClick: () => {
            const name = el('input', 'input st-stpick__newname', { type: 'text', placeholder: 'Name (optional)', 'aria-label': 'Name of the new station',
                onKeydown: e => { if (e.key === 'Enter') { e.preventDefault(); create(); } } });
            const go = el('button', 'st-btn st-btn--sm st-btn--primary', { type: 'button', text: 'Create', onClick: () => create() });
            const create = async () => {
                go.disabled = true;
                const id = await createStation(null, name.value.trim());
                go.disabled = false;
                if (id === null) return;
                onCreated?.(id);
                picker.pick(id);
            };
            row.replaceChildren(name, go);
            name.focus();
        } });
        row.append(start);
        return row;
    }

    paintWarn();
    return el('div', 'st-stpick', {}, picker.el, warn);
}
