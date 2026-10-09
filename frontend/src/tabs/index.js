// The viewer's tabs: the module behind each lazy tab, loaded once when first
// wanted, and the one activateTab that shows a tab, starts what it runs and
// stops what the last one ran. A lazy tab registers its own actions when it
// loads; until then a feature asks loaded(id) and gets nothing.

import { settings } from '../core/state.js';
import { config } from '../core/config.js';
import { ships } from '../core/store.js';
import { map } from '../map.js';
import { onRefresh, restart as restartRefresh } from '../data.js';
import { saveSettings, closeSettings } from '../features/settings/settings.js';
import { showTargetcard } from '../features/targetcard/card.js';
import * as fireworks from '../overlays/fireworks.js';
import { register } from '../actions.js';

const LOADERS = {
    plots: () => import('./plots/plots.js'),
    ships: () => import('./shiptable/shiptable.js'),
    realtime: () => import('./realtime/realtime.js'),
    decoder: () => import('./decoder/decoder.js'),
    log: () => import('./log/log.js'),
    about: () => import('../features/about/markdown.js'),
};

const modules = {};
const loading = {};

export function load(id) {
    if (!loading[id]) loading[id] = LOADERS[id]().then((m) => (modules[id] = m));
    return loading[id];
}

// the tab's module if it has loaded
export function loaded(id) {
    return modules[id];
}

// { leaving(): before a tab is shown, shown(): after }
let hooks = { leaving: () => {}, shown: () => {} };

export function init(h) {
    hooks = h;
}

let logViewer = null;

export function activateTab(a) {
    // Block decoder tab if decoder is disabled
    if (a === "decoder" && !config.features.decoder) {
        return;
    }

    hooks.leaving();
    closeSettings();

    Array.from(document.getElementById("menubar").children).forEach((e) => (e.className = e.className.replace(" active", "")));
    Array.from(document.getElementById("menubar_mini").children).forEach((e) => (e.className = e.className.replace(" active", "")));

    const tabcontent = document.getElementsByClassName("tabcontent");

    for (var i = 0; i < tabcontent.length; i++) tabcontent[i].style.display = "none";

    document.getElementById(a).style.display = "block";
    if (a === "map") {
        document.getElementById("tableside").style.display = "flex";
        map?.updateSize();
    }

    const tabElement = document.getElementById(a + "_tab");
    if (tabElement) tabElement.className += " active";

    const tabMiniElement = document.getElementById(a + "_tab_mini");
    if (tabMiniElement) tabMiniElement.className += " active";

    settings.tab = a;
    saveSettings();

    hooks.shown();

    restartRefresh();

    if (a != "map") fireworks.stop();

    if (a == "log") {
        load('log').then(({ LogViewer }) => {
            if (settings.tab !== 'log' || !config.features.log || logViewer) return;
            logViewer = new LogViewer();
            logViewer.connect();
        }).catch((err) => console.error('Failed to load log tab module:', err));
    }
    if (a != 'log' && logViewer) {
        logViewer.disconnect();
        logViewer = null;
    }

    if (a == "realtime" && config.features.realtime) {
        load('realtime').then((m) => {
            if (settings.tab !== 'realtime' || !config.features.realtime) return;
            m.activate();
        }).catch((err) => console.error('Failed to load realtime tab module:', err));
    } else if (a != 'realtime') {
        loaded('realtime')?.deactivate();
    }
    if (a === "about") {
        load('about').then(({ setup }) => setup())
            .catch((err) => console.error('Failed to load about tab module:', err));
    }
    if (a === "decoder" && !loaded('decoder')) {
        // Preload so Decode/Clear button clicks resolve from cache.
        load('decoder').catch((err) => console.error('Failed to load decoder tab module:', err));
    }
}

// the tab the settings name, or the one to fall back on when that one is off
export function resolveTab() {
    if (settings.tab == "settings") settings.tab = "stat";

    // Check if requested tab is disabled and redirect to map
    if (settings.tab === "about" && !config.features.about_md) settings.tab = "map";
    if (settings.tab == "realtime" && !config.features.realtime) {
        settings.tab = "map";
    }
    if (settings.tab == "log" && !config.features.log) {
        settings.tab = "map";
    }
    if (settings.tab == "decoder" && !config.features.decoder) {
        settings.tab = "map";
    }

    if (settings.tab != "realtime" && settings.tab != "about" && settings.tab != "map" && settings.tab != "plots" && settings.tab != "ships" && settings.tab != "stat" && settings.tab != "log" && settings.tab != "decoder") {
        settings.tab = "stat";
        alert("Invalid tab specified");
    }
    return settings.tab;
}

export function selectTab() {
    activateTab(resolveTab());
}

// the map, with vessel `m`'s card open when the receiver knows it
export function selectMapTab(m) {
    document.getElementById("map_tab").click();
    if (m in ships) showTargetcard('ship', m);
}

export async function openRealtimeForMMSI(m) {
    (await load('realtime')).openForMMSI(m);
}

onRefresh("plots", async () => {
    const plots = loaded('plots') || await load('plots').then((m) => { m.setAbsoluteTime(settings.plot_absolute_time); return m; });
    await plots.update();
});

onRefresh("ships", async () => {
    await (await load('ships')).update();
});

register({
    activateTab: (e, d) => activateTab(d.tab),
    decodeNMEA: async () => (await load('decoder')).decode(),
    clearDecoder: async () => (await load('decoder')).clear(),
});
