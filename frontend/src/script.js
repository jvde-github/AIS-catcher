import { createViewerConfig, applyServerFeatures } from "./features/server-config/server-config.js";
import { config } from './core/config.js';
import { createServerMaps } from "./features/server-maps/server-maps.js";
import { settings, isAndroid, isKiosk } from './core/state.js';
import { register } from './actions.js';
import * as filter from './core/filter.js';
import * as components from '@aiscatcher/ui/components.js';
import * as mapui from '@aiscatcher/map/mapui.js';
import * as tooltipLib from '@aiscatcher/ui/tooltip.js';
import { createSearch } from '@aiscatcher/ui/search.js';
import { createLocalSearch } from './features/search/search.js';
import { debounce, decodeHTMLEntities } from '@aiscatcher/ui/components.js';
import { ships as shipsDB, cardMmsi as card_mmsi, cardType as card_type } from './core/store.js';
import { hasValidCoords } from '@aiscatcher/core/geo.js';
import * as community from './overlays/community.js';
import * as kiosk from './features/kiosk/kiosk.js';
import * as measure from './features/measure/measure.js';
import * as boxselect from './features/boxselect/boxselect.js';
import * as replay from './features/replay/replay.js';
import * as mapObjects from './features/mapobjects/mapobjects.js';
import * as range from './features/range/range.js';
import * as ticker from './features/ticker/ticker.js';
import * as planecard from './features/planecard/planecard.js';
import * as targetcard from './features/targetcard/targetcard.js';
import { getShipName, getCallSign } from './core/names.js';
import {
    ol, map, create as createMap, renderer as mapRenderer, markerLayer, shapeLayer, extraLayer, trackLayer,
    labelLayer, basemaps, overlapmaps, addOverlayLayer, baseMapsChanged,
} from './map.js';
import { init as initData, receiver, refresh } from './data.js';
import {
    settingsPanel, effects as settingsEffects, onSave as onSettingsSave, onOpen as onSettingsOpen,
    onDefaults as onSettingsDefaults, saveSettings, saveMapView, updateMapURL, loadSettings, loadSettingsFromURL,
    restoreDefaultSettings, buildSettingsTabs, closeSettings, updateSettingsTab,
} from './features/settings/settings.js';
import { showDialog, isDialogOpen, showNotification } from './dialog.js';
import * as stat from './tabs/stat/stat.js';
import * as about from './features/about/about.js';
import { bucketChips, buildStatcard } from './features/counts/counts.js';
import { updateFilterUI } from './features/filter/filter.js';
import { updateSortMarkers, relayoutTablecard } from './features/tablecard/tablecard.js';
import { vesselLabel, drawStation } from './features/follow/follow.js';
import {
    init as initCard, closeTargetcard, targetcardVisible, restoreCardLayout, saveCardLayout, fitTargetcard,
    applyTargetcardPinStyling, setTargetcardStyle, updateFocusMarker,
} from './features/targetcard/card.js';
import {
    init as initTabs, load as loadTab, loaded as loadedTab, selectTab, resolveTab, selectMapTab,
} from './tabs/index.js';
import { init as initPanels, panels, setPanels, applyPanels, applyMenuLabels, applyToolbarMode } from './panels.js';
import { setGraphVisibility, setPlotAbsoluteTime } from './tabs/plots/graphs.js';
import { init as initContextMenu, MENU_CHECKS, showContextMenu } from './features/contextmenu/contextmenu.js';
import { redrawMap } from './fleet.js';
import { init as initHover, stopHover, handlePointerMove, handleClick, getFeature } from './features/hover/hover.js';
import * as replaybar from './features/replay/replaybar.js';
import { init as initReceiver } from './features/receiver/receiver.js';
import { updateDarkMode, setDarkMode } from './features/theme/theme.js';
import { init as initFollow, openFocus } from './features/follow/follow.js';
import { installPluginApi, loadPlugins } from './plugin-api.js';
import './app.css';

