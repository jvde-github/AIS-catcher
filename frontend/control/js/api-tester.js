// Try the API and MCP: a request to one of the station's servers as a program or
// an assistant makes it, with its answer and the curl line that makes it again.
// The control panel makes the request on the station, so a server that listens
// there only is reached too.

import { el } from './dom.js';
import { ICONS } from './icons.js';

const SVG = {
    send: '<svg class="st-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 12h14M13 6l6 6-6 6"/></svg>'
};
// the paths that are not in the API's own description
const FEEDS = [
    { path: '/geojson', service: 'geojson', group: 'Feeds', summary: 'The ships as GeoJSON.', params: [] },
    { path: '/allpath.geojson', service: 'geojson', group: 'Feeds', summary: 'The ships\' tracks as GeoJSON.', params: [] },
    { path: '/kml', service: 'kml', group: 'Feeds', summary: 'The ships as KML, for Google Earth.', params: [] },
    { path: '/metrics', service: 'metrics', group: 'Feeds', summary: 'Message counts for Prometheus.', params: [] }
];

// the routes, by the service each belongs to, grouped by what they are about
let routes = null;
async function loadRoutes() {
    if (routes) return routes;
    const r = await fetch('/api/openapi');
    if (!r.ok) throw new Error('the API description is not available (' + r.status + ')');
    const spec = await r.json();
    routes = Object.entries(spec.paths || {}).map(([path, item]) => {
        const get = item.get || {};
        const system = path.startsWith('/api/v1/system') || path === '/api/v1/health';
        const part = path.split('/')[3] || '';
        return {
            path, service: system ? 'system' : 'api', summary: get.summary || '',
            group: system ? 'System' : part ? part[0].toUpperCase() + part.slice(1) : 'API',
            params: (get.parameters || []).map(p => ({ name: p.name, in: p.in, required: !!p.required, description: p.description || '', type: p.schema?.type || 'string' }))
        };
    });
    return routes;
}

// state that outlives a redraw of the page: the choices, and the key typed this visit
const kept = { mode: 'api', port: null, path: '/api/v1/vessels', values: {}, key: '', last: null, more: false,
    tools: {}, tool: null, mcpValues: {}, mcpLast: null };

// `servers`: [{ label, port, serve, bind }], each with what it serves;
// `highlight`: text to coloured JSON
export function renderApiTester(main, { servers, highlight, port }) {
    if (port) kept.port = port;
    main.append(el('p', 'field-desc st-svcs__lead', { text: 'Send a request to one of this station\'s servers.' }));
    const modes = [['api', 'API'], ['mcp', 'MCP']];
    const seg = el('div', 'st-seg st-try__mode', { role: 'radiogroup', 'aria-label': 'What to try' });
    const body = el('div', 'st-try');
    main.append(seg, body);
    const paintMode = () => {
        seg.replaceChildren(...modes.map(([key, label]) => el('button', 'st-seg__opt', {
            type: 'button', role: 'radio', 'aria-checked': String(kept.mode === key), text: label,
            onClick: () => { kept.mode = key; paintMode(); }
        })));
        if (kept.mode === 'mcp') tryMcp(body, servers, highlight); else tryApi(body, servers, highlight);
    };
    paintMode();
}

// the server to send to, among those that offer the mode's services
function serverField(list, onChange) {
    if (!list.some(s => s.port === kept.port)) kept.port = list[0].port;
    const sel = el('select', 'select', { 'aria-label': 'Server', onChange: e => { kept.port = Number(e.target.value); onChange(); } },
        list.map(s => el('option', '', { value: s.port, selected: s.port === kept.port, text: s.label })));
    return el('label', 'field st-stations__field st-stations__grow', {}, el('span', 'field-label', { text: 'Server' }), sel);
}

function keyField(onChange) {
    return el('label', 'field st-stations__field', {}, el('span', 'field-label', { text: 'Key' }),
        el('input', 'input', { type: 'password', value: kept.key, autocomplete: 'off', placeholder: 'When the server has keys',
            onInput: e => { kept.key = e.target.value; }, onChange }));
}

// a line to copy: a curl command, or the line that connects an assistant
function copyLine(text) {
    return el('div', 'st-try__curl', {},
        el('code', '', { text }),
        el('button', 'st-btn st-btn--sm', { type: 'button', html: ICONS.copy + 'Copy', onClick: e => {
            const b = e.currentTarget;
            navigator.clipboard?.writeText(text).then(() => { b.lastChild.textContent = 'Copied'; }, () => {});
        } }));
}

// a body as it came, coloured when it is JSON
function bodyView(text, highlight) {
    let pretty = text, json = false;
    try { pretty = JSON.stringify(JSON.parse(text), null, 2); json = true; } catch { /* not JSON: as it came */ }
    const pre = el('pre', 'st-try__body');
    if (json && highlight) pre.innerHTML = highlight(pretty); else pre.textContent = pretty;
    return pre;
}

