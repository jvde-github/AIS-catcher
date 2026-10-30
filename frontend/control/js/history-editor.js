// Data & history: how long ships and tracks are kept, and what survives a
// restart, in the units people think in: the tracking section, which every
// server reads. Where it keeps its files is folded away under Files.

import { el, on } from './dom.js';
import { App, ConfigStore, registerEditor } from './config-store.js';
import { api } from './api.js';
import { hub } from './hub.js';

const KEYS = ['history', 'expire', 'track_memory', 'track_time', 'backup', 'file', 'places'];
const DEFAULTS = { history: 1800, expire: false, track_memory: 1024, track_time: 3600, backup: 10 };
const KEEP = [[600, '10 minutes'], [1800, '30 minutes'], [3600, '1 hour'], [6 * 3600, '6 hours'], [12 * 3600, '12 hours']];
const TRACKS = [
    { key: 'compact', label: 'Compact', track_memory: 512, track_time: 1800 },
    { key: 'standard', label: 'Standard', track_memory: 1024, track_time: 3600 },
    { key: 'long', label: 'Long', track_memory: 4096, track_time: 21600 }
];
const EVERY = [[5, 'Every 5 min'], [10, 'Every 10 min'], [30, 'Every 30 min'], [60, 'Every hour']];
const LIVE_MS = 30000;
const CONTAINER = 'history-config';

const duration = s => (s % 3600 === 0 ? `${s / 3600} h` : `${Math.round(s / 60)} min`);
const ago = s => (s < 90 ? 'just now' : s < 3600 ? `${Math.round(s / 60)} min ago` : `${Math.round(s / 3600)} h ago`);