let searchPanel = null;

const ui = mapui.create({
    map: () => map,
    settings: () => settings,
    chrome: {
        fallback: (edge) => {
            const cls = document.body.classList;
            let inset = mapGap();
            if (edge === "top" && cls.contains("ticker-open") && !cls.contains("ticker-bottom")) inset += cssPx("--size-ticker", 44);
            if (edge === "bottom" && cls.contains("ticker-open") && cls.contains("ticker-bottom")) inset += cssPx("--size-ticker", 44);
            return inset;
        },
    },
    card: {
        mount: document.getElementById("targetcard"),
        sectionAction: "toggleTargetcardSection",
        keepRows: '[data-action="targetcardSelectSelf"]',
        itemAvailable: (el) => el.id !== 'targetcard_realtime_option' || config.features.realtime,
        layout: {
            fullBleed: () => cardFullBleed(),
            topInset: () => mapTopInset(),
            dockOffset: () => tickerInset(),
            freeWidth: () => freeMapWidth(),
            preferDock: () => !!settings.targetcard_top_left,
            pinned: () => settings.targetcard_pinned ? { x: settings.targetcard_pinned_x, y: settings.targetcard_pinned_y } : null,
        },
    },
    /* top-down: Escape closes the first open one; a card taking the stage
       closes the ones marked stage. The dialog closes itself on Escape. */
    stack: [
        { isOpen: () => isDialogOpen() },
        { isOpen: () => searchPanel?.isOpen(), close: () => searchPanel.close(true), stage: true },
        { isOpen: () => settingsPanel.isOpen(), close: () => settingsPanel.close(), stage: true },
        { isOpen: () => document.getElementById("menubar").classList.contains("visible"), close: () => hideMenu(), stage: true },
        { isOpen: () => targetcardVisible(), close: () => closeTargetcard() },
    ],
    toolbar: {
        bar: document.getElementById("maptoolbar"),
        rail: document.querySelector(".map-button-box.map-pill:not(.map-bottom)"),
        wantWide: () => settings.map_toolbar === "wide" && !isKiosk() && !panels.replay,
        freeWidth: () => freeMapWidth(),
        reserve: () => cssPx("--size-map-controls", 44) + mapGap() * 2,
    },
    menu: {
        mount: document.getElementById("context-menu"),
        checks: MENU_CHECKS,
        groupLabels: { focus: "Map", tracks: "Tracks", vessel: "Details", lookup: "Open on", display: "Display",
                       tools: "Tools", card: "Card", replay: "Replay", charts: "Charts", app: "App" },
        before: document.querySelector('#context-menu li[data-action="toggleReplaycard"]')?.nextSibling,
    },
});

initCard({
    ui,
    stopHover: () => stopHover(),
});
initPanels({ ui });
mapRenderer(redrawMap);
initHover({ ui });
initFollow({ ui });
initReceiver({ closeSearch: () => searchPanel.close() });

initContextMenu({ menu: ui.menu });

installPluginApi();

// the chrome's actions; the features register their own (actions.js)
register({
    // header bar
    toggleMenu: () => toggleMenu(),
    hideMenu: () => hideMenu(),
    headerClick: () => headerClick(),
    openWebControl: () => openWebControl(),
    toggleScreenSize: () => toggleScreenSize(),
    toggleSearch: () => searchPanel.toggle(),

    // the settings panel's system page
    showPlugins: () => showPlugins(),
    showServerErrors: () => showServerErrors(),
});

let tab_title_station = decodeHTMLEntities(config.station),
    tab_title_count = null,
    tab_title = "AIS-catcher";

function updateTitle() {
    document.title = (tab_title_count ? " (" + tab_title_count + ") " : "") + tab_title + " " + tab_title_station;
}

const objectLayer = mapObjects.objectLayer;
const rangeLayer = range.rangeLayer;

function headerClick() {
    window.open("https://www.aiscatcher.org");
}

