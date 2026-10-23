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
    { key: 'geojson', label: 'GeoJSON', path: '/geojson', desc: 'Ships for maps and GIS tools.' },
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

// ---- what a server or a webhook shows: from which stations, once there are two or more, and
// which MMSIs are let through or kept out, its view; on a server also whether places go out with
// their outlines. Kinds and how lately heard are the page's own filter's. It writes `data.view`;
// the terms it does not edit, set in the file, stay as they are. `everyShip`: the main server,
// whose map shows every ship; `outlines`: the server sends places; `byStation`: a server, whose
// stations are its `station_id`, which its live streams keep to as well as its view.
const EDITED = ['stations', 'mmsi_allow', 'mmsi_deny'];

const idList = (v) => (Array.isArray(v) ? v : v === undefined || v === null || v === '' ? [] : String(v).split(','))
    .map(x => String(x).trim()).filter(Boolean);

export function renderShows(host, data, { stations = [], touched, rerender, everyShip = false, outlines = false, byStation = false }) {
    const many = stations.length >= 2;
    const f = data.view && typeof data.view === 'object' ? { ...data.view } : {};
    const commit = (redraw = true) => {
        const kept = Object.keys(f).filter(k => f[k] !== undefined && f[k] !== null && !(Array.isArray(f[k]) && !f[k].length));
        if (kept.length) data.view = Object.fromEntries(kept.map(k => [k, f[k]])); else delete data.view;
        touched();
        if (redraw) rerender();
    };
    const box = section('Shows', DOCS.services);
    if (everyShip) {
        if (outlines) box.append(outlinesRow(data, touched));
        host.append(box);
        return;
    }
    // the stations chosen: a server's station_id (its view's stations from before), a webhook's view
    const chosenIds = () => byStation ? idList(data.station_id ?? f.stations) : idList(f.stations);
    const choose = (list) => {
        if (byStation) {
            delete f.stations;
            if (list.length) data.station_id = list.map(Number); else delete data.station_id;
        } else if (list.length) f.stations = list.map(Number);
        else delete f.stations;
        commit();
    };
    if (many) {
        const chosen = chosenIds();
        const some = chosen.length > 0;
        const seg = el('div', 'st-seg', { role: 'radiogroup', 'aria-label': 'Stations' },
            [['all', 'All stations'], ['some', 'Chosen stations']].map(([key, label]) => el('button', 'st-seg__opt', {
                type: 'button', role: 'radio', 'aria-checked': String((key === 'some') === some), text: label,
                onClick: () => {
                    if ((key === 'some') === some) return;
                    choose(key === 'all' ? [] : stations.map(st => String(st.key)));
                }
            })));
        box.append(el('div', 'st-shows__row', {}, el('span', 'field-label', { text: 'Stations' }), seg));
        if (some) box.append(el('div', 'st-pickchips', { role: 'group', 'aria-label': 'Stations shown' },
            stations.map(st => el('button', 'st-pickchip', { type: 'button', 'aria-pressed': String(chosen.includes(String(st.key))), text: st.label,
                onClick: () => {
                    const on = chosen.includes(String(st.key));
                    choose(on ? chosen.filter(k => k !== String(st.key)) : [...chosen, String(st.key)]);
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
    if (outlines) box.append(outlinesRow(data, touched));
    host.append(box);
}

// places go out with their outline, or as a marker alone
function outlinesRow(data, touched) {
    return switchRow('', data.place_geometry === undefined || on(data.place_geometry),
        v => { if (v) delete data.place_geometry; else data.place_geometry = false; touched(); },
        el('strong', '', { text: 'Place outlines' }), 'Off sends each place as a marker, without its shape.');
}

// what serves places, so their outlines are a question
export const sendsPlaces = (served) => ['ui', 'api', 'mcp'].some(k => served.includes(k));

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

// the folded part of a server's map settings, open or not as the owner left it, by server
const moreOpen = new Set();
const CHEVRON = '<svg class="st-adv__chev" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M9 6l6 6-6 6"/></svg>';

// A server's map settings, as the main server's page has them: the map page a visitor gets in a
// section of its own, and under More, folded, the links, tiles and plugins, with a count of those
// set. One setting to a line. `key`: which server's More this is.
function renderViewerFields(main, s, fields, touched, key) {
    const sum = el('span', 'st-adv__sum');
    const rest = fields.filter(f => f.section !== 'Map page');
    const set = () => rest.filter(({ field: f }) => {
        const v = s[f.jsonpath || f.name];
        return v !== undefined && v !== '' && !(Array.isArray(v) && !v.length);
    }).length;
    const count = () => { const n = set(); sum.textContent = n ? n + ' set' : ''; };
    const field = ({ field: f }) => {
        const name = f.jsonpath || f.name;
        const node = Renderer.renderField(f, 0, s, (fl, v) => {
            if (v === undefined || v === '' || (Array.isArray(v) && !v.length)) delete s[name]; else s[name] = v;
            touched();
            count();
        }, () => {}, '');
        node.classList.add('st-opt');
        if (f.type === 'toggle') node.classList.add('st-opt--switch');
        else if (f.type === 'list') node.classList.add('st-opt--wide');
        node.removeAttribute('style');
        return node;
    };
    const page = fields.filter(f => f.section === 'Map page');
    main.append(section('Map page', DOCS.viewer, el('div', 'st-srv__viewer st-srv__page', {}, ...page.map(field))));

    const body = el('div', 'st-adv__body st-secs st-srv__viewer');
    const groups = new Map();
    for (const f of rest) {
        if (!groups.has(f.section)) {
            const g = el('div', 'st-sec__grid fieldset');
            body.append(el('section', 'st-sec', {}, el('h4', 'st-section__title', { text: f.section }), g));
            groups.set(f.section, g);
        }
        groups.get(f.section).append(field(f));
    }
    const more = el('details', 'st-adv st-srv__more', moreOpen.has(key) ? { open: '' } : {},
        el('summary', '', { innerHTML: CHEVRON + 'Links, map tiles and plugins' }, sum), body);
    more.addEventListener('toggle', () => { if (more.open) moreOpen.add(key); else moreOpen.delete(key); });
    count();
    main.append(more);
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
        if (s.main || s.active === false) return true;
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

    // the page's header: Active, Open, and Delete behind the menu; the main server stays, and always serves
    function actions(box, i, { deleted, rerender }) {
        const s = state.list[i];
        if (s.main) return box.append(openButton('/viewer/'));
        const at = address(s);
        const serve = servedKeys(s);
        const on = s.active !== false;
        box.append(el('label', 'st-switch', {},
            el('input', '', { type: 'checkbox', checked: on, onChange: e => {
                if (e.target.checked) delete s.active; else s.active = false;
                touched();
                rerender?.();
            } }),
            el('span', 'st-switch__track'), 'Active'));
        if (on && at && (serve.includes('ui') || serve.includes('api')))
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
            renderShows(main, s, { everyShip: true, outlines: true, touched, rerender });
            renderViewerFields(main, s, viewerFields, touched, 'main');
            return;
        }
        if (s.active === false)
            main.append(el('p', 'field-desc st-svcs__lead', { text: 'Switched off: it keeps its settings and serves nothing until it is on again.' }));
        else if (!listening(s))
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
        // what it sends, before how its map looks
        renderShows(main, s, { stations: stations(), touched, rerender, outlines: sendsPlaces(servedKeys(s)), byStation: true });
        if (servedKeys(s).includes('ui'))
            renderViewerFields(main, s, viewerFields, touched, 'port ' + s.port);
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
            if (s.active === false) continue; // switched off: it claims no port
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