export function createHistoryEditor(host) {
    const state = { data: {}, statsFile: '', saved: null, timer: null };
    const editor = registerEditor(CONTAINER, { save: () => save(), data: () => state.data });
    const touched = () => { editor.touched(); render(); };
    const value = k => (state.data[k] === undefined ? DEFAULTS[k] : state.data[k]);
    const set = (k, v) => { if (v === undefined || v === '') delete state.data[k]; else state.data[k] = v; touched(); };

    // one setting to a line: its words on the left, its control on the right
    const row = (label, desc, ...controls) => el('div', 'st-hrow', {},
        el('div', 'st-hrow__text', {}, el('strong', '', { text: label }), desc ? el('span', '', {}, desc) : null),
        el('div', 'st-hrow__controls', {}, ...controls));
    const select = (options, current, onChange, label) => el('select', 'st-input select', { 'aria-label': label, onChange: e => onChange(e.target.value) },
        options.map(([v, t]) => el('option', '', { value: v, selected: String(v) === String(current), text: t })));
    const toggle = (checked, onChange, label) => el('label', 'st-switch', { title: label },
        el('input', '', { type: 'checkbox', checked, 'aria-label': label, onChange: e => onChange(e.target.checked) }), el('span', 'st-switch__track'));
    const number = (v, onChange, attrs) => el('input', 'st-input st-hrow__num', { type: 'number', value: v ?? '', ...attrs, onChange: e => onChange(e.target.value === '' ? undefined : Number(e.target.value)) });

    function render() {
        host.replaceChildren();
        // ---- how long a silent ship stays
        const keep = Number(value('history'));
        const known = KEEP.some(([s]) => s === keep) && !state.customKeep;
        const keepSel = select([...KEEP, ['custom', 'Custom…']], known ? keep : 'custom', v => {
            if (v === 'custom') { state.customKeep = true; render(); } else { state.customKeep = false; set('history', Number(v) === DEFAULTS.history ? undefined : Number(v)); }
        }, 'Keep silent ships for');
        host.append(row('Keep silent ships on the map for', 'After this they leave the map',
            known ? null : number(Math.round(keep / 60), v => set('history', v === undefined ? undefined : Math.max(1, v) * 60), { min: 1, max: 720, 'aria-label': 'Minutes' }),
            known ? null : el('span', 'st-hrow__unit', { text: 'min' }), keepSel));

        host.append(row('Forget ship details when it leaves', 'Name and destination too',
            toggle(on(value('expire')), v => set('expire', v ? true : undefined), 'Forget ship details')));

        // ---- track length, with what each choice means
        const mem = Number(value('track_memory')), age = Number(value('track_time'));
        const preset = state.customTracks ? null : TRACKS.find(p => p.track_memory === mem && p.track_time === age);
        const trackSel = select([...TRACKS.map(p => [p.key, `${p.label} · ${duration(p.track_time)}`]), ['custom', 'Custom…']], preset ? preset.key : 'custom', v => {
            const p = TRACKS.find(x => x.key === v);
            if (!p) { state.customTracks = true; render(); return; }
            state.customTracks = false;
            state.data.track_memory = p.track_memory;
            state.data.track_time = p.track_time;
            touched();
        }, 'Track length');
        host.append(row('Track length', 'Longer tracks use more memory', trackSel));
        if (!preset)
            host.append(row('', null,
                el('label', 'st-hrow__pair', {}, 'Memory', number(mem, v => set('track_memory', v), { min: 16, max: 262144 }), 'KB'),
                el('label', 'st-hrow__pair', {}, 'Up to', number(Math.round(age / 60), v => set('track_time', v === undefined ? undefined : v * 60), { min: 0, max: 10080 }), 'min')));

        // ---- the backup: on while there is a file to write to
        const kept = !!String(state.data.file || '').trim();
        const every = Number(value('backup')) || 10;
        const last = !kept ? 'Off: every restart starts afresh'
            : state.saved === null ? 'Ships, tracks and statistics, on disk'
            : `Ships, tracks and statistics are saved to disk · last saved ${ago(state.saved)}`;
        host.append(row('Keep data over a restart', last,
            kept ? select(EVERY, EVERY.some(([m]) => m === every) ? every : 10, v => set('backup', Number(v) === DEFAULTS.backup ? undefined : Number(v)), 'How often') : null,
            toggle(kept, v => {
                if (v) state.data.file = state.lastFile || state.statsFile;
                else { state.lastFile = state.data.file; delete state.data.file; }
                touched();
            }, 'Keep data over a restart')));

        host.append(el('details', 'st-hist__adv', {},
                el('summary', '', { text: 'Files' }),
                row('Backup file', 'Where they are kept',
                    el('input', 'st-input', { type: 'text', value: state.data.file || '', placeholder: state.statsFile, onChange: e => set('file', e.target.value.trim() || undefined) })),
                row('Place directory', 'A folder of place files, in place of the places edited here',
                    el('input', 'st-input', { type: 'text', value: state.data.places || '', onChange: e => set('places', e.target.value.trim() || undefined) }))));
    }

    async function poll() {
        if (!host.isConnected) return;
        const live = await api('/api/stations/live').catch(() => null);
        if (live && live.backup_age_s !== undefined && live.backup_age_s !== state.saved) { state.saved = live.backup_age_s; if (!editor.dirty) render(); }
    }

    async function save() {
        const saved = await editor.write(cfg => {
            const t = (cfg.tracking ||= {});
            for (const k of KEYS) {
                if (state.data[k] === undefined) delete t[k];
                else t[k] = state.data[k];
            }
            if (!Object.keys(t).length) delete cfg.tracking;
        }, 'Data & history');
        if (saved) App.notify('success', 'Data & history saved');
    }

    async function load() {
        const [cfg, status] = await Promise.all([ConfigStore.fetch(), hub.status()]);
        const t = cfg.tracking || {};
        state.data = {};
        for (const k of KEYS) if (t[k] !== undefined) state.data[k] = t[k];
        state.statsFile = status.stats_file || '';
        render();
        poll();
    }

    host.classList.add('st-hist');
    state.timer = setInterval(poll, LIVE_MS);
    load().catch(e => App.notify('error', 'Could not read Data & history: ' + e.message));
    return {
        destroy() {
            clearInterval(state.timer);
            editor.destroy();
        }
    };
}

