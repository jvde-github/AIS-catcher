// Services: the servers of the configuration and what is built on what the station knows. Its
// pages: the overview; the main server (beside the control panel) and the other servers; the API
// to try; Home Assistant; webhooks.

import { el } from './dom.js';
import { ConfigStore, createSimpleConfigManager, createChannelManager } from './config-manager.js';
import { createServicesEditor, renderShows, servedKeys, readersOf, serverName, SERVICE_COLUMNS } from './services-editor.js';
import { renderApiTester } from './api-tester.js';
import { DOCS, docLink, outputDocs } from './docs.js';
import { webviewerSchema, webhookSchema, mqttStateSchema } from './schema.js';

// the map's settings on a server that serves it, named by what a visitor sees; the page's own
// come first, what only an owner looks for (links, tiles, plugins) folds away under More
const MAP_LABELS = {
    station: ['Title', 'Above the map; the only station\'s name when empty'],
    station_link: ['Title links to', 'A page about the station, opened from the title'],
    realtime: ['Realtime tab', 'Messages as they arrive'],
    replay: ['Replay', 'Scrub back through recent tracks on the map'],
    msg: ['Last message on the ship card', 'The vessel\'s latest NMEA'],
    decoder: ['Decoder tab', 'Decode a pasted NMEA sentence'],
    log: ['Log tab', 'What the station is doing'],
    webcontrol_http: ['Control panel link', 'Opened from the map\'s menu'],
    mbtiles: ['MBTiles maps', 'Base maps from .mbtiles files, one per line'],
    mboverlay: ['MBTiles overlays', 'Overlays from .mbtiles files, one per line'],
    fstiles: ['Tile folder maps', 'Base maps from folders of tiles, one per line'],
    fsoverlay: ['Tile folder overlays', 'Overlays from folders of tiles, one per line'],
    plugin_dir: ['Plugin folder', 'Plugins the map loads'],
    context: ['Settings name', 'Keeps a visitor\'s settings for this map apart from another map\'s']
};
const MAP_SECTIONS = { station: 'Map page', station_link: 'Map page', realtime: 'Map page', replay: 'Map page', msg: 'Map page', decoder: 'Map page', log: 'Map page',
    webcontrol_http: 'Links', mbtiles: 'Map tiles', mboverlay: 'Map tiles', fstiles: 'Map tiles', fsoverlay: 'Map tiles', plugin_dir: 'Plugins', context: 'Plugins' };
const MAP_FIELDS = Object.keys(MAP_SECTIONS).map(name => {
    const l = MAP_LABELS[name];
    return { field: { ...webviewerSchema[name], ...(l ? { label: l[0], tooltip: l[1] } : {}) }, section: MAP_SECTIONS[name] };
});

const HA_CONTAINER = 'svc-ha-container';
const HOOKS_CONTAINER = 'svc-webhooks-container';
// the pages the servers' host does not draw
const OWN_PAGES = ['overview', 'ha', 'webhooks'];
const withMain = list => (list.some(s => s && s.main === true) ? list : [{ main: true }, ...list]);
const listOf = v => (Array.isArray(v) ? v : []);

// what a server shows in a few words, for the Overview: its stations (station_id, else its view's), and
// whether its view limits more
function showsText(view, choices, stationId) {
    const v = view && typeof view === 'object' ? view : {};
    const raw = stationId ?? v.stations;
    const ids = (Array.isArray(raw) ? raw : raw === undefined || raw === null || raw === '' ? [] : String(raw).split(',')).map(x => String(x).trim()).filter(Boolean);
    const parts = [];
    if (ids.length) parts.push(ids.map(k => choices.find(c => String(c.key) === k)?.label || 'station ' + k).join(', '));
    if (Object.keys(v).some(k => k !== 'stations')) parts.push('limited');
    return parts.length ? parts.join(' · ') : 'Every ship';
}

// a page of its own: its title and line, its docs and an action on the right
function servicePage(host, title, desc, docs, action) {
    const main = el('div', 'st-main', {},
        el('div', 'st-main__head', {},
            el('div', '', {}, el('h2', 'st-main__title', { text: title }), el('p', 'st-main__desc', { text: desc })),
            el('div', 'st-main__actions', {}, docs ? docLink(docs) : null, action)));
    host.replaceChildren(main);
    return main;
}

// The Services items of the rail: from the section's pages while they are open (their edits
// count), else from the configuration. `port`: the main server's; `add`: + Add server.
export function servicesRailItems(cfg, section, { port, add }) {
    const live = section?.live() || {};
    const ha = live.ha || cfg.mqtt_state;
    const hooks = live.hooks || cfg.webhooks;
    return [{ key: 'overview', label: 'Overview' },
        ...withMain(live.servers || listOf(cfg.services)).map((sv, i) => ({ key: sv.main ? 'main' : 'srv' + i, label: serverName(sv) + (sv.main && port ? ' · ' + port : ''),
            dot: sv.main ? undefined : sv.active !== false })),
        { key: 'add', label: '+ Add server', onClick: add },
        { key: 'ha', label: 'Home Assistant', dot: !!(ha && ha.url) },
        { key: 'webhooks', label: 'Webhooks' + (Array.isArray(hooks) && hooks.length ? ' · ' + hooks.length : '') }];
}

