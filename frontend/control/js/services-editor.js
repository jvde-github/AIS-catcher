// The configuration's `services`: the servers, the web addresses this station answers on. The
// main server, beside the control panel on its port, comes first and always serves the map. Each
// serves the map, the API (/api/v1), MCP (/mcp), System (/api/v1/system), GeoJSON and Prometheus
// as chosen, and says who may read it: programs on this machine, holders of a key, or anyone.
// The servers take a save at once.

import { el, on, moreMenu } from './dom.js';
import { App, ConfigStore, Renderer, registerEditor } from './config-manager.js';
import { DOCS, docLink } from './docs.js';
import { ICONS } from './icons.js';

const CONTAINER = 'services-config';
// `legacy`: the viewer's switch that offered it before it was a service of its own
const SERVICES = [
    { key: 'ui', label: 'Map', path: '/', desc: 'The map, and what it reads.', docs: DOCS.viewer },
    { key: 'api', label: 'API', path: '/api/v1', desc: 'Vessels, stations, places and events.', docs: DOCS.api },
    { key: 'mcp', label: 'MCP', path: '/mcp', desc: 'The same, for an AI assistant.', docs: DOCS.mcp },
    { key: 'system', label: 'System', path: '/api/v1/system', desc: 'Health, receivers and the log. Never public.', docs: DOCS.system },
    { key: 'geojson', label: 'GeoJSON', path: '/geojson', desc: 'Ships for maps and GIS tools.', legacy: 'geojson' },
    { key: 'metrics', label: 'Prometheus', path: '/metrics', desc: 'Message counts for Prometheus and Grafana.', legacy: 'prome', docs: DOCS.prometheus }
];
// what a key guards on a server that has keys; on the network the first three need one, or Public
const GUARDED = ['api', 'mcp', 'system', 'geojson', 'metrics'];
const STRICT = ['api', 'mcp', 'system'];

const OPEN_ICON = '<svg class="st-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M14 5h5v5M19 5l-8 8M18 14v5H5V6h5"/></svg>';

const local = bind => !bind || bind === 'localhost' || bind === '::1' || String(bind).startsWith('127.');

// a link that opens a server in a new tab
export function openButton(href) {
    return el('a', 'st-btn st-btn--sm', { href, target: '_blank', rel: 'noopener', html: OPEN_ICON + 'Open' });
}

// the services, as the Services overview has them in its columns
export const SERVICE_COLUMNS = SERVICES.map(({ key, label, path }) => ({ key, label, path }));

// what a server serves: the map unless switched off, the rest when switched on, Prometheus under
// its old name too; `builtIn`: the main server, the map always on
export function servedKeys(data, builtIn = false) {
    const map = builtIn || data.ui === undefined || on(data.ui);
    return SERVICES.filter(s => s.key === 'ui' ? map : on(data[s.key]) || (s.legacy && on(data[s.legacy]))).map(s => s.key);
}

// a server as the panel names it: Main, its description, or its port
export const serverName = s => (s.main ? 'Main' : String(s.description || '').trim() || `Server · ${s.port ?? '–'}`);

// who may read a server, in a few words: the map is never guarded, the rest by keys or Public
export function readersOf(data, bind) {
    const keys = Array.isArray(data.keys) && data.keys.length > 0;
    if (on(data.public)) return 'Anyone';
    if (local(bind)) return keys ? 'This machine, or a key' : 'This machine';
    return keys ? 'With a key' : 'Anyone';
}

// a setting's words on the left, its switch on the right, as the settings beside it have them
function switchRow(cls, checked, onChange, title, desc, disabled) {
    return el('label', 'st-svc ' + (cls || ''), {},
        el('span', 'st-svc__text', {}, title, desc ? el('span', 'st-svc__desc', { text: desc }) : null),
        el('span', 'st-switch', {},
            el('input', '', { type: 'checkbox', checked: !!checked, disabled: !!disabled, onChange: e => onChange(e.target.checked) }),
            el('span', 'st-switch__track')));
}

