// Flow: which input's messages reach which output, and where the routes are drawn. A zone is a
// route: an output with zones takes the inputs that share one, an output without takes the inputs
// without. A connection is drawn from an input's dot to an output's and named; the name is the zone.
// The services read the ship table, which every input feeds: what each shows is on its own page.

import { formatBytes } from '@aiscatcher/core/text.js';
import { el } from './dom.js';
import { App, ConfigStore, ZoneColors, registerEditor } from './config-manager.js';
import { OUTPUT_TYPES, channelTitle } from './schema.js';
import { describeInput } from './inputs.js';
import { stationName } from './stations.js';

const SVG_NS = 'http://www.w3.org/2000/svg';
const CONTAINER = 'flow-zones';
// what the outputs are doing, as the main server beside the panel reports it
const OUTPUT_STATS = '/viewer/api/output_stats.json';
const STATS_MS = 5000;
// the radio inputs: without a sharing key the community map takes only these
const LIVE_INPUTS = new Set(['RTLSDR', 'AIRSPY', 'AIRSPYHF', 'HACKRF', 'SDRPLAY', 'HYDRASDR', 'SOAPYSDR']);

// the statistics' output types, each to the type of output that reports it
const STAT_TYPES = {};
OUTPUT_TYPES.forEach(o => {
    if (o.statType) STAT_TYPES[o.statType] = o.value;
    (o.statTypes || []).forEach(t => { STAT_TYPES[t] = o.value; });
});