// The section in `pane`. `select(key)` opens one of its pages through the rail, `follow(key)` tells
// the rail which page shows, `changed()` that what the rail lists changed; `stations()` the stations
// a server can show; `port()` the main server's port; `highlight` colours JSON.
export function createServicesSection(pane, { select, follow, changed, stations, port, highlight }) {
    const serversHost = el('div', 'st st-pages st-single', { id: 'svc-servers' });
    const overviewHost = el('div', 'st st-svcpage', { hidden: true });
    const haHost = el('div', 'st st-svcpage', { hidden: true });
    const hooksHost = el('div', 'st st-svcpage', { hidden: true });
    pane.replaceChildren(serversHost, overviewHost, haHost, hooksHost);

    let page = 'main', current = 'overview';
    let ha = null, hooks = null;
    const editor = createServicesEditor({
        onSaved: () => drawServers(),
        onInvalid: i => select('srv' + i),
        onEdit: changed,
        stations,
        viewerFields: MAP_FIELDS
    });
    const ready = editor.load().catch(() => {});
    let loaded = false;
    ready.then(() => {
        loaded = true;
        show(current);
        changed();
    });

    // ---- the servers' pages and the API's, one at a time, drawn by the servers' editor
    function serverPages() {
        return [
            ...editor.state.list.map((sv, i) => ({ key: sv.main ? 'main' : 'srv' + i, title: serverName(sv),
                actions: box => {
                    const port = testerPort(sv);
                    if (port) box.append(el('button', 'st-btn st-btn--sm', { type: 'button', text: 'Try the API', onClick: () => tryOn(port) }));
                    box.append(docLink(DOCS.services));
                    editor.actions(box, i, { deleted: () => { page = 'main'; drawServers(); }, rerender: drawServers });
                },
                render: (main, title) => editor.renderDetail(main, i, { title, rerender: drawServers }) })),
            { key: 'tester', title: 'Try the API',
                actions: box => box.append(docLink(DOCS.api, 'API docs'), docLink(DOCS.mcp, 'MCP docs')),
                render: main => renderApiTester(main, { servers: testerServers(), highlight, port: tryPort }) }
        ];
    }
    function drawServers() {
        const list = serverPages();
        if (!list.some(p => p.key === page)) page = list[0].key;
        const p = list.find(x => x.key === page);
        const title = el('h2', 'st-main__title', { text: p.title });
        const actions = el('div', 'st-main__actions');
        const main = el('div', 'st-main', {}, el('div', 'st-main__head', {}, el('div', '', {}, title), actions));
        p.actions(actions);
        p.render(main, title);
        serversHost.replaceChildren(main);
        if (!serversHost.hidden) follow(page);
        changed();
    }

    let tryPort = null;
    function tryOn(port) {
        tryPort = port;
        if (current === 'tester') drawServers();
        else select('tester');
    }
    // the port a server can be tried on: one that serves something the tester asks for
    const TRIED = ['api', 'mcp', 'geojson', 'kml', 'metrics'];
    function testerPort(sv) {
        const port = sv.main ? editor.state.viewerPort : Number(sv.port);
        const serve = servedKeys(sv, !!sv.main);
        return port && sv.active !== false && serve.some(k => TRIED.includes(k)) ? port : null;
    }

    // the servers as the API tester offers them, with what each serves
    function testerServers() {
        const st = editor.state;
        return st.list.map(sv => (sv.main
            ? { label: `Main · ${st.viewerPort}`, port: st.viewerPort, bind: st.bind, serve: servedKeys(sv, true) }
            : { label: serverName(sv), port: Number(sv.port), bind: sv.bind, serve: servedKeys(sv) })).filter(s => s.port);
    }

    // ---- the overview: every server against every service, who may read it and what it shows
    function drawOverview() {
        ConfigStore.fetch().then(cfg => {
            const main = servicePage(overviewHost, 'Services', 'Each server is one web address.', DOCS.services,
                el('button', 'st-btn st-btn--sm', { type: 'button', text: '+ Add server', onClick: add }));
            const bind = editor.state.bind;
            const choices = stations();
            const servers = loaded ? editor.state.list : withMain(listOf(cfg.services));
            const rows = servers.map((sv, i) => ({ ...(sv.main
                ? { key: 'main', name: 'Main', addr: `${bind}:${port() || '–'}`, served: servedKeys(sv, true), readers: readersOf(sv, bind), view: sv.view, ids: sv.station_id }
                : { key: 'srv' + i, name: serverName(sv), addr: `${sv.bind || '127.0.0.1'}:${sv.port ?? '–'}`,
                    served: sv.active === false ? [] : servedKeys(sv), readers: sv.active === false ? 'Switched off' : readersOf(sv, sv.bind || '127.0.0.1'), view: sv.view, ids: sv.station_id }),
                tryPort: loaded ? testerPort(sv) : null }));
            // what each shows says something once there are stations to choose from, or a view set in the file
            const showsCol = choices.length >= 2 || rows.some(r => (r.view && Object.keys(r.view).length) || r.ids !== undefined);
            const table = el('table', 'st-svcgrid', {},
                el('tr', '', {}, ['Server', ...SERVICE_COLUMNS.map(c => c.label), 'Who can read it', ...(showsCol ? ['Shows'] : []), ''].map(t => el('th', '', { text: t }))),
                rows.map(r => el('tr', '', {},
                    el('td', '', {}, el('button', 'st-svcgrid__name', { type: 'button', onClick: () => select(r.key) },
                        el('strong', '', { text: r.name }), el('small', '', { text: r.addr }))),
                    SERVICE_COLUMNS.map(c => {
                        const on = r.served.includes(c.key);
                        return el('td', 'st-svcgrid__ck' + (on ? ' is-on' : ''), { text: on ? '✓' : '·', title: c.label + (on ? ' at ' + c.path : ': off') });
                    }),
                    el('td', '', { text: r.readers }),
                    showsCol ? el('td', '', { text: showsText(r.view, choices, r.ids) }) : null,
                    el('td', 'st-svcgrid__try', {}, r.tryPort ? el('button', 'st-link', { type: 'button', text: 'Try ›', title: `Try the API on ${r.name}`, onClick: () => tryOn(r.tryPort) }) : null))));
            const haCfg = (ha?.loaded ? ha.data : cfg.mqtt_state) || {};
            const hookList = (hooks?.loaded ? hooks.data : cfg.webhooks) || [];
            const more = el('div', 'st-svcmore', {},
                [['ha', 'Home Assistant', haCfg.url ? 'Connected to ' + haCfg.url : 'Off'],
                    ['webhooks', 'Webhooks', hookList.length ? `${hookList.length} posting events` : 'None']].map(([key, label, sub]) =>
                    el('button', 'st-svcmore__row', { type: 'button', onClick: () => select(key) },
                        el('span', '', {}, el('strong', '', { text: label }), el('small', '', { text: sub })),
                        el('span', 'st-svcmore__go', { text: 'Open ›' }))));
            main.append(el('div', 'st-svcgrid__wrap', {}, table),
                el('h3', 'st-section__title st-svcmore__title', { text: 'Also built on what this station knows' }), more);
        }).catch(() => {});
    }

    // ---- Home Assistant and webhooks: what publishes the tracker's state
    function loadHomeAssistant() {
        const main = servicePage(haHost, 'Home Assistant', 'Vessels and stations as MQTT entities.', outputDocs('mqtt_state'));
        main.append(el('div', '', { id: HA_CONTAINER }));
        return createSimpleConfigManager({ schema: mqttStateSchema, channelType: 'mqtt_state', containerId: HA_CONTAINER, variant: 'plain', onChange: changed });
    }
    function loadWebhooks() {
        const main = servicePage(hooksHost, 'Webhooks', 'Events posted as they happen.', outputDocs('webhooks'),
            el('button', 'st-btn st-btn--sm', { type: 'button', text: '+ Add webhook', onClick: () => manager.addItem() }));
        main.append(el('div', '', { id: HOOKS_CONTAINER }));
        const manager = createChannelManager({ channelType: 'webhooks', schema: webhookSchema, containerId: HOOKS_CONTAINER, title: 'Webhook', variant: 'outputs',
            essentials: new Set(['description', 'url', 'events']),
            nameOf: item => item.url || 'New webhook',
            extra: (node, item) => renderShows(node, item, { stations: stations(), touched: () => manager.markDirty(), rerender: () => manager.render() }),
            empty: ['No webhooks yet', 'For Home Assistant, Node-RED or your own service.'],
            onChange: changed });
        return manager;
    }

    // ---- which page shows: the rail's key
    function show(key) {
        current = key || 'overview';
        serversHost.hidden = OWN_PAGES.includes(current);
        overviewHost.hidden = current !== 'overview';
        haHost.hidden = current !== 'ha';
        hooksHost.hidden = current !== 'webhooks';
        if (current === 'overview') drawOverview();
        else if (current === 'ha') ha ||= loadHomeAssistant();
        else if (current === 'webhooks') hooks ||= loadWebhooks();
        else if (loaded && (page !== current || !serversHost.firstChild)) { page = current; drawServers(); }
        else page = current;
    }

    // a new server, on its page
    function add() {
        ready.then(() => select('srv' + editor.add()));
    }

    return {
        show,
        add,
        // what the pages hold with their edits, where they have loaded
        live: () => ({ servers: loaded ? editor.state.list : null, ha: ha?.loaded ? ha.data : null, hooks: hooks?.loaded ? hooks.data : null }),
        destroy() {
            editor.destroy();
            ha?.destroy();
            hooks?.destroy();
        }
    };
}
