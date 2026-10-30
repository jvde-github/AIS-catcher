// The viewer's tabs, one table: each tab's menu entry, the server feature it
// needs, the module behind it (loaded once, when first wanted), what shows and
// hides it and what refreshes it. showTab is the one way to a tab. A lazy tab's
// module registers its own actions and exports what it does: shown(), hidden(),
// update(). Until it has loaded a feature asks loaded(id) and gets nothing.

import { settings } from '../core/state.js';
import { config } from '../core/config.js';
import { ships } from '../core/store.js';
import { map } from '../map.js';
import { updateMap } from '../fleet.js';
import { refresh, restart as restartRefresh } from '../data.js';
import { saveSettings, effects } from '../settings-store.js';
import { closeSettings } from '../features/settings/settings.js';
import { closeSearch } from '../features/search/search-panel.js';
import { hideMenu, applyPanels } from '../panels.js';
import { showTargetcard } from '../features/targetcard/card.js';
import { updateStatistics } from './stat/stat.js';
import * as fireworks from '../overlays/fireworks.js';
import { register } from '../actions.js';

const lazyUpdate = (id) => async () => (await load(id)).update();

/* { id, label, title, icon: the menu entry; feature: the server's say, when it
   has one; load: the lazy module; shown, hidden, refresh: when the tab has them
   here rather than in its module; action: an entry that is a button, not a tab } */
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
        refresh: () => updateStatistics(),
    },
    {
        id: 'plots', label: 'Plots', title: 'Graphs of message and vessel counts over time', icon: 'leaderboard',
        load: () => import('./plots/plots.js'), refresh: lazyUpdate('plots'),
    },
    {
        id: 'ships', label: 'Ships', title: 'Searchable table of received vessels', icon: 'table',
        load: () => import('./shiptable/shiptable.js'), refresh: lazyUpdate('ships'),
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

// the entries of what the server has switched off give way
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
    if (!loading[id]) loading[id] = tabOf(id).load().then((m) => (modules[id] = m));
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

// a lazy tab may have been left, or switched off, while its module loaded
function enterLazy(tab) {
    load(tab.id).then((m) => {
        if (settings.tab === tab.id && enabled(tab)) m.shown?.();
    }).catch((err) => console.error('Failed to load ' + tab.id + ' tab module:', err));
}

// the tab `id`; without one the tab the settings name, or the one to fall back on
export function showTab(id = resolveTab()) {
    const tab = tabOf(id);
    if (!tab || !enabled(tab)) return;

    closeSearch();
    hideMenu();
    closeSettings();

    for (const t of TABS)
        for (const el of buttons[t.id]) el.classList.toggle("active", t === tab);

    const tabcontent = document.getElementsByClassName("tabcontent");
    for (let i = 0; i < tabcontent.length; i++) tabcontent[i].style.display = "none";
    document.getElementById(id).style.display = "block";
    tab.shown?.();

    settings.tab = id;
    saveSettings();

    applyPanels();

    if (current && current !== tab) leave(current);
    current = tab;
    if (tab.load) enterLazy(tab);

    restartRefresh();
}

// the tab the settings name, or the one to fall back on when that one is off
export function resolveTab() {
    if (settings.tab == "settings") settings.tab = "stat";

    const tab = tabOf(settings.tab);
    if (!tab) settings.tab = "stat";
    else if (!enabled(tab)) settings.tab = "map";

    return settings.tab;
}

// the map, with vessel `m`'s card open when the receiver knows it
export function selectMapTab(m) {
    showTab('map');
    if (m in ships) showTargetcard('ship', m);
}

export async function openRealtimeForMMSI(m) {
    (await load('realtime')).openForMMSI(m);
}

// what the data loop calls for the tab on screen
export function refreshTab() {
    return tabOf(settings.tab)?.refresh?.();
}

// values on the page are written in these: what is on screen is written again
const rewrite = () => { refresh(); loaded('ships')?.reset(); };
effects({ coordinate_format: rewrite, table_shiptype_use_icon: rewrite, metric: rewrite });

register({
    activateTab: (e, d) => showTab(d.tab),
    decodeNMEA: async () => (await load('decoder')).decode(),
    clearDecoder: async () => (await load('decoder')).clear(),
});