const safeLink = url => (url && /^https?:\/\//i.test(url) ? url : null);
// a zone's name: letters, digits, - and _
const zoneName = t => String(t || '').trim().toLowerCase().replace(/[^a-z0-9_-]+/g, '-').replace(/^-+|-+$/g, '');
// the host of a web address, for a connection's first name
const hostOf = url => { try { return new URL(url).hostname; } catch { return ''; } };

// Every output an input can feed, with its zones and its key in the rail: `type` is its kind (the
// Outputs page's value), `hint` the word a new connection to it is named after
export function collectOutputs(cfg) {
    const outputs = [];
    outputs.push({ key: 'sharing', type: 'sharing', name: 'Community map', hint: 'community', zones: Array.isArray(cfg.sharing_zone) ? cfg.sharing_zone : [],
        active: cfg.sharing === true, liveOnly: !String(cfg.sharing_key || '').trim() });
    // stdout takes every input; "0" and "none" are the silent levels
    if (cfg.screen !== undefined && !['0', 'none'].includes(String(cfg.screen).toLowerCase()))
        outputs.push({ key: 'screen', type: 'screen', name: 'Screen', zones: [], active: true, fixed: true });
    OUTPUT_TYPES.forEach(({ value, configKey, flowLabel }) => {
        if (!configKey) return;
        (cfg[configKey] || []).forEach((item, index) => {
            const name = item.description ? `${flowLabel} · ${item.description}` : channelTitle(flowLabel, item);
            outputs.push({ key: `${value}:${index}`, type: value, index, name, hint: item.description || item.host || hostOf(item.url) || flowLabel,
                zones: item.zone || [], active: item.active !== false, link: item.link });
        });
    });
    return outputs;
}

// an input's zones: its own, and those of its engines
function inputZones(r) {
    const own = Array.isArray(r.zone) ? r.zone : [];
    const engines = Array.isArray(r.engines) ? r.engines.flatMap(e => (Array.isArray(e.zone) ? e.zone : [])) : [];
    return [...new Set([...own, ...engines])];
}
// whether an output takes an input, as the engine routes: a zone is a route
function routes(o, r) {
    if (o.fixed) return true;
    if (o.liveOnly && !LIVE_INPUTS.has(String(r.input || '').toUpperCase())) return false;
    const zi = inputZones(r);
    if (o.zones.length) return o.zones.some(z => zi.includes(z));
    return !zi.length;
}
// how many switched-on outputs take this input
export function inputRouting(item) {
    return ConfigStore.fetch().then(cfg => collectOutputs(cfg).filter(o => o.active && routes(o, item)).length).catch(() => null);
}
// how many switched-on inputs feed this output
export function outputRouting(item) {
    const zones = Array.isArray(item.zone) ? item.zone : Array.isArray(item.sharing_zone) ? item.sharing_zone : [];
    const o = { zones, liveOnly: item.sharing !== undefined && !String(item.sharing_key || '').trim() };
    return ConfigStore.fetch().then(cfg => (cfg.receiver || []).filter(r => r.active !== false && routes(o, r)).length).catch(() => null);
}

function setZones(obj, key, zones) {
    if (!obj || !zones) return;
    if (zones.length) obj[key] = [...zones]; else delete obj[key];
}

function setStatus(node, tone, text) {
    node.className = 'st-status st-status--' + tone;
    node.textContent = text;
}
function putStats(node, parts) {
    node.replaceChildren(...parts.map(t => el('span', 'st-num', { text: t })));
}
function statParts(type, s) {
    const parts = [`${formatBytes(s.bytes_out)} out`];
    if (s.bytes_in > 0) parts.push(`${formatBytes(s.bytes_in)} in`);
    if (type !== 'udp') parts.push(`ok/fail ${s.connect_ok}/${s.connect_fail}`);
    if (s.reconnects > 0) parts.push(`${s.reconnects} reconnects`);
    if (s.dropped > 0) parts.push(`${s.dropped} dropped`);
    return parts;
}

function zoneChip(z) {
    const c = el('span', 'st-chip st-chip--zone', { text: z });
    c.style.setProperty('--z', ZoneColors.css(z));
    return c;
}

// A card of the drawing: its name (a link to its page), its status, a second line, its zones and
// statistics, and the dot a connection is drawn from or to
function flowNode({ name, isInput, onClick, link, handle }) {
    const url = safeLink(link);
    const status = el('span');
    const top = el('div', 'st-node__top', {},
        el('button', 'st-node__name', { type: 'button', text: name, title: 'Open its settings', onClick }),
        url ? el('a', '', { href: url, target: '_blank', rel: 'noopener', title: url,
            html: '<svg class="st-node__link" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-label="its page"><path d="M7 17L17 7M9 7h8v8"/></svg>' }) : null,
        el('span', 'st-spacer'), status);
    const line = el('div', 'st-node__sub');
    const chips = el('span', 'st-node__zones');
    const stats = el('span', 'st-node__stats');
    const card = el('article', 'st-node ' + (isInput ? 'st-node--in' : 'st-node--out'), {}, top, line, el('div', 'st-node__bottom', {}, chips, stats));
    if (handle)
        card.append(el('button', 'st-node__hd st-node__hd--' + (isInput ? 'out' : 'in'), { type: 'button',
            title: isInput ? 'Draw a connection from here' : 'Connect here', 'aria-label': (isInput ? 'Connect from ' : 'Connect to ') + name, onClick: handle }));
    return { card, status, stats, chips, line };
}

// The Flow page in `pane`. `open(tab, key)` shows a page of the rail; `receivers()` the Inputs
// page's rows with their edits, while it is open; `pages()` the Inputs and Outputs pages' managers,
// which get the zones as they are edited here, so a save of theirs writes the same; `running()`
// whether the receiver runs; `shown()` whether this page shows; `onSaved` after a save.
export function createFlowEditor(pane, { open, receivers, pages, running, shown, onSaved }) {
    const $ = id => pane.querySelector('#' + id);
    const inputsEl = $('flow-inputs'), outputsEl = $('flow-outputs'), svg = $('flow-svg'), hint = $('flow-hint'), bar = $('flow-name'), list = $('flow-zones');
    // the edits: each input's and output's zones, saved together
    let flow = null;
    let armed = { from: null, pending: null };
    const nodes = { inputs: [], outputs: [] };
    let renderId = 0, observer = null, statsTimer = null;
    const editor = registerEditor(CONTAINER, { save: () => save(), data: () => flow });

    async function save() {
        const saved = await editor.write(cfg => {
            (cfg.receiver || []).forEach((r, i) => { if (flow.inputs[i]) setZones(r, 'zone', flow.inputs[i].zone); });
            flow.outputs.forEach(o => {
                if (o.key === 'sharing') setZones(cfg, 'sharing_zone', o.zones);
                else if (o.index !== undefined) {
                    const t = OUTPUT_TYPES.find(x => x.value === o.type);
                    const row = t && Array.isArray(cfg[t.configKey]) ? cfg[t.configKey][o.index] : null;
                    setZones(row, 'zone', o.zones);
                }
            });
        }, 'the connections');
        if (!saved) return;
        App.notify('success', 'Connections saved');
        onSaved?.();
    }

    // the pages that hold the same rows take the zones as edited here
    const outputZonesOf = key => flow.outputs.find(o => o.key === key)?.zones;
    function syncZones() {
        const { inputs, outputs, outputType } = pages();
        if (inputs) flow.inputs.forEach((r, i) => { if (inputs.data[i]) setZones(inputs.data[i], 'zone', r.zone); });
        if (outputs && outputType === 'sharing') setZones(outputs.data, 'sharing_zone', outputZonesOf('sharing'));
        else if (outputs && outputs.config.isList)
            outputs.data.forEach((row, i) => { const z = outputZonesOf(`${outputType}:${i}`); if (z) setZones(row, 'zone', z); });
    }
    function touched() {
        editor.touched();
        syncZones();
        draw();
    }
    function connect(i, key, name) {
        const z = zoneName(name);
        if (!z) return App.notify('error', 'Give the connection a name: letters, digits, - or _');
        const r = flow.inputs[i], o = flow.outputs.find(x => x.key === key);
        r.zone = [...new Set([...(r.zone || []), z])];
        if (!o.zones.includes(z)) o.zones.push(z);
        armed = { from: null, pending: null };
        touched();
    }
    function dropFromZone(z, side, idx) {
        if (side === 'in') flow.inputs[idx].zone = (flow.inputs[idx].zone || []).filter(x => x !== z);
        else { const o = flow.outputs.find(x => x.key === idx); o.zones = o.zones.filter(x => x !== z); }
        touched();
    }
    function dropZone(z) {
        flow.inputs.forEach(r => { r.zone = (r.zone || []).filter(x => x !== z); });
        flow.outputs.forEach(o => { o.zones = o.zones.filter(x => x !== z); });
        touched();
    }
    const allZones = () => [...new Set([...flow.inputs.flatMap(x => x.zone || []), ...flow.outputs.flatMap(x => x.zones)])];

    // ---- what the page shows from the edits: chips, warnings, the lines, the list, the naming bar
    function draw() {
        if (!flow) return;
        nodes.inputs.forEach((n, i) => {
            const r = flow.inputs[i];
            n.chips.replaceChildren(...inputZones(r).map(zoneChip));
            const to = flow.outputs.filter(o => routes(o, r));
            n.card.dataset.armed = String(armed.from === i);
            n.card.dataset.nowhere = String(!to.length);
            n.line.textContent = n.station + (to.length ? '' : (n.station ? ' · ' : '') + 'goes nowhere');
        });
        const radio = flow.inputs.some(r => LIVE_INPUTS.has(String(r.input || '').toUpperCase()));
        nodes.outputs.forEach(n => {
            const o = flow.outputs.find(x => x.key === n.key);
            n.chips.replaceChildren(...o.zones.map(zoneChip));
            const from = flow.inputs.filter(r => routes(o, r));
            n.card.dataset.nothing = String(!from.length);
            n.line.textContent = !from.length ? (o.liveOnly && !radio ? 'Radio inputs only, until it has a sharing key' : 'Receives nothing')
                : o.fixed ? 'Every input' : o.zones.length ? 'Its connections' : o.liveOnly ? 'Radio inputs without a zone, until it has a key' : 'Inputs without a zone';
        });
        drawLines();
        // the hint, and the naming bar while a connection waits for its name
        hint.textContent = armed.pending ? '' : flow.inputs.length < 2
            ? 'One input reaches every output. Connections need two inputs.'
            : armed.from !== null
            ? `Now click the dot of the output for ${flow.inputs[armed.from].label}.`
            : 'Click an input\'s dot, then an output\'s, to connect them.';
        renderNameBar();
        renderZoneList();
    }
    function drawLines() {
        if (!flow) return;
        svg.innerHTML = '';
        const box = svg.getBoundingClientRect();
        const graph = svg.parentElement.getBoundingClientRect();
        svg.setAttribute('height', graph.height);
        svg.style.height = graph.height + 'px';
        const w = box.width;
        nodes.inputs.forEach((ni, i) => {
            const r = flow.inputs[i];
            nodes.outputs.forEach(no => {
                const o = flow.outputs.find(x => x.key === no.key);
                if (!routes(o, r)) return;
                const a = ni.card.getBoundingClientRect(), b = no.card.getBoundingClientRect();
                if (!a.height || !b.height) return;
                const y1 = a.top + a.height / 2 - box.top, y2 = b.top + b.height / 2 - box.top;
                const zone = o.zones.find(z => inputZones(r).includes(z));
                const p = document.createElementNS(SVG_NS, 'path');
                p.setAttribute('d', `M0 ${y1} C${w / 2} ${y1} ${w / 2} ${y2} ${w} ${y2}`);
                p.setAttribute('class', 'st-route' + (zone ? '' : ' st-route--all'));
                if (zone) { p.style.stroke = ZoneColors.css(zone); p.dataset.zone = zone; }
                svg.appendChild(p);
            });
        });
    }
    function renderNameBar() {
        const p = armed.pending;
        bar.hidden = !p;
        if (!p) return bar.replaceChildren();
        const r = flow.inputs[p.i], o = flow.outputs.find(x => x.key === p.key);
        const input = el('input', 'st-input st-input--mono', { id: 'flow-zone-name', 'aria-label': 'Name of the connection', value: p.name });
        bar.replaceChildren(el('span', 'st-flow__what', {}, el('strong', '', { text: r.label }), ' → ', el('strong', '', { text: o.name })),
            el('span', 'st-flow__lbl', { text: 'Name' }), input,
            el('button', 'st-btn st-btn--sm st-btn--primary', { type: 'submit', text: 'Connect' }));
        const zones = allZones();
        if (zones.length) {
            bar.append(el('span', 'st-flow__or', { text: 'or add to' }));
            zones.forEach(z => bar.append(el('button', 'st-flow__join', { type: 'button', onClick: () => connect(p.i, p.key, z) }, zoneChip(z))));
        }
        bar.append(el('button', 'st-btn st-btn--sm', { type: 'button', text: 'Cancel', onClick: () => { armed = { from: null, pending: null }; draw(); } }));
        bar.onsubmit = e => { e.preventDefault(); connect(p.i, p.key, input.value); };
        requestAnimationFrame(() => { input.focus(); input.select(); });
    }
    function renderZoneList() {
        const zones = allZones();
        if (!zones.length) {
            list.replaceChildren(el('p', 'st-flow__none', { text: flow.inputs.length < 2 ? '' : 'No connections: every input reaches every output.' }));
            return;
        }
        const rows = zones.map(z => {
            const member = (label, side, idx) => el('span', 'st-flow__member', {}, label,
                el('button', 'st-flow__drop', { type: 'button', text: '×', title: `Take ${label} out of ${z}`, onClick: () => dropFromZone(z, side, idx) }));
            const ins = flow.inputs.map((r, i) => ((r.zone || []).includes(z) ? member(r.label, 'in', i) : null)).filter(Boolean);
            const outs = flow.outputs.filter(o => o.zones.includes(z)).map(o => member(o.name, 'out', o.key));
            return el('div', 'st-flow__zone', {}, zoneChip(z),
                el('span', 'st-flow__members', {}, ...ins, el('span', 'st-flow__arrow', { text: '→' }), ...outs),
                el('button', 'st-link', { type: 'button', text: 'Remove', onClick: () => dropZone(z) }));
        });
        list.replaceChildren(el('h3', 'st-section__title', { text: 'Connections' }), ...rows);
    }

    // ---- what the receiver says: whether it runs, and each output's traffic and connection
    function paintInput(n) {
        if (!n.active) setStatus(n.status, 'off', 'Off');
        else if (running()) setStatus(n.status, 'ok', 'Running');
        else setStatus(n.status, 'off', 'Stopped');
    }
    const paintOutput = o => setStatus(o.status, o.active ? 'ok' : 'off', o.active ? 'Active' : 'Off');
    function paintStats() {
        // the receiver can stop or start while the page is open: the inputs follow it
        nodes.inputs.forEach(paintInput);
        const outputs = nodes.outputs;
        if (!running()) {
            // nothing is connected while it is stopped: back to what the configuration says
            outputs.forEach(o => { putStats(o.stats, []); paintOutput(o); });
            return;
        }
        fetch(OUTPUT_STATS)
            .then(r => { if (!r.ok) throw new Error(r.status); return r.json(); })
            .then(stat => {
                // the statistics list the switched-on outputs of each type in the configuration's order
                const pools = {};
                (stat.outputs || []).forEach(o => {
                    const type = o.description === 'Community Feed' ? 'sharing' : STAT_TYPES[o.type];
                    if (type) (pools[type] = pools[type] || []).push(o);
                });
                outputs.forEach(o => {
                    const m = o.active && pools[o.type] && pools[o.type].shift();
                    if (!m) return putStats(o.stats, []);
                    putStats(o.stats, statParts(o.type, m.stats));
                    // a connection type says whether it is connected; UDP and HTTP have no connection to report
                    if (o.type !== 'udp' && o.type !== 'http')
                        setStatus(o.status, m.stats.connected ? 'ok' : 'bad', m.stats.connected ? 'Connected' : 'Not connected');
                });
            })
            .catch(() => outputs.forEach(o => putStats(o.stats, [])));
    }

    function stop() {
        observer?.disconnect();
        observer = null;
        clearInterval(statsTimer);
        statsTimer = null;
    }

    // the page drawn afresh, from the edits not saved yet or else the configuration
    function load() {
        stop();
        const id = ++renderId;
        ConfigStore.fetch()
            .then(cfg => {
                if (!shown() || id !== renderId) return;
                if (!flow || !editor.dirty) {
                    const rows = receivers() || cfg.receiver || [];
                    flow = {
                        inputs: rows.map((r, i) => ({ ...JSON.parse(JSON.stringify(r)), label: describeInput(r, i, rows).title, index: i })),
                        outputs: collectOutputs(cfg).map(o => ({ ...o, zones: [...o.zones] }))
                    };
                }
                armed = { from: null, pending: null };
                const stations = Array.isArray(cfg.stations) ? cfg.stations : [];
                const stationOf = r => {
                    if (r.station === undefined || r.station === null || r.station === '') return '';
                    const s = stations.find(st => Number(st.id) === Number(r.station));
                    return s ? stationName(s) : 'station ' + r.station;
                };
                const routable = flow.inputs.length >= 2;
                nodes.inputs = flow.inputs.map((r, i) => {
                    const n = flowNode({ name: r.label, isInput: true, onClick: () => open('input', i),
                        handle: routable ? () => { armed = { from: armed.from === i ? null : i, pending: null }; draw(); } : null });
                    return { ...n, station: stationOf(r), active: r.active !== false };
                });
                nodes.outputs = flow.outputs.map(o => {
                    const n = flowNode({ name: o.name, isInput: false, link: o.link,
                        onClick: () => open('output', o.index !== undefined ? `${o.type}:${o.index}` : o.type),
                        handle: o.fixed || !routable ? null : () => {
                            if (armed.from === null) return App.notify('info', 'Click the dot of an input first', 3000);
                            const r = flow.inputs[armed.from];
                            if (o.zones.some(z => (r.zone || []).includes(z))) { armed = { from: null, pending: null }; return draw(); }
                            const name = o.zones[0] || (r.zone || [])[0] || zoneName(o.hint) || 'route';
                            armed = { from: armed.from, pending: { i: armed.from, key: o.key, name } };
                            draw();
                        } });
                    return { ...o, ...n };
                });
                nodes.inputs.forEach(paintInput);
                nodes.outputs.forEach(paintOutput);
                inputsEl.replaceChildren(...nodes.inputs.map(n => n.card));
                outputsEl.replaceChildren(...nodes.outputs.map(n => n.card));
                draw();
                paintStats();
                statsTimer = setInterval(() => { if (shown()) paintStats(); }, STATS_MS);
                requestAnimationFrame(drawLines);
                observer = new ResizeObserver(drawLines);
                observer.observe($('flow-patch'));
            })
            .catch(() => {
                if (shown() && id === renderId) hint.textContent = 'Could not read the configuration.';
            });
    }

    return {
        load,
        stop,
        destroy() { stop(); editor.destroy(); }
    };
}
