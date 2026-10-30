import { initServerConfig, applyServerFeatures, updateServerConfig } from './features/server-config/viewer-config.js';
import { config } from './core/config.js';
import { settings, isAndroid, isKiosk } from './core/state.js';
import { register } from './actions.js';
import * as filter from './core/filter.js';
import * as components from '@aiscatcher/ui/components.js';
import * as tooltipLib from '@aiscatcher/ui/tooltip.js';
import { debounce } from '@aiscatcher/ui/components.js';
import { cardFullBleed } from './ui.js';
import * as community from './overlays/community.js';
import * as measure from './features/measure/measure.js';
import * as boxselect from './features/boxselect/boxselect.js';
import * as replay from './features/replay/replay.js';
import * as mapObjects from './features/mapobjects/mapobjects.js';
import * as range from './features/range/range.js';
import * as ticker from './features/ticker/ticker.js';
import {
    map, create as createMap, markerLayer, shapeLayer, extraLayer, trackLayer, labelLayer, addOverlayLayer,
} from './map.js';
import { restoreFocus } from './data.js';
import { buildSettingsTabs } from './features/settings/settings.js';
import {
    saveMapView, loadSettings, loadSettingsFromURL, restoreDefaultSettings, runAllEffects,
} from './settings-store.js';
import { showDialog, showNotification } from './dialog.js';
import * as about from './features/about/about.js';
import { buildStatcard } from './features/counts/counts.js';
import { relayoutTablecard } from './features/tablecard/tablecard.js';
import { vesselLabel, openFocus } from './features/follow/follow.js';
import { buildCards, dragTargetcardBy, fitTargetcard } from './features/targetcard/card.js';
import { loaded as loadedTab, showTab, resolveTab } from './tabs/index.js';
import { applyPanels, showMenu } from './panels.js';
import { init as initContextMenu, closeContextMenu, showContextMenu } from './features/contextmenu/contextmenu.js';
import { redraw } from './fleet.js';
import { stopHover, handlePointerMove, handleClick, getFeature } from './features/hover/hover.js';
import { syncFocus } from './features/station/station.js';
import { installPluginApi, loadPlugins } from './plugin-api.js';
// their settings' effects and their actions register on load
import './features/kiosk/kiosk.js';
import './features/theme/theme.js';
import './tabs/plots/graphs.js';
import './app.css';

initContextMenu();

installPluginApi();

// the chrome's actions; the features register their own (actions.js)
register({
    // header bar
    headerClick: () => headerClick(),
    openWebControl: () => openWebControl(),
    toggleScreenSize: () => toggleScreenSize(),

    // the settings panel's system page
    showPlugins: () => showPlugins(),
    showServerErrors: () => showServerErrors(),
});

const objectLayer = mapObjects.objectLayer;
const rangeLayer = range.rangeLayer;

function headerClick() {
    window.open("https://www.aiscatcher.org", '_blank', 'noopener');
}

function openWebControl() {
    if (config.webcontrol_http) {
        window.open(config.webcontrol_http, '_blank', 'noopener');
    }
}

function initMap() {
    tooltipLib.create({ selector: ".map-pill .map-button" }).prepare();

    createMap([trackLayer, rangeLayer, shapeLayer, objectLayer, markerLayer, labelLayer, extraLayer, measure.measureVector,
        replay.hullLayer, replay.markerLayer]);

    map.on('movestart', function () {
        stopHover();
    });

    map.on('moveend', function (evt) {
        mapObjects.viewChanged(map.getView().getZoom());
        saveMapView();
        debouncedDrawMap();
    });

    map.on('pointermove', function (evt) {
        if (evt.dragging) { map.getTargetElement().style.cursor = ''; return; }
        const pixel = map.getEventPixel(evt.originalEvent);
        handlePointerMove(pixel, evt.originalEvent.target);
    });

    map.on('click', function (evt) {
        handleClick(evt.pixel, evt.originalEvent.target, evt);
    });

    map.getTargetElement().addEventListener('pointerleave', function () {
        map.getTargetElement().style.cursor = '';
        stopHover();
    });

    map.getTargetElement().addEventListener('contextmenu', function (evt) {

        if (evt.target.closest('#replaybar')) return;

        const f = getFeature(map.getEventPixel(evt), map.getTargetElement())

        if (!f)
                showContextMenu(evt, 0, null, replay.isActive() ? ['ctx-replay-map'] : ['settings', 'ctx-map']);
        else if ('station' in f) {
            showContextMenu(evt, null, null, ["station", "ctx-map"]);
        }
        else if ('replayMmsi' in f)
            showContextMenu(evt, f.replayMmsi, 'ship', ["ctx-replay-ship"]);
        else if ('ship' in f)
            showContextMenu(evt, f.ship.mmsi, 'ship', ["ship", "ship-map"]);
        else if ('plane' in f)
            showContextMenu(evt, f.plane.hexident, 'plane', ["plane", "plane-map"]);
    });
}

function showPlugins() {
    const list = config.plugins.loaded.length
        ? config.plugins.loaded.map((p) => p.version > 0 ? `${p.name} (v${p.version})` : p.name).join('\n')
        : '(none)';
    showDialog("Plugins", "<pre>Loaded plugins:\n" + list + "</pre>");
}