function meta(status, ms, text, type, note) {
    const ok = status >= 200 && status < 300 && !note;
    return el('div', 'st-try__meta', {},
        el('span', 'st-try__status' + (ok ? ' is-ok' : ' is-bad'), { text: String(status) }),
        el('span', '', { text: `${ms} ms · ${size(text.length)}${type ? ' · ' + type.split(';')[0] : ''}` }),
        note ? el('span', 'st-try__note', { text: note }) : null);
}

// ---- the API: a GET of one of its routes
function tryApi(body, servers, highlight) {
    const tryable = servers.filter(s => ['api', 'system', 'geojson', 'metrics'].some(k => s.serve.includes(k)));
    if (!tryable.length)
        return body.replaceChildren(el('div', 'st-empty', {}, el('strong', '', { text: 'No server offers the API yet' }),
            el('span', '', { text: 'Switch API on for Main, or add a server with it, and save.' })));
    loadRoutes().then(list => draw(list)).catch(e => body.replaceChildren(el('p', 'st-warn-note', { text: 'Could not read the API description: ' + e.message })));

    function draw(list) {
        const serverRow = serverField(tryable, () => draw(list));
        const server = tryable.find(s => s.port === kept.port);
        const offered = [...list, ...FEEDS].filter(r => server.serve.includes(r.service));
        if (!offered.some(r => r.path === kept.path)) kept.path = offered[0]?.path;
        const route = offered.find(r => r.path === kept.path);

        const groups = new Map();
        offered.forEach(r => { if (!groups.has(r.group)) groups.set(r.group, []); groups.get(r.group).push(r); });
        const routeSel = el('select', 'select', { 'aria-label': 'Endpoint', onChange: e => { kept.path = e.target.value; draw(list); } },
            [...groups.entries()].map(([g, rs]) => el('optgroup', '', { label: g },
                rs.map(r => el('option', '', { value: r.path, selected: r.path === kept.path, text: r.path })))));

        const values = kept.values[kept.path] || (kept.values[kept.path] = defaults(route));
        const url = el('code', 'st-try__url');
        const paint = () => { url.textContent = 'GET ' + target(route, values); };
        const param = p => el('label', 'field st-try__param', {},
            el('span', 'field-label', {}, p.name, p.required ? el('span', 'st-try__req', { text: ' required' }) : null, el('span', 'st-try__in', { text: ' · ' + p.in })),
            el('input', 'input', { type: 'text', value: values[p.name] ?? '', placeholder: p.type, onInput: e => { values[p.name] = e.target.value; paint(); } }),
            p.description ? el('p', 'field-desc', { text: p.description }) : null);
        // the required and those given up front; the other filters folded away
        const all = route?.params || [];
        const front = all.filter(p => p.required || p.name === 'limit' || String(values[p.name] ?? '').trim());
        const rest = all.filter(p => !front.includes(p));
        const more = rest.length ? el('details', 'st-try__more', kept.more ? { open: '' } : {},
            el('summary', '', { text: `${rest.length} more parameter${rest.length === 1 ? '' : 's'}` }),
            el('div', 'st-try__params', {}, rest.map(param))) : null;
        more?.addEventListener('toggle', () => { kept.more = more.open; });
        const result = el('div', 'st-try__result');
        const send = el('button', 'st-btn st-btn--primary', { type: 'button', html: SVG.send + 'Send', onClick: () => go() });

        body.replaceChildren(
            el('div', 'st-stations__row', {}, serverRow,
                el('label', 'field st-stations__field st-stations__grow', {}, el('span', 'field-label', { text: 'Endpoint' }), routeSel)),
            route?.summary ? el('p', 'field-desc st-try__summary', { text: route.summary }) : null,
            front.length ? el('div', 'st-try__params', {}, front.map(param)) : null,
            more,
            keyField(),
            el('div', 'st-try__go', {}, url, send),
            result);
        paint();
        if (kept.last && kept.last.port === kept.port && kept.last.path === kept.path) show(kept.last);

        async function go() {
            const missing = all.find(p => p.required && !String(values[p.name] ?? '').trim());
            if (missing) return result.replaceChildren(el('p', 'st-warn-note', { text: `Give ${missing.name}.` }));
            const t = target(route, values);
            send.disabled = true;
            const start = performance.now();
            try {
                const r = await fetch(`/api/try/${server.port}${t}`, { headers: kept.key ? { Authorization: 'Bearer ' + kept.key } : {} });
                const text = await r.text();
                kept.last = { port: server.port, path: kept.path, status: r.status, ms: Math.round(performance.now() - start), type: r.headers.get('Content-Type') || '', text, target: t };
                show(kept.last);
            } catch (e) {
                result.replaceChildren(el('p', 'st-warn-note', { text: e.message }));
            } finally {
                send.disabled = false;
            }
        }

        function show(res) {
            result.replaceChildren(meta(res.status, res.ms, res.text, res.type), bodyView(res.text, highlight),
                copyLine(curlLine(server, res.target, !!kept.key)));
        }
    }
}