function section(title, docs, ...kids) {
    return el('section', 'st-svcs', {},
        el('div', 'st-svcs__head', {}, el('h3', 'st-section__title', { text: title }), docs ? docLink(docs) : null), ...kids);
}

// ---- which ships a server or a webhook shows: from which stations, once there are two or more,
// and which MMSIs are let through or kept out. Kinds and how lately heard are the viewer's own
// filter's. It writes `data.filter`; the terms it does not edit, set in
// the file, stay as they are.
const EDITED = ['stations', 'mmsi_allow', 'mmsi_deny'];

export function renderShows(host, data, { stations = [], touched, rerender }) {
    const many = stations.length >= 2;
    const f = data.filter && typeof data.filter === 'object' ? { ...data.filter } : {};
    const commit = (redraw = true) => {
        const kept = Object.keys(f).filter(k => f[k] !== undefined && f[k] !== null && !(Array.isArray(f[k]) && !f[k].length));
        if (kept.length) data.filter = Object.fromEntries(kept.map(k => [k, f[k]])); else delete data.filter;
        touched();
        if (redraw) rerender();
    };
    const box = section('Shows', DOCS.services);
    if (many) {
        const chosen = Array.isArray(f.stations) ? f.stations.map(String) : [];
        const some = chosen.length > 0;
        const seg = el('div', 'st-seg', { role: 'radiogroup', 'aria-label': 'Stations' },
            [['all', 'All stations'], ['some', 'Chosen stations']].map(([key, label]) => el('button', 'st-seg__opt', {
                type: 'button', role: 'radio', 'aria-checked': String((key === 'some') === some), text: label,
                onClick: () => {
                    if ((key === 'some') === some) return;
                    if (key === 'all') delete f.stations; else f.stations = stations.map(st => st.key);
                    commit();
                }
            })));
        box.append(el('div', 'st-shows__row', {}, el('span', 'field-label', { text: 'Stations' }), seg));
        if (some) box.append(el('div', 'st-pickchips', { role: 'group', 'aria-label': 'Stations shown' },
            stations.map(st => el('button', 'st-pickchip', { type: 'button', 'aria-pressed': String(chosen.includes(String(st.key))), text: st.label,
                onClick: () => {
                    const on = chosen.includes(String(st.key));
                    f.stations = on ? f.stations.filter(k => String(k) !== String(st.key)) : [...f.stations, st.key];
                    commit();
                } }))));
        box.append(el('p', 'field-desc', { text: some ? 'Ships these stations heard in the last hour.' : 'The ships every station hears.' }));
    }
    {
        const mmsis = (key, label, hint) => {
            const area = el('textarea', 'input st-shows__mmsi', { rows: 2, spellcheck: 'false', placeholder: hint,
                value: (Array.isArray(f[key]) ? f[key] : []).join('\n'),
                onChange: e => {
                    const list = e.target.value.split(/[\s,]+/).map(x => x.trim()).filter(x => /^\d{1,9}$/.test(x)).map(Number);
                    if (list.length) f[key] = list; else delete f[key];
                    commit(false);
                } });
            return el('label', 'field st-shows__field', {}, el('span', 'field-label', { text: label }), area);
        };
        box.append(el('div', 'st-stations__row', {}, mmsis('mmsi_allow', 'Only these MMSIs', 'none: every vessel'), mmsis('mmsi_deny', 'Never these MMSIs', 'none')));
        const other = Object.keys(f).filter(k => !EDITED.includes(k));
        if (other.length) box.append(el('p', 'field-desc', { text: 'Also set in the configuration file: ' + other.join(', ') + '.' }));
    }
    host.append(box);
}

// a key just made: shown until the tab is read again, as only its fingerprint is kept
const revealed = new Map();