function showServerErrors() {
    const errs = config.plugins.errors;
    // Only v3 is flagged — its inline-string handlers don't run under strict
    // CSP. v4+ plugins still work; declaring v4 doesn't mean "outdated."
    const outdated = config.plugins.loaded
        .filter((p) => p.version > 0 && p.version < 4)
        .map((p) => p.name);

    const sections = [];
    if (outdated.length > 0) {
        sections.push(
            "Warning: deprecated v3 plugins (inline-string handlers don't run under strict CSP). Update from AIS-Catcher-PLUGINS:\n  " +
            outdated.join('\n  ')
        );
    }
    if (errs.length > 0) sections.push(errs.join('\n'));

    showDialog("Server Errors", sections.length === 0 ? "None" : ("<pre>" + sections.join('\n\n') + "</pre>"));
}

function toggleScreenSize() {
    const doc = window.document;
    const docEl = doc.documentElement;

    const requestFullScreen = docEl.requestFullscreen || docEl.mozRequestFullScreen || docEl.webkitRequestFullScreen || docEl.msRequestFullscreen || docEl.webkitEnterFullscreen;
    const cancelFullScreen = doc.exitFullscreen || doc.mozCancelFullScreen || doc.webkitExitFullscreen || doc.msExitFullscreen || doc.webkitExitFullscreen;

    if (!doc.fullscreenElement && !doc.mozFullScreenElement && !doc.webkitFullscreenElement && !doc.msFullscreenElement) {
        requestFullScreen.call(docEl);
    } else {
        cancelFullScreen.call(doc);
    }
}

function initFullScreen() {
    document.addEventListener("fullscreenchange", handleFullScreenChange);
    document.addEventListener("mozfullscreenchange", handleFullScreenChange);
    document.addEventListener("webkitfullscreenchange", handleFullScreenChange);
    document.addEventListener("msfullscreenchange", handleFullScreenChange);
}

function handleFullScreenChange() {
    closeContextMenu();
}

const debouncedDrawMap = debounce(redraw, 250);

document.getElementById('zoom-in').addEventListener('click', function () {
    let view = map.getView();
    let zoom = view.getZoom();
    view.setZoom(zoom + 1);
});

document.getElementById('zoom-out').addEventListener('click', function () {
    let view = map.getView();
    let zoom = view.getZoom();
    view.setZoom(zoom - 1);
});

function announceStickyState() {
    const notes = [];
    if (filter.isActive()) notes.push("vessel filter on (" + filter.describe().join(", ") + ")");
    if (settings.fix_center) notes.push("following " + vesselLabel(settings.center_point));
    if (settings.labels_active_only) notes.push("labels only for the selected vessel");
    if (isKiosk()) notes.push("kiosk mode on");
    if (!notes.length) return;

    showNotification(notes.map((n) => n[0].toUpperCase() + n.slice(1)).join(" · "), "warning", 10000);
}

const urlParams = new URLSearchParams(window.location.search);
buildStatcard();
restoreDefaultSettings();

community.init();
measure.init();
boxselect.init();
mapObjects.init();

if (!cardFullBleed()) {
    document.querySelectorAll('aside').forEach((aside) => {
        const dragHandle = aside.querySelector('.draggable');
        if (!dragHandle) return;
        if (aside.id === "targetcard") {
            dragTargetcardBy(dragHandle);
        } else components.draggable(dragHandle, aside);
    });
}
else {
    // hide all spans with class "draggable-hide-if-not-active"
    document.querySelectorAll('.draggable-hide-if-not-active').forEach((span) => {
        span.style.display = 'none';
    });
}

ticker.init();

replay.init();

initServerConfig();

loadPlugins();

console.log("Load settings");
loadSettings();
restoreFocus();
applyPanels();

console.log("Load settings from URL parameters");

loadSettingsFromURL();
buildCards();
syncFocus(false);
community.applySharingState();

if (config.features.managed) {
    const pollSharingState = async () => {
        if (document.hidden) return;
        try {
            const r = await fetch("api/sharing_state.json");
            if (!r.ok) return;
            const s = await r.json();
            await updateServerConfig(s.config_version);
            community.updateSharingState(s.sharing, s.sharing_uuid, s.engine_running);
        } catch (e) { /* transient */ }
    };
    pollSharingState();
    setInterval(pollSharingState, 10000);
}

console.log("Setup tabs");
initFullScreen();

// a map in a display:none container has a 0x0 viewport and asks for no tiles,
// so reveal the target first and let the tile fetch overlap the rest of startup
document.getElementById(resolveTab()).style.display = "block";
initMap();

import('./overlays/ducting.js')
    .then((m) => m.init(addOverlayLayer))
    .catch((err) => console.error('Failed to load ducting module:', err));

applyServerFeatures();

console.log("Switch to active tab");
showTab();

buildSettingsTabs();

runAllEffects();

if (urlParams.get("mmsi")) openFocus(urlParams.get("mmsi"), urlParams.get("zoom"));

about.showWelcome();
announceStickyState();

if (isAndroid()) showMenu();

// Re-apply chart colors after all stylesheets load (Firefox iframe quirk).
window.addEventListener('load', () => {
    requestAnimationFrame(() => {
        requestAnimationFrame(() => {
            loadedTab('plots')?.updateColors();
        });
    });
});

// A resize can cross the breakpoints mapTopInset and the ticker's width rules
// read, so the whole layout has to converge - not just the card. Debounced
// because a window drag fires this continuously and each pass measures.
window.addEventListener('resize', debounce(() => {
    applyPanels({ reposition: false });
    fitTargetcard();
    relayoutTablecard();
}, 150));