// ---- MCP: the tools a server offers an assistant, and a call of one
let rpcId = 0;
async function rpc(port, method, params) {
    const start = performance.now();
    const body = JSON.stringify({ jsonrpc: '2.0', id: ++rpcId, method, ...(params ? { params } : {}) });
    const headers = { 'Content-Type': 'application/json' };
    if (kept.key) headers.Authorization = 'Bearer ' + kept.key;
    const r = await fetch(`/api/try/${port}/mcp`, { method: 'POST', headers, body });
    const text = await r.text();
    let json = null;
    try { json = JSON.parse(text); } catch { /* not JSON-RPC: shown as it came */ }
    return { status: r.status, ms: Math.round(performance.now() - start), text, json, sent: body };
}

function tryMcp(body, servers, highlight) {
    const tryable = servers.filter(s => s.serve.includes('mcp'));
    if (!tryable.length)
        return body.replaceChildren(el('div', 'st-empty', {}, el('strong', '', { text: 'No server offers MCP yet' }),
            el('span', '', { text: 'Switch MCP on for Main, or add a server with it, and save.' })));
    draw();

    function draw() {
        const serverRow = serverField(tryable, () => draw());
        const server = tryable.find(s => s.port === kept.port);
        const tools = kept.tools[server.port];
        const head = el('div', 'st-stations__row', {}, serverRow);
        if (!tools || tools.error) {
            const load = el('button', 'st-btn', { type: 'button', text: 'Load tools', onClick: () => list() });
            body.replaceChildren(head, keyField(() => list()),
                tools?.error ? el('p', 'st-warn-note', { text: tools.error }) : el('p', 'field-desc', { text: 'Reading the tools…' }),
                tools?.error ? el('div', 'st-try__go', {}, load) : null);
            if (!tools) list();
            return;
        }
        if (!tools.some(t => t.name === kept.tool)) kept.tool = tools[0]?.name;
        const tool = tools.find(t => t.name === kept.tool);
        const toolSel = el('select', 'select', { 'aria-label': 'Tool', onChange: e => { kept.tool = e.target.value; draw(); } },
            tools.map(t => el('option', '', { value: t.name, selected: t.name === kept.tool, text: t.name })));
        head.append(el('label', 'field st-stations__field st-stations__grow', {}, el('span', 'field-label', { text: 'Tool' }), toolSel));

        const schema = tool?.inputSchema || {};
        const props = Object.entries(schema.properties || {});
        const required = new Set(schema.required || []);
        const values = kept.mcpValues[kept.tool] || (kept.mcpValues[kept.tool] = props.some(([n]) => n === 'limit') ? { limit: '10' } : {});
        const preview = el('pre', 'st-try__body st-try__sent');
        const paint = () => { preview.textContent = JSON.stringify(callBody(), null, 2); };
        const arg = ([name, p]) => el('label', 'field st-try__param', {},
            el('span', 'field-label', {}, name, required.has(name) ? el('span', 'st-try__req', { text: ' required' }) : null, el('span', 'st-try__in', { text: ' · ' + (p.type || 'string') })),
            p.type === 'boolean'
                ? el('input', '', { type: 'checkbox', checked: values[name] === true, onChange: e => { values[name] = e.target.checked; paint(); } })
                : el('input', 'input', { type: 'text', value: values[name] ?? '', placeholder: p.type || 'string', onInput: e => { values[name] = e.target.value; paint(); } }),
            p.description ? el('p', 'field-desc', { text: p.description }) : null);
        const callBody = () => ({ jsonrpc: '2.0', id: rpcId + 1, method: 'tools/call', params: { name: kept.tool, arguments: args(props, values) } });
        const result = el('div', 'st-try__result');
        const call = el('button', 'st-btn st-btn--primary', { type: 'button', html: SVG.send + 'Call', onClick: () => go() });

        body.replaceChildren(head,
            tool?.description ? el('p', 'field-desc st-try__summary', { text: tool.description }) : null,
            props.length ? el('div', 'st-try__params', {}, props.map(arg)) : el('p', 'field-desc', { text: 'This tool takes no arguments.' }),
            keyField(),
            el('details', 'st-try__more', {}, el('summary', '', { text: 'The JSON-RPC message' }), preview),
            el('div', 'st-try__go', {}, el('code', 'st-try__url', { text: `POST /mcp · tools/call ${kept.tool}` }), call),
            result,
            el('div', 'st-try__connect', {},
                el('span', 'field-label', { text: 'Connect an assistant' }),
                copyLine(claudeLine(server, !!kept.key)),
                el('p', 'field-desc', { text: 'Adds it to Claude Code; other assistants take the same URL.' })));
        paint();
        if (kept.mcpLast && kept.mcpLast.port === server.port && kept.mcpLast.tool === kept.tool) show(kept.mcpLast);

        async function go() {
            const missing = props.find(([n]) => required.has(n) && !String(values[n] ?? '').trim());
            if (missing) return result.replaceChildren(el('p', 'st-warn-note', { text: `Give ${missing[0]}.` }));
            call.disabled = true;
            try {
                const res = await rpc(server.port, 'tools/call', { name: kept.tool, arguments: args(props, values) });
                kept.mcpLast = { ...res, port: server.port, tool: kept.tool };
                show(kept.mcpLast);
            } catch (e) {
                result.replaceChildren(el('p', 'st-warn-note', { text: e.message }));
            } finally {
                call.disabled = false;
                paint();
            }
        }

        // the tool's answer first, as an assistant reads it; the whole reply folded below
        function show(res) {
            const reply = res.json;
            const failed = reply?.error ? `JSON-RPC error ${reply.error.code}: ${reply.error.message}` : reply?.result?.isError ? 'The tool reported an error' : '';
            const content = reply?.result?.content || [];
            const text = content.filter(c => c.type === 'text').map(c => c.text).join('\n');
            result.replaceChildren(
                meta(res.status, res.ms, res.text, 'application/json', failed),
                text ? bodyView(text, highlight) : bodyView(res.text, highlight),
                text ? el('details', 'st-try__more', {}, el('summary', '', { text: 'The whole reply' }), bodyView(res.text, highlight)) : null,
                copyLine(mcpCurl(server, res.sent, !!kept.key)));
        }
    }

    async function list() {
        const port = kept.port;
        try {
            const res = await rpc(port, 'tools/list');
            if (res.status !== 200 || !res.json?.result?.tools)
                kept.tools[port] = { error: res.status === 401 ? 'This server needs a key: give it above, then Load tools.' : `The server answered ${res.status}: ${res.json?.detail || res.json?.error?.message || res.text.slice(0, 120)}` };
            else
                kept.tools[port] = res.json.result.tools;
        } catch (e) {
            kept.tools[port] = { error: e.message };
        }
        draw();
    }
}