// What a server serves (`builtIn`: the main server, the map always on, as the panel shows it)
// and who may read it. `data` changes in place; `touched` says so and `rerender` draws the page
// again.
export function renderServing(host, data, { builtIn = false, bind, touched, rerender }) {
    const served = servedKeys(data, builtIn);
    const has = k => served.includes(k);
    const set = (k, value) => {
        // the switch says it now, not Prometheus's old name; the map is on unless said otherwise
        SERVICES.forEach(s => { if (s.legacy) delete data[s.legacy]; });
        if (k === 'ui') { if (value) delete data.ui; else data.ui = false; }
        else if (value) data[k] = true; else delete data[k];
        touched();
        rerender();
    };
    host.append(section('Serves', DOCS.services, ...SERVICES.map(s => switchRow('', has(s.key), v => set(s.key, v),
        el('span', 'st-svc__name', {}, el('strong', '', { text: s.label }), el('code', '', { text: s.path })),
        builtIn && s.key === 'ui' ? 'Always on: this panel shows it.' : s.desc, builtIn && s.key === 'ui'))));

    if (GUARDED.some(has)) {
        const keys = Array.isArray(data.keys) ? data.keys : [];
        const open = on(data.public);
        const box = section('Access', DOCS.keys);
        box.append(el('p', 'field-desc st-svcs__where', { text: local(bind)
            ? 'This machine only: no key needed here.'
            : 'On the network: the API, MCP and System need a key, or Public.' }));
        box.append(switchRow('', open, v => { if (v) data.public = true; else delete data.public; touched(); rerender(); },
            el('strong', '', { text: 'Public' }), 'Data without a key; System still needs one.'));
        box.append(renderKeys(data, keys, { touched, rerender, system: has('system') }));
        if (!local(bind) && !keys.length && !open && STRICT.some(has))
            box.append(el('p', 'st-warn-note', { text: 'On the network it needs a key or Public to save.' }));
        else if (!local(bind) && has('system') && open && !keys.length)
            box.append(el('p', 'st-warn-note', { text: 'A public server needs a key for System.' }));
        host.append(box);
    }

    if (has('ui') || has('api') || has('mcp'))
        host.append(section('Places', DOCS.places, switchRow('', data.place_geometry === undefined || on(data.place_geometry),
            v => { if (v) delete data.place_geometry; else data.place_geometry = false; touched(); },
            el('strong', '', { text: 'Place outlines' }), 'Off sends markers only.')));
}

