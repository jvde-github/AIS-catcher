// The Stations page: one station at a time, the rail listing them. First the
// default station, 0: where each input stands that names no station. It is
// always there and keeps its number.
// A station stands where it is put (typed in or picked on the map). GPS fixes move
// it when it follows GPS, and the reports of the vessel it rides, by its MMSI,
// move it before GPS does. GPS comes from the inputs that name the station, or
// from every input when none does.
// The tab's footer saves it like any other section; a renumbered station takes
// the inputs that name it along, a deleted one leaves them at the default station.

import Map from 'ol/Map.js';
import View from 'ol/View.js';
import Feature from 'ol/Feature.js';
import Point from 'ol/geom/Point.js';
import VectorLayer from 'ol/layer/Vector.js';
import VectorSource from 'ol/source/Vector.js';
import Style from 'ol/style/Style.js';
import CircleStyle from 'ol/style/Circle.js';
import Fill from 'ol/style/Fill.js';
import Stroke from 'ol/style/Stroke.js';
import {fromLonLat, toLonLat} from 'ol/proj.js';
import { el, moreMenu } from './dom.js';
import { App, ConfigStore, registerEditor } from './config-store.js';
import { api } from './api.js';
import { addBasemap } from './basemap.js';
import { icon } from './icons.js';
import { DOCS, docLink } from './docs.js';
import { num, validPos, isDefaultStation as isDefault, stationName } from './stations.js';

const CONTAINER = 'hub-stations-container';
export const TAGS = { fixed: 'Fixed', gps: 'GPS', vessel: 'Vessel' };
const OWN_NOTE = 'Where your antenna stands.';
const DEFAULT_NOTE = 'The default station: inputs stand here unless they pick another.';
// a fix older than this is shown as stale
const FRESH_S = 300;
const LIVE_MS = 5000;

const hasPos = s => validPos(num(s.lat), num(s.lon));
const fmt = v => Number(v).toFixed(4);
const ago = s => (s < 60 ? `${s} s ago` : s < 3600 ? `${Math.round(s / 60)} min ago` : `${Math.round(s / 3600)} h ago`);

// what moves a station: the vessel its MMSI names, else GPS when it follows it, else nothing
export function modeOf(s) {
    if (s.mmsi) return 'vessel';
    if (s.gps === true) return 'gps';
    return 'fixed';
}

// The settings as the configuration keeps them
function clean(s) {
    const out = {};
    if (Number.isFinite(num(s.id))) out.id = num(s.id);
    if (String(s.name || '').trim()) out.name = String(s.name).trim();
    if (Number.isFinite(num(s.lat))) out.lat = num(s.lat);
    if (Number.isFinite(num(s.lon))) out.lon = num(s.lon);
    if (Number.isFinite(num(s.alt))) out.alt = num(s.alt);
    if (Number.isFinite(num(s.mmsi))) out.mmsi = num(s.mmsi);
    if (s.gps === true) out.gps = true;
    out.share_loc = !!s.share_loc;
    return out;
}