function openWebControl() {
    if (config.webcontrol_http) {
        window.open(config.webcontrol_http, '_blank');
    }
}

const dynamicStyle = document.createElement("style");
document.head.appendChild(dynamicStyle);

function applyDynamicStyling() {
    let style = ``

    if (!isAndroid())
        style += `
            @media only screen and (min-width: 750px) {
                #menubar {
                    position: fixed;
                    top: 70px;
                    left: 10px;
                    right: 0;
                    width: 500px;
                    border: solid;
                    border-color: var(--color-menu-border);
                    border-radius: 5px;
                    box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1), 0 4px 6px -2px rgba(0, 0, 0, 0.05);
                }
            }
    `;

    dynamicStyle.innerHTML = style;
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

searchPanel = createSearch({
    root: document.getElementById('search-panel'),
    scrim: document.getElementById('search-scrim'),
    triggers: [document.getElementById('header-search-button')],
    storageKey: 'viewerSearchType',
    types: [
        { id: 'all', label: 'Everything', placeholder: 'Search vessels, stations and places…' },
        { id: 'ships', label: 'Vessels', placeholder: 'Name, MMSI, callsign or IMO…' },
        { id: 'stations', label: 'Stations', placeholder: 'Station name or identifier…' },
        { id: 'ports', label: 'Places', placeholder: 'Place name or UN/LOCODE…' },
    ],
    search: createLocalSearch({receiver: () => receiver, ships: () => shipsDB, shipName: getShipName, callSign: getCallSign}),
    onOpen: () => {
        if (settings.tab !== 'map') selectMapTab();
        ui.dismiss();
        closeTargetcard();
        setPanels({table: false, measure: false});
    },
    onSelect: async result => {
        if (result.type === 'ship') {
            await openFocus(result.id);
        } else {
            if (hasValidCoords(result.lat, result.lon)) {
                map.getView().animate({center: ol.proj.fromLonLat([result.lon, result.lat]), zoom: 12, duration: 500});
            }
            if (result.type === 'port' || result.type === 'place') mapObjects.openPorts([result.object]);
        }
    },
    onError: () => showNotification('Could not open the search result', 'error'),
});

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

function hideMenu() {
    if (document.getElementById("menubar").classList.contains("visible") && !isAndroid()) {
        toggleMenu();
    }
}

function showMenu() {
    if (!document.getElementById("menubar").classList.contains("visible")) {
        toggleMenu();
    }
}

function toggleMenu() {
    const menubar = document.getElementById("menubar");
    menubar.classList.toggle("visible");
    document.getElementById("menubar_mini").classList.toggle("showflex");
    document.getElementById("menubar_mini").classList.toggle("hidden");

    const menuButton = document.getElementById("header_menu_button");
    menuButton.classList.toggle("menu_icon");
    menuButton.classList.toggle("close_icon");

    syncMenuScrim();
}

function syncMenuScrim() {
    const menubar = document.getElementById("menubar");
    const floats = ["absolute", "fixed"].includes(getComputedStyle(menubar).position);
    document.getElementById("menubar-overlay").classList
        .toggle("active", menubar.classList.contains("visible") && floats);
}

window.matchMedia("(min-width: 750px)").addEventListener("change", syncMenuScrim);

function initFullScreen() {
    document.addEventListener("fullscreenchange", handleFullScreenChange);
    document.addEventListener("mozfullscreenchange", handleFullScreenChange);
    document.addEventListener("webkitfullscreenchange", handleFullScreenChange);
    document.addEventListener("msfullscreenchange", handleFullScreenChange);
}

function handleFullScreenChange() {
    ui.menu.close?.();
}

const debouncedDrawMap = debounce(redrawMap, 250);

// The ticker owns the top strip. A card that starts above it hides the very
// thing that announced the vessel, so cards begin below it while it is out.
window.addEventListener("resize", () => ui.chrome.invalidate());

const cssPx = ui.chrome.px;

const mqShort = window.matchMedia("(max-height: 800px)");

/* Room is a question about the map pane, which a side panel takes its slice
   out of — the same width the stylesheet asks about with @container mappane. */
function paneWidth() {
    const el = document.getElementById("map");
    return el && el.clientWidth ? el.clientWidth : window.innerWidth;
}

function paneNarrow() {
    return paneWidth() <= 750;
}

function cardFullBleed() {
    return paneWidth() <= 500 || mqShort.matches;
}

function tickerAtTop() {
    return document.body.classList.contains("ticker-open") && !settings.ticker_bottom;
}

function mapInset(edge) {
    return ui.chrome.inset(edge);
}

function tickerInset() {
    if (cardFullBleed() || paneNarrow() || !tickerAtTop()) return 0;
    return mapInset("top");
}

function mapTopInset() {
    if (cardFullBleed()) return 0;

    // On a narrow screen the card takes the whole top anyway; giving the bar
    // its strip there would cost the card room it has none of to spare.
    if (paneNarrow()) return mapGap();

    return mapInset("top");
}

function mapGap() {
    return ui.chrome.gap();
}

/* The map pane already stops where a side panel begins, so its own width is
   the width on offer. */
function freeMapWidth() {
    const el = document.getElementById("map");
    if (el && el.clientWidth > 0) return el.clientWidth;

    const mapSize = map ? map.getSize() : null;
    return mapSize && mapSize[0] > 0 ? mapSize[0] : window.innerWidth;
}

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

const androidStyle = document.createElement("style");
document.head.appendChild(androidStyle);

function updateAndroid() {
    const sel = isAndroid() ? ".noandroid" : ".android";
    androidStyle.textContent = sel + " { display: none !important; }";
}

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

settingsEffects({
    dark_mode: () => updateDarkMode(),
    map_toolbar: () => applyToolbarMode(),
    menu_labels: () => applyMenuLabels(),
    coordinate_format: () => { refresh(); loadedTab('ships')?.reset(); },
    table_shiptype_use_icon: () => { refresh(); loadedTab('ships')?.reset(); },
    metric: () => { refresh(); loadedTab('ships')?.reset(); },
    plot_absolute_time: (v) => loadedTab('plots')?.setAbsoluteTime(v),
    shipcard_style: (v) => setTargetcardStyle(v),
    show_signal_graphs: (v) => setGraphVisibility('signal', v, false),
    show_ppm_graphs: (v) => setGraphVisibility('ppm', v, false),
    kiosk: () => { kiosk.updateKiosk(); applyPanels(); },
});
onSettingsOpen(() => {
    searchPanel?.close();
    updateFilterUI();
});
onSettingsDefaults(() => {
    updateSortMarkers();
    setDarkMode(settings.dark_mode);
    updateFocusMarker();
    applyPanels();
});
onSettingsSave(saveCardLayout);

community.init();
measure.init();
range.init();
boxselect.init();
mapObjects.init({
    navigate: ({ lat, lon }) => ui.reveal([lon, lat], undefined, { minZoom: 8, center: true }),
    setTableOpen: (on) => {
        if (on) closeSettings();
        setPanels({ table: on, ...(on ? { replay: false } : {}) });
    },
});
targetcard.init({
    goTo: (lat, lon) => ui.reveal([lon, lat], undefined, { minZoom: 12, center: true }),
    card: ui.card,
});

if (!cardFullBleed()) {
    document.querySelectorAll('aside').forEach((aside) => {
        const dragHandle = aside.querySelector('.draggable');
        if (!dragHandle) return;
        if (aside.id === "targetcard") {
            ui.card.draggable(dragHandle);
            aside.addEventListener("card:moved", (e) => {
                if (!settings.targetcard_pinned) return;
                settings.targetcard_pinned_x = e.detail.x;
                settings.targetcard_pinned_y = e.detail.y;
                saveSettings();
            });
        } else components.draggable(dragHandle, aside);
    });
}
else {
    // hide all spans with class "draggable-hide-if-not-active"
    document.querySelectorAll('.draggable-hide-if-not-active').forEach((span) => {
        span.style.display = 'none';
    });
}

planecard.init();

ticker.init({
    buckets: bucketChips(),
    bucketHidden: (b) => filter.isActive() && filter.isHidden("bucket", b),
    navigate: ({ lat, lon }, minZoom) => {
        ui.reveal([lon, lat], undefined, { minZoom, center: true });
    },
});

replaybar.init();

const applyServerMaps = createServerMaps({
    ol, basemaps: () => basemaps, overlays: () => overlapmaps, getMap: () => map,
    refresh: baseMapsChanged,
});

const serverConfig = createViewerConfig({
    current: config,
    applyMaps: applyServerMaps,
    applyRuntime: async (next, {restartChanged, aboutChanged}) => {
        if (!next.features.realtime) loadedTab('realtime')?.deactivate(true);
        tab_title_station = decodeHTMLEntities(next.station);
        updateTitle();
        community.updateSharingState(next.features.sharing, next.features.sharing_uuid);
        const oldTab = settings.tab;
        if (resolveTab() !== oldTab) selectTab();
        if (!next.features.replay && replay.isActive()) replay.stop();
        if (aboutChanged && settings.tab === "about")
            loadTab('about').then(m => m.setup()).catch(console.error);
        if (restartChanged && next.restart_required?.length)
            showNotification(next.restart_required.join(", ") + ": required to apply these settings", "info");
        updateSettingsTab();
        redrawMap();
    },
    pluginsChanged: () => {
        showDialog('Plugins changed', '<p>Reload the viewer to apply the updated plugins.</p><button type="button" class="btn btn-primary" id="reload-viewer-plugins">Reload</button>');
        document.getElementById('reload-viewer-plugins').addEventListener('click', () => {
            saveSettings();
            updateMapURL();
            if (card_type === "ship" && card_mmsi) {
                const url = new URL(location.href);
                url.searchParams.set("mmsi", card_mmsi);
                history.replaceState(null, "", url);
            }
            location.reload();
        });
    },
});
applyServerMaps(config.maps);

initData({
    configVersion: (version) => serverConfig.update(version),
    shipsLoaded: (response) => {
        drawStation();
        tab_title_count = Object.keys(shipsDB).length;
        updateTitle();
        community.pushVesselsToCommunityPopup(response.dynamic);
    },
});
stat.init({
    onStation: (name) => {
        tab_title_station = name;
        updateTitle();
    },
});
initTabs({
    leaving: () => {
        searchPanel?.close();
        hideMenu();
    },
    shown: () => applyPanels(),
});

loadPlugins();

console.log("Load settings");
loadSettings();
restoreCardLayout();
applyPanels();

console.log("Load settings from URL parameters");

loadSettingsFromURL();
setTargetcardStyle(settings.shipcard_style);   // the card was built before the settings were read
applyDynamicStyling();
community.applySharingState();

if (config.features.managed) {
    const pollSharingState = async () => {
        if (document.hidden) return;
        try {
            const r = await fetch("api/sharing_state.json");
            if (!r.ok) return;
            const s = await r.json();
            await serverConfig.update(s.config_version);
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

updateDarkMode();

console.log("Switch to active tab");
selectTab();

if (urlParams.get("mmsi")) openFocus(urlParams.get("mmsi"), urlParams.get("zoom"));
updateSortMarkers();
saveSettings();
setGraphVisibility('signal', settings.show_signal_graphs, false);
setGraphVisibility('ppm', settings.show_ppm_graphs, false);
setPlotAbsoluteTime(settings.plot_absolute_time, false);
targetcard.prepare();
buildSettingsTabs();

applyServerFeatures(config);
applyMenuLabels();

about.showWelcome();
announceStickyState();
kiosk.updateKiosk();
updateAndroid();

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
applyTargetcardPinStyling()
