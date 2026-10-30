// The configuration's `services`: the servers, the web addresses this station answers on. The
// main server, beside the control panel on its port, comes first and always serves the map. Each
// serves the map, the API (/api/v1), MCP (/mcp), System (/api/v1/system), GeoJSON and Prometheus
// as chosen, and says who may read it: programs on this machine, holders of a key, or anyone.
// The servers take a save at once.

import { el, isActive, on, moreMenu } from './dom.js';
import { api } from './api.js';
import { hub } from './hub.js';
import { App, ConfigStore, registerEditor } from './config-store.js';
import { Renderer } from './config-widgets.js';
import { DOCS, docLink } from './docs.js';
import { icon } from './icons.js';

const CONTAINER = 'services-config';
// `legacy`: the viewer's switch that offered it before it was a service of its own
const SERVICES = [
    { key: 'ui', label: 'Map', path: '/', desc: 'The map, and what it reads.', docs: DOCS.viewer },
    { key: 'api', label: 'API', path: '/api/v1', desc: 'Vessels, stations, places and events.', docs: DOCS.api },
    { key: 'mcp', label: 'MCP', path: '/mcp', desc: 'The same, for an AI assistant.', docs: DOCS.mcp },
    { key: 'system', label: 'System', path: '/api/v1/system', desc: 'Health, receivers and the log. Never open on the network.', docs: DOCS.system },
    { key: 'geojson', label: 'GeoJSON', path: '/geojson', desc: 'Ships for maps and GIS tools.' },
    { key: 'kml', label: 'KML', path: '/kml', desc: 'Ships for Google Earth.' },
    { key: 'metrics', label: 'Prometheus', path: '/metrics', desc: 'Message counts for Prometheus and Grafana.', legacy: 'prome', docs: DOCS.prometheus }
];
// what a server's keys guard; a server without keys is open to whoever reaches it
const GUARDED = ['api', 'mcp', 'system', 'geojson', 'kml', 'metrics'];

const local = bind => !bind || bind === 'localhost' || bind === '::1' || String(bind).startsWith('127.');

// a link that opens a server in a new tab
export function openButton(href) {
    return el('a', 'st-btn st-btn--sm', { href, target: '_blank', rel: 'noopener', html: icon('open') + 'Open' });
}

// the services, as the Services overview has them in its columns
export const SERVICE_COLUMNS = SERVICES.map(({ key, label, path }) => ({ key, label, path }));

const hasKeys = data => Array.isArray(data.keys) && data.keys.length > 0;

// what a server serves: the map unless switched off (with keys, only when switched on), the rest
// when switched on, Prometheus under its old name too; `builtIn`: the main server, the map always on
export function servedKeys(data, builtIn = false) {
    const map = builtIn || (data.ui === undefined ? !hasKeys(data) : on(data.ui));
    return SERVICES.filter(s => s.key === 'ui' ? map : on(data[s.key]) || (s.legacy && on(data[s.legacy]))).map(s => s.key);
}

// a server as the panel names it: Main, its description, or its port
export const serverName = s => (s.main ? 'Main' : String(s.description || '').trim() || `Server · ${s.port ?? '–'}`);

// who may read a server, in a few words: the map is never guarded, the rest by its keys, or
// anyone who reaches it when it has none
export function readersOf(data, bind) {
    if (hasKeys(data)) return 'With a key';
    return on(data.public) || !local(bind) ? 'Anyone' : 'This machine';
}

// A server is Open or Keys. Keys when it has them, or the owner chose Keys and has none yet
// (`wantKeys`); open when it says `public`, or on this machine. Choosing Open on a server with
// keys puts them aside (`stashed`) so that choosing Keys again before a save brings them back.
const wantKeys = new WeakSet();
const stashed = new WeakMap();
export function accessMode(data, bind) {
    if (hasKeys(data) || wantKeys.has(data)) return 'keys';
    return on(data.public) || local(bind) ? 'open' : 'keys';
}