export function createStationsEditor(host, { describeInput, onSaved, onList, initial } = {}) {
    const state = { stations: [], receivers: [], selected: 0, live: {}, deleted: new Set() };
    const current = () => state.stations[state.selected];
    // stations listed besides the default one
    const several = () => state.stations.length > 1;
    let map = null, picking = false, timer = null;

    // the footer saves what is dirty, this tab included
    const editor = registerEditor(CONTAINER, { save: () => save(), data: () => state.stations });
    const touched = () => editor.touched();

    // ---- the inputs at a station, by the number it had when loaded; at the default station those that name none, or 0
    const named = r => r.station !== undefined && r.station !== null && r.station !== '' && Number(r.station) !== 0;
    const users = s => (isDefault(s) || s._from !== null) ? state.receivers
        .map((r, i) => ({ r, i }))
        .filter(({ r }) => (isDefault(s) ? !named(r) : named(r) && Number(r.station) === s._from))
        .map(({ r, i }) => (describeInput ? describeInput(r, i, state.receivers).title : `Input #${i + 1}`)) : [];

    // what the live poll says of a station
    const liveOf = s => state.live[s.id] || {};

    // the rail lists the stations, and is told of a name or a number typed
    const main = el('div', 'st-main st-stations__main');
    host.className = 'st st-single st-stations';
    host.replaceChildren(main);
    const listChanged = () => onList?.();

    function add() {
        const id = state.stations.reduce((m, s) => Math.max(m, Number(s.id) || 0), 0) + 1;
        state.stations.push({ id, share_loc: false, _from: null });
        state.selected = state.stations.length - 1;
        touched();
        render();
    }

    function remove(i) {
        const s = state.stations[i];
        if (isDefault(s)) return;
        const named = users(s);
        const note = named.length ? `\n\n${named.join(', ')} ${named.length === 1 ? 'uses' : 'use'} it and will move to the default station.` : '';
        if (!confirm(`Delete ${stationName(s)}?${note}`)) return;
        if (s._from !== null) state.deleted.add(s._from);
        state.stations.splice(i, 1);
        state.selected = Math.min(i, state.stations.length - 1);
        touched();
        render();
    }

    // ---- the chosen station
    function field(label, input, cls) {
        return el('label', 'st-field st-stations__field' + (cls ? ' ' + cls : ''), {}, el('span', 'st-label', { text: label }), input);
    }

    // text that takes a comma for the point too: a number field shows the browser's own decimals,
    // 51,98 where the map and the configuration say 51.98
    function numberInput(s, key, attrs, onSet) {
        const { min, max, step } = attrs || {};
        return el('input', 'st-input', {
            type: 'text', inputMode: step === 1 ? 'numeric' : 'decimal', value: s[key] ?? '', spellcheck: 'false', autocomplete: 'off',
            onInput: e => {
                const v = e.target.value.trim().replace(',', '.');
                const n = Number(v);
                const bad = v !== '' && (!Number.isFinite(n) || (min !== undefined && n < min) || (max !== undefined && n > max));
                e.target.classList.toggle('invalid', bad);
                if (bad) return;
                if (v === '') delete s[key]; else s[key] = n;
                touched();
                onSet?.();
                listChanged();
            }
        });
    }

    function render() {
        if (map) { map.setTarget(null); map = null; }
        picking = false;
        main.replaceChildren();
        const s = current();
        const named = users(s);
        const title = el('h2', 'st-main__title st-stations__title', { text: stationName(s) });
        const used = named.length ? el('span', 'st-stations__used', {}, el('span', 't-small t-subtle', { text: 'Used by' }),
            named.map(n => el('span', 'st-chip st-chip--all', { text: n })))
            : el('span', 't-small t-subtle', { text: isDefault(s) ? 'No input stands here' : 'No input uses it yet' });
        main.append(el('div', 'st-main__head', {},
            el('div', 'st-main__headtext st-stations__headtext', {}, title, used),
            el('div', 'st-main__actions', {}, docLink(DOCS.stations), isDefault(s) ? null :
                moreMenu('More actions', [['Delete…', () => remove(state.selected), true]]))));

        const name = el('input', 'st-input', {
            type: 'text', value: s.name || '', placeholder: 'e.g. Harbour mast', spellcheck: 'false',
            onInput: e => { s.name = e.target.value; title.textContent = stationName(s); touched(); listChanged(); }
        });
        // station 0 keeps its number
        if (isDefault(s))
            main.append(el('p', 'st-help st-stations__ownnote', { text: several() ? DEFAULT_NOTE : OWN_NOTE }),
                el('div', 'st-stations__row', {}, field('Name', name, 'st-stations__grow')));
        else
            main.append(el('div', 'st-stations__row', {},
                field('Number', numberInput(s, 'id', { min: 1, step: 1, max: 2147483647 }, () => { title.textContent = stationName(s); }), 'st-stations__num'),
                field('Name', name, 'st-stations__grow')),
                el('p', 'st-help st-stations__namenote', { text: 'Written as station_id in every output.' }));

        renderPosition(s, named);
        main.append(shareSwitch(s));
        listChanged();
    }

    // Where it stands, then what moves it: GPS when it follows it, a vessel when it rides one, the vessel
    // first. The default station rides a vessel only while it is the only station.
    function renderPosition(s, named) {
        const body = el('div', 'st-stations__source');
        main.append(el('div', 'st-stations__pos', {}, el('span', 'st-label', { text: 'Position' })), body);
        renderFixed(s, body);
        body.append(el('p', 'st-help st-stations__note', { text: 'Where it stands until GPS or a vessel moves it.' }));
        main.append(gpsSwitch(s, named));
        if (!(isDefault(s) && several() && !s.mmsi)) main.append(vesselField(s));
    }

    // a setting that is on or off, with what it means under its name
    function switchRow(title, text, checked, onChange) {
        return el('label', 'st-stations__share', {},
            el('span', 'st-switch', {},
                el('input', '', { type: 'checkbox', checked: !!checked, onChange: e => onChange(e.target.checked) }),
                el('span', 'st-switch__track')),
            el('span', 'st-stations__sharetext', {}, el('strong', '', { text: title }), el('span', '', { text })));
    }

    // whether it shows
    function shareSwitch(s) {
        return switchRow('Share position', 'Off keeps it on this machine.', s.share_loc, on => { s.share_loc = on; touched(); listChanged(); });
    }

    // GPS: from the inputs at this station, or every input when none names it
    function gpsSwitch(s, named) {
        const from = named.length ? named.join(', ') : 'every input';
        const row = switchRow('Follow GPS', `Moves with each fix from ${from}.`, s.gps === true, on => {
            s.gps = on;
            touched();
            listChanged();
            render();
        });
        if (s.gps !== true) return row;
        return el('div', 'st-stations__gps', {}, row, statusLine('gps', s),
            el('p', 'st-help', {}, 'A GPS, or an AIS receiver passing GPS sentences. ', docLink(DOCS.gps, 'About GPS')));
    }

    // the vessel it is on: its reports go before GPS
    function vesselField(s) {
        const line = statusLine('vessel', s);
        const input = numberInput(s, 'mmsi', { step: 1, min: 1, max: 999999999 }, () => paintStatus(line, 'vessel', s));
        input.placeholder = 'None';
        return el('div', 'st-stations__vessel', {},
            el('div', 'st-stations__row', {}, field('Rides a vessel (MMSI)', input, 'st-stations__short')),
            el('p', 'st-help', { text: 'On board a ship: its reports move the station, ahead of GPS.' }),
            line);
    }

    function renderFixed(s, body) {
        const lat = numberInput(s, 'lat', { step: 0.0001, min: -90, max: 90 }, () => placeMarker());
        const lon = numberInput(s, 'lon', { step: 0.0001, min: -180, max: 180 }, () => placeMarker());
        body.append(el('div', 'st-stations__row', {},
            field('Latitude', lat, 'st-stations__grow'),
            field('Longitude', lon, 'st-stations__grow'),
            field('Antenna height (m)', numberInput(s, 'alt', { step: 1, min: -1000, max: 20000 }), 'st-stations__grow')));

        // the map: the station where it stands; Pick on map takes the next click
        const mapEl = el('div', 'pe-map__canvas');
        const wrap = el('div', 'pe pe-map st-stations__map');
        const pick = el('button', 'pe-tool st-stations__tool st-stations__pick', { type: 'button', 'aria-pressed': 'false', html: icon('pin', 'pe-icon') + '<span>Pick on map</span>',
            onClick: () => {
                picking = !picking;
                pick.setAttribute('aria-pressed', String(picking));
                wrap.classList.toggle('is-picking', picking);
                pick.lastChild.textContent = picking ? 'Click the map…' : 'Pick on map';
            } });
        const zoom = by => () => map.getView().animate({ zoom: map.getView().getZoom() + by, duration: 150 });
        // the browser's own position, where it may give one: on this machine, or over https
        const here = navigator.geolocation && window.isSecureContext ? el('button', 'pe-tool st-stations__tool st-stations__here', { type: 'button', html: icon('here', 'pe-icon') + '<span>Use my location</span>',
            onClick: () => navigator.geolocation.getCurrentPosition(p => {
                s.lat = Number(p.coords.latitude.toFixed(5));
                s.lon = Number(p.coords.longitude.toFixed(5));
                lat.value = s.lat;
                lon.value = s.lon;
                placeMarker(true);
                map.getView().setZoom(12);
                touched();
                listChanged();
            }, e => App.notify('error', 'The browser gave no position: ' + e.message), { timeout: 10000 }) }) : null;
        wrap.append(mapEl,
            el('div', 'pe-toolbar pe-toolbar--tools', { role: 'toolbar', 'aria-label': 'Position' }, here, pick),
            el('div', 'pe-toolbar pe-toolbar--view', { role: 'toolbar', 'aria-label': 'Map view' },
                el('button', 'pe-tool pe-tool--icon', { type: 'button', title: 'Zoom in', html: icon('zoomIn', 'pe-icon'), onClick: zoom(1) }),
                el('button', 'pe-tool pe-tool--icon', { type: 'button', title: 'Zoom out', html: icon('zoomOut', 'pe-icon'), onClick: zoom(-1) })));
        body.append(wrap);
        const marker = new Feature();
        const accent = getComputedStyle(document.documentElement).getPropertyValue('--color-accent').trim() || '#3b5bdb';
        const layer = new VectorLayer({
            source: new VectorSource({ features: [marker] }),
            style: [
                new Style({ image: new CircleStyle({ radius: 18, fill: new Fill({ color: 'rgba(59,91,219,0.18)' }) }) }),
                new Style({ image: new CircleStyle({ radius: 8, fill: new Fill({ color: accent }), stroke: new Stroke({ color: '#fff', width: 2.5 }) }) })
            ]
        });
        map = new Map({
            target: mapEl, controls: [],
            layers: [layer],
            view: new View({ center: fromLonLat([4.4, 51.95]), zoom: 4 })
        });
        addBasemap(map, wrap);
        function placeMarker(center) {
            if (!map) return;
            if (!hasPos(s)) { marker.setGeometry(undefined); return; }
            const at = fromLonLat([num(s.lon), num(s.lat)]);
            marker.setGeometry(new Point(at));
            if (center) map.getView().setCenter(at);
        }
        if (hasPos(s)) { map.getView().setZoom(11); placeMarker(true); }
        map.on('singleclick', e => {
            if (!picking) return;
            const [lo, la] = toLonLat(e.coordinate);
            s.lat = Number(la.toFixed(5));
            s.lon = Number(lo.toFixed(5));
            lat.value = s.lat;
            lon.value = s.lon;
            placeMarker();
            pick.click();
            touched();
            listChanged();
        });
        requestAnimationFrame(() => map && map.updateSize());
    }

    // a status line: the dot, the words, kept current by the live poll
    function statusLine(kind, s) {
        const line = el('p', 'st-stations__status', { dataset: { status: kind } });
        paintStatus(line, kind, s);
        return line;
    }

    function paintStatus(line, kind, s) {
        const live = liveOf(s);
        let ok = false, text;
        if (kind === 'gps') {
            if (live.fix_age_s !== undefined && validPos(live.lat, live.lon)) {
                ok = live.fix_age_s < FRESH_S;
                text = `Last fix ${fmt(live.lat)}, ${fmt(live.lon)} · ${ago(live.fix_age_s)}`;
            } else text = 'Waiting for a fix';
        } else {
            if (live.vessel) {
                ok = live.vessel.report_age_s < FRESH_S;
                text = `${live.vessel.name || 'MMSI ' + live.vessel.mmsi} · last report ${ago(live.vessel.report_age_s)}`;
            } else text = s.mmsi ? 'Not heard yet' : '';
        }
        // no vessel, nothing to report
        line.hidden = !text;
        line.replaceChildren(el('span', 'st-stations__dot' + (ok ? ' is-ok' : '')), text);
    }

    // ---- what the stations are doing now, while the tab shows
    async function poll() {
        if (!host.isConnected || !host.offsetParent) return;
        try {
            const body = await api('/api/stations/live');
            state.live = {};
            for (const s of body.stations || []) state.live[s.id] = s;
        } catch { return; }
        const s = current();
        if (s) main.querySelectorAll('[data-status]').forEach(line => paintStatus(line, line.dataset.status, s));
    }

    // ---- saving
    // the reader takes 1.5 for a number; the rest it checks, and its answer shows as the save's
    function validate() {
        const i = state.stations.findIndex(s => s.id !== undefined && s.id !== '' && !Number.isInteger(num(s.id)));
        return i < 0 ? null : [i, `${stationName(state.stations[i])}: the number must be a whole number.`];
    }

    async function save() {
        const problem = validate();
        if (problem) {
            state.selected = problem[0];
            render();
            App.notify('error', problem[1]);
            return;
        }
        const stations = state.stations.map(clean);
        let rewired = false, receivers = [];
        // the inputs follow their station to its new number, or go to the default station
        const saved = await editor.write(cfg => {
            const moved = new globalThis.Map();
            for (const s of state.stations) if (s._from !== null) moved.set(s._from, Number(s.id));
            for (const r of cfg.receiver || []) {
                if (r.station === undefined || r.station === null || r.station === '') continue;
                const from = Number(r.station);
                if (moved.has(from) && moved.get(from) !== from) { r.station = moved.get(from); rewired = true; }
                else if (!moved.has(from) && state.deleted.has(from)) { delete r.station; rewired = true; }
            }
            cfg.stations = stations;
            receivers = cfg.receiver || [];
        }, 'the stations');
        if (!saved) return;
        // from here on the stations are what was saved
        state.stations = stations.map(s => ({ ...s, _from: s.id }));
        state.deleted.clear();
        state.receivers = receivers;
        App.notify('success', 'Stations saved');
        onSaved?.(rewired);
        render();
    }

    async function load() {
        const cfg = await ConfigStore.fetch();
        state.receivers = cfg.receiver || [];
        const listed = (cfg.stations || []).map(s => ({ ...s, _from: Number(s.id) }));
        // a file written by hand without it: the default station, unsaved until the page is
        const zero = listed.find(isDefault);
        state.stations = [zero || { id: 0, share_loc: false, _from: null }, ...listed.filter(s => !isDefault(s))];
        state.selected = Number.isInteger(initial) && initial < state.stations.length ? initial : 0;
        render();
        poll();
    }

    timer = setInterval(poll, LIVE_MS);
    const ready = load().catch(e => App.notify('error', 'Could not read the stations: ' + e.message));

    return {
        // settles once the stations are read
        ready,
        // the stations with their edits, the default station first
        stations: () => state.stations,
        selected: () => state.selected,
        select(key) {
            if (!(Number.isInteger(key) && key < state.stations.length)) key = 0;
            if (key === state.selected) return;
            state.selected = key;
            render();
        },
        add,
        destroy() {
            clearInterval(timer);
            if (map) map.setTarget(null);
            editor.destroy();
        }
    };
}