// the keys a server accepts, each by its name and what it may read; a new one is made by the
// control panel, shown once, and kept as its SHA-256
function renderKeys(data, keys, { touched, rerender, system }) {
    const wrap = el('div', 'st-keys');
    const scopesOf = k => (Array.isArray(k.scopes) && k.scopes.length ? k.scopes : ['data']);
    keys.forEach((k, i) => {
        const shown = revealed.get(k.sha256);
        wrap.append(el('div', 'st-key', {},
            el('div', 'st-key__head', {},
                el('strong', 'st-key__name', { text: k.name || 'Key ' + (i + 1) }),
                ...scopesOf(k).map(s => el('span', 'st-chip st-chip--all', { text: s === 'system' ? 'System' : 'Data' })),
                el('span', 'st-key__id', { text: String(k.sha256 || '').slice(0, 8), title: 'The start of its SHA-256' }),
                el('button', 'st-btn st-btn--sm st-key__remove', { type: 'button', text: 'Remove', onClick: () => {
                    if (!confirm(`Remove the key ${k.name || i + 1}? Programs using it lose access when saved.`)) return;
                    keys.splice(i, 1);
                    if (keys.length) data.keys = keys; else delete data.keys;
                    touched();
                    rerender();
                } })),
            shown ? el('div', 'st-key__reveal', {},
                el('code', '', { text: shown }),
                el('button', 'st-btn st-btn--sm', { type: 'button', html: ICONS.copy + 'Copy', onClick: e => {
                    navigator.clipboard?.writeText(shown).then(() => { e.currentTarget.lastChild.textContent = 'Copied'; }, () => {});
                } }),
                el('span', 't-small t-subtle', { text: 'Shown once. Sent as "Authorization: Bearer …"; works once saved.' })) : null));
    });
    const form = el('div', 'st-key__new');
    const start = el('button', 'st-keys__add', { type: 'button', html: ICONS.plus + 'New key', onClick: () => {
        const name = el('input', 'input', { type: 'text', placeholder: 'Who it is for, e.g. home-assistant', 'aria-label': 'Key name' });
        const dataScope = el('input', '', { type: 'checkbox', checked: true });
        const sysScope = el('input', '', { type: 'checkbox', checked: false, disabled: !system });
        const go = el('button', 'st-btn st-btn--sm st-btn--primary', { type: 'button', text: 'Make key', onClick: async () => {
            const scopes = [dataScope.checked ? 'data' : null, sysScope.checked ? 'system' : null].filter(Boolean);
            if (!scopes.length) return App.notify('error', 'A key needs Data, System or both');
            go.disabled = true;
            try {
                const r = await fetch('/api/key', { method: 'POST' });
                if (!r.ok) throw new Error(r.status);
                const made = await r.json();
                revealed.set(made.sha256, made.key);
                const k = { name: name.value.trim() || `key ${keys.length + 1}`, sha256: made.sha256 };
                if (!(scopes.length === 1 && scopes[0] === 'data')) k.scopes = scopes;
                keys.push(k);
                data.keys = keys;
                touched();
                rerender();
            } catch (e) {
                go.disabled = false;
                App.notify('error', 'Could not make a key: ' + e.message);
            }
        } });
        form.replaceChildren(name,
            el('label', 'st-key__scope', {}, dataScope, 'Data'),
            el('label', 'st-key__scope', { title: system ? '' : 'Switch System on to give a key to it' }, sysScope, 'System'),
            go);
        name.focus();
    } });
    form.append(start);
    wrap.append(form);
    return wrap;
}

// A server's map settings, as the main server's page has them: the same fields, in the same
// sections, one setting to a line
function renderViewerFields(main, s, fields, touched) {
    const box = el('div', 'st-secs st-srv__viewer');
    const grids = new Map();
    const grid = name => {
        if (!grids.has(name)) {
            const g = el('div', 'st-sec__grid fieldset');
            box.append(el('section', 'st-sec', {}, el('h4', 'st-section__title', { text: name }), g));
            grids.set(name, g);
        }
        return grids.get(name);
    };
    for (const { field, section: name } of fields) {
        const key = field.jsonpath || field.name;
        const node = Renderer.renderField(field, 0, s, (f, v) => {
            if (v === undefined || v === '' || (Array.isArray(v) && !v.length)) delete s[key]; else s[key] = v;
            touched();
        }, () => {}, '');
        node.classList.add('st-opt');
        if (field.type === 'toggle') node.classList.add('st-opt--switch');
        else if (field.type === 'list') node.classList.add('st-opt--wide');
        node.removeAttribute('style');
        grid(name || 'Map').append(node);
    }
    main.append(box);
}