// the arguments as the tool's schema types them: numbers as numbers, nothing for what is left empty
function args(props, values) {
    const out = {};
    for (const [name, p] of props) {
        const v = values[name];
        if (p.type === 'boolean') { if (v === true) out[name] = true; continue; }
        const t = String(v ?? '').trim();
        if (!t) continue;
        out[name] = (p.type === 'integer' || p.type === 'number') && Number.isFinite(Number(t)) ? Number(t) : t;
    }
    return out;
}

function defaults(route) {
    const v = {};
    (route?.params || []).forEach(p => { if (p.name === 'limit') v.limit = '10'; });
    return v;
}

// the path with its path parameters filled in and its query from what is given
function target(route, values) {
    if (!route) return '';
    let path = route.path;
    const query = [];
    for (const p of route.params) {
        const v = String(values[p.name] ?? '').trim();
        if (p.in === 'path') path = path.replace(`{${p.name}}`, encodeURIComponent(v) || `{${p.name}}`);
        else if (v) query.push(`${encodeURIComponent(p.name)}=${encodeURIComponent(v)}`);
    }
    return path + (query.length ? '?' + query.join('&') : '');
}

// where a shell reaches a server: on the station when it listens there only
function origin(server) {
    const local = !server.bind || server.bind === 'localhost' || String(server.bind).startsWith('127.');
    return `http://${local ? '127.0.0.1' : location.hostname}:${server.port}`;
}

// the same request from a shell
function curlLine(server, t, keyed) {
    return `curl ${keyed ? '-H "Authorization: Bearer $KEY" ' : ''}"${origin(server)}${t}"`;
}

function mcpCurl(server, sent, keyed) {
    return `curl -X POST "${origin(server)}/mcp" -H 'Content-Type: application/json' ${keyed ? '-H "Authorization: Bearer $KEY" ' : ''}-d '${sent}'`;
}

function claudeLine(server, keyed) {
    return `claude mcp add --transport http aiscatcher ${origin(server)}/mcp${keyed ? ' --header "Authorization: Bearer $KEY"' : ''}`;
}

const size = n => (n < 1024 ? `${n} B` : n < 1048576 ? `${(n / 1024).toFixed(1)} KB` : `${(n / 1048576).toFixed(1)} MB`);
