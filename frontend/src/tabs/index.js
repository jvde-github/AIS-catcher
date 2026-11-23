// The viewer's tabs, one table. showTab is the one way to a tab. A lazy tab's
// module is loaded once, when first wanted; it registers its own actions and
// exports shown(), hidden(), update(). Until it has loaded, loaded(id) gives nothing.

import { settings } from '../core/state.js';
import { config } from '../core/config.js';
import { map } from '../map.js';
import { updateMap } from '../fleet.js';
import { refresh, startPoll } from '../poll.js';
import { saveSettings, effects } from '../settings-store.js';
import * as fireworks from '../overlays/fireworks.js';
import { register } from '../actions.js';

// set once by script.js, so that this module imports no features
const viewer = { dismiss: () => {}, layout: () => {} };
export function wire(parts) {
    Object.assign(viewer, parts);
}

/* { id, label, title, icon: the menu entry; feature: the server's say, when it
   has one; load: the lazy module; shown, hidden, refresh: when the tab has them
   here rather than in its module (a module's own refresh is its update); action:
   an entry that is a button, not a tab } */
const TABS = [
    {
        id: 'map', label: 'Map', title: 'Live map of received vessels', icon: 'map',
        shown: () => {
            document.getElementById("tableside").style.display = "flex";
            map?.updateSize();
        },
        hidden: () => fireworks.stop(),
        refresh: () => updateMap(),
    },
    {
        id: 'stat', label: 'Statistics', title: 'System and receiver statistics', icon: 'business_center',
        load: () => import('./stat/stat.js'),
    },
    {
        id: 'plots', label: 'Plots', title: 'Graphs of message and vessel counts over time', icon: 'leaderboard',
        load: () => import('./plots/plots.js'),
    },
    {
        id: 'ships', label: 'Ships', title: 'Searchable table of received vessels', icon: 'table',
        load: () => import('./shiptable/shiptable.js'),
    },
    {
        id: 'realtime', label: 'NMEA', title: 'Live stream of received NMEA messages', icon: 'timer',
        feature: (c) => c.features.realtime, load: () => import('./realtime/realtime.js'),
    },
    {
        id: 'log', label: 'Log', title: 'Application log', icon: 'terminal',
        feature: (c) => c.features.log, load: () => import('./log/log.js'),
    },
    {
        id: 'decoder', label: 'Decoder', title: 'Decode raw NMEA sentences', icon: 'comment',
        feature: (c) => c.features.decoder, load: () => import('./decoder/decoder.js'),
    },
    {
        id: 'about', label: 'About', title: 'About this station', icon: 'info',
        feature: (c) => c.features.about_md, load: () => import('./about/about.js'),
    },
    {
        id: 'webcontrol', label: 'Control', title: 'Open Web Control', icon: 'play_circle',
        feature: (c) => c.webcontrol_http, action: 'openWebControl',
    },
];

const tabOf = (id) => TABS.find((t) => t.id === id && !t.action);
const enabled = (tab) => !tab.feature || !!tab.feature(config);

// ─── the two menus ───────────────────────────────────────────────────────────

const buttons = {};

function buildMenus() {
    const menubar = document.getElementById("menubar");
    const mini = document.getElementById("menubar_mini");
    for (const t of TABS) {
        const data = t.action ? { action: t.action } : { action: 'activateTab', tab: t.id };

        const item = document.createElement("div");
        item.id = t.id + "_tab";
        item.title = t.title;
        Object.assign(item.dataset, data);
        item.innerHTML = `<i class="${t.icon}_icon"></i><span>${t.label}</span>`;
        menubar.appendChild(item);

        const pill = document.createElement("i");
        pill.id = t.id + "_tab_mini";
        pill.title = t.title;
        pill.className = t.icon + "_icon";
        Object.assign(pill.dataset, data);
        pill.innerHTML = `<span class="mb-name">${t.label}</span>`;
        mini.appendChild(pill);

        buttons[t.id] = [item, pill];
    }
}

buildMenus();

export function applyTabFeatures() {
    for (const t of TABS) {
        if (!t.feature) continue;
        for (const el of buttons[t.id]) el.style.display = enabled(t) ? "" : "none";
    }
}

// ─── the lazy modules ────────────────────────────────────────────────────────

const modules = {};
const loading = {};

export function load(id) {
    // a failed load is tried again on the next open
    if (!loading[id]) loading[id] = tabOf(id).load().then((m) => (modules[id] = m), (e) => { delete loading[id]; throw e; });
    return loading[id];
}

// the tab's module if it has loaded
export function loaded(id) {
    return modules[id];
}

// ─── showing one ─────────────────────────────────────────────────────────────

let current = null;

function leave(tab) {
    tab.hidden?.();
    if (tab.load) loaded(tab.id)?.hidden?.();
}

export async function showTab(id = resolveTab()) {
    const tab = tabOf(id);
    if (!tab || !enabled(tab)) return;

    viewer.dismiss();
    if (tab === current) return;

    if (current) leave(current);
    current = tab;
    startPoll(null);

    for (const t of TABS)
        for (const el of buttons[t.id]) el.classList.toggle("active", t === tab);
    for (const el of document.getElementsByClassName("tabcontent")) el.style.display = "none";
    document.getElementById(id).style.display = "block";

    settings.tab = id;
    saveSettings();
    viewer.layout();
    tab.shown?.();

    let mod = tab;
    if (tab.load) {
        try {
            mod = await load(id);
        } catch (err) {
            console.error('Failed to load ' + id + ' tab module:', err);
            if (current === tab) current = null;
            return;
        }
        // left, or switched off, while its module loaded
        if (current !== tab || !enabled(tab)) return;
        mod.shown?.();
    }
    startPoll(tab.refresh || mod.update);
}

// the tab the settings name, or the one to fall back on when that one is off
export function resolveTab() {
    const tab = tabOf(settings.tab);
    if (!tab) settings.tab = "stat";
    else if (!enabled(tab)) settings.tab = "map";

    return settings.tab;
}

export async function openRealtimeForMMSI(m) {
    (await load('realtime')).openForMMSI(m);
}

// values on the page are written in these: what is on screen is written again
const rewrite = () => { refresh(); loaded('ships')?.reset(); };
effects({ coordinate_format: rewrite, table_shiptype_use_icon: rewrite, metric: rewrite });

register({
    activateTab: (e, d) => showTab(d.tab),
    decodeNMEA: async () => (await load('decoder')).decode(),
    clearDecoder: async () => (await load('decoder')).clear(),
});