// The editor of the servers, for the Services pages: each server its own page, the main one first
// and always there. `viewerFields`: the map's settings, on every server that serves it; `stations`:
// the stations a server can show; `onEdit`: a name or a port typed; `onInvalid(i)`: the server a
// save refused, to show; `onSaved`: after a save.
export function createServicesEditor({ onSaved, onInvalid, onEdit, viewerFields = [], stations = () => [] } = {}) {
    const state = { list: [], viewerPort: 0, controlPort: 0, bind: '127.0.0.1', saved: new Set(), listening: null };
    const editor = registerEditor(CONTAINER, { save: () => save(), data: () => state.list });
    const touched = () => editor.touched();

    async function load() {
        const [cfg, status] = await Promise.all([ConfigStore.fetch(), fetch('/api/status').then(r => r.json()).catch(() => ({}))]);
        state.list = Array.isArray(cfg.services) ? JSON.parse(JSON.stringify(cfg.services)) : [];
        if (!state.list.some(s => s.main === true)) state.list.unshift({ main: true });
        state.saved = new Set(state.list.filter(s => !s.main).map(s => Number(s.port)));
        state.viewerPort = Number(status.viewer) || 0;
        state.controlPort = Number(location.port) || 0;
        state.bind = status.bind || '127.0.0.1';
        state.listening = Array.isArray(status.listening) ? new Set(status.listening) : null;
    }

    // the ports that listen, as the station says
    async function refreshListening() {
        const status = await fetch('/api/status').then(r => r.json()).catch(() => ({}));
        if (Array.isArray(status.listening)) state.listening = new Set(status.listening);
    }

    // false for a saved server whose port does not listen: another program has it
    function listening(s) {
        if (s.main) return true;
        const p = Number(s.port);
        return !state.listening || !state.saved.has(p) || state.listening.has(p);
    }

    // the saved servers whose ports do not listen once the station has opened them: asked again
    // for a few seconds, as opening them takes a moment, until every one listens
    async function deafPorts(list) {
        for (let tries = 12; ; tries--) {
            await refreshListening();
            const deaf = list.filter(s => !listening(s)).map(s => s.port);
            if (!deaf.length || tries === 1) return deaf;
            await new Promise(r => setTimeout(r, 250));
        }
    }

    function freePort() {
        const taken = new Set([state.viewerPort, state.controlPort, ...state.list.map(s => Number(s.port))]);
        let p = 8110;
        while (taken.has(p)) p++;
        return p;
    }

    function add() {
        state.list.push({ port: freePort(), ui: false, api: true });
        touched();
        return state.list.length - 1;
    }

    // the address a browser here would use for a server, when it can reach it
    function address(s) {
        const host = location.hostname;
        const here = host === 'localhost' || host === '127.0.0.1' || host === '::1';
        if (local(s.bind) && !here) return null;
        return `${location.protocol}//${host.includes(':') ? '[' + host + ']' : host}:${s.port}`;
    }

    // the page's header: Open, and Delete behind the menu; the main server stays
    function actions(box, i, { deleted }) {
        const s = state.list[i];
        if (s.main) return box.append(openButton('/viewer/'));
        const at = address(s);
        const serve = servedKeys(s);
        if (at && (serve.includes('ui') || serve.includes('api')))
            box.append(openButton(at + (serve.includes('ui') ? '/' : '/api/v1')));
        box.append(moreMenu('More actions', [['Delete…', () => {
            if (!confirm(`Delete the server on port ${s.port}? It stops serving when saved.`)) return;
            state.list.splice(i, 1);
            touched();
            deleted();
        }, true]]));
    }

    // a server's page under `title`, the page's heading, which follows the name and the port as typed
    function renderDetail(main, i, { title, rerender }) {
        const s = state.list[i];
        // the map beside the panel shows everything; what a server shows is the server's
        if (s.main) {
            main.append(el('p', 'field-desc st-svcs__lead', { text: `The map this panel shows, on port ${state.viewerPort || 'next to it'}. Add servers for a public map or an API.` }));
            renderServing(main, s, { builtIn: true, bind: state.bind, touched, rerender });
            renderViewerFields(main, s, viewerFields, touched);
            return;
        }
        if (!listening(s))
            main.append(el('p', 'st-warn-note', { text: `Port ${s.port} is not listening: in use elsewhere? See the log.` }));
        // ---- where it listens
        // the list and the title follow the port in place: a redraw here would lose the click that ends the typing
        const renamed = () => {
            touched();
            title.textContent = serverName(s);
            onEdit?.();
        };
        const port = el('input', 'input', { type: 'number', min: 1, max: 65535, step: 1, value: s.port ?? '', onInput: e => {
            const v = e.target.value;
            if (v === '') delete s.port; else s.port = Number(v);
            renamed();
        } });
        // its name: what the panel calls it, for a public map or an API for automation
        const name = el('input', 'input', { type: 'text', value: s.description || '', placeholder: 'e.g. Public map', spellcheck: 'false', onInput: e => {
            const v = e.target.value;
            if (v.trim()) s.description = v; else delete s.description;
            renamed();
        } });
        const where = local(s.bind) ? 'local' : (s.bind === '0.0.0.0' || s.bind === '::') ? 'network' : 'custom';
        const seg = el('div', 'st-seg', { role: 'radiogroup', 'aria-label': 'Listen on' },
            [['local', 'This machine'], ['network', 'Network']].map(([key, label]) => el('button', 'st-seg__opt', {
                type: 'button', role: 'radio', 'aria-checked': String(where === key), text: label,
                onClick: () => { if (key === 'local') delete s.bind; else s.bind = '0.0.0.0'; touched(); rerender(); }
            })),
            where === 'custom' ? el('span', 'st-seg__opt', { role: 'radio', 'aria-checked': 'true', text: s.bind }) : null);
        main.append(el('div', 'st-stations__row st-srv__row', {},
            el('label', 'field st-stations__field st-stations__grow', {}, el('span', 'field-label', { text: 'Name' }), name),
            el('label', 'field st-stations__field st-stations__num', {}, el('span', 'field-label', { text: 'Port' }), port),
            el('div', 'field st-stations__field st-stations__grow', {}, el('span', 'field-label', { text: 'Listens on' }), seg)));

        renderServing(main, s, { bind: s.bind, touched, rerender });

        // ---- what its map adds, as the main server's
        if (servedKeys(s).includes('ui'))
            renderViewerFields(main, s, viewerFields, touched);

        renderShows(main, s, { stations: stations(), touched, rerender });
    }

    function clean(s) {
        if (s.main) {
            const out = { ...s, main: true };
            delete out.port;
            delete out.bind;
            if (Array.isArray(out.keys) && !out.keys.length) delete out.keys;
            return out;
        }
        const out = { ...s, port: Number(s.port) };
        if (!out.bind || out.bind === '127.0.0.1') delete out.bind;
        if (Array.isArray(out.keys) && !out.keys.length) delete out.keys;
        return out;
    }

    function validate() {
        const seen = new Set();
        for (let i = 0; i < state.list.length; i++) {
            const s = state.list[i], p = Number(s.port);
            if (s.main) continue;
            if (!Number.isInteger(p) || p < 1 || p > 65535) return [i, 'A server needs a port from 1 to 65535.'];
            if (p === state.viewerPort || p === state.controlPort) return [i, `Port ${p} is the ${p === state.viewerPort ? 'main server' : 'control panel'}'s.`];
            if (seen.has(p)) return [i, `Two servers on port ${p}.`];
            seen.add(p);
            if (!servedKeys(s).length) return [i, `The server on port ${p} serves nothing: switch on what it offers.`];
        }
        return null;
    }

    async function save() {
        const problem = validate();
        if (problem) {
            App.notify('error', problem[1]);
            onInvalid?.(problem[0]);
            return;
        }
        const list = state.list.map(clean);
        if (!await editor.write(cfg => { cfg.services = list; }, 'the services')) return;
        state.list = list.map(s => ({ ...s }));
        state.saved = new Set(list.filter(s => !s.main).map(s => s.port));
        const deaf = await deafPorts(list);
        if (deaf.length) App.notify('error', `Saved, but nothing listens on port ${deaf.join(', ')}: another program has it`, 8000);
        else App.notify('success', 'Services saved: the servers serve as saved now');
        onSaved?.();
    }

    return {
        state, load, add, actions, renderDetail,
        destroy() { editor.destroy(); }
    };
}