// why a server could not be saved as it stands: Keys without a key, or System open on the network
export function accessProblem(data, bind, builtIn = false) {
    const served = servedKeys(data, builtIn);
    if (!GUARDED.some(k => served.includes(k))) return null;
    const keys = hasKeys(data);
    if (!keys && accessMode(data, bind) === 'keys') return `${serverName(data)}: make a key, or choose Open.`;
    if (!keys && !local(bind) && served.includes('system')) return `${serverName(data)}: System is never open on the network. Choose Keys, or switch System off.`;
    return null;
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

// A server page's section that folds: closed, its head reads the section's state in a few words;
// open, its rows are there to edit. The owner's choice is kept per server and section; a section
// with a problem opens itself unless the owner closed it, and the problem stands in its head.
const foldOpen = new Map();
function fold(title, docs, { key, state = '', warn = '', open: openByDefault = false }, ...kids) {
    const open = foldOpen.has(key) ? foldOpen.get(key) : openByDefault || !!warn;
    const body = el('div', 'st-fold__body', { id: 'fold-' + key.replace(/\W+/g, '-') }, ...kids);
    const toggle = el('button', 'st-fold__toggle', { type: 'button', 'aria-expanded': String(open), 'aria-controls': body.id, html: icon('chevronRight', 'st-fold__chev') },
        el('span', 'st-fold__title', { text: title }),
        el('span', 'st-fold__state' + (warn ? ' st-fold__state--warn' : ''), { text: warn || state }));
    const sec = el('section', 'st-svcs st-fold' + (open ? ' st-fold--open' : ''), {},
        el('div', 'st-svcs__head st-fold__head', {}, el('h3', 'st-section__title st-fold__h', {}, toggle), docs ? docLink(docs) : null), body);
    toggle.addEventListener('click', () => {
        const now = toggle.getAttribute('aria-expanded') !== 'true';
        foldOpen.set(key, now);
        toggle.setAttribute('aria-expanded', String(now));
        sec.classList.toggle('st-fold--open', now);
    });
    return sec;
}

// ---- HTTPS on a server: the files of a certificate it already has. `until`, the station's word for
// a port that serves it, is when the certificate in use expires, in seconds. A path outside the
// configuration's folder can only be set in the file: the panel shows it and leaves it.
const dayOf = seconds => new Date(seconds * 1000).toLocaleDateString(undefined, { day: 'numeric', month: 'long', year: 'numeric', timeZone: 'UTC' });

export function tlsLine(data, until, now = Date.now() / 1000) {
    if (!data.tls) return { state: 'Off: plain HTTP' };
    if (!until) return { state: 'HTTPS once saved and read' };
    return until < now ? { warn: `certificate expired on ${dayOf(until)}` } : { state: `HTTPS on, certificate valid until ${dayOf(until)}` };
}

export function outsideFolder(path, folder) {
    const p = String(path || '').replace(/\\/g, '/');
    if (!p) return false;
    if (!/^([a-zA-Z]:)?\//.test(p)) return p.split('/').includes('..');
    const base = String(folder || '').replace(/\\/g, '/').replace(/\/+$/, '');
    return !base || !(p + '/').startsWith(base + '/') || p.split('/').includes('..');
}

function renderTLS(host, data, { until, folder, touched, foldKey }) {
    const field = (key, label, placeholder) => {
        const value = data.tls?.[key] ?? '';
        const fixed = outsideFolder(value, folder);
        const input = el('input', 'st-input', { type: 'text', value, placeholder, spellcheck: 'false', readOnly: fixed, title: fixed ? 'Outside the configuration\'s folder: set in the file' : '', onInput: e => {
            const tls = { ...data.tls };
            if (e.target.value.trim()) tls[key] = e.target.value.trim(); else delete tls[key];
            if (Object.keys(tls).length) data.tls = tls; else delete data.tls;
            touched();
        } });
        return el('label', 'st-field st-stations__field st-stations__grow', {}, el('span', 'st-label', { text: label }), input);
    };
    const { state, warn } = tlsLine(data, until);
    host.append(fold('HTTPS', DOCS.services, { key: foldKey + ':https', state, warn },
        el('div', 'st-stations__row st-srv__row', {}, field('cert', 'Certificate', 'fullchain.pem'), field('key', 'Private key', 'privkey.pem')),
        el('p', 'st-help', { text: 'Files you already have, in the configuration\'s folder; one elsewhere is set in the configuration file. A renewed certificate is picked up within a minute.' })));
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

export function renderShows(host, data, { stations = [], touched, rerender, everyShip = false, outlines = false, byStation = false, foldKey = null }) {
    const many = stations.length >= 2;
    const f = data.view && typeof data.view === 'object' ? { ...data.view } : {};
    const commit = (redraw = true) => {
        const kept = Object.keys(f).filter(k => f[k] !== undefined && f[k] !== null && !(Array.isArray(f[k]) && !f[k].length));
        if (kept.length) data.view = Object.fromEntries(kept.map(k => [k, f[k]])); else delete data.view;
        touched();
        if (redraw) rerender();
    };
    const kids = [];
    const box = { append: (...n) => kids.push(...n) };
    // what it shows in a few words, for the head of the fold
    const state = () => {
        const parts = [];
        if (!everyShip && byStation && many) {
            const ids = idList(data.station_id ?? f.stations);
            if (ids.length) parts.push(ids.map(k => stations.find(st => String(st.key) === String(k))?.label || 'station ' + k).join(', '));
        }
        const allow = idList(f.mmsi_allow).length, deny = idList(f.mmsi_deny).length;
        parts.push(allow ? allow + (allow === 1 ? ' MMSI only' : ' MMSIs only') : deny ? 'all but ' + deny + (deny === 1 ? ' MMSI' : ' MMSIs') : 'every vessel');
        if (outlines) parts.push(data.view?.place_geometry === false ? 'places as markers' : 'places with their shape');
        return parts.join(' · ');
    };
    const done = () => host.append(foldKey ? fold('Shows', DOCS.services, { key: foldKey + ':shows', state: state() }, ...kids) : section('Shows', DOCS.services, ...kids));
    if (everyShip) {
        if (outlines) box.append(outlinesRow(data, touched));
        done();
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
        box.append(el('div', 'st-shows__row', {}, el('span', 'st-label', { text: 'Stations' }), seg));
        if (some) box.append(el('div', 'st-pickchips', { role: 'group', 'aria-label': 'Stations shown' },
            stations.map(st => el('button', 'st-pickchip', { type: 'button', 'aria-pressed': String(chosen.includes(String(st.key))), text: st.label,
                onClick: () => {
                    const on = chosen.includes(String(st.key));
                    choose(on ? chosen.filter(k => k !== String(st.key)) : [...chosen, String(st.key)]);
                } }))));
        box.append(el('p', 'st-help', { text: some ? 'Ships these stations heard in the last hour.' : 'The ships every station hears.' }));
    }
    {
        const mmsis = (key, label, hint) => {
            const area = el('textarea', 'st-input st-shows__mmsi', { rows: 2, spellcheck: 'false', placeholder: hint,
                value: (Array.isArray(f[key]) ? f[key] : []).join('\n'),
                onChange: e => {
                    const list = e.target.value.split(/[\s,]+/).map(x => x.trim()).filter(x => /^\d{1,9}$/.test(x)).map(Number);
                    if (list.length) f[key] = list; else delete f[key];
                    commit(false);
                } });
            return el('label', 'st-field st-shows__field', {}, el('span', 'st-label', { text: label }), area);
        };
        box.append(el('div', 'st-stations__row', {}, mmsis('mmsi_allow', 'Only these MMSIs', 'none: every vessel'), mmsis('mmsi_deny', 'Never these MMSIs', 'none')));
        const other = Object.keys(f).filter(k => !EDITED.includes(k));
        if (other.length) box.append(el('p', 'st-help', { text: 'Also set in the configuration file: ' + other.join(', ') + '.' }));
    }
    if (outlines) box.append(outlinesRow(data, touched));
    done();
}

// places go out with their outline, or as a marker alone
function outlinesRow(data, touched) {
    const view = () => (data.view && typeof data.view === 'object' ? data.view : (data.view = {}));
    return switchRow('', data.view?.place_geometry === undefined || on(data.view.place_geometry),
        v => { if (v) delete view().place_geometry; else view().place_geometry = false; touched(); },
        el('strong', '', { text: 'Share outlines' }), 'Each place with its shape; off, a marker alone.');
}

// what serves places, so their outlines are a question
export const sendsPlaces = (served) => ['ui', 'api', 'mcp'].some(k => served.includes(k));

// a key just made: shown until the tab is read again, as only its fingerprint is kept
const revealed = new Map();

// What a server serves (`builtIn`: the main server, the map always on, as the panel shows it)
// and who may read it. `data` changes in place; `touched` says so and `rerender` draws the page
// again.
export function renderServing(host, data, { builtIn = false, bind, touched, rerender, foldKey = 'srv' }) {
    const served = servedKeys(data, builtIn);
    const has = k => served.includes(k);
    const set = (k, value) => {
        // the switch says it now, not Prometheus's old name; the map is on unless said otherwise, with keys off
        SERVICES.forEach(s => { if (s.legacy) delete data[s.legacy]; });
        if (k === 'ui') {
            if (!value) data.ui = false;
            else if (hasKeys(data)) data.ui = true;
            else delete data.ui;
        }
        else if (value) data[k] = true; else delete data[k];
        touched();
        rerender();
    };
    // each service on a line: its name, path and what it is for
    const rows = SERVICES.map(s => switchRow('st-svc--line', has(s.key), v => set(s.key, v),
        el('span', 'st-svc__name', {}, el('strong', '', { text: s.label }), el('code', '', { text: s.path })),
        builtIn && s.key === 'ui' ? 'Always on: this panel shows it.' : s.desc, builtIn && s.key === 'ui'));
    const names = SERVICES.filter(s => has(s.key)).map(s => s.label);
    host.append(fold('Serves', DOCS.services, { key: foldKey + ':serves', open: true, state: names.join(', '), warn: names.length ? '' : 'serves nothing: switch on what it offers' }, ...rows));

    if (GUARDED.some(has)) {
        const keys = Array.isArray(data.keys) ? data.keys : [];
        const net = !local(bind);
        const mode = accessMode(data, bind);
        const choose = m => {
            if (m === 'keys') {
                delete data.public;
                wantKeys.add(data);
                const kept = stashed.get(data);
                if (kept) { data.keys = kept; stashed.delete(data); }
            } else {
                wantKeys.delete(data);
                if (keys.length) stashed.set(data, keys);
                delete data.keys;
                if (net) data.public = true; else delete data.public;
            }
            touched();
            rerender();
        };
        const kids = [];
        const box = { append: (...n) => kids.push(...n) };
        box.append(el('div', 'st-seg st-access', { role: 'radiogroup', 'aria-label': 'Access' },
            [['open', 'Open'], ['keys', 'Keys']].map(([m, label]) => el('button', 'st-seg__opt', {
                type: 'button', role: 'radio', 'aria-checked': String(mode === m), text: label,
                onClick: () => { if (mode !== m) choose(m); }
            }))));
        box.append(el('p', 'st-help st-svcs__where', { text: mode === 'keys'
            ? 'Whoever reaches this server needs one of these keys for everything it serves.'
            : net ? 'Anyone who can reach this server reads what it serves.'
                  : 'This machine only: anyone on it reads what this server serves.' }));
        if (mode === 'keys') box.append(renderKeys(data, keys, { touched, rerender, system: has('system') }));
        const problem = accessProblem(data, bind, builtIn);
        const why = problem ? problem.slice(problem.indexOf(': ') + 2) : '';
        if (problem) box.append(el('p', 'st-warn-note', { text: why }));
        const state = mode === 'keys' ? 'Keys · ' + (keys.map(k => k.name).filter(Boolean).join(', ') || 'none yet')
            : net ? 'Open to anyone who reaches it' : 'Open on this machine';
        host.append(fold('Access', DOCS.keys, { key: foldKey + ':access', state, warn: why }, ...kids));
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
                el('button', 'st-btn st-btn--sm', { type: 'button', html: icon('copy') + 'Copy', onClick: e => {
                    navigator.clipboard?.writeText(shown).then(() => { e.currentTarget.lastChild.textContent = 'Copied'; }, () => {});
                } }),
                el('span', 't-small t-subtle', { text: 'Shown once. Sent as "Authorization: Bearer …"; works once saved.' })) : null));
    });
    const form = el('div', 'st-key__new');
    const start = el('button', 'st-keys__add', { type: 'button', html: icon('plus') + 'New key', onClick: () => {
        const name = el('input', 'st-input', { type: 'text', placeholder: 'Who it is for, e.g. home-assistant', 'aria-label': 'Key name' });
        const dataScope = el('input', '', { type: 'checkbox', checked: true });
        const sysScope = el('input', '', { type: 'checkbox', checked: false, disabled: !system });
        const go = el('button', 'st-btn st-btn--sm st-btn--primary', { type: 'button', text: 'Make key', onClick: async () => {
            const scopes = [dataScope.checked ? 'data' : null, sysScope.checked ? 'system' : null].filter(Boolean);
            if (!scopes.length) return App.notify('error', 'A key needs Data, System or both');
            go.disabled = true;
            try {
                const made = await api('/api/key', { method: 'POST' });
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

// A server's map settings, as the main server's page has them, in one fold: the map page a
// visitor gets, then as its tail the links, tiles and plugins. One setting to a line. `key`:
// which server's fold this is. Closed, the head names the tabs that are on, the tiles and the plugins.
function renderViewerFields(main, s, fields, touched, key) {
    const rest = fields.filter(f => f.section !== 'Map page');
    const isSet = ({ field: f }) => {
        const v = s[f.jsonpath || f.name];
        return v !== undefined && v !== '' && !(Array.isArray(v) && !v.length);
    };
    const field = ({ field: f }) => {
        const name = f.jsonpath || f.name;
        const node = Renderer.renderField(f, s, (fl, v) => {
            if (v === undefined || v === '' || (Array.isArray(v) && !v.length)) delete s[name]; else s[name] = v;
            touched();
        }, () => {});
        node.classList.add('st-opt');
        if (f.type === 'toggle') node.classList.add('st-opt--switch');
        else if (f.type === 'list') node.classList.add('st-opt--wide');
        node.removeAttribute('style');
        return node;
    };
    const page = fields.filter(f => f.section === 'Map page');
    const tail = el('div', 'st-srv__tail st-srv__viewer');
    const groups = new Map();
    for (const f of rest) {
        if (!groups.has(f.section)) {
            const g = el('div', 'st-sec__grid st-fieldset');
            tail.append(el('section', 'st-sec', {}, el('h4', 'st-section__title', { text: f.section }), g));
            groups.set(f.section, g);
        }
        groups.get(f.section).append(field(f));
    }
    // the head: the tabs and cards that are on, then whether the tiles and plugins are the owner's
    const tabs = page.filter(f => f.field.type === 'toggle' && on(s[f.field.jsonpath || f.field.name])).map(f => f.field.label.replace(/ tab$/, '').replace(/ on the ship card$/, ''));
    const tiles = rest.some(f => f.section === 'Map tiles' && isSet(f)) ? 'own tiles' : 'default tiles';
    const plugins = rest.some(f => f.section === 'Plugins' && isSet(f)) ? 'plugins' : 'no plugins';
    main.append(fold('Map page', DOCS.viewer, { key: key + ':map', state: [tabs.length ? tabs.join(', ') : 'no tabs', tiles, plugins].join(' · ') },
        el('div', 'st-srv__viewer st-srv__page', {}, ...page.map(field)), tail));
}

// The editor of the servers, for the Services pages: each server its own page, the main one first
// and always there. `viewerFields`: the map's settings, on every server that serves it; `stations`:
// the stations a server can show; `onEdit`: a name or a port typed; `onInvalid(i)`: the server a
// save refused, to show; `onSaved`: after a save.
export function createServicesEditor({ onSaved, onInvalid, onEdit, viewerFields = [], stations = () => [] } = {}) {
    const state = { list: [], viewerPort: 0, controlPort: 0, bind: '127.0.0.1', saved: new Set(), listening: null, tls: new Map(), folder: '' };
    const editor = registerEditor(CONTAINER, { save: () => save(), data: () => state.list });
    const touched = () => editor.touched();

    async function load() {
        const [cfg, status] = await Promise.all([ConfigStore.fetch(), hub.status()]);
        state.list = Array.isArray(cfg.services) ? JSON.parse(JSON.stringify(cfg.services)) : [];
        if (!state.list.some(s => s.main === true)) state.list.unshift({ main: true });
        state.saved = new Set(state.list.filter(s => !s.main).map(s => Number(s.port)));
        state.viewerPort = Number(status.viewer) || 0;
        state.controlPort = Number(location.port) || 0;
        state.bind = status.bind || '127.0.0.1';
        state.listening = Array.isArray(status.listening) ? new Set(status.listening) : null;
        state.folder = status.config_folder || '';
        readTLS(status);
    }

    // the ports with HTTPS and when their certificates expire, as the station says now
    function readTLS(status) {
        state.tls = new Map((status.tls || []).map(t => [t.port, t.until]));
    }

    // the ports that listen, as the station says now
    async function refreshListening() {
        const status = await hub.refreshStatus();
        if (Array.isArray(status.listening)) state.listening = new Set(status.listening);
        readTLS(status);
    }

    // false for a saved server whose port does not listen: another program has it
    function listening(s) {
        if (s.main || !isActive(s)) return true;
        const p = Number(s.port);
        return !state.listening || !state.saved.has(p) || state.listening.has(p);
    }

    // the saved servers whose ports do not listen once the station has opened them: asked again
    // for a few seconds, as opening them takes a moment, until every one listens
    async function deafPorts(list) {
        for (let tries = 12; ; tries--) {
            await refreshListening();
            const deaf = list.filter(s => !listening(s)).map(s => s.port);
            const settling = list.some(s => isActive(s) && !!s.tls !== state.tls.has(s.main ? state.viewerPort : Number(s.port)));
            if ((!deaf.length && !settling) || tries === 1) return deaf;
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
        return `${s.tls ? 'https' : 'http'}://${host.includes(':') ? '[' + host + ']' : host}:${s.port}`;
    }

    // the page's header: Active, Open, and Delete behind the menu; the main server stays, and always serves
    function actions(box, i, { deleted, rerender }) {
        const s = state.list[i];
        if (s.main) return box.append(openButton('/viewer/'));
        const at = address(s);
        const serve = servedKeys(s);
        const live = isActive(s);
        box.append(el('label', 'st-switch', {},
            el('input', '', { type: 'checkbox', checked: live, onChange: e => {
                if (e.target.checked) delete s.active; else s.active = false;
                touched();
                rerender?.();
            } }),
            el('span', 'st-switch__track'), 'Active'));
        if (live && at && (serve.includes('ui') || serve.includes('api')))
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
            main.append(el('p', 'st-help st-svcs__lead', { text: `The map this panel shows, on port ${state.viewerPort || 'next to it'}. Add servers for a public map or an API.` }));
            renderTLS(main, s, { until: state.tls.get(state.viewerPort), folder: state.folder, touched, foldKey: 'main' });
            renderServing(main, s, { builtIn: true, bind: state.bind, touched, rerender, foldKey: 'main' });
            renderShows(main, s, { everyShip: true, outlines: true, touched, rerender, foldKey: 'main' });
            renderViewerFields(main, s, viewerFields, touched, 'main');
            return;
        }
        if (!isActive(s))
            main.append(el('p', 'st-help st-svcs__lead', { text: 'Switched off: it keeps its settings and serves nothing until it is on again.' }));
        else if (!listening(s))
            main.append(el('p', 'st-warn-note', { text: `Port ${s.port} is not listening: in use elsewhere? See the log.` }));
        // ---- where it listens
        // the list and the title follow the port in place: a redraw here would lose the click that ends the typing
        const renamed = () => {
            touched();
            title.textContent = serverName(s);
            onEdit?.();
        };
        const port = el('input', 'st-input', { type: 'number', min: 1, max: 65535, step: 1, value: s.port ?? '', onInput: e => {
            const v = e.target.value;
            if (v === '') delete s.port; else s.port = Number(v);
            renamed();
        } });
        // its name: what the panel calls it, for a public map or an API for automation
        const name = el('input', 'st-input', { type: 'text', value: s.description || '', placeholder: 'e.g. Public map', spellcheck: 'false', onInput: e => {
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
            el('label', 'st-field st-stations__field st-stations__grow', {}, el('span', 'st-label', { text: 'Name' }), name),
            el('label', 'st-field st-stations__field st-stations__num', {}, el('span', 'st-label', { text: 'Port' }), port),
            el('div', 'st-field st-stations__field st-stations__grow', {}, el('span', 'st-label', { text: 'Listens on' }), seg)));

        renderTLS(main, s, { until: state.tls.get(Number(s.port)), folder: state.folder, touched, foldKey: 'srv' + i });
        renderServing(main, s, { bind: s.bind, touched, rerender, foldKey: 'srv' + i });
        // what it sends, before how its map looks
        renderShows(main, s, { stations: stations(), touched, rerender, outlines: sendsPlaces(servedKeys(s)), byStation: true, foldKey: 'srv' + i });
        if (servedKeys(s).includes('ui'))
            renderViewerFields(main, s, viewerFields, touched, 'srv' + i);
    }

    function clean(s) {
        if (s.main) {
            const out = { ...s, main: true };
            delete out.port;
            delete out.bind;
            if (Array.isArray(out.keys) && !out.keys.length) delete out.keys;
            return out;
        }
        const out = { ...s };
        if (s.port !== undefined) out.port = Number(s.port);
        if (!out.bind || out.bind === '127.0.0.1') delete out.bind;
        if (Array.isArray(out.keys) && !out.keys.length) delete out.keys;
        return out;
    }

    // What only this page knows: Keys chosen with no key yet would save as open. The ports and what
    // a server serves the reader checks, and its answer shows as the save's.
    function validate() {
        for (let i = 0; i < state.list.length; i++) {
            const s = state.list[i];
            if (!s.main && !isActive(s)) continue;
            const access = accessProblem(s, s.bind, !!s.main);
            if (access) return [i, access];
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
