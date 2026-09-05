import { settings, isAndroid, isKiosk } from './core/state.js';
import { ShippingClass } from '../shared/core/constants.js';
import { SPEED_PALETTES, palette as validPalette, bucketColor, speedBucket, paletteCSS } from '../shared/core/palette.js';
import * as filter from './core/filter.js';
import * as components from '../shared/components.js';
import * as mapui from '../shared/mapui.js';
import * as tooltipLib from '../shared/tooltip.js';
import * as tableLib from '../shared/table.js';
import * as markersLib from '../shared/markers.js';
import * as panelLib from '../shared/panel.js';
import * as settingsStorage from '../shared/settings.js';
import { debounce, decodeHTMLEntities, copyToClipboard } from './core/util.js';
import {
    ships as shipsDB, planes as planesDB, paths, pathsFrom, station,
    counts as shipCounts, shipsSince, planesSince, clock,
    cardMmsi as card_mmsi, cardType as card_type,
    hoverMmsi as hoverMMSI, hoverType, markerTracks as marker_tracks,
    setShips, setPlanes, setPaths, setPathsFrom, setStation,
    setCounts as setShipCounts, setShipsSince, setPlanesSince, setClock,
    setCard, setHover, setMarkerTracks,
} from './core/store.js';
import { calcOffset1M, hasValidCoords } from '../shared/core/geo.js';
import { validityBand } from '../shared/binary.js';
import { init as initRainRadar } from './overlays/rainradar.js';
import * as fireworks from './overlays/fireworks.js';
import * as community from './overlays/community.js';
import * as kiosk from './features/kiosk.js';
import * as measure from './features/measure.js';
import * as boxselect from './features/boxselect.js';
import * as replay from './features/replay.js';
import * as binary from './features/binary.js';
import * as stations from '../shared/stations.js';
import * as range from './features/range.js';
import * as ticker from './features/ticker.js';
import * as planecard from './features/planecard.js';
import * as targetcard from './features/targetcard.js';
import { getDistanceVal, getDistanceUnit, getDistanceConversion, getSpeedVal, getSpeedUnit, getSpeedConversion, getDimVal, getDimUnit, getDraughtVal, getShipDimension, getLatValFormat, getLonValFormat } from './core/units.js';
import { CHANGE, getEtaVal, getDeltaTimeVal, getCountryName, getStatusVal, getMmsiTypeVal, getStringfromMsgType, getStringfromGroup, getStringfromChannels, getShipTypeShort, getShipTypeFull, sanitizeString, formatBytes, isHttpUrl, formatTime, compactCount, getICAO, getICAOFromHexIdent } from '../shared/core/text.js';
import { getShipName, getCallSign, setShipNameProvider, setCallSignProvider } from './core/names.js';
import { flagHTML, bucketChip } from '../shared/components.js';

// Named imports (instead of `import * as ...`) so Vite tree-shakes everything
// outside this list. Plugins relying on other OL classes via window.ol will
// need to be updated; the contract is "what script.js + bundled plugins use".
import OlMap from 'ol/Map';
import OlView from 'ol/View';
import OlFeature from 'ol/Feature';
import TileLayer from 'ol/layer/Tile';
import VectorLayer from 'ol/layer/Vector';
import VectorTileLayer from 'ol/layer/VectorTile';
import OSMSource from 'ol/source/OSM';
import XYZSource from 'ol/source/XYZ';
import TileWMSSource from 'ol/source/TileWMS';
import VectorSource from 'ol/source/Vector';
import TileGrid from 'ol/tilegrid/TileGrid';
import Point from 'ol/geom/Point';
import LineString from 'ol/geom/LineString';
import Polygon from 'ol/geom/Polygon';
import CircleGeom from 'ol/geom/Circle';
import Style from 'ol/style/Style';
import Stroke from 'ol/style/Stroke';
import Fill from 'ol/style/Fill';
import Icon from 'ol/style/Icon';
import CircleStyle from 'ol/style/Circle';
import Text from 'ol/style/Text';
import { fromLonLat, toLonLat, transformExtent, get as getProjection } from 'ol/proj';
import { getLength } from 'ol/sphere';
import { containsCoordinate, getWidth, getTopLeft } from 'ol/extent';
import 'ol/ol.css';

const MENU_CHECKS = {
    toggleTargetcardPin: () => settings.targetcard_pinned,
    toggleShipcardStyle: () => settings.shipcard_style === "tabs",
    toggleTrackCtx: () => trackIsShown(context_mmsi),
    toggleAllTracks: () => settings.show_all_tracks,
    toggleTrackCutoff: () => trackCutoff > 0,
    toggleLabel: () => settings.show_labels != "never",
    toggleRange: () => settings.show_range,
    toggleAttribution: () => attributionPinned,
    toggleTicker: () => settings.ticker,
    toggleReplaycard: () => replaycardVisible(),
    toggleMeasurecard: () => measurecardVisible(),
    toggleCommunityPane: () => community.isPaneOpen(),
    toggleKioskMode: () => isKiosk(),
    ToggleFireworks: () => fireworks.isRunning(),
    toggleGraphVisibility: (el) => settings[GRAPH_KINDS[el.dataset.graph].setting],
    toggleDarkMode: () => settings.dark_mode,
    toggleScreenSize: () => !!document.fullscreenElement,
    replayToggleLabels: () => replay.getLabels(),
    replaySetSpeed: (el) => replay.getSpeed() == el.dataset.speed,
};

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
    markers: {
        sheet: 'icons.png',
        hover: () => ({ type: hoverType, id: hoverMMSI }),
        selected: () => ({ type: card_type, id: card_mmsi }),
        opacity: (ship) => getShipOpacity(ship),
        lookup: (type, id) => (type === 'ship' ? shipsDB[id]?.raw : type === 'plane' ? planesDB[id]?.raw : null),
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
        { isOpen: () => !!(dialogModal && dialogModal.isOpen()) },
        { isOpen: () => settingsPanel.isOpen(), close: () => settingsPanel.close(), stage: true },
        { isOpen: () => document.getElementById("menubar").classList.contains("visible"), close: () => hideMenu(), stage: true },
        { isOpen: () => targetcardVisible(), close: () => showTargetcard(null, null) },
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
        before: document.querySelector('#context-menu li[data-action="toggleReplaycard"]')?.nextSibling,
    },
});



const ol = {
    Map: OlMap,
    View: OlView,
    Feature: OlFeature,
    layer: { Tile: TileLayer, Vector: VectorLayer, VectorTile: VectorTileLayer },
    source: { OSM: OSMSource, XYZ: XYZSource, TileWMS: TileWMSSource, Vector: VectorSource },
    tilegrid: { TileGrid },
    geom: { Point, LineString, Polygon, Circle: CircleGeom },
    style: { Style, Stroke, Fill, Icon, Circle: CircleStyle, Text },
    proj: { fromLonLat, toLonLat, transformExtent, get: getProjection },
    sphere: { getLength },
    extent: { containsCoordinate, getWidth, getTopLeft },
};
window.ol = ol;

// window.__SERVER_CONFIG__ is set by /custom/plugins.js (emitted by the C++
// side). Defaulted here so the frontend works standalone (e.g. Vite dev).
const config = window.__SERVER_CONFIG__ || {
    build: { version: 'unknown', describe: 'unknown' },
    context: 'aiscatcher',
    station: '',
    webcontrol_http: '',
    features: {
        share_location: false, save_messages: false, replay: true,
        realtime: false, log: false, decoder: false,
        managed: false, about_md: false,
    },
    receivers: [],
    plugins: { loaded: [], errors: [] }, // loaded: [{ name, version }, ...]
};

let plotsModule = null;
let shipTableModule = null;
let decoderModule = null;
let realtimeModule = null;

// Plugin contract version. Bumped when the plugin-facing API changes shape.
// Plugins guard with `if (typeof AISCatcher !== 'undefined' &&
// AISCatcher.PLUGIN_API_VERSION >= N)`. Public surface is window.AISCatcher
// (defined below). Bare-global aliases mirror AISCatcher for back-compat but
// are deprecated.
//   v4: addTargetcardItem() requires a function callback (CSP-clean).
//   v5: script.js is an ES module; override hooks (setShipFilter,
//       setRefreshInterval, setShipNameProvider, setCallSignProvider) and
//       map helpers (addTileLayer, removeTileLayerAll, ...) are on AISCatcher.
const PLUGIN_API_VERSION = 5;

// ─────────────────────────────────────────────────────────────────────────────
// Inline-handler migration: actions registry + delegated dispatcher.
// HTML uses data-action="name" (click), or data-on-change / data-on-input /
// data-on-contextmenu="name" for other event types. Handlers receive
// (event, dataset, element).
//
// NOTE: declared near the top of the file because plugins (called from
// loadPlugins() further down) need ACTIONS to be initialized — `const` is in
// the temporal dead zone until reached. The arrow-function bodies below
// reference function declarations elsewhere in the file; those hoist, so they
// resolve correctly at call time.
// ─────────────────────────────────────────────────────────────────────────────

const ACTIONS = {
    // header bar
    toggleMenu: () => toggleMenu(),
    hideMenu: () => hideMenu(),
    headerClick: () => headerClick(),
    activateTab: (e, d) => activateTab(e, d.tab),
    openWebControl: () => openWebControl(),
    toggleInfoPanel: () => toggleInfoPanel(),
    toggleScreenSize: () => toggleScreenSize(),
    showReceiverDialog: () => showReceiverDialog(),
    selectReceiver: (e, d) => selectReceiver(d.idx),

    // replay
    toggleReplaycard: () => toggleReplaycard(),
    replayMenu: (e) => showContextMenu(e, 0, null, ['ctx-replay-map']),
    replaySetSpeed: (e, d) => replay.setSpeed(Number(d.speed)),
    replayToggle: () => replayToggle(),
    replayCycleSpeed: () => replay.cycleSpeed(),
    replayToggleLabels: () => replay.toggleLabels(),
    replaySeek: (e, d, el) => replaySeek(el),

    // tableside / generic close buttons / dialog
    hideTablecard: () => hideTablecard(),
    updateTableSort: (e, dataset, el) => updateTableSort(e, dataset, el),
    tablePagePrev: () => turnTablePage(-1),
    tablePageNext: () => turnTablePage(1),
    closeSettings: () => closeSettings(),
    closeDialog: () => closeDialog(),

    // settings dialog: simple wrappers
    applyDefaultSettings: () => applyDefaultSettings(),
    setDarkMode: (e, d, el) => setDarkMode(el.checked),
    showPlugins: () => showPlugins(),
    setCoordinateFormat: (e, d, el) => setCoordinateFormat(el.value),
    setMetrics: (e, d, el) => setMetrics(el.value),
    setTableIcon: (e, d, el) => setTableIcon(el.checked),
    setFading: (e, d, el) => setFading(el.checked),
    setRangeSwitch: (e, d, el) => range.setRangeSwitch(el.checked),
    setRangeTimePeriod: (e, d, el) => range.setRangeTimePeriod(el.value),
    setKiosk: (e, d, el) => { kiosk.setKiosk(el.checked); applyPanels(); },
    setKioskRotationSpeed: (e, d, el) => kiosk.setKioskRotationSpeed(el.value),
    setKioskPanMap: (e, d, el) => kiosk.setKioskPanMap(el.checked),
    setGraphVisibility: (e, d, el) => setGraphVisibility(d.graph, el.checked),
    setPlotAbsoluteTime: (e, d, el) => setPlotAbsoluteTime(el.checked),
    setMapToolbar: (e, d, el) => { settings.map_toolbar = el.value; saveSettings(); applyToolbarMode(); },
    setMapSetting: (e, d, el) => setMapSetting(d.key,
        el.type === 'checkbox' ? el.checked :
        (el.type === 'range' || el.type === 'number') ? Number(el.value) : el.value),
    setTrackHistory: (e, d, el) => setTrackHistory(TRACK_HISTORY_STOPS[el.value]),
    setBinaryDisplay: (e, d, el) => binary.setBinaryDisplay(el.value),
    setBinaryCategory: (e, d, el) => binary.setBinaryCategory(d.cat, el.checked),
    setBinaryColorClass: (e, d, el) => binary.setBinaryColorClass(el.checked),
    setBinaryIdLabels: (e, d, el) => binary.setBinaryIdLabels(el.checked),
    setBinaryGroupAreas: (e, d, el) => binary.setBinaryGroupAreas(el.checked),
    setRangeColor: (e, d, el) => range.setRangeColor(el.value, d.field),
    setMapSettingDistanceColor: (e, d, el) => { range.removeDistanceCircles(); setMapSetting('distance_circle_color', el.value); },
    setShowTrackOnSelect: (e, d, el) => { settings.show_track_on_select = el.checked; saveSettings(); },
    setShowTrackOnHover: (e, d, el) => { settings.show_track_on_hover = el.checked; saveSettings(); },
    setTrackVisibility: (e, d, el) => setTrackVisibility(el.value),
    applyColorToAllTracks: (e, d, el) => applyColorToAllTracks(el.value),
    setTrackColorMode: (e, d, el) => setTrackColorMode(el.value),
    setTrackSpeedPalette: (e, d, el) => setTrackSpeedPalette(el.value),
    setTrackSpeedMax: (e, d, el) => setTrackSpeedMax(Number(el.value)),
    resetTrackColorsToDefault: () => resetTrackColorsToDefault(),
    setTrackClassColor: (e, d, el) => setTrackClassColor(d.shipclass, el.value),

    // settings: range/icon-scale/opacity sliders (oninput = display only, onchange = persist)
    setIconScale: (e, d, el) => { settings.icon_scale = Number(el.value); redrawMap(); saveSettings(); },
    setMapOpacity: (e, d, el) => {
        if (!settings.basemap_opacity) settings.basemap_opacity = {};
        settings.basemap_opacity[baseMapSelect(el.dataset.kind).value] = Math.round((1 - Number(el.value)) * 100) / 100;
        setMapOpacity();
        saveSettings();
    },
    updateBaseMapDim: (e, d, el) => updateBaseMapDimLabel(el.dataset.kind, el.value),
    updateSliderDisplay: (e, d, el) => updateSliderDisplay(d.display, el.value),
    updateTrackHistoryDisplay: (e, d, el) => updateSliderDisplay('trackHistory', TRACK_HISTORY_STOPS[el.value]),

    // context menu (depends on global context_mmsi/card_mmsi)
    toggleTickerSide: () => {
        settings.ticker_bottom = !settings.ticker_bottom;
        const btn = document.getElementById("ticker_side");
        if (btn) {
            const label = settings.ticker_bottom ? "Move the bar to the top" : "Move the bar to the bottom";
            btn.title = label;
            btn.setAttribute("aria-label", label);
        }
        saveSettings();
        applyPanels();
        fitTargetcard();
    },
    toggleFollowStationCtx: () => toggleFollow("STATION"),
    copyTextCtx: () => copyText(context_mmsi),
    copyTextICAO: () => copyText(getICAOFromHexIdent(context_mmsi)),
    downloadCSV: () => shipTableModule?.downloadCSV(),
    unpinCenter: () => unpinCenter(),
    toggleAllTracks: () => settings.show_all_tracks ? deleteAllTracks() : showAllTracks(),
    deleteAllTracks: () => deleteAllTracks(),
    toggleTrackCutoff: () => toggleTrackCutoff(),
    startBoxSelect: () => { boxselect.start(); showNotification('Drag a rectangle to enable tracks (Esc to cancel)'); },
    ToggleFireworks: () => fireworks.toggle(),
    toggleLabel: () => toggleLabel(),
    toggleKioskMode: () => { kiosk.toggleKioskMode(); applyPanels(); },
    toggleRange: () => range.toggleRange(),
    toggleTrackCtx: () => toggleTrack(context_mmsi),
    toggleFollowCtx: () => toggleFollow(context_mmsi),
    mapResetViewZoomCtx: () => mapResetViewZoom(13, context_mmsi),
    showTargetcardCtx: (e, d) => showTargetcard(d.kind, context_mmsi),
    openAISCatcherSiteCtx: () => openExt('aiscatcher', context_mmsi),
    showVesselDetailCtx: () => showVesselDetail(context_mmsi),
    showNMEACtx: () => showNMEA(context_mmsi),
    openRealtimeForMMSICtx: async () => {
        if (!realtimeModule) realtimeModule = await import('./tabs/realtime.js');
        realtimeModule.openForMMSI(context_mmsi);
    },
    copyCoordinatesCtx: () => copyCoordinates(context_mmsi),
    openGoogleSearchCtx: (e, d) => openExt('google', d.icao ? getICAOFromHexIdent(context_mmsi) : context_mmsi),
    openVesselFinderCtx: () => openExt('vesselfinder', context_mmsi),
    openAISHubCtx: () => openExt('aishub', context_mmsi),
    showServerErrors: () => showServerErrors(),
    openSettings: () => openSettings(),
    toggleDarkMode: () => toggleDarkMode(),

    // vessel filter
    openFilterPanel: () => openFilterPanel(),
    resetFilter: () => resetFilter(),
    toggleFilterBucket: (e, d) => {
        filter.toggle("bucket", d.bucket);
        applyFilter();
    },
    toggleFilterItem: (e, d) => {
        filter.toggle(d.kind, d.kind === "bucket" ? d.id : Number(d.id));
        applyFilter();
    },
    setAllFilterItems: (e, d) => {
        filter.setAll(d.kind, d.on === "1");
        applyFilter();
    },
    setFilterNumber: (e, d, el) => setFilterValue(d.key, el.value === "" ? null : Number(el.value)),
    setFilterChoice: (e, d, el) => setFilterValue(d.key, el.value),
    setFilterFlag: (e, d, el) => setFilterValue(d.key, el.checked),
    updateFilterSeenDisplay: (e, d, el) => {
        const label = document.getElementById("filter_seen_label");
        const v = Number(el.value) || 0;
        if (label) label.textContent = "Seen within" + (v > 0 ? " (" + v + " min)" : " (any)");
    },
    toggleGraphVisibility: (e, d) => toggleGraphVisibility(d.graph),
    setPlotsMode: (e, d) => plotsModule?.setMode(d.mode),

    // realtime / decoder controls
    openRealtimeFilters: () => realtimeModule?.openFilters(),
    addRealtimeFilter: () => realtimeModule?.addFilterFromInput(),
    clearRealtimeFilters: () => realtimeModule?.clearFilters(),
    realtimeFilterKindChanged: () => realtimeModule?.filterKindChanged(),
    toggleBackgroundStreaming: () => realtimeModule?.toggleBackgroundStreaming(),
    toggleRealtimePause: () => realtimeModule?.togglePause(),
    clearRealtimeTable: () => realtimeModule?.clear(),
    decodeNMEA: async () => {
        if (!decoderModule) decoderModule = await import('./tabs/decoder.js');
        decoderModule.decode();
    },
    clearDecoder: async () => {
        if (!decoderModule) decoderModule = await import('./tabs/decoder.js');
        decoderModule.clear();
    },

    // map tab buttons
    setMeasureMode: () => { measure.setMeasureMode(); showNotification('Shift+click on start point/object'); },
    toggleMeasurecard: () => toggleMeasurecard(),
    toggleTargetcardPin: () => toggleTargetcardPin(),
    toggleShipcardStyle: () => toggleShipcardStyle(),
    toggleTargetcardSize: () => toggleTargetcardSize(),
    targetcardSelectSelf: (e, d, el) => { if (settings.shipcard_style !== "tabs") targetcardselect(el); },
    targetcardContextMenu: (e) => showContextMenu(e, card_mmsi, card_type, ['object', 'object-map', 'ctx-targetcard']),
    showTargetcardClose: () => showTargetcard(null, null),
    showBinaryMessageDialogCard: () => binary.showBinaryMessageDialog(card_mmsi),
    openRealtimeForMMSICard: async () => {
        if (!realtimeModule) realtimeModule = await import('./tabs/realtime.js');
        realtimeModule.openForMMSI(card_mmsi);
    },
    openAISCatcherSiteCard: () => openExt('aiscatcher', card_mmsi),
    toggleTrackCard: () => toggleTrack(card_mmsi),
    toggleFollowCard: () => toggleFollow(card_mmsi),
    openFlightAwareCard: () => openExt('flightaware', card_mmsi),
    openPlaneSpottersCard: () => openExt('planespotters', card_mmsi),
    openADSBExchangeCard: () => openExt('adsbexchange', card_mmsi),
    toggleStatcard: () => toggleStatcard(),
    toggleTicker: () => toggleTicker(),
    toggleTablecard: () => toggleTablecard(),
    mapSettingsContextMenu: (e, d, el) => showContextMenu(e, '', '', ['settings', 'ctx-map'], el),
    toggleCommunityPane: () => community.toggleCommunityPane(),
    openMapSettings: () => openSettingsTab("Map"),
    toggleAttribution: () => toggleAttribution(),
    mainspaceContextMenu: (e) => showContextMenu(e, 0, '', ['settings']),
    plotsContextMenu: (e) => showContextMenu(e, '', 'charts', ['settings', 'ctx-charts']),

    // info panel
    showAboutFromInfoPanel: () => { toggleInfoPanel(); showAboutDialog(); },

    // dynamically-rendered targetcard items
    rotateTargetcardIcons: () => ui.card.footer.rotate(card_type),
    techInfo: (e) => { e.stopPropagation(); ui.card.popover.toggle(document.getElementById("tech_popover"), document.getElementById("targetcard_tech_info")); },
    shiptypeInfo: (e) => { e.stopPropagation(); ui.card.popover.toggle(document.getElementById("shiptype_popover"), document.getElementById("targetcard_shiptype_info")); },
    shipHistory: (e) => { e.stopPropagation(); ui.card.section.open("changes"); },
    toggleTargetcardSection: (e, d, el) => { e.stopPropagation(); ui.card.section.toggle(el?.dataset.section || d.section); },
    showNMEAContextCopy: (e, d) => showContextMenu(e, d.copy || '', 'ship', ['settings', 'copy-text']),
    removeRealtimeFilter: (e, d) => realtimeModule?.removeFilter(d.kind, d.value),
};

// Public plugin API. Mutable state uses getters so plugins always see the
// live value rather than a snapshot taken at namespace-build time.
window.AISCatcher = {
    PLUGIN_API_VERSION,

    addTargetcardItem: targetcard.addItem,
    ACTIONS,

    addShipcardItem: targetcard.addItem,
    showShipcard: showTargetcard,

    showDialog,
    closeDialog,
    showNotification,
    showTargetcard,
    showContextMenu,
    openFocus,

    addTileLayer,
    removeTileLayer,
    removeTileLayerAll,
    addOverlayLayer,
    removeOverlayLayer,
    removeOverlayLayerAll,

    setShipFilter(fn) { shipFilterOverride = fn; },
    setRefreshInterval(ms) {
        refreshIntervalMs = ms;
        if (interval) {
            clearInterval(interval);
            interval = setInterval(refresh_data, refreshIntervalMs);
        }
    },
    setShipNameProvider,
    setCallSignProvider,

    get config() { return config; },
    get settings() { return settings; },
    get station() { return station; },
    get shipsDB() { return shipsDB; },
    get planesDB() { return planesDB; },
    get card_mmsi() { return card_mmsi; },
    get card_type() { return card_type; },
    get context_mmsi() { return context_mmsi; },
    get context_type() { return context_type; },
};

// Mirror AISCatcher keys onto window so legacy plugins using bare globals
// (addTargetcardItem, card_mmsi, ...) still resolve. Module top-level no
// longer leaks to window since script.js is an ES module.
for (const k of Object.keys(window.AISCatcher)) {
    if (k in window) continue;
    const desc = Object.getOwnPropertyDescriptor(window.AISCatcher, k);
    if (desc.get) {
        Object.defineProperty(window, k, { get: desc.get, configurable: true });
    } else {
        window[k] = desc.value;
    }
}

window.__app__ = {
    get activeReceiver() { return activeReceiver; },
    get getTableShiptype() { return getTableShiptype; }, // sprite-coupled, can't move
    get tableRowClick() { return tableRowClick; },
    get selectMapTab() { return selectMapTab; },
    get saveSettings() { return saveSettings; },
    get fetchShips() { return fetchShips; },
    get fetchTableRows() { return fetchTableRows; },
    get shipsSince() { return clock; },
    get receivers() { return config.receivers || []; },
    get setReceiver() { return onReceiverChange; },
    get shipsVisible() { return () => Object.values(shipsDB).filter(shipVisible).map((e) => e.raw); },
    get mmsiVisible() { return (mmsi) => !shipsDB[mmsi] || shipVisible(shipsDB[mmsi]); },
};

function _bindDelegatedActions() {
    const handlers = [
        ['click',       '[data-action]',         'action'],
        ['change',      '[data-on-change]',      'onChange'],
        ['input',       '[data-on-input]',       'onInput'],
        ['contextmenu', '[data-on-contextmenu]', 'onContextmenu'],
    ];
    for (const [evt, sel, key] of handlers) {
        document.body.addEventListener(evt, (e) => {
            const el = e.target.closest(sel);
            if (!el) return;
            const fn = ACTIONS[el.dataset[key]];
            if (fn) fn(e, el.dataset, el);
        });
    }
}

if (document.body) _bindDelegatedActions();
else document.addEventListener('DOMContentLoaded', _bindDelegatedActions);

let interval,
    activeReceiver = 0,
    lastPathFetch = 0,
    lastFullPathFetch = 0,
    trackCutoff = 0,
    map,
    basemaps = {},
    overlapmaps = {},
    shipsTimeout = 1800,
    shipsLastCleanup = 0,
    planesTimeout = 300,
    planesLastCleanup = 0,
    hover_feature = undefined,
    logViewer = null,
    tab_title_station = config.station,
    tab_title_count = null,
    context_mmsi = null,
    context_type = null,
    tab_title = "AIS-catcher";

const BASEMAP_KEYS = { day: "map_day", night: "map_night" };
function baseMapRows() { return [...document.querySelectorAll(".basemap-row")]; }
function baseMapSelect(kind) { return document.querySelector('.basemap-select[data-kind="' + kind + '"]'); }

// `let` so AISCatcher.setRefreshInterval can mutate.
let refreshIntervalMs = 2500;
let shipFilterOverride = null;
let updateInProgress = false;
let activeTileLayer = undefined;
let hover_enabled_track = false,
    select_enabled_track = false;

let center;
const context = config.context;
if (typeof window.loadPlugins === 'undefined') {
    window.loadPlugins = function () { };
}

const hover_info = document.getElementById('hover-info');

let isFetchingShips = false;

// legs whose points never reported a speed, in speed-colored mode
const TRACK_SPEED_UNKNOWN_COLOR = '#9aa0a6';

function getDefaultTrackColors() {
    return {
        [ShippingClass.CARGO]: '#00ff7f',
        [ShippingClass.B]: '#ff00ff',
        [ShippingClass.PASSENGER]: '#0000ff',
        [ShippingClass.SPECIAL]: '#a52a2a',
        [ShippingClass.TANKER]: '#ff0000',
        [ShippingClass.HIGHSPEED]: '#ffff00',
        [ShippingClass.OTHER]: '#12a5ed',
        [ShippingClass.UNKNOWN]: '#12a5ed',
        [ShippingClass.FISHING]: '#ff1493',
        [ShippingClass.ATON]: '#0000f1',
        [ShippingClass.STATION]: '#0000f1',
        [ShippingClass.SARTEPIRB]: '#ff0000',
        [ShippingClass.PLANE]: '#ff0040',
        [ShippingClass.HELICOPTER]: '#d01616'
    };
}

function resetTrackColorsToDefault() {
    settings.track_class_colors = getDefaultTrackColors();
    updateTrackColorInputs();
    saveSettings();
    redrawMap();
}

const DEFAULT_SETTINGS = {
        counter: true,
        fading: false,
        android: false,
        kiosk: false,
        welcome: true,
        coordinate_format: "decimal",
        icon_scale: 1,
        track_weight: 1,
        track_opacity: 1,
        track_history: 30,
        track_trash_threshold: 30,
        track_color_mode: "class",
        track_speed_palette: "turbo",
        track_speed_max: 20,
        show_range: false,
        distance_circles: true,
        distance_circle_color: '#1c71d8',
        map_day: "OpenStreetMap",
        map_overlay: [],
        map_night: "OpenStreetMap",
        zoom: 3,
        lat: 0,
        lon: 0,
        table_shiptype_use_icon: true,
        tableside_column: "shipname",
        tableside_order: "ascending",
        range_timeframe: '24h',
        range_color: "#FFA500",
        range_color_short: "#FFDAB9",
        range_color_dark: "#4B4B4B",
        range_color_dark_short: "#303030",
        fix_center: false,
        center_point: "station",
        tooltipLabelColor: "#ffffff",
        tooltipLabelColorDark: "#ffffff",
        tooltipLabelShadowColor: "#000000",
        tooltipLabelShadowColorDark: "#000000",
        tooltipLabelFontSize: 9,
        shiphover_color: "#FFA500",
        shipselection_color: "#943b3e",
        shipoutline_border: "#A9A9A9",
        shipoutline_inner: "#808080",
        shipoutline_opacity: 0.9,
        circle_scale: 6.0,
        targetcard_pinned: false,
        targetcard_pinned_x: null,
        targetcard_pinned_y: null,
        dark_mode: false,
        center_radius: 0,
        show_station: true,
        ticker: true,
        ticker_bottom: true,
        metric: "DEFAULT",
        setcoord: true,
        tab: "map",
        show_labels: "dynamic",
        labels_declutter: true,
        labels_prioritize_active: true,
        labels_active_only: false,
        label_class_background: true,
        eri: true,
        loadURL: true,
        map_opacity: 0.5,
        layer_opacity: { Aircraft: 1 },
        basemap_opacity: {
            "OpenFreeMap Positron": 1, "OpenFreeMap Positron (no labels)": 1,
            "OpenFreeMap Bright": 1, "OpenFreeMap Liberty": 1,
            "OpenFreeMap Dark": 1, "OpenFreeMap Dark (no labels)": 1,
            "OpenStreetMap": 0.6, "Satellite": 0.6,
        },
        show_track_on_hover: false,
        show_track_on_select: false,
        show_all_tracks: false,
        targetcard_top_left: true,
        targetcard_open_max: true,
        shipcard_style: "tabs",
        shipcard_active_tab: "summary",
        map_toolbar: "compact",
        show_signal_graphs: true,
        show_ppm_graphs: true,
        plot_absolute_time: true,
        kiosk_rotation_speed: 5,
        kiosk_pan_map: true,
        shiptable_columns: ["shipname", "mmsi", "imo", "callsign", "shipclass", "region", "lat", "lon", "last_signal", "level", "distance", "bearing", "speed", "repeat", "ppm", "status"],
        realtime_background_streaming: false,
        realtime_filters: [],
        ship_filter: {},
        binary_messages: "highlight",
        binary_color_class: true,
        binary_id_labels: true,
        binary_group_areas: false,
        binary_exclude: ["aton"]
};


const settingsStore = settingsStorage.create({
    key: context,
    defaults: DEFAULT_SETTINGS,
    target: settings,
    version: 2,
    migrate: updateForLegacySettings,
});

function restoreDefaultSettings() {
    settingsStore.applyDefaults();
    settings.track_class_colors = getDefaultTrackColors();
}

// NMEA Decoder Functions
function toggleInfoPanel() {
    const overlay = document.getElementById('info-overlay');
    const infoPanel = document.querySelector('.info-panel');

    overlay.classList.toggle('active');
    infoPanel.classList.toggle('active');
}

function updateTitle() {
    document.title = (tab_title_count ? " (" + tab_title_count + ") " : "") + tab_title + " " + tab_title_station;
}

function applyDefaultSettings() {
    const t = settings.tab;

    let android = settings.android;
    let darkmode = settings.dark_mode;
    const ship_filter = settings.ship_filter;
    restoreDefaultSettings();

    settings.android = android;
    settings.dark_mode = darkmode;
    if (ship_filter) settings.ship_filter = ship_filter;

    updateSortMarkers();
    setDarkMode(settings.dark_mode);
    setMetrics(settings.metric, false);
    updateMapLayer();
    setFading(settings.fading);

    updateFocusMarker();
    range.removeDistanceCircles();

    settings.tab = t;
    settings.welcome = false;
    saveSettings();

    redrawMap();
    applyPanels();
    updateSettingsTab();
    showNotification("Settings restored to defaults", "success");
}

// some functions useful in plugins
function addTileLayer(title, layer) {
    basemaps[title] = layer;
}

function removeTileLayer(title) {
    delete basemaps[title];
}
function removeTileLayerAll() {
    basemaps = {};
}
function addOverlayLayer(title, layer) {
    overlapmaps[title] = layer;
    if (typeof map !== 'undefined' && map && document.getElementById('overlayContainer')) {
        map.addLayer(layer);
        layer.setVisible(false);
        const visible = Array.isArray(settings.map_overlay) && settings.map_overlay.includes(title);
        layer.setVisible(visible);
        layer.setOpacity(getLayerOpacity(title));
        addOverlayCheckbox(title);
    }
}
function attributionHTML(layer) {
    const a = layer?.getSource()?.getAttributions();
    const raw = typeof a === 'function' ? a() : a;
    const html = Array.isArray(raw) ? raw.join(', ') : (raw || '');
    return html || layer?.get?.('attributions') || '';
}

// <option> holds text only, so the credit has to lose its markup there
function attributionPlain(layer) {
    const div = document.createElement('div');
    div.innerHTML = attributionHTML(layer);
    return (div.textContent || '').replace(/\s+/g, ' ').trim();
}

function updateBaseMapDimLabel(kind, v) {
    document.querySelector('.basemap-row[data-kind="' + kind + '"] .basemap-dim-label').textContent =
        `Dimming (${Math.round(parseFloat(v) * 100)}%)`;
}

function refreshBaseMapRows() {
    for (const kind of Object.keys(BASEMAP_KEYS)) {
        const sel = baseMapSelect(kind);
        if (!sel) continue;
        sel.value = settings[BASEMAP_KEYS[kind]];
        const key = sel.value;

        const note = sel.closest(".basemap-row").querySelector(".basemap-credit");
        const credit = attributionHTML(basemaps[key]);
        note.innerHTML = credit;
        note.style.display = credit ? "" : "none";

        const dim = 1 - getBaseOpacity(key);
        document.querySelector('.basemap-dim[data-kind="' + kind + '"]').value = dim;
        updateBaseMapDimLabel(kind, dim);

        const active = (kind === "night") === !!settings.dark_mode;
        baseMapRows().filter((r) => r.dataset.kind === kind).forEach((r) => r.classList.toggle("basemap-active", active));
        document.querySelector('.basemap-title[data-kind="' + kind + '"]')?.classList.toggle("basemap-active", active);
    }
}

// polling overlays get their source only once switched on
function refreshOverlayCredits() {
    document.querySelectorAll('.overlay-row').forEach(row => {
        const credit = attributionHTML(overlapmaps[row.querySelector('input')?.id]);
        let note = row.querySelector('.map-attribution-note');

        if (!credit) {
            if (note) note.remove();
            return;
        }
        if (!note) {
            note = document.createElement('span');
            note.className = 'map-attribution-note';
            row.appendChild(note);
        }
        note.innerHTML = credit;
    });
}

function addOverlayCheckbox(title) {
    const overlayContainer = document.getElementById('overlayContainer');
    if (!overlayContainer || overlayContainer.querySelector(`#${CSS.escape(title)}`)) return;

    const checkbox = document.createElement('input');
    checkbox.type = 'checkbox';
    checkbox.id = title;
    checkbox.name = title;
    checkbox.checked = settings.map_overlay.includes(title);

    const label = document.createElement('label');
    label.setAttribute('for', title);
    label.textContent = title;

    const row = document.createElement('div');
    row.className = 'overlay-row';
    row.appendChild(checkbox);
    row.appendChild(label);

    const dim = document.createElement('div');
    dim.className = 'overlay-dim';

    const slider = document.createElement('input');
    slider.type = 'range';
    slider.className = 'slider';
    slider.min = 0;
    slider.max = 1;
    slider.step = 0.01;
    slider.title = `Dimming for ${title}`;

    const readout = document.createElement('span');
    readout.className = 'overlay-dim-value';

    slider.addEventListener('input', function () {
        setLayerOpacity(title, this.value);
        updateDimRow(row);
    });
    slider.addEventListener('change', saveSettings);

    dim.appendChild(slider);
    dim.appendChild(readout);
    row.appendChild(dim);
    row.classList.toggle('overlay-off', !checkbox.checked);
    updateDimRow(row);

    overlayContainer.appendChild(row);

    checkbox.addEventListener('change', function () {
        overlapmaps[title].setVisible(this.checked);
        if (this.checked) {
            if (!settings.map_overlay.includes(title)) settings.map_overlay.push(title);
        } else {
            const i = settings.map_overlay.indexOf(title);
            if (i > -1) settings.map_overlay.splice(i, 1);
        }
        row.classList.toggle('overlay-off', !this.checked);
        saveSettings();
        redrawMap();
    });
}

function updateDimRow(row) {
    const title = row.querySelector('input[type="checkbox"]')?.id;
    const slider = row.querySelector('.overlay-dim input[type="range"]');
    if (!title || !slider) return;
    const v = getLayerOpacity(title);
    slider.value = v;
    row.querySelector('.overlay-dim-value').textContent = `${Math.round(v * 100)}%`;
}

function syncOverlayDimmers() {
    document.querySelectorAll('.overlay-row').forEach(updateDimRow);
}
function removeOverlayLayer(title) {
    delete overlapmaps[title];
}
function removeOverlayLayerAll() {
    overlapmaps = {};
}

const includeShip = (ship) => true;

const notificationContainer = document.getElementById("notification-container");

// https://stackoverflow.com/questions/51805395/navigator-clipboard-is-undefined
async function copyClipboard(t) {
    try {
        await copyToClipboard(t);
    } catch (error) {
        showDialog("Action", "No privilege for program to copy to the clipboard. Please select and copy (CTRL-C) the following string manually: " + t);
        return false;
    }
    return true;
}

const SETTINGS_TAB_GROUPS = [
    { title: "General", subs: [["System", "General"]] },
    {
        title: "Map", subs: [
            ["Map", "General"], ["Ship Outline", "Ships"], ["Ship Labels", "Labels"],
            ["Tracks", "Tracks"], ["Station Range", "Range"], ["Binary Messages", "Binary"],
            ["Kiosk", "Kiosk"]
        ]
    },
    { title: "Filter", subs: [["Filter", "Vessels"]] },
    { title: "Plots", subs: [["Graphs", "Plots"]] },
    { title: "Table", subs: [["Table", "Table"]] },
];

const settingsPanel = panelLib.create({
    window: document.getElementById("settings"),
    groups: SETTINGS_TAB_GROUPS,
    skipGrouping: ".st-group, .filter-checks, #overlayContainer",
    onOpen: () => {
        updateSettingsTab();
        updateFilterUI();
        syncThemedSettings();
        refreshOverlayCredits();
    },
});

function buildSettingsTabs() {
    settingsPanel.build();
    settingsPanel.bind(
        { get: (k) => settings[k], set: (k, v) => { settings[k] = v; saveSettings(); }, stage: (k, v) => { settings[k] = v; } },
        { captions: {
            circle_scale: (v) => `Width (${parseFloat(v).toFixed(1)})`,
            shipoutline_opacity: (v) => `Opacity (${parseFloat(v).toFixed(2)})`,
            track_opacity: (v) => `Opacity (${Math.round(parseFloat(v) * 100)}%)`,
          },
          onChange: (k, v, el, staged) => {
            if (k === "label_class_background") updateLabelColorRows();
            if (k === "shipcard_style") targetcard.setStyle(v);
            /* while a slider is dragged the styles re-run; the release rebuilds the features */
            if (staged) [markerLayer, planeLayer, shapeLayer, trackLayer, labelLayer].forEach((l) => l.changed());
            else redrawMap();
        } });
    panelLib.adoptClasses(document);   // column headers outside the window too
    document.querySelectorAll(".tablecard_inner table thead > tr > th, .signal-table thead th")
        .forEach((el) => el.classList.add("col-header"));
}

function updateSettingsChevrons() {
    settingsPanel.syncScrollers();
}

function syncThemedSettings() {
    refreshBaseMapRows();
    updateLabelColorRows();
}

// the header icon and the context menu land on the first page, not wherever the panel was left
function openSettings() {
    settingsPanel.open("General");
}

function closeSettings() {
    settingsPanel.close();
}

function closeTableSide() {
    setPanels({ table: false });
}

function setCoordinateFormat(format) {
    settings.coordinate_format = format;
    saveSettings();

    refresh_data();
    shipTableModule?.reset();
}

async function copyCoordinates(m) {
    const pos = vesselPosition(m);
    if (!pos) {
        showNotification("Ship not found", "error");
        return;
    }
    if (await copyClipboard(pos.lat + "," + pos.lon)) showNotification("Coordinates copied to clipboard", "success");
}

const shapeStyleFunction = ui.markers.hull;
const trackStyleFunction = ui.markers.track;
const markerStyle = ui.markers.marker;
const planeStyle = ui.markers.plane;
const buildLabelText = ui.markers.labelText;
const labelStyle = ui.markers.label((feature) => decodeHTMLEntities('ship' in feature
    ? (feature.ship.shipname || feature.ship.mmsi.toString())
    : (feature.plane.callsign || getICAO(feature.plane))));
const hoverCircleStyleFunction = ui.markers.hoverRing;
const selectCircleStyleFunction = ui.markers.selectRing;

const markerVector = new ol.source.Vector({
    features: []
})

const shapeVector = new ol.source.Vector({
    features: []
});

const extraVector = new ol.source.Vector({
    features: []
});

const trackVector = new ol.source.Vector({
    features: []
});

const labelVector = new ol.source.Vector({
    features: []
});

const planeVector = new ol.source.Vector({
    features: []
});

const markerLayer = new ol.layer.Vector({
    source: markerVector,
    style: markerStyle
})

const binaryLayer = binary.binaryLayer;

const planeLayer = new ol.layer.Vector({
    source: planeVector,
    style: planeStyle,
    visible: false
})

const shapeLayer = new ol.layer.Vector({
    source: shapeVector,
    style: shapeStyleFunction
});

const extraLayer = new ol.layer.Vector({
    source: extraVector
});

const trackLayer = new ol.layer.Vector({
    source: trackVector,
    style: trackStyleFunction
});

const rangeLayer = range.rangeLayer;

const labelLayer = new ol.layer.Vector({
    source: labelVector,
    style: labelStyle,
    declutter: settings.labels_declutter ?? true
});

let shapeFeatures = {};
let markerFeatures = {};

let stationFeature = undefined;
let hoverCircleFeature = undefined;
let selectCircleFeature = undefined;

async function fetchJSON(l, m) {
    let response;
    try {
        response = await fetch(l + "?" + m);
    } catch (error) {
        showDialog("Error", error);
        return null;
    }
    if (!response.ok) {
        showDialog("Error", "Server returned " + response.status);
        return null;
    }
    return response.text();
}

function objectToTableHtml(obj, copyContext) {
    let tableHtml = '<table class="kv-table">';
    for (let key in obj) {
        let value = obj[key];
        if (Array.isArray(value) || (value !== null && typeof value === 'object')) value = JSON.stringify(value);
        if (value == null) value = '';
        const safeKey = sanitizeString(String(key));
        const safeVal = sanitizeString(String(value));
        const empty = safeVal === '';
        const copyAttrs = copyContext && !empty ? ` data-on-contextmenu="showNMEAContextCopy" data-copy="${safeVal}"` : '';
        const cell = empty ? '<span class="kv-empty">-</span>' : safeVal;
        tableHtml += `<tr><td class="kv-key">${safeKey}</td><td class="kv-value"${copyAttrs}>${cell}</td></tr>`;
    }
    return tableHtml + "</table>";
}

async function showJSONTableDialog(url, m, copyContext) {
    const s = await fetchJSON(url, m);
    if (s == null) return;

    let obj;
    try {
        obj = JSON.parse(s);
    } catch (error) {
        showDialog("Error", "Invalid response from server");
        return;
    }
    // api/message decodes the stored NMEA on request and returns an array
    if (Array.isArray(obj)) obj = obj[0] || {};
    showDialogPlain(objectToTableHtml(obj, copyContext));
}

async function showNMEA(m) {
    if (config.features.save_messages) {
        await showJSONTableDialog("api/message", m + "&receiver=" + activeReceiver, true);
    } else if (config.features.managed) {
        showDialog("Error", 'Enable the "Msgs" setting in the viewer configuration of the control panel.');
    } else {
        showDialog("Error", 'Please enable "-N MSG on" in AIS-catcher settings.');
    }
}

async function showVesselDetail(m) {
    await showJSONTableDialog("api/vessel", m + "&receiver=" + activeReceiver, false);
}

async function copyText(m) {
    if (await copyClipboard(m)) showNotification("Content copied to clipboard", "success");
}


const EXT_LINKS = {
    aiscatcher:    id => `https://www.aiscatcher.org/ship/details/${id}`,
    google:        id => `https://www.google.com/search?q=${id}`,
    vesselfinder:  id => `https://www.vesselfinder.com/vessels/details/${id}`,
    aishub:        id => `https://www.aishub.net/vessels?Ship[mmsi]=${id}`,
    planespotters: id => `https://www.planespotters.net/hex/${getICAOFromHexIdent(id)}`,
    adsbexchange:  id => `https://globe.adsbexchange.com/?icao=${getICAOFromHexIdent(id)}`,
    flightaware:   id => `https://flightaware.com/live/modes/${getICAOFromHexIdent(id)}/redirect`,
};
function openExt(key, id) { window.open(EXT_LINKS[key](id)); }

const contextMenu = document.getElementById("context-menu");

let replayPausedByMenu = false;

ui.menu.add([
    { action: "replayToggleLabels", label: "Ship labels", icon: "label", group: "replay", tags: ["ctx-replay-map", "ctx-replay-ship"], check: true },
    ...replay.speeds().map((sp) => ({ action: "replaySetSpeed", label: sp + "\u00d7", group: "replay", tags: ["ctx-replay-map", "ctx-replay-ship"], check: true, data: { speed: String(sp) } })),
]);

function hideContextMenu() {
    ui.menu.close();
}

function showContextMenu(event, mmsi, type, context, anchorEl) {

    if (event && event.preventDefault) {
        event.preventDefault();
        event.stopPropagation();
    }

    context_mmsi = mmsi;
    context_type = type;

    // reading a menu over a moving fleet means the vessel it refers to has
    // sailed on by the time you pick an item
    if (replay.isPlaying()) {
        replay.pause();
        replayPausedByMenu = true;
    }

    if (context.includes('object')) context.push(type);
    if (context.includes('object-map')) context.push(type + "-map");

    const unpinCovered = isFollowing(context_mmsi) || (context.includes("station") && isFollowing("STATION"));
    const unpinContext = context.includes("ctx-map") || context.includes("ctx-replay-map");
    document.getElementById("ctx_unpin").innerText =
        isFollowing("STATION") ? "Unfollow station" : "Unfollow " + vesselLabel(settings.center_point);
    document.getElementById("ctx_follow").innerText = isFollowing(context_mmsi) ? "Unfollow vessel" : "Follow vessel";
    document.getElementById("ctx_follow_station").innerText = isFollowing("STATION") ? "Unfollow station" : "Follow station";
    document.getElementById("ctx_trackcutoff").innerText = trackCutoff ? "Tracks from " + trackCutoffLabel() : "Tracks from now";

    const hidden = (el) =>
        (el.classList.contains("ctx-realtime") && !config.features.realtime) ||
        (el.classList.contains("ctx-replay") && !config.features.replay) ||
        (el.classList.contains("noandroid") && isAndroid()) ||
        (el.classList.contains("android") && !isAndroid()) ||
        (el.classList.contains("nokiosk") && isKiosk()) ||
        (el.classList.contains("kiosk") && !isKiosk()) ||
        (el.classList.contains("ctx-noalltracks") && settings.show_all_tracks) ||
        (el.classList.contains("ctx-removealltracks") && !(settings.show_all_tracks || marker_tracks.size > 0 || trackCutoff)) ||
        (el.classList.contains("ctx-selectedtracks") && (settings.show_all_tracks || marker_tracks.size === 0)) ||
        (el.id === "ctx_menu_unpin" && !(settings.fix_center && unpinContext && !unpinCovered));

    ui.menu.open({
        tags: context,
        show: (el, byTag) => byTag && !hidden(el),
        anchor: anchorEl,
        center: context.includes("center"),
        x: event ? event.pageX : 0,
        y: event ? event.pageY : 0,
        onClose: () => {
            if (replayPausedByMenu) {
                replayPausedByMenu = false;
                replay.play();
            }
        },
    });
}

let dialogModal = null;

function showDialog(title, message, hideTitle) {
    if (!dialogModal) {
        dialogModal = components.modal({
            id: "dialog-box", cardClass: "modal-fit", bodyClass: "dialog-message",
            // some callers widen the card for their content; every close path resets it
            onClose: () => { dialogModal.card.style.maxWidth = ""; },
        });
        dialogModal.root.classList.add("scrim-strong");
    }
    dialogModal.card.classList.toggle("dialog-hide-title", !!hideTitle);
    dialogModal.setTitle(title || "");
    dialogModal.body.innerHTML = message;
    dialogModal.open();
}

function showDialogPlain(message) {
    showDialog("", message, true);
}

function closeDialog() {
    if (dialogModal) dialogModal.close();
}

function showNotification(message, type = "info", duration) {
    (type === "error" ? console.error : console.log)("[notification] " + message);
    return components.toast(type, message, { duration, container: "notification-container" });
}

function headerClick() {
    window.open("https://www.aiscatcher.org");
}

function openWebControl() {
    if (config.webcontrol_http) {
        window.open(config.webcontrol_http, '_blank');
    }
}

function updateMapLayer() {
    if (activeTileLayer) {
        setMapOpacity();
        triggerMapLayer();
    }
}

function setMap(kind, key) {
    settings[BASEMAP_KEYS[kind]] = key;
    triggerMapLayer();
    saveSettings();
}

function triggerMapLayer() {

    if (activeTileLayer)
        activeTileLayer.setVisible(false);

    const key = settings.dark_mode ? settings.map_night : settings.map_day;
    activeTileLayer = key in basemaps ? basemaps[key] : basemaps[Object.keys(basemaps)[0]];
    if (!activeTileLayer) return;

    activeTileLayer.setVisible(true);

    for (const overlay of settings.map_overlay) {
        if (overlay in overlapmaps) overlapmaps[overlay].setVisible(true);
    }

    document.getElementById("map_attributions").innerHTML = attributionHTML(activeTileLayer);

    flashAttribution();
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

function opacityOf(bag, title) {
    const v = (bag || {})[title];
    return Number(v === undefined ? settings.map_opacity : v);
}

function getLayerOpacity(title) {
    return opacityOf(settings.layer_opacity, title);
}

function setLayerOpacity(title, value) {
    settings.layer_opacity[title] = Number(value);
    overlapmaps[title]?.setOpacity(Number(value));
}

function getBaseOpacity(title) {
    return opacityOf(settings.basemap_opacity, title);
}

function setMapOpacity() {
    for (let key in basemaps)
        basemaps[key].setOpacity(getBaseOpacity(key));

    for (let key in overlapmaps)
        overlapmaps[key].setOpacity(getLayerOpacity(key));

    syncOverlayDimmers();
}

let clickTimeout = undefined;
const handleClick = function (pixel, target, event) {
    const feature = target.closest('.ol-control') ? undefined : map.forEachFeatureAtPixel(pixel,
        function (feature) { if ('ship' in feature || 'plane' in feature || 'link' in feature || 'binary' in feature || 'replayMmsi' in feature) { return feature; } }, { hitTolerance: 10 });

    if (clickTimeout) {
        clearTimeout(clickTimeout);
        clickTimeout = null;
        if (!feature) return;
    }

    let included = feature && 'ship' in feature && feature.ship.mmsi in shipsDB;

    if (event.originalEvent.shiftKey || measure.isActive()) {
        measure.handleMapClick(included ? feature.ship.mmsi : null, () => ol.proj.toLonLat(map.getCoordinateFromPixel(pixel)));
        return;
    }

    if (feature && 'link' in feature && !included) {
        window.open(feature.link, '_blank');
    } else if (feature && feature.station_mmsi && feature.station_mmsi in shipsDB) {
        closeDialog();
        closeSettings();
        showTargetcard('ship', feature.station_mmsi, pixel);
        return;
    } else if (feature && feature.binary === true && !feature.is_associated) {
        closeDialog();
        closeSettings();
        binary.click(feature);
        return;
    } else if (feature && 'replayMmsi' in feature) {
        closeDialog();
        closeSettings();
            showContextMenu(event.originalEvent, feature.replayMmsi, 'ship', ["ctx-replay-ship"]);
        return;
    } else if (feature && 'ship' in feature) {
        closeDialog();
        closeSettings();
        showTargetcard('ship', feature.ship.mmsi, pixel);
    }
    else if (feature && 'plane' in feature) {
        closeDialog();
        closeSettings();
        showTargetcard('plane', feature.plane.hexident, pixel);
    }
    else {
        clickTimeout = setTimeout(function () {
            showTargetcard(null, null);
            clickTimeout = null;
        }, 300);
    }
};

function initMap() {
    tooltipLib.create({ selector: ".map-pill .map-button" }).prepare();

    map = new ol.Map({

        target: 'map',
        view: new ol.View({
            center: ol.proj.fromLonLat([settings.lon || 0, settings.lat || 0]),
            zoom: settings.zoom || 6,
            enableRotation: false,
        }),
        controls: []
    })

    for (let [key, value] of Object.entries(basemaps)) {
        map.addLayer(value);
        value.setVisible(false);
    }

    for (let [key, value] of Object.entries(overlapmaps)) {
        map.addLayer(value);
        value.setVisible(false);
    }

    [trackLayer, rangeLayer, shapeLayer, binaryLayer, markerLayer, labelLayer, extraLayer, measure.measureVector,
     replay.hullLayer, replay.markerLayer].forEach(layer => {
        map.addLayer(layer);
    });

    triggerMapLayer();

    map.on('movestart', function () {
        stopHover();
    });

    map.on('moveend', function (evt) {
        binary.viewChanged(map.getView().getZoom());
        debouncedSaveMapView();
        debouncedDrawMap();
    });

    map.on('pointermove', function (evt) {
        if (evt.dragging) return;
        const pixel = map.getEventPixel(evt.originalEvent);
        handlePointerMove(pixel, evt.originalEvent.target);
    });

    map.on('click', function (evt) {
        handleClick(evt.pixel, evt.originalEvent.target, evt);
    });

    map.getTargetElement().addEventListener('pointerleave', function () {
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

    for (const kind of Object.keys(BASEMAP_KEYS)) {
        const sel = baseMapSelect(kind);
        sel.innerHTML = '';
        Object.keys(basemaps).forEach(key => {
            const option = document.createElement("option");
            option.value = key;
            option.textContent = key;
            option.title = attributionPlain(basemaps[key]) || key;
            sel.appendChild(option);
        });
        sel.addEventListener('change', function () {
            setMap(kind, this.value);
            refreshBaseMapRows();
        });
    }
    refreshBaseMapRows();

    Object.keys(overlapmaps).forEach(addOverlayCheckbox);

    setMapOpacity();
}

function toggleLabel() {
    if (settings.show_labels == "never") {
        settings.show_labels = "always";
    } else
        settings.show_labels = "never";

    saveSettings();
    redrawMap();
}

function setMetrics(s, notify = true) {
    if (s.toUpperCase() == "DEFAULT") settings.metric = "DEFAULT";
    else if (s.toUpperCase() == "METRIC") settings.metric = "SI";
    else if (s.toUpperCase() == "IMPERIAL") settings.metric = "IMPERIAL";
    else settings.metric = "DEFAULT";

    if (notify) showNotification("Switched units to " + s);
    saveSettings();

    refresh_data();
    shipTableModule?.reset();
}

function setTableIcon(s) {
    settings.table_shiptype_use_icon = s;
    saveSettings();

    refresh_data();
    shipTableModule?.reset();
}

function getMetrics() {
    if (settings.metric == "DEFAULT") return "Default";
    if (settings.metric == "SI") return "Metric";
    if (settings.metric == "IMPERIAL") return "Imperial";
    return "Default";
}

const BUCKET_ELEMENT = {
    am: "statcard_moving",
    as: "statcard_stationary",
    bm: "statcard_class_b_moving",
    bs: "statcard_class_b_stationary",
    aton: "statcard_aton",
    base: "statcard_station",
    sarte: "statcard_sarte",
    air: "statcard_heli",
};

function bucketChips() {
    return filter.BUCKETS.map((b) => ({
        ...b,
        label: b.label + " \u2014 click to show or hide",
        action: "toggleFilterBucket",
        countId: BUCKET_ELEMENT[b.id],
    }));
}

function buildStatcard() {
    const box = document.querySelector("#statcard .statcard_inner");
    if (!box || box.children.length) return;

    for (const chip of bucketChips()) box.appendChild(bucketChip(chip, chip.countId).item);
}

function shipVisible(entry) {
    if (entry.show === undefined) entry.show = filter.shipPasses(entry.raw);
    return entry.show;
}

function countShips() {
    const buckets = {};
    let total = 0, shown = 0;

    for (const key in shipsDB) {
        const entry = shipsDB[key];
        const b = filter.bucketOf(entry.raw);
        buckets[b] = (buckets[b] || 0) + 1;
        total++;
        if (shipVisible(entry)) shown++;
    }
    setShipCounts({ total, shown, buckets });
}

function updateMarkerCountTooltip() {
    for (const [id, elementId] of Object.entries(BUCKET_ELEMENT)) {
        if (shipsDB == null) {
            document.getElementById(elementId).innerHTML = "";
            continue;
        }
        const v = shipCounts.buckets[id] || 0;
        setCount(elementId, v);
        const item = document.getElementById(elementId)?.closest(".stat-item");
        if (item) {
            item.dataset.zero = v === 0 ? "true" : "false";
            item.classList.toggle("stat-off", filter.isActive() && filter.isHidden("bucket", id));
        }
    }
}

function updateTableSort(event, dataset, header) {
    settings.tableside_column = header.getAttribute("data-column");
    settings.tableside_order = tableLib.nextOrder(header);

    saveSettings();
    tablePage = 0;
    updateSortMarkers();
    updateTablecard();
}

function updateSortMarkers() {
    tableLib.markSort(document.getElementById("tableside"),
        settings.tableside_column, settings.tableside_order);
}

const compareString = tableLib.compareString;
const compareNumber = (a, b) => tableLib.compareNumber(a, b, settings.tableside_order);

document.getElementById('shipSearchSide').addEventListener('input', () => { tablePage = 0; updateTablecard(); });

const TABLE_STALE_SEC = 600;

let tablePage = 0;
let tablePerPage = 0;
let tableRedrawing = false;

function tablePageSize() {
    return tableLib.pageSize(
        document.querySelector(".tablecard_inner"),
        document.querySelector(".mytable thead"),
        document.querySelector("#tablecardBody tr"));
}

function tableRowsFitting() {
    return tableLib.rowsFitting(
        document.querySelector(".tablecard_inner"),
        document.querySelectorAll("#tablecardBody tr"));
}

function renderTablePager(total, pages) {
    tableLib.renderPager(document.getElementById("tablePager"), tablePage, pages, total);
}

function turnTablePage(step) {
    tablePage += step;
    updateTablecard();
}

function updateTablecard() {
    if (!document.getElementById("tableside").classList.contains("active")) return;

    const tableBody = document.getElementById("tablecardBody");
    tableBody.innerHTML = "";

    if (shipsDB == null) return;

    let shipKeys = Object.keys(shipsDB);

    let column = settings.tableside_column;
    let order = settings.tableside_order;

    const sortFunctions = {
        flag: (a, b) => compareString(shipsDB[a].raw.country, shipsDB[b].raw.country),
        shipname: (a, b) => compareString(getShipName(shipsDB[a].raw), getShipName(shipsDB[b].raw)),
        distance: (a, b) => compareNumber(shipsDB[a].raw.distance, shipsDB[b].raw.distance),
        speed: (a, b) => compareNumber(shipsDB[a].raw.speed, shipsDB[b].raw.speed),
        type: (a, b) => compareNumber(shipsDB[a].raw.shipclass, shipsDB[b].raw.shipclass),
        last_signal: (a, b) => compareNumber(shipsDB[b].raw.last_signal, shipsDB[a].raw.last_signal),
    };

    if (column in sortFunctions) {
        shipKeys.sort((keyA, keyB) => {
            const comparisonResult = sortFunctions[column](keyA, keyB);
            return order === "ascending" ? comparisonResult : -comparisonResult;
        });
    }

    const nameQuery = document.getElementById('shipSearchSide').value.toLowerCase();

    document.getElementById("table_dist_unit").textContent = getDistanceUnit();
    document.getElementById("table_spd_unit").textContent = getSpeedUnit();

    const shown = shipKeys.filter((key) => {
        if (!(key in shipsDB) || !shipVisible(shipsDB[key])) return false;
        if (!nameQuery) return true;
        return String(getShipName(shipsDB[key].raw) || shipsDB[key].raw.mmsi).toLowerCase().includes(nameQuery);
    });

    const perPage = tablePerPage || tablePageSize();
    if (!perPage) {
        if (!tableRedrawing) {
            tableRedrawing = true;
            // re-enter only if a height exists by then; a panel that stays too
            // short waits for the next resize instead of retrying every frame
            requestAnimationFrame(() => { tableRedrawing = false; if (tablePageSize()) updateTablecard(); });
        }
        return;
    }

    const pages = Math.max(1, Math.ceil(shown.length / perPage));
    tablePage = Math.min(Math.max(0, tablePage), pages - 1);

    const rows = [];
    for (const key of shown.slice(tablePage * perPage, (tablePage + 1) * perPage)) {
        const ship = shipsDB[key].raw;
        const shipName = String(getShipName(ship) || ship.mmsi);
        const age = clock - ship.last_signal;

        const dist = ship.distance != null ? getDistanceVal(ship.distance) : "-";
        const spd = ship.speed != null ? getSpeedVal(ship.speed) : "-";
        const cls = (isSelectedShip(ship.mmsi) ? " selected" : "") +
            (age > TABLE_STALE_SEC ? " stale" : "");

        rows.push(`<tr class="${cls.trim()}" data-mmsi="${ship.mmsi}">` +
            `<td class="col-name"><span class="table-name">${flagHTML(ship.country, "", getCountryName(ship.country))}` +
            `<span>${shipName}` +
            (ship.repeat > 0 ? `<span class="row-relay" title="Received through another station">&#8635;</span>` : "") +
            `</span></span></td>` +
            `<td class="num col-dist">${dist}</td>` +
            `<td class="num col-spd">${spd}</td>` +
            `<td class="col-type">${getTableShiptype(ship)}</td>` +
            `<td class="num col-last">${getDeltaTimeVal(age)}</td>` +
            `</tr>`);
    }

    tableBody.innerHTML = rows.join('');
    renderTablePager(shown.length, pages);

    const fits = tableRowsFitting();
    if (fits && fits !== perPage && rows.length === perPage && !tableRedrawing) {
        tablePerPage = fits;
        tableRedrawing = true;
        updateTablecard();
        tableRedrawing = false;
    }

    tableBody.onmouseover = function(e) {
        const tr = e.target.closest('tr[data-mmsi]');
        if (tr) startHover('ship', parseInt(tr.dataset.mmsi));
    };
    tableBody.onmouseout = function(e) { stopHover(); };
    tableBody.onclick = function(e) {
        const tr = e.target.closest('tr[data-mmsi]');
        if (tr) showTargetcard('ship', parseInt(tr.dataset.mmsi));
    };
    tableBody.oncontextmenu = function(e) {
        const tr = e.target.closest('tr[data-mmsi]');
        if (tr) showContextMenu(e, parseInt(tr.dataset.mmsi), "ship", ["object", "object-map"]);
    };
}

function isSelectedShip(mmsi) {
    return card_type === "ship" && mmsi == card_mmsi;
}

function syncTableSelection() {
    document.querySelectorAll('#tablecardBody tr[data-mmsi]').forEach(tr =>
        tr.classList.toggle('selected', isSelectedShip(tr.dataset.mmsi)));
}

function setCount(id, value) {
    document.getElementById(id).innerText = compactCount(value);
}

function updateMarkerCount() {
    if (shipsDB == null) {
        setCount("markerCount", 0);
        return;
    }
    if (replay.isActive()) return;

    countShips();
    renderCounts();
}

function renderCounts() {
    const active = filter.isActive();
    setCount("markerCount", active ? shipCounts.shown : shipCounts.total);
    const el = document.getElementById("markerCount");
    if (el) {
        el.classList.toggle("count-filtered", active);
        el.parentElement.title = active
            ? shipCounts.shown + " of " + shipCounts.total + " vessels shown"
            : shipCounts.total + " vessels";
    }

    updateFilterIndicator();
    ticker.setCounts({ shown: shipCounts.shown, total: shipCounts.total, filtered: active, buckets: shipCounts.buckets });

    if (panels.statcard) updateMarkerCountTooltip();
}

function updateMeasureIndicator() {
    const btn = document.getElementById("measure-btn");
    if (!btn) return;

    const n = measure.count();
    btn.classList.toggle("is-active", panels.measure || n > 0);
    btn.title = n ? "Measure distance (" + n + ")" : "Measure distance";
}

function updateFilterIndicator() {
    const btn = document.getElementById("filter-btn");
    if (!btn) return;

    const active = filter.isActive();
    btn.classList.toggle("is-active", active);
    btn.title = active
        ? "Filter: " + filter.describe().join(" \u00b7 ") + " (" + shipCounts.shown + " of " + shipCounts.total + ")"
        : "Filter vessels";

    const tableBtn = document.getElementById("shipTableFilterBtn");
    if (tableBtn) {
        tableBtn.classList.toggle("filter-on", active);
        tableBtn.title = btn.title;
    }

    const note = document.getElementById("filter_empty");
    if (note) note.classList.toggle("visible", active && shipCounts.total > 0 && shipCounts.shown === 0);
}

const filterProbe = { shipclass: 0, speed: 0 };

function applyFilter() {
    saveSettings();
    if (shipsDB != null)
        for (const key in shipsDB) shipsDB[key].show = undefined;

    // whichever view is on screen owns the counters: replay reports its own
    // fleet as it redraws, and updateMarkerCount stands down while it does
    replay.refresh();
    updateMarkerCount();
    redrawMap();
    updateTablecard();
    shipTableModule?.update();
    realtimeModule?.applyVesselFilter();
    updateFilterUI();
}

function openFilterPanel() {
    openSettingsTab("Filter");
}

function openSettingsTab(title) {
    settingsPanel.open(title);
}

function updateFilterUI() {
    const set = (id, value) => {
        const el = document.getElementById(id);
        if (el) {
            if (el.type === "checkbox") el.checked = !!value;
            else el.value = value ?? "";
        }
    };
    set("filter_seen", filter.get("seen"));
    set("filter_validated", filter.get("validated"));
    set("filter_repeated", filter.get("repeated"));

    const seenLabel = document.getElementById("filter_seen_label");
    if (seenLabel) {
        const v = Number(filter.get("seen")) || 0;
        seenLabel.textContent = "Seen within" + (v > 0 ? " (" + v + " min)" : " (any)");
    }

    const summary = document.getElementById("filter_summary");
    if (summary) {
        summary.textContent = filter.isActive()
            ? shipCounts.shown + " of " + shipCounts.total + " vessels shown"
            : "Showing every vessel";
        summary.classList.toggle("filter-on", filter.isActive());
    }

    for (const [kind, box] of Object.entries(FILTER_LISTS)) {
        buildFilterList(box, kind);
        document.querySelectorAll("#" + box + " input").forEach((el) => {
            const id = kind === "bucket" ? el.dataset.id : Number(el.dataset.id);
            el.checked = !filter.isHidden(kind, id);
        });
    }

    syncDualRange("filter_speed", "speed_min", "speed_max", getSpeedConversion, getSpeedUnit());
    syncDualRange("filter_distance", "distance_min", "distance_max", getDistanceConversion, getDistanceUnit());

    const known = station != null && station.lat != null && station.lon != null;
    document.getElementById("filter_distance")?.closest("section")?.classList.toggle("st-off", !known);
}

const FILTER_LISTS = { bucket: "filter_senders", class: "filter_classes", status: "filter_statuses" };

function buildFilterList(boxId, kind) {
    const box = document.getElementById(boxId);
    if (!box || box.dataset.built) return;
    box.dataset.built = "1";
    box.innerHTML = filter.LISTS[kind].items().map((item) => {
        const icon = item.pos
            ? '<span class="' + (item.icon || "shipicon") + '" style="' +
              components.chipStyle({ ...item, scale: item.scale || 1.15 }) + '"></span>'
            : "";
        return '<label class="filter-check"><input type="checkbox" data-on-change="toggleFilterItem"' +
            ' data-kind="' + kind + '" data-id="' + item.id + '">' + icon +
            "<span>" + item.label + "</span></label>";
    }).join("");
}

function buildDualRange(el) {
    if (el.dataset.built) return;
    el.dataset.built = "1";
    const { min, max, step } = el.dataset;
    el.innerHTML =
        '<div class="dr-track"><div class="dr-fill"></div>' +
        '<input type="range" class="dr-min" min="' + min + '" max="' + max + '" step="' + step + '">' +
        '<input type="range" class="dr-max" min="' + min + '" max="' + max + '" step="' + step + '"></div>';

    const lo = el.querySelector(".dr-min"), hi = el.querySelector(".dr-max");
    const commit = () => {
        const key = el.dataset.key;
        const a = Number(lo.value), b = Number(hi.value);
        setFilterValue(key + "_min", a <= Number(min) ? null : a);
        setFilterValue(key + "_max", b >= Number(max) ? null : b);
    };
    lo.addEventListener("input", () => { if (Number(lo.value) > Number(hi.value)) hi.value = lo.value; paintDualRange(el); });
    hi.addEventListener("input", () => { if (Number(hi.value) < Number(lo.value)) lo.value = hi.value; paintDualRange(el); });
    lo.addEventListener("change", commit);
    hi.addEventListener("change", commit);
}

function paintDualRange(el) {
    const lo = el.querySelector(".dr-min"), hi = el.querySelector(".dr-max");
    const min = Number(el.dataset.min), max = Number(el.dataset.max);
    const a = Number(lo.value), b = Number(hi.value);
    const span = max - min || 1;
    const fill = el.querySelector(".dr-fill");
    fill.style.left = ((a - min) / span) * 100 + "%";
    fill.style.right = ((max - b) / span) * 100 + "%";

    const title = document.getElementById(el.dataset.title);
    if (title) title.textContent = el.dataset.label + " (" + rangeText(el, a, b, min, max) + ")";
}

function rangeText(el, a, b, min, max) {
    const unit = el._unit || "";
    const n = (v) => Math.round(el._convert ? el._convert(v) : v);
    if (a <= min && b >= max) return "any";
    if (b >= max) return n(a) + " " + unit + "+";
    if (a <= min) return "up to " + n(b) + " " + unit;
    return n(a) + " - " + n(b) + " " + unit;
}

function syncDualRange(id, minKey, maxKey, convert, unit) {
    const el = document.getElementById(id);
    if (!el) return;
    buildDualRange(el);
    el._convert = convert;
    el._unit = unit;
    el.querySelector(".dr-min").value = filter.get(minKey) ?? el.dataset.min;
    el.querySelector(".dr-max").value = filter.get(maxKey) ?? el.dataset.max;
    paintDualRange(el);
}

function setFilterValue(key, value) {
    filter.set(key, value);
    applyFilter();
}

function resetFilter() {
    filter.reset();
    applyFilter();
}

// ─── map panels ──────────────────────────────────────────────────────────────
const panels = {
    targetcard: false,
    statcard: false,
    table: false,
    measure: false,
    replay: false,
};

function normalisePanels() {
    if (panels.replay) {
        panels.table = false;
        panels.measure = false;
        closeSettings();
    }

    // both of these take the space, and the position, the targetcard occupies
    if ((panels.replay || panels.measure) && panels.targetcard) showTargetcard(null, null);
}

function setPanels(patch, opts) {
    Object.assign(panels, patch);
    normalisePanels();
    applyPanels(opts);
}

function tickerWanted() {
    return !!settings.ticker && !panels.replay && !isKiosk() && settings.tab === "map";
}

let panelEls = null;

function panelElements() {
    if (panelEls) return panelEls;

    panelEls = {
        targetcard: document.getElementById("targetcard"),
        statcard: document.getElementById("statcard"),
        measure: document.getElementById("measurecard"),
        replay: document.getElementById("replaybar"),
        table: document.getElementById("tableside"),
        countersBtn: document.getElementById("counters-btn"),
        tableBtn: document.getElementById("table-btn"),
        popovers: [...document.querySelectorAll("#targetcard .card-popover")],
    };
    return panelEls;
}

// `reposition: false` when the caller places the targetcard itself straight after.
// showTargetcard knows the pixel that was clicked and this does not - and it must
// place the card after filling it, since the placement measures its height.
function applyPanels(opts = {}) {
    const el = panelElements();
    const tickerOn = tickerWanted();

    el.targetcard.classList.toggle("visible", panels.targetcard);
    el.statcard.style.display = panels.statcard ? "block" : "none";
    el.measure.classList.toggle("visible", panels.measure);
    el.replay.classList.toggle("visible", panels.replay);
    el.table.classList.toggle("active", panels.table);

    document.body.classList.toggle("replay-open", panels.replay);
    document.body.classList.toggle("ticker-open", tickerOn);
    document.body.classList.toggle("ticker-bottom", !!settings.ticker_bottom);
    document.body.classList.toggle("table-open", panels.table);

    applyToolbarMode();
    ui.chrome.invalidate();
    /* the map pane narrows for the panel — OpenLayers has to be told */
    if (map) mapui.trackResize(map);

    el.countersBtn?.classList.toggle("is-active", panels.statcard);
    el.tableBtn?.classList.toggle("is-active", panels.table);
    updateMeasureIndicator();

    if (!panels.targetcard) ui.card.popover.closeAll();
    if (!panels.statcard) resetCardPosition(el.statcard);
    if (!panels.measure) resetCardPosition(el.measure);

    ticker.setEnabled(tickerOn);

    if (panels.targetcard && opts.reposition !== false) positionAside(undefined, el.targetcard);
}

function resetCardPosition(el) {
    if (!el || !el.style.left) return;

    for (const prop of ["left", "top", "right", "bottom"]) el.style.removeProperty(prop);
}

function toggleStatcard() {
    if (!panels.statcard) updateMarkerCountTooltip();
    setPanels({ statcard: !panels.statcard });
}

function toggleTicker() {
    settings.ticker = !settings.ticker;
    saveSettings();
    applyPanels();
}

function toggleTablecard() {
    if (!panels.table && window.innerWidth < 800) {
        settings.tab = "ships";
        selectTab();
        return;
    }

    setPanels({ table: !panels.table });
    updateTablecard();
}

function hideTablecard() {
    if (panels.table) toggleTablecard();
}

function setFading(b) {
    if (b != settings.fading) {
        toggleFading();
    }
}

function toggleFading() {
    settings.fading = !settings.fading;
    saveSettings();
    redrawMap();
}

const GRAPH_KINDS = {
    signal: { setting: 'show_signal_graphs', header: 'Signal Level' },
    ppm: { setting: 'show_ppm_graphs', header: 'Frequency Shift' }
};

function setGraphVisibility(type, show, save = true) {
    const kind = GRAPH_KINDS[type];
    if (kind) {
        settings[kind.setting] = show;
        document.querySelectorAll('.graph-panel').forEach(panel => {
            const header = panel.querySelector('header');
            if (header && header.textContent.includes(kind.header)) {
                panel.style.display = show ? '' : 'none';
            }
        });
    }
    if (save) saveSettings();
}

function setPlotAbsoluteTime(on, save = true) {
    settings.plot_absolute_time = on;
    plotsModule?.setAbsoluteTime(on);
    if (save) saveSettings();
}

function toggleGraphVisibility(type) {
    const kind = GRAPH_KINDS[type];
    if (kind) setGraphVisibility(type, !settings[kind.setting]);
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

async function fetchShips(noDoubleFetch = true) {
    if (isFetchingShips && noDoubleFetch) {
        console.log("A fetch operation is already running.");
        return false;
    }

    isFetchingShips = true;
    try {
        return await fetchShipsBody();
    } finally {
        isFetchingShips = false;
    }
}

// The ships table's columns beyond the map's row, pulled only while that tab
// is open: one full pass on opening, then only ships heard since, merged by MMSI.
const tableKeys = ["mmsi", "bearing", "level", "ppm", "count", "msg_type", "last_group", "group_mask", "altitude", "received_stations", "mmsi_type", "region"];
let tableSince = 0;

async function fetchTableRows() {
    try {
        const response = await fetch("api/ships_table.json?receiver=" + activeReceiver + (tableSince > 0 ? "&since=" + tableSince : ""));
        if (!response.ok) return;
        const data = await response.json();
        (data.rows || []).forEach((v) => {
            const s = Object.fromEntries(tableKeys.map((k, i) => [k, v[i]]));
            if (s.mmsi in shipsDB) Object.assign(shipsDB[s.mmsi].raw, s);
        });
        if (data.time) tableSince = data.time - 1;
    } catch (error) {
        console.log("failed loading table rows:", error);
    }
}

async function fetchShipsBody() {
    let ships = {};

    try {
        const response = await fetch("api/ships_array.json?receiver=" + activeReceiver + (shipsSince > 0 ? "&since=" + shipsSince : ""));
        if (!response.ok) {
            console.log("failed loading ships: HTTP " + response.status);
            return false;
        }
        ships = await response.json();
    } catch (error) {
        console.log("failed loading ships: " + error);
        return false;
    }

    const dynamicKeys = [
        "mmsi", "lat", "lon", "distance",
        "heading", "cog", "speed", "status", "age", "flags",
        "shipclass", "country", "binary", "station"
    ];

    const staticKeys = [
        "mmsi", "shipname", "callsign", "destination",
        "shiptype", "imo",
        "to_bow", "to_stern", "to_port", "to_starboard",
        "draught", "eta_month", "eta_day", "eta_hour", "eta_minute",
        "eni", "vendorid", "model", "serial"
    ];

    const serverTime = ships.time || 0;
    const isIncremental = shipsSince > 0;

    if (!isIncremental) {
        setShips({});
        setStation({});
    }

    // Process static data first (name/voyage)
    if (ships.static) {
        ships.static.forEach((v) => {
            const s = Object.fromEntries(staticKeys.map((k, i) => [k, v[i]]));
            s.shipname = sanitizeString(s.shipname || "");
            s.callsign = sanitizeString(s.callsign || "");
            s.destination = sanitizeString(s.destination || "");
            s.eni = sanitizeString(s.eni || "");
            s.vendorid = sanitizeString(s.vendorid || "");
            const mmsi = s.mmsi;
            if (mmsi in shipsDB) {
                Object.assign(shipsDB[mmsi].raw, s);
                shipsDB[mmsi].show = undefined;
            } else {
                shipsDB[mmsi] = { raw: s };
            }
        });
    }

    // Process dynamic data (position/signal)
    if (ships.dynamic) {
        ships.dynamic.forEach((v) => {
            const s = Object.fromEntries(dynamicKeys.map((k, i) => [k, v[i]]));
            s.last_signal = serverTime - (s.age || 0);
            delete s.age;

            const flags = s.flags;
            s.validated = (flags & 3) == 2 ? -1 : flags & 3;
            s.repeat = (flags >> 2) & 3;
            s.virtual_aid = (flags >> 4) & 1;
            s.approx = (flags >> 5) & 1;
            s.channels = (flags >> 6) & 0b1111;
            s.cs_unit = (flags >> 10) & 3;
            s.raim = (flags >> 12) & 3;
            s.dte = (flags >> 14) & 3;
            s.assigned = (flags >> 16) & 3;
            s.display = (flags >> 18) & 3;
            s.dsc = (flags >> 20) & 3;
            s.band = (flags >> 22) & 3;
            s.msg22 = (flags >> 24) & 3;
            s.off_position = (flags >> 26) & 3;
            s.maneuver = (flags >> 28) & 3;

            const mmsi = s.mmsi;
            if (mmsi in shipsDB) {
                Object.assign(shipsDB[mmsi].raw, s);
                shipsDB[mmsi].show = undefined;
            } else {
                shipsDB[mmsi] = { raw: s };
            }
        });
    }

    // Filter ships after merge
    for (const mmsi in shipsDB) {
        if (!(shipFilterOverride ?? includeShip)(shipsDB[mmsi].raw)) {
            delete shipsDB[mmsi];
        }
    }

    if (ships.timeout) shipsTimeout = ships.timeout;

    setShipsSince(serverTime - 1);
    setClock(serverTime);
    filter.setClock(serverTime);

    // periodically expire ships older than timeout
    if (isIncremental && serverTime - shipsLastCleanup > shipsTimeout / 2) {
        for (const mmsi in shipsDB) {
            if (serverTime - shipsDB[mmsi].raw.last_signal > shipsTimeout)
                delete shipsDB[mmsi];
        }
        shipsLastCleanup = serverTime;
    }

    capShipsDB();

    if (Object.hasOwn(ships, "station")) setStation(ships.station);
    drawStation();

    center = {};
    if (String(settings.center_point).toUpperCase() == "STATION") {
        center = station;
    } else if (settings.center_point in shipsDB) {
        let ship = shipsDB[settings.center_point].raw;
        center = { lat: ship.lat, lon: ship.lon };
    }

    tab_title_count = Object.keys(shipsDB).length;
    updateTitle();

    community.pushVesselsToCommunityPopup(ships.dynamic);

    return true;
}

const MAX_SHIPS = 50000;

function capShipsDB() {
    const keys = Object.keys(shipsDB);
    if (keys.length <= MAX_SHIPS) return;

    keys.sort((a, b) => shipsDB[b].raw.last_signal - shipsDB[a].raw.last_signal);
    for (let i = MAX_SHIPS; i < keys.length; i++) delete shipsDB[keys[i]];

    console.log("shipsDB capped at " + MAX_SHIPS + ", dropped " + (keys.length - MAX_SHIPS) + " quiet vessels");
}

async function fetchPlanes() {

    let planes = {};

    try {
        const response = await fetch("api/planes_array.json" + (planesSince > 0 ? "?since=" + planesSince : ""));
        if (!response.ok) {
            console.log("failed loading planes: HTTP " + response.status);
            return false;
        }
        planes = await response.json();
    } catch (error) {
        console.log("failed loading planes: " + error);
        return false;
    }

    const keys = [
        "hexident",
        "lat",
        "lon",
        "altitude",
        "speed",
        "heading",
        "vertrate",
        "squawk",
        "callsign",
        "airborne",
        "nMessages",
        "last_signal",
        "category",
        "level",
        "country",
        "distance",
        "message_types",
        "message_subtypes",
        "group_mask",
        "last_group",
        "bearing"
    ];

    const serverTime = planes.time || 0;
    const isIncremental = planesSince > 0;

    if (!isIncremental) setPlanes({});

    if (planes.values) {
        planes.values.forEach((v) => {
            const p = Object.fromEntries(keys.map((k, i) => [k, v[i]]));

            p.shipclass = ShippingClass.PLANE;
            p.validated = 1;
            p.name = p.callsign || getICAOFromHexIdent(p.hexident);

            const hex = p.hexident;
            if (hex in planesDB) {
                Object.assign(planesDB[hex].raw, p);
            } else {
                planesDB[hex] = { raw: p };
            }
        });
    }

    setPlanesSince(serverTime - 1);

    // Periodically expire planes silently dropped by the server's activity filter.
    if (isIncremental && serverTime - planesLastCleanup > planesTimeout / 2) {
        for (const hex in planesDB) {
            if (serverTime - planesDB[hex].raw.last_signal > planesTimeout)
                delete planesDB[hex];
        }
        planesLastCleanup = serverTime;
    }

    return true;
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

// Plugin API. `action` is either a function (preferred, CSP-clean) or the name
// of an entry in ACTIONS. Inline JS expression strings (the pre-CSP plugin
// style) no longer work — strict CSP forbids them; convert plugins to pass a
// function instead.

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

// we calculate the lat/lon for 1m move in direction of heading
// underlying calculation uses an offset of 100m and then scales down to 1.

function setMapSetting(a, v) {
    settings[a] = v;
    saveSettings();
    if (a === "label_class_background") updateLabelColorRows();
    redrawMap();
}

function updateLabelColorRows() {
    document.querySelectorAll(".label-colors").forEach((s) => { s.hidden = !!settings.label_class_background; });
}

function setTrackHistory(minutes) {
    settings.track_history = minutes;
    saveSettings();

    // deltas only move forwards, so reaching further back needs a full refetch
    const want = trackWindowStart();
    const covered = pathsFrom === 0 || (pathsFrom > 0 && want >= pathsFrom);

    if (covered) {
        redrawMap();
        return;
    }

    setPaths({});
    lastPathFetch = 0;
    setPathsFrom(-1);
    fetchTracks().then(redrawMap);
}

function setTrackClassColor(shipClass, color) {
    settings.track_class_colors[ShippingClass[shipClass]] = color;
    saveSettings();
    redrawMap();
}

function applyColorToAllTracks(color) {
    // Use default blue color if no color provided
    const colorToApply = color || '#12a5ed';
    for (let classKey in ShippingClass) {
        settings.track_class_colors[ShippingClass[classKey]] = colorToApply;
    }
    updateTrackColorInputs();
    saveSettings();
    redrawMap();
}

function updateTrackColorInputs() {
    for (const key of Object.keys(ShippingClass)) {
        document.getElementById(`settings_track_${key.toLowerCase()}_color`).value = settings.track_class_colors[ShippingClass[key]];
    }
}

function setTrackColorMode(mode) {
    settings.track_color_mode = mode === "speed" ? "speed" : "class";
    updateTrackColorModeUI();
    saveSettings();
    redrawMap();
}

function setTrackSpeedPalette(key) {
    settings.track_speed_palette = validPalette(key);
    updateSpeedLegend();
    saveSettings();
    redrawMap();
}

function setTrackSpeedMax(knots) {
    settings.track_speed_max = knots;
    updateSpeedLegend();
    saveSettings();
    redrawMap();
}

// Only one of the two coloring blocks is on screen at a time: the per-class
// pickers, or the palette with its scale.
function updateTrackColorModeUI() {
    const speed = settings.track_color_mode === "speed";
    document.querySelectorAll(".track-class-color").forEach((el) => el.classList.toggle("hidden", speed));
    document.querySelectorAll(".track-speed-color").forEach((el) => el.classList.toggle("hidden", !speed));
    updateSpeedLegend();
}

// the scale is read at a glance, so its ticks carry no decimals
const speedWhole = (knots) => Math.round(Number(getSpeedVal(knots)));

function updateSpeedLegend() {
    const bar = document.getElementById("track_speed_legend");
    if (!bar) return;
    bar.style.background = paletteCSS(settings.track_speed_palette);
    document.getElementById("track_speed_scale_label").textContent =
        `Preview (0 - ${speedWhole(settings.track_speed_max)} ${getSpeedUnit()})`;
}

function targetcardselect(e) {
    if (!ui.card.keep.toggle(e)) toggleTargetcardSize();
    saveSettings();
}

function toggleTargetcardSize() {
    if (settings.shipcard_style === "tabs") return;   // the tabbed card stays maximised
    ui.card.setMax(!ui.card.isMax());
    fitTargetcard();
    if (isTargetcardMax()) adjustMapForTargetcard();
}

function syncReceiverUI() {
    const btn = document.getElementById("receiver-btn");
    if (!btn) return;
    btn.classList.toggle("active", activeReceiver !== 0);
    const r = (config.receivers || []).find((x) => x.idx === activeReceiver);
    btn.title = r ? "Receiver: " + r.label : "Select receiver";
}

function updateReceiverSelect(receivers) {
    const wrap = document.getElementById("receiver-btn-wrap");
    if (!wrap) return;
    wrap.style.display = !receivers || receivers.length <= 1 ? "none" : "flex";
    syncReceiverUI();
}

function showReceiverDialog() {
    const receivers = config.receivers || [];
    let html = '<div class="receiver-list">';
    for (const r of receivers) {
        html += `<div class="receiver-option${r.idx === activeReceiver ? " active" : ""}"` +
            ` data-action="selectReceiver" data-idx="${r.idx}">${sanitizeString(r.label)}</div>`;
    }
    showDialog("Select Receiver", html + "</div>");
}

function selectReceiver(idx) {
    closeDialog();
    onReceiverChange(idx);
}

function onReceiverChange(idx) {
    if (replaycardVisible()) toggleReplaycard();
    activeReceiver = parseInt(idx, 10) || 0;
    setShipsSince(0);
    tableSince = 0;
    binary.resetSince();
    range.resetUpdateTime();
    lastPathFetch = 0;
    setPaths({});
    setPathsFrom(-1);
    syncReceiverUI();
    refresh_data();
}

// fetches main statistics from the server
async function fetchStatistics() {
    try {
        const response = await fetch("api/stat.json?receiver=" + activeReceiver);
        if (!response.ok) return;
        return await response.json();
    } catch (error) {
        return;
    }
}

function updateStat(stat, tf) {
    [0, 1, 2, 3].forEach((e) => (document.getElementById("stat_" + tf + "_channel" + e).innerText = stat[tf].channel[e].toLocaleString()));

    document.getElementById("stat_" + tf + "_count").innerText = stat[tf].count.toLocaleString();
    document.getElementById("stat_" + tf + "_dist").innerText = getDistanceVal(stat[tf].dist) + " " + getDistanceUnit();
    document.getElementById("stat_" + tf + "_vessel_count").innerText = stat[tf].vessels.toLocaleString();
    document.getElementById("stat_" + tf + "_msg123").innerText = (stat[tf].msg[0] + stat[tf].msg[1] + stat[tf].msg[2]).toLocaleString();
    document.getElementById("stat_" + tf + "_msg5").innerText = stat[tf].msg[4].toLocaleString();
    document.getElementById("stat_" + tf + "_msg18").innerText = stat[tf].msg[17].toLocaleString();
    document.getElementById("stat_" + tf + "_msg19").innerText = stat[tf].msg[18].toLocaleString();
    document.getElementById("stat_" + tf + "_msg68").innerText = (stat[tf].msg[5] + stat[tf].msg[7]).toLocaleString();
    document.getElementById("stat_" + tf + "_msg1214").innerText = (stat[tf].msg[11] + stat[tf].msg[13]).toLocaleString();
    document.getElementById("stat_" + tf + "_msg24").innerText = stat[tf].msg[23].toLocaleString();
    document.getElementById("stat_" + tf + "_msg4").innerText = stat[tf].msg[3].toLocaleString();
    document.getElementById("stat_" + tf + "_msg9").innerText = stat[tf].msg[8].toLocaleString();
    document.getElementById("stat_" + tf + "_msg21").innerText = (stat[tf].msg[20] + (stat[tf].msg[27] || 0)).toLocaleString();
    document.getElementById("stat_" + tf + "_msg27").innerText = stat[tf].msg[26].toLocaleString();

    let count_other = 0;
    [7, 10, 11, 13, 15, 16, 17, 20, 22, 23, 25, 26].forEach((i) => (count_other += stat[tf].msg[i - 1]));
    document.getElementById("stat_" + tf + "_msgother").innerText = count_other.toLocaleString();
}

function renderDevices(stat) {
    const el = document.getElementById("stat_devices");
    if (!el) return;
    const prod = stat.product || [], vend = stat.vendor || [], ser = stat.serial || [], mod = stat.model || [], rate = stat.sample_rate || [], dlab = stat.device_label || [];
    const n = Math.max(prod.length, vend.length, ser.length, mod.length, rate.length, dlab.length);
    el.replaceChildren();
    for (let i = 0; i < n; i++) {
        let name = dlab[i];
        if (!name) {
            name = prod[i] || mod[i] || "Device " + (i + 1);
            if (ser[i] && ser[i] !== "-") name += " (" + ser[i] + ")";
        }

        const label = document.createElement("span");
        label.textContent = i === 0 ? "Device" : "";
        const icon = document.createElement("i");
        icon.className = "info_icon";
        const value = document.createElement("span");
        value.className = "device-value";
        value.append(name, icon);
        const row = document.createElement("div");
        row.className = "device-row";
        row.title = "Show device details";
        row.append(label, value);
        const engines = String(mod[i] || "").split("\n").filter((e) => e !== "");
        const info = { Device: prod[i], Vendor: vend[i], Serial: ser[i] };
        if (engines.length > 1) engines.forEach((e, k) => (info["Engine " + (k + 1)] = e));
        else info.Engine = engines[0];
        info["Sample rate"] = rate[i];

        row.onclick = () => showDialogPlain(objectToTableHtml(info));
        el.appendChild(row);
    }
}

async function updateStatistics() {
    const stat = await fetchStatistics();

    if (stat) {
        // in bulk....
        const statText = (v) => (Array.isArray(v) ? v.join(", ") : v != null ? v : "");
        ["os", "tcp_clients", "hardware", "build_describe", "build_date", "station"].forEach(
            (e) => (document.getElementById("stat_" + e).textContent = statText(stat[e])),
        );

        renderDevices(stat);

        if (stat.station_link != "") {
            const el = document.getElementById("stat_station");
            el.textContent = "";
            const a = document.createElement("a");
            a.href = isHttpUrl(stat.station_link) ? stat.station_link : "#";
            a.textContent = stat.station;
            el.appendChild(a);
        }

        const statSharingElement = document.getElementById("stat_sharing");
        community.updateSharingState(stat.sharing, stat.sharing_uuid, stat.engine_running);
        const [sharingText, sharingClass] = community.sharingDisplay();
        statSharingElement.innerHTML = `<a href="${stat.sharing_link}" target="_blank" class="${sharingClass}">${sharingText}</a>`;

        document.getElementById("stat_update_time").textContent = Number(refreshIntervalMs / 1000).toFixed(1) + " s";
        let title = document.getElementById("stat_station").textContent;
        if (title != "" && title != null) {
            tab_title_station = title;
            updateTitle();
        }
        document.getElementById("stat_memory").innerText = stat.memory ? Number(stat.memory / 1000000).toFixed(1) + " MB" : "N/A";
        if (stat.track_time != null) {
            const t = stat.track_time;
            let age = "unlimited";
            if (t > 0) {
                const d = Math.floor(t / 86400), h = Math.floor((t % 86400) / 3600), m = Math.floor((t % 3600) / 60), s = t % 60;
                age = [d && d + "d", h && h + "h", m && m + "m", s && s + "s"].filter(Boolean).join(" ");
            }
            const mem = stat.track_memory >= 1024 ? Number(stat.track_memory / 1024).toFixed(0) + " MB" : stat.track_memory + " KB";
            document.getElementById("stat_track").innerText = age + " / " + mem;
        }
        document.getElementById("stat_received").innerText = formatBytes(stat.received);
        document.getElementById("stat_msg_rate").innerText = Number(stat.msg_rate).toFixed(1) + " msg/s";
        document.getElementById("stat_msg_min_rate").innerText = Number(stat.last_minute.count).toFixed(0) + " msg/min";
        document.getElementById("stat_run_time").innerHTML = getDeltaTimeVal(stat.run_time);

        updateStat(stat, "total");
        updateStat(stat, "session");
        updateStat(stat, "last_minute");
        updateStat(stat, "last_hour");
        updateStat(stat, "last_day");

        document.getElementById("stat_total_vessel_count").innerText = "-";
        document.getElementById("stat_session_vessel_count").innerText = stat.vessel_count;

        let outputSection = document.getElementById("output_stats");
        if (!outputSection) return;

        if (stat.outputs && stat.outputs.length > 0) {
            let html = "";
            for (let i = 0; i < stat.outputs.length; i++) {
                html += "<section>";
                const o = stat.outputs[i];
                const s = o.stats;
                const showStatus = o.type !== "UDP" && !o.type.startsWith("HTTP");

                let name = sanitizeString(o.description || o.type);
                if (o.link && isHttpUrl(o.link)) {
                    const href = sanitizeString(o.link);
                    name = `<a href="${href}" target="_blank" rel="noopener" title="${href}" style="color:inherit">${name}` +
                        ` <svg xmlns="http://www.w3.org/2000/svg" width="12" height="12" viewBox="0 -960 960 960" fill="currentColor" style="vertical-align:-1px"><path d="M200-120q-33 0-56.5-23.5T120-200v-560q0-33 23.5-56.5T200-840h280v80H200v560h560v-280h80v280q0 33-23.5 56.5T760-120H200Zm188-212-56-56 372-372H520v-80h320v320h-80v-184L388-332Z"/></svg></a>`;
                }
                html += `<div><span>Output</span><span>${name}</span></div>`;
                if (showStatus) {
                    const connected = s.connected ? "Connected" : "Not connected";
                    html += `<div><span>Status</span><span class="${s.connected ? "status-ok" : "status-bad"}">${connected}</span></div>`;
                }
                html += `<div><span>Bytes out / in</span><span>${formatBytes(s.bytes_out)} / ${formatBytes(s.bytes_in)}</span></div>`;
                if (o.type !== "UDP")
                    html += `<div><span>Connect ok / fail</span><span>${s.connect_ok} / ${s.connect_fail}</span></div>`;
                if (s.reconnects > 0)
                    html += `<div><span>Reconnects</span><span>${s.reconnects}</span></div>`;
                if (s.dropped > 0)
                    html += `<div><span>Dropped</span><span>${s.dropped}</span></div>`;
                html += "</section>";
            }
            outputSection.innerHTML = html;
        } else {
            outputSection.innerHTML = "";
        }
    }
}

function tableRowClick(m) {
    let ship = shipsDB[m].raw;
    if (ship.lat == null || ship.lon == null) return;

    selectMapTab(m);
}

function getTooltipContent(ship) {
    const sub = (ship.shiptype ? getShipTypeShort(ship.shiptype) + ' - ' : '') +
        'received ' + getDeltaTimeVal(clock - ship.last_signal) + ' ago';

    // the vessel is one band wearing its validity, each message card below it
    // another wearing its kind, their bars in one line
    let content = '<div class="tip-band" style="--band: ' + validityBand(ship.validated) + '"><div class="tooltip-card">' +
        flagHTML(ship.country, 'flag-tooltip', getCountryName(ship.country)) +
        '<div>' +
        (getShipName(ship) || ship.mmsi) +
        '<span class="tooltip-dim"> at </span>' + getSpeedVal(ship.speed) + ' ' + getSpeedUnit() +
        '<div class="tooltip-sub">' + sub + '</div>' +
        '</div>' +
        '</div></div>';

    content += binary.shipTooltip(ship);

    return content;
}

function getTooltipContentPlane(plane) {
    const altitude = plane.airborne == 1 ? (plane.altitude ? Math.round(plane.altitude) + ' ft' : '-') : 'ground';
    const speed = plane.speed ? Math.round(plane.speed) : '-';
    return '<div class="tooltip-card">' +
        flagHTML(plane.country, 'flag-tooltip', getCountryName(plane.country)) +
        '<div>' +
        sanitizeString(plane.callsign || getICAO(plane)) +
        '<span class="tooltip-dim"> at </span>' + altitude + '/' + speed + ' kts' +
        '<div class="tooltip-sub">received ' + getDeltaTimeVal(planesSince - plane.last_signal) + ' ago</div>' +
        '</div>' +
        '</div>';
}

const fadeCurve = markersLib.fadeCurve;

function fadeOpacity(age) {
    if (settings.fading == false) return 1;
    return fadeCurve(age);
}

function getShipOpacity(ship) {
    return fadeOpacity(clock - ship.last_signal);
}

function getShipCSSClassAndStyle(ship, opacity = 1) {
    getSprite(ship);
    let style = `opacity: ${opacity};`;
    let scale = settings.icon_scale;

    style += `background-position: -${ship.cx - 0}px -${ship.cy - 0}px; width: 20px; height: 20px; transform: rotate(${ship.rot}rad) scale(${scale});`;

    return { class: "sprites", style: style, hint: ship.hint };
}

function getTableShiptype(ship, opacity = 1) {
    if (ship == null) return "";

    const { class: classValue, style, hint } = getShipCSSClassAndStyle(ship, opacity);
    return settings.table_shiptype_use_icon
        ? `<span class="table-shiptype-icon"><span class="${classValue}" style="${style}" title="${hint}"></span></span>`
        : hint;
}

function syncCircleFeature(feature, raw, mmsi, styleFn) {
    if (feature) {
        if (raw && raw.lon && raw.lat) {
            feature.setGeometry(new ol.geom.Point(ol.proj.fromLonLat([raw.lon, raw.lat])));
            return feature;
        }
        extraVector.removeFeature(feature);
        return undefined;
    }

    if (raw && raw.lon && raw.lat) {
        feature = new ol.Feature(new ol.geom.Point(ol.proj.fromLonLat([raw.lon, raw.lat])));
        feature.setStyle(styleFn);
        feature.mmsi = mmsi;
        extraVector.addFeature(feature);
        return feature;
    }
    return undefined;
}

function updateFocusMarker() {
    const raw = card_type == 'ship' ? shipsDB[card_mmsi]?.raw : card_type == 'plane' ? planesDB[card_mmsi]?.raw : null;
    selectCircleFeature = syncCircleFeature(selectCircleFeature, raw, card_mmsi, selectCircleStyleFunction);
}

function mapFreeBox() {
    return ui.chrome.freeBox(map.getTargetElement());
}

const showTooltipShip = (tooltip, mmsi, pixel, distance, angle = 0) => {

    tooltip.innerHTML = mmsi;
    tooltip.classList.toggle('tooltip-bands', typeof mmsi === 'string' && mmsi.includes('tip-band'));

    if (pixel) {
        const { offsetWidth: tw, offsetHeight: th } = tooltip;
        const at = ui.chrome.alongCourse(mapFreeBox(), tw, th, pixel, distance, angle);

        Object.assign(tooltip.style, {
            left: `${at.left}px`,
            top: `${at.top}px`,
            visibility: 'visible'
        });
    }
};

const stopHover = function () {

    if (!hoverMMSI) return;

    debounceShowHoverTrack.cancel();

    hover_info.style.visibility = 'hidden';
    hover_info.style.left = '0px';
    hover_info.style.top = '0px';

    if (hover_enabled_track) hideTrack(hoverMMSI);

    const dc = hover_feature && ('distancecircle' in hover_feature || 'rangering' in hover_feature);
    const sf = hoverType == 'ship' && hoverMMSI in shapeFeatures;

    setHover(undefined, undefined);
    hover_feature = undefined;
    hover_enabled_track = false;

    if (dc) rangeLayer.changed();

    if (sf)
        shapeLayer.changed();

    updateHoverMarker();
    trackLayer.changed();
}

function showHoverTrack(mmsi) {
    if (mmsi) {
        hover_enabled_track = !trackIsShown(hoverMMSI);
        if (hover_enabled_track) {
            showTrack(mmsi);
        }
    }
}

let attributionTimer = null;
let attributionPinned = false;

function showAttribution(on) {
    const foldout = document.getElementById('map-attribution-foldout');
    clearTimeout(attributionTimer);
    foldout.classList.toggle('visible', on);
}

function flashAttribution() {
    showAttribution(true);
    attributionTimer = setTimeout(() => showAttribution(attributionPinned), 3000);
}

// the panel also flashes for 3s on a basemap change, and a menu tick must not
// report that passing state as something the user switched on
function toggleAttribution() {
    attributionPinned = !attributionPinned;
    showAttribution(attributionPinned);
}

let lastHoverPixel = null;

const startHover = function (type, mmsi, pixel, feature) {

    if (type != 'ship' && type != 'tooltip' && type != 'plane') return;
    lastHoverPixel = pixel;

    if (mmsi !== hoverMMSI || hoverType !== type) {
        stopHover();

        setHover(mmsi, type);
        hover_feature = feature;

        const shipRaw = type == 'ship' ? shipsDB[mmsi]?.raw : null;
        const planeRaw = type == 'plane' ? planesDB[mmsi]?.raw : null;
        if (shipRaw && shipRaw.lon && shipRaw.lat) {
            showTooltipShip(hover_info, getTooltipContent(shipRaw), pixel, 15, shipRaw.cog);
            if (settings.show_track_on_hover && pixel) {
                debounceShowHoverTrack(mmsi);
            }
            if (mmsi in shapeFeatures) {
                shapeFeatures[mmsi].changed();
            }
            trackLayer.changed();
        } else if (planeRaw && planeRaw.lon && planeRaw.lat) {
            showTooltipShip(hover_info, getTooltipContentPlane(planeRaw), pixel, 15, planeRaw.heading);
        }
        else {
            showTooltipShip(hover_info, hoverMMSI, pixel, 0);

            if (hover_feature && ('distancecircle' in hover_feature || 'rangering' in hover_feature))
                rangeLayer.changed();
        }

        updateHoverMarker();
    }
}

// the ring sits on the hovered vessel or plane, or on a hovered map object's marker
function hoverObjectRaw() {
    const g = hoverType == 'tooltip' && hover_feature && hover_feature.getGeometry ? hover_feature.getGeometry() : null;
    if (!g || g.getType() !== 'Point') return null;
    const [lon, lat] = ol.proj.toLonLat(g.getCoordinates());
    return { lon, lat };
}

function updateHoverMarker() {
    const raw = hoverType == 'ship' ? shipsDB[hoverMMSI]?.raw : hoverType == 'plane' ? planesDB[hoverMMSI]?.raw : hoverObjectRaw();
    const had = hoverCircleFeature != undefined;
    hoverCircleFeature = syncCircleFeature(hoverCircleFeature, raw, hoverMMSI, hoverCircleStyleFunction);
    if (had && !hoverCircleFeature) {
        stopHover();
    }
}

const normalizePixel = (coord) => {
    const view = map.getView(),
        projection = view.getProjection(),
        centerX = view.getCenter()[0],
        worldWidth = ol.extent.getWidth(projection.getExtent());

    coord[0] -= Math.floor((coord[0] - centerX) / worldWidth + 0.5) * worldWidth;

    const [x, y] = map.getPixelFromCoordinate(coord),
        [width, height] = map.getSize();

    return [
        Math.max(0, Math.min(width - 1, x)),
        Math.max(0, Math.min(height - 1, y))
    ];
};

function getFeature(pixel, target) {
    const feature = target.closest('.ol-control') ? undefined : map.forEachFeatureAtPixel(pixel,
        function (feature) { if ('ship' in feature || 'plane' in feature || 'tooltip' in feature || 'binary' in feature) { return feature; } }, { hitTolerance: 10 });

    return feature;
}

const handlePointerMove = function (pixel, target) {
    const feature = getFeature(pixel, target);

    if (feature) {
        const geometry = feature.getGeometry();
        const geometryType = geometry.getType();
        if (geometryType === 'Point') {
            const coordinate = geometry.getCoordinates();
            pixel = normalizePixel(coordinate);
        }
    }

    if (feature && 'ship' in feature && feature.ship.mmsi in shipsDB) {
        const mmsi = feature.ship.mmsi;
        startHover('ship', mmsi, pixel, feature);
    }
    else if (feature && 'plane' in feature && feature.plane.hexident in planesDB) {
        const hexident = feature.plane.hexident;
        startHover('plane', hexident, pixel, feature);
    }
    else if (feature && feature.station_mmsi && feature.station_mmsi in shipsDB) {
        const ship = shipsDB[feature.station_mmsi].raw;
        startHover('tooltip', binary.stationTooltip(feature, getTooltipContent(ship)), pixel, feature);
    }
    else if (feature && feature.binary === true) {
        if (feature.is_associated && feature.binary_mmsi && feature.binary_mmsi in shipsDB) {
            startHover('ship', feature.binary_mmsi, pixel, feature);
            return;
        } else if (feature.binary_object) {
            startHover('tooltip', binary.markerTooltip(feature), pixel, feature);
        } else {
            startHover('tooltip', "Binary Message", pixel, feature);
        }
    }
    else if (feature && 'tooltip' in feature) {
        startHover('tooltip', feature.tooltip, pixel, feature);
    } else if (hoverMMSI || hoverType) {
        stopHover();
    }

    measure.updateMeasureEnd(
        feature && 'ship' in feature ? feature.ship.mmsi : null,
        () => ol.proj.toLonLat(map.getCoordinateFromPixel(pixel)));
};

function persistSettings() {
    if (map !== undefined) {
        const center = ol.proj.toLonLat(map.getView().getCenter());
        settings.lat = center[1];
        settings.lon = center[0];
        settings.zoom = map.getView().getZoom();
    }
    settingsStore.save();
    community.notifyCommunityPopupView();
    updateMapURL();
}

const debouncedSaveMapView = debounce(persistSettings, 250);
const debouncedDrawMap = debounce(redrawMap, 250);
const debounceShowHoverTrack = debounce(showHoverTrack, 250);

function updateMapURL() {
    if (isAndroid()) return;

    let view = map.getView();
    let center = ol.proj.toLonLat(view.getCenter()); // Converts the center coordinates to [lon, lat]
    let newURL = window.location.href.split("?")[0] + "?lat=" + center[1].toFixed(4) + "&lon=" + center[0].toFixed(4) + "&zoom=" + view.getZoom().toFixed(2) + "&tab=" + settings.tab;
    history.replaceState(null, null, newURL);
}

function saveSettings() {
    // the tabbed card neither folds nor keeps rows: those stay as classic left them
    if (settings.shipcard_style !== "tabs") {
        settings.targetcard_max = isTargetcardMax();
        settings.targetcard_rows = ui.card.keep.state();
    }
    settings.activeReceiver = activeReceiver;

    const filters = realtimeModule?.getFilters();
    if (filters !== null && filters !== undefined) settings.realtime_filters = filters;
    const bg = realtimeModule?.getBackgroundStreaming();
    if (bg !== null && bg !== undefined) settings.realtime_background_streaming = bg;

    persistSettings();

    if (settingsPanel.isOpen())
        updateSettingsTab();
}

function updateForLegacySettings() {
    if ('latlon_in_dms' in settings) {

        settings.coordinate_format = settings.latlon_in_dms ? "dms" : "decimal";
        delete settings.latlon_in_dms;
    }

    if ('realtime_filter_mmsis' in settings) {
        if (Array.isArray(settings.realtime_filter_mmsis) && settings.realtime_filter_mmsis.length && !(settings.realtime_filters || []).length)
            settings.realtime_filters = settings.realtime_filter_mmsis.map((m) => ({ kind: 'mmsi', value: m }));
        delete settings.realtime_filter_mmsis;
    }

    settings.basemap_opacity = { ...DEFAULT_SETTINGS.basemap_opacity, ...(settings.basemap_opacity || {}) };

    if (!("showPlanesAtFirst" in settings)) {
        settings.showPlanesAtFirst = true;
        settings.map_overlay.push("Aircraft");
    }

    for (const key of ["max", "pinned", "pinned_x", "pinned_y", "top_left", "rows"]) {
        const was = "shipcard_" + key;
        if (was in settings) {
            settings["targetcard_" + key] = settings[was];
            delete settings[was];
        }
    }
    if (Array.isArray(settings.targetcard_rows)) {
        settings.targetcard_rows = settings.targetcard_rows.map((id) =>
            typeof id === "string" ? id.replace(/^shipcard_/, "targetcard_") : id);
    }

    if (Array.isArray(settings.map_overlay) && settings.map_overlay.includes("Community Feed")) {
        settings.map_overlay = settings.map_overlay.filter(t => t !== "Community Feed");
        queueMicrotask(() => showDialog("Community feed has moved",
            "The on-map Community Feed overlay has been replaced by a new <b>Community Pane</b> that opens the full aiscatcher.org map in a popup window.<br><br>Right-click the map and choose <b>Toggle Community Pane</b>, or use the network button in the map controls."));
    }
}

function loadSettings() {
    settingsStore.load(urlParams.has("reset"));
    if (settingsStore.upgraded()) showNotification("Settings were reset for this version of the viewer");
    if (settings.activeReceiver) activeReceiver = settings.activeReceiver;
    if (!isTargetcardMax()) toggleTargetcardSize();

    if (ui.card.keep.restore(settings.targetcard_rows)) {
        if (settings.targetcard_max != isTargetcardMax()) toggleTargetcardSize();
    } else {
        settings.targetcard_rows = [];
    }

    settings.android = false;
}

function convertStringSettingsToActual() {
    // targetcard_max is written by saveSettings and absent from the defaults
    const extraBooleans = ['targetcard_max'];

    for (const key of Object.keys(DEFAULT_SETTINGS).concat(extraBooleans)) {
        const v = settings[key];
        if (typeof v !== 'string') continue;
        const want = extraBooleans.includes(key) ? 'boolean' : typeof DEFAULT_SETTINGS[key];
        if (want === 'boolean')
            settings[key] = v === "true";
        else if (want === 'number' && v !== '' && !isNaN(v))
            settings[key] = Number(v);
    }
}

function loadSettingsFromURL() {
    for (const [key, value] of urlParams.entries()) {
        if (Object.hasOwn(settings, key)) {
            if (key === 'map_overlay') {
                if (!Array.isArray(settings[key])) settings[key] = [];
                settings[key].push(value);
            } else {
                settings[key] = value;
            }
        }
    }

    convertStringSettingsToActual();
}

function vesselPosition(m) {
    if (m == null) return null;

    if (replay.isActive()) {
        const fix = replay.fixFor(m);
        if (fix) return fix;
    }
    if (m in shipsDB) {
        const ship = shipsDB[m].raw;
        if (ship.lat != null && ship.lon != null) return { lat: ship.lat, lon: ship.lon };
    }
    return null;
}

function mapResetViewZoom(z, m) {
    const pos = vesselPosition(m);
    if (pos) {
        let view = map.getView();
        view.setCenter(ol.proj.fromLonLat([pos.lon, pos.lat]));
        view.setZoom(Math.min(view.getMaxZoom(), Math.max(z, view.getZoom() + 1)));
    }

    targetcardMinIfMaxonMobile();
}

function mapResetView(z) {

    let view = map.getView();
    view.setZoom(Math.min(view.getMaxZoom(), Math.max(z, view.getZoom() + 1)));
    targetcardMinIfMaxonMobile();
}

function targetcardVisible() {
    return panels.targetcard;
}

function measurecardVisible() {
    return panels.measure;
}

function toggleMeasurecard() {
    if (!panels.measure) ui.dismiss();
    setPanels({ measure: !panels.measure });
}

function replaycardVisible() {
    return panels.replay;
}

// While replay owns the map the live layers step aside, so the two never draw
// the same vessel twice. Exiting just puts them back and lets the normal
// refresh rebuild from shipsDB.
function setLiveLayersVisible(on) {
    [markerLayer, trackLayer, labelLayer, shapeLayer, binaryLayer]
        .forEach(l => l.setVisible(on));
    planeLayer.setVisible(on && Array.isArray(settings.map_overlay) && settings.map_overlay.includes("Aircraft"));
}

function toggleReplaycard() {
    // which panels step aside is normalisePanels's business, not this function's
    setPanels({ replay: !panels.replay });

    if (panels.replay) {
        measure.cancel();

        // the bar owns the map from the moment it opens
        showTargetcard(null, null);
        stopHover();
        setLiveLayersVisible(false);

        replay.refreshBounds().then(() => {
            // the user may have closed the bar while the fetch was in flight
            if (!panels.replay) return;
            updateReplaycard();
            replayShowAt(replayScrubTime(document.getElementById("replayScrub").value));
        });
    } else {
        replayLoadAt.cancel();
        stopReplay();
    }
}

// opening the bar already loaded the frame; this load is the fallback
async function replayToggle() {
    if (replay.isLoading()) return;

    if (replay.isPlaying()) {
        replay.pause();
        return;
    }

    if (!replay.isActive()) {
        replayLoadAt.cancel();
        const at = replayScrubTime(document.getElementById("replayScrub").value);

        if (!(await replayShowAt(at))) return;
    }
    replay.play();
}

function stopReplay() {
    replay.stop();
    redrawMap();
    updateReplaycard();
}

// The scrubber's 0..1000 value mapped onto the replayable timeline.
function replayScrubTime(value) {
    const tl = replay.getTimeline();
    return tl.start + (tl.end - tl.start) * (Number(value) / 1000);
}

// Dragging the slider is the primary way in: the labels follow immediately,
// and shortly after the drag settles the fleet for that moment is loaded and
// drawn. Play then only starts the clock on what is already on screen.
const replayLoadAt = debounce((at) => replayShowAt(at), 250);

function replaySeek(el) {
    const tl = replay.getTimeline();
    if (tl.end <= tl.start) return;

    const at = replayScrubTime(el.value);

    if (replay.isActive()) {
        replay.seek(at);
        return;
    }

    updateReplaycard();
    replayLoadAt(at);
}

async function replayShowAt(at) {
    const ok = await replay.load(at);
    updateReplaycard();
    return ok;
}

// The playhead as wall-clock time. Seconds matter on a short span; over a
// multi-day one the date does instead, and there is no room for both.
function replayStamp(unixSec, spanSec) {
    if (spanSec > 86400) {
        const d = new Date(unixSec * 1000);
        return d.toLocaleDateString(undefined, { month: "short", day: "numeric" }) + " " +
            d.toLocaleTimeString(undefined, { hour: "2-digit", minute: "2-digit" });
    }
    return formatTime(unixSec);
}

// Time left, collapsing to days once it stops fitting as h:mm:ss.
function replayRemain(sec) {
    const s = Math.max(0, Math.round(sec));

    if (s >= 86400) {
        const days = Math.floor(s / 86400);
        const hours = Math.floor((s % 86400) / 3600);
        return days + "d" + (hours ? " " + hours + "h" : "");
    }

    const h = Math.floor(s / 3600);
    const m = Math.floor((s % 3600) / 60);
    const ss = String(s % 60).padStart(2, "0");
    return h > 0 ? h + ":" + String(m).padStart(2, "0") + ":" + ss : m + ":" + ss;
}

// Runs at animation rate while playing, so element lookups are cached and every
// write is guarded: most of the bar only changes on discrete events, and the
// time labels only when the displayed second does.
let replayEls = null;
const replayShown = {};

function updateReplaycard() {
    const bar = document.getElementById("replaybar");
    if (!bar || !bar.classList.contains("visible")) return;

    if (!replayEls)
        replayEls = {
            scrub: document.getElementById("replayScrub"),
            speed: document.getElementById("replaySpeed"),
            labels: document.getElementById("replayLabels"),
            play: document.getElementById("replayPlay"),
            fill: document.getElementById("replayFill"),
            elapsed: document.getElementById("replayElapsed"),
            remaining: document.getElementById("replayRemaining"),
        };
    const ui = replayEls, shown = replayShown;
    const { start, end } = replay.getTimeline();

    bar.classList.toggle("playing", replay.isPlaying());
    bar.classList.toggle("loading", replay.isLoading());

    const speed = replay.getSpeed();
    if (shown.speed !== speed) {
        shown.speed = speed;
        ui.speed.textContent = speed + "×";
    }

    const labelsOn = replay.getLabels();
    if (shown.labelsOn !== labelsOn) {
        shown.labelsOn = labelsOn;
        ui.labels.classList.toggle("on", labelsOn);
        ui.labels.setAttribute("aria-pressed", labelsOn);
        ui.labels.title = labelsOn ? "Hide ship labels" : "Show ship labels";
        ui.labels.setAttribute("aria-label", ui.labels.title);
    }

    // no history at all means nothing to start
    const hasHistory = end > start;
    ui.play.disabled = !hasHistory || replay.isLoading();
    ui.scrub.disabled = !hasHistory;

    const playTitle = replay.isLoading() ? "Loading" : replay.isPlaying() ? "Pause" : "Play";
    if (shown.playTitle !== playTitle) {
        shown.playTitle = playTitle;
        ui.play.title = playTitle;
    }

    const at = replay.isActive() ? replay.getInstant() : replayScrubTime(ui.scrub.value);

    if (replay.isActive() && hasHistory) {
        const v = Math.round(((at - start) / (end - start)) * 1000);
        if (Number(ui.scrub.value) !== v) ui.scrub.value = v;
    }

    const fill = (Number(ui.scrub.value) / 10) + "%";
    if (shown.fill !== fill) {
        shown.fill = fill;
        ui.fill.style.width = fill;
    }

    const atSec = hasHistory ? Math.floor(at) : -1;
    if (shown.atSec !== atSec || shown.start !== start || shown.end !== end) {
        shown.atSec = atSec;
        shown.start = start;
        shown.end = end;
        ui.elapsed.textContent = hasHistory ? replayStamp(at, end - start) : "--:--";
        ui.elapsed.title = hasHistory ? new Date(at * 1000).toLocaleString() : "";
        ui.remaining.textContent = hasHistory ? "-" + replayRemain(end - at) : "--:--";
    }
}

async function ToggleTrackOnMap(m) {

    if (marker_tracks.has(Number(m))) {
        marker_tracks.delete(Number(m));
        redrawMap();
    } else {
        marker_tracks.add(Number(m));
        await fetchTracks();
        targetcardMinIfMaxonMobile();
        redrawMap();
    }
}

async function toggleTrack(m) {
    if (settings.show_track_on_select && card_mmsi == m && card_type == 'ship') {
        select_enabled_track = !select_enabled_track;
    }
    else {
        ToggleTrackOnMap(m);
    }
    targetcard.updateTrackOption();

}

async function showTrack(m) {
    if (!marker_tracks.has(Number(m))) {
        ToggleTrackOnMap(m);
    }
    targetcard.updateTrackOption();

}

async function hideTrack(m) {
    if (marker_tracks.has(Number(m))) {
        ToggleTrackOnMap(m);
    }
    targetcard.updateTrackOption();

}

function trackIsShown(m) {
    return marker_tracks.has(Number(m));
}

function vesselLabel(m) {
    if (m == null) return "";
    if (String(m).toUpperCase() == "STATION") return "the station";

    const ship = m in shipsDB ? shipsDB[m].raw : null;
    return (ship && getShipName(ship)) || String(m);
}

function pinVessel(m) {
    settings.center_point = m;
    settings.fix_center = true;
    saveSettings();
    drawStation();
    applyFixedCenter();
    targetcard.updateFollowOption();
    showNotification("Following " + vesselLabel(m));
}

function unpinCenter() {
    const was = settings.center_point;
    settings.fix_center = false;
    saveSettings();
    drawStation();
    targetcard.updateFollowOption();
    showNotification("No longer following " + vesselLabel(was));
}

function isFollowing(m) {
    return settings.fix_center && m != null &&
        String(settings.center_point).toUpperCase() === String(m).toUpperCase();
}

function toggleFollow(m) {
    if (m == null) return;

    if (isFollowing(m)) unpinCenter();
    else pinVessel(m);
}

function setTrackVisibility(mode) {
    if (mode === 'all') return showAllTracks();
    if (mode === 'none') return deleteAllTracks();
    settings.show_all_tracks = false;
    saveSettings();
    redrawMap();
    targetcard.updateTrackOption();
}

async function showAllTracks() {
    settings.show_all_tracks = true;
    trackCutoff = 0;
    lastPathFetch = 0;
    select_enabled_track = hover_enabled_track = false;
    await fetchTracks();
    redrawMap();
    targetcard.updateTrackOption();
    saveSettings();
}

async function showTracksForMMSIs(mmsis) {
    let added = 0;
    for (const m of mmsis) {
        if (!marker_tracks.has(Number(m))) {
            marker_tracks.add(Number(m));
            added++;
        }
    }
    if (added > 0) {
        lastPathFetch = 0;
        await fetchTracks();
        redrawMap();
        targetcard.updateTrackOption();
    }
    return mmsis.length;
}

function deleteAllTracks() {
    settings.show_all_tracks = false;
    trackCutoff = 0;
    lastPathFetch = 0;
    setMarkerTracks(new Set());
    let p = {};

    if (card_type == 'ship' && card_mmsi && settings.show_track_on_select) {
        marker_tracks.add(Number(card_mmsi));
        select_enabled_track = true;

        if (paths[card_mmsi]) {
            p[card_mmsi] = paths[card_mmsi];
        }
    }

    setPaths(p);

    redrawMap(); targetcard.updateTrackOption();
    saveSettings();
}

async function toggleTrackCutoff() {
    trackCutoff = trackCutoff ? 0 : (clock || Math.floor(Date.now() / 1000));
    setPaths({});
    setPathsFrom(-1);
    lastPathFetch = 0;
    await fetchTracks();
    redrawMap();
    targetcard.updateTrackOption();
    showNotification(trackCutoff ? "Tracks now start from " + trackCutoffLabel()
        : "Tracks restored to the full history", "success");
}

function trackCutoffLabel() {
    const ago = Math.max(0, (clock || 0) - trackCutoff);
    return new Date(Date.now() - ago * 1000)
        .toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
}

function mergeTrackPoints(older, newer) {
    const merged = [];
    let i = 0, j = 0;

    while (i < newer.length && j < older.length) {
        const fresh = newer[i], prev = older[j];
        if (fresh[2] === prev[2]) {
            prev[0] = fresh[0]; prev[1] = fresh[1]; prev[3] = fresh[3];
            merged.push(prev);
            i++; j++;
        } else if (fresh[2] > prev[2]) {
            merged.push(fresh); i++;
        } else {
            merged.push(prev); j++;
        }
    }
    while (i < newer.length) merged.push(newer[i++]);
    while (j < older.length) merged.push(older[j++]);
    return merged;
}

async function fetchTracks() {
    if (marker_tracks.size == 0 && settings.show_all_tracks == false) return true;

    let a;
    let isDelta = false;
    try {
        if (settings.show_all_tracks) {
            // deltas accumulate points the server has pruned, so resync with a full fetch every hour
            isDelta = lastPathFetch > 0 && Date.now() - lastFullPathFetch < 3600 * 1000;
            let sinceParam = "&since=" + lastPathFetch;
            if (!isDelta) {
                setPathsFrom(trackWindowStart());
                sinceParam = pathsFrom ? "&since=" + pathsFrom : "";
                lastFullPathFetch = Date.now();
            }
            a = await fetch("api/allpath.json?receiver=" + activeReceiver + sinceParam);
        } else {
            for (var mmsi of marker_tracks) {
                if (!(mmsi in shipsDB)) {
                    ToggleTrackOnMap(mmsi);
                }
            }
            const wanted = Array.from(marker_tracks).filter((m) => !shipsDB[m] || shipVisible(shipsDB[m]));
            if (wanted.length === 0) return true;
            const mmsi_str = wanted.join(",");
            setPathsFrom(0);
            a = await fetch("api/path.json?" + mmsi_str + "&receiver=" + activeReceiver);
        }

        const newPaths = await a.json();

        let maxTs = lastPathFetch;
        for (const mmsi in newPaths)
            for (const pt of newPaths[mmsi])
                if (pt[3] > maxTs) maxTs = pt[3];
        if (maxTs > lastPathFetch) lastPathFetch = maxTs;

        if (!isDelta) {
            setPaths(newPaths);
        } else {
            for (const mmsi in newPaths) {
                paths[mmsi] = paths[mmsi]
                    ? mergeTrackPoints(paths[mmsi], newPaths[mmsi])
                    : newPaths[mmsi];
            }
            for (const mmsi in paths) {
                if (!(mmsi in shipsDB)) delete paths[mmsi];
            }
        }
    } catch (error) {
        console.log("Error loading path: " + error);
        if (!isDelta) { setPaths({}); setPathsFrom(-1); }
        lastPathFetch = 0;
        return false;
    }

    const cutoff = trackCutoff;
    if (cutoff > 0) {
        for (const mmsi in paths) {
            const arr = paths[mmsi];
            let k = 0;
            while (k < arr.length && arr[k][3] >= cutoff) k++;
            paths[mmsi] = arr.slice(0, k + 1);
        }
    }

    return true;
}

function isTargetcardMax() {
    return ui.card.isMax();
}

function targetcardMinIfMaxonMobile() {
    if (targetcardVisible() && mapui.paneCramped() && isTargetcardMax()) toggleTargetcardSize();
}

let stationDrawn = '';

function drawStation() {
    const onVessel = !!binary.ownVessel();
    const hidden = settings.show_station == false || stationCoords() == null || onVessel;
    const key = hidden ? '' : `${station.lat},${station.lon},${station.gps}`;
    if (key === stationDrawn) return;
    stationDrawn = key;

    if (stationFeature) {
        extraVector.removeFeature(stationFeature);
        stationFeature = undefined;
    }
    if (hidden) return;

    const { canvas, size } = stations.stationCanvas(station.gps);
    stationFeature = new ol.Feature({ geometry: new ol.geom.Point(ol.proj.fromLonLat([station.lon, station.lat])) });
    stationFeature.setStyle(new ol.style.Style({ image: new ol.style.Icon({ img: canvas, width: size, height: size }) }));
    stationFeature.tooltip = stations.stationBand({ name: config.station, gps: station.gps, mmsi: station.mmsi });
    stationFeature.station = true;
    extraVector.addFeature(stationFeature);
}

function followTargetIsStation() {
    return String(settings.center_point).toUpperCase() == "STATION";
}

function stationCoords() {
    return station != null && Object.hasOwn(station, "lat") && Object.hasOwn(station, "lon") ? station : null;
}

let followAnimating = false;

function panTo(lon, lat, smooth) {
    const view = map.getView();

    if (view.getInteracting()) return;
    if (view.getAnimating() && !followAnimating) return;

    const coord = ol.proj.fromLonLat([lon, lat]);
    const from = view.getCenter();
    const size = map.getSize();

    if (from != null) {
        const px = Math.hypot(coord[0] - from[0], coord[1] - from[1]) / view.getResolution();
        if (px < 0.5) return;
        // a jump too big to read as motion is not worth easing
        smooth = smooth && size != null && px <= 2 * Math.max(size[0], size[1]);
    }

    if (!smooth) {
        if (followAnimating) view.cancelAnimations();
        view.setCenter(coord);
        return;
    }

    view.animate({ center: coord, duration: refreshIntervalMs }, () => { followAnimating = false; });
    followAnimating = true;
}

function applyFixedCenter() {
    if (!settings.fix_center) return;

    const replaying = replay.isActive();

    let target = null;
    if (followTargetIsStation()) {
        target = stationCoords();
    } else if (replaying || settings.center_point in shipsDB) {
        // prefers the frame being drawn over where the vessel is right now
        target = vesselPosition(settings.center_point);
    } else if (shipCounts.total > 0) {
        // the followed vessel aged out or left range; replay owns its own
        // fleet, so a gap in the live set says nothing while it is running
        const dropped = settings.center_point;
        settings.center_point = "STATION";
        settings.fix_center = false;
        targetcard.updateFollowOption();
        showNotification("No longer following " + vesselLabel(dropped) + ": out of range");
        return;
    }

    if (target == null) return;

    panTo(target.lon, target.lat, !replaying);

    settings.lat = target.lat;
    settings.lon = target.lon;
}

function moveMapCenter(px) {
    stopHover();
    ui.panTo(px);
}

function adjustMapForTargetcard(pixel) {
    const db = card_type == 'ship' ? shipsDB : card_type == 'plane' ? planesDB : null;
    const raw = db && card_mmsi in db ? db[card_mmsi].raw : null;
    if (!raw || !raw.lat || !raw.lon) return;

    if (ui.reveal([raw.lon, raw.lat], pixel, card_type == 'ship' ? { minZoom: 4 } : undefined) === "panned") stopHover();
}

// The ticker owns the top strip. A card that starts above it hides the very
// thing that announced the vessel, so cards begin below it while it is out.
window.addEventListener("resize", () => ui.chrome.invalidate());

const cssPx = ui.chrome.px;

function applyToolbarMode() {
    ui.toolbar.apply();
}

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

function pinTargetcard() {
    const at = ui.card.pin();
    settings.targetcard_pinned = true;
    settings.targetcard_pinned_x = at.x;
    settings.targetcard_pinned_y = at.y;
    showNotification("Card pinned to this position");
    saveSettings();
}

function unpinTargetcard() {
    ui.card.unpin();
    settings.targetcard_pinned = false;
    settings.targetcard_pinned_x = null;
    settings.targetcard_pinned_y = null;
    showNotification("Card unpinned");
    saveSettings();
}

function applyTargetcardPinStyling() {
    ui.card.markPinned(settings.targetcard_pinned);
}

function toggleShipcardStyle() {
    settings.shipcard_style = settings.shipcard_style === "tabs" ? "classic" : "tabs";
    targetcard.setStyle(settings.shipcard_style);
    if (settingsPanel.sync) settingsPanel.sync();
    saveSettings();
}

function toggleTargetcardPin() {
    if (settings.targetcard_pinned) unpinTargetcard();
    else pinTargetcard();
}

function fitTargetcard() {
    if (!targetcardVisible()) return;
    ui.card.fit();
}

function positionAside(pixel, aside) {
    stopHover();
    if (!aside.offsetParent) return;

    if (settings.kiosk && settings.kiosk_pan_map && card_type == 'ship' && card_mmsi in shipsDB) {
        moveMapCenter(pixel);
        const mapSize = map.getSize();
        pixel = [mapSize[0] / 2, mapSize[1] / 2];
    }

    /* beside the target the card already leaves it visible; only a docked
       card may end up covering it */
    const how = ui.card.open(pixel);
    if (how === "dock") adjustMapForTargetcard(pixel);
    return how;
}

function showTargetcard(type, m, pixel = undefined) {
    targetcard.resetHistory();
    if (m != null) ui.dismiss();   // a card takes the stage, wherever it was opened from

    const aside = document.getElementById("targetcard");
    const visible = targetcardVisible();

    let ship = m in shipsDB ? shipsDB[m].raw : null;
    let ship_old = card_mmsi in shipsDB ? shipsDB[card_mmsi].raw : null;
    const prev_mmsi = card_mmsi;

    // a vessel the receiver does not know is out of range: say so, no card
    if (type == 'ship' && m != null && !ship) {
        showNotification("MMSI " + m + " is out of range", "error");
        return;
    }

    if (select_enabled_track && (card_mmsi != m || m == null)) {
        select_enabled_track = false;

        if (!(card_mmsi == hoverMMSI && hover_enabled_track && hoverType == 'ship')) {
            hideTrack(card_mmsi);
        }
    }

    if (m != null && !visible) {
        setPanels({ targetcard: true, measure: false }, { reposition: false });
        select_enabled_track = false;
    } else if (visible && m == null) {
        setPanels({ targetcard: false });
    }

    if (type !== card_type) {
        document.querySelectorAll('#targetcard_content [data-context-type]').forEach(element => {
            if (element.dataset.contextType === type) {
                element.style.display = '';
            } else {
                element.style.display = 'none';
            }
        });

        ui.card.footer.show(type);
    }

    setCard(m, type);
    if (type == 'ship' && m != null) targetcard.loadVessel(m);
    syncTableSelection();

    if (targetcardVisible()) {
        if (settings.show_track_on_select && card_type == 'ship') {
            if (hoverMMSI === m && hover_enabled_track && hoverType == 'ship') {
                hover_enabled_track = false;
                select_enabled_track = true;
            }
            else if (!trackIsShown(m)) {
                select_enabled_track = true;
                showTrack(m);
            }
        }

        /* a fresh card opens at the size the setting asks for, except on a
           cramped pane which always opens compact; a target out of range has
           nothing to show and opens compact too; switching ship with the
           card up keeps the size the user has */
        const known = card_type != 'ship' || ship != null;
        const wantMax = known && settings.targetcard_open_max && !mapui.paneCramped();
        if (settings.shipcard_style !== "tabs" && !visible && isTargetcardMax() !== wantMax) toggleTargetcardSize();
        if (ship && (!visible || prev_mmsi !== m) && !hasValidCoords(ship.lat, ship.lon))
            showNotification("No position received for " + (getShipName(ship) || m), "error");
        positionAside(pixel, aside);

        if (card_type == 'ship') targetcard.populate();
        else if (card_type == 'plane') planecard.populate();

        // trigger reflow for iPad Safari
        aside.style.display = 'none';
        aside.offsetHeight;
        aside.style.display = '';

        fitTargetcard();
    }

    trackLayer.changed();
    labelLayer.changed();
    updateFocusMarker();
}

const getSprite = markersLib.applySprite;
const getPlaneSprite = markersLib.applyPlaneSprite;
const spriteFor = markersLib.spriteFor;
const SpritesAll = ui.markers.sheet;

async function updateMap() {
    // Opening the replay bar hands the map over: from that point the live
    // layers are on their way out and polling for ships, tracks and planes
    // fetches data the user is no longer looking at. Closing it resumes.
    if (replaycardVisible() || replay.isActive()) return;

    const ok = await fetchShips();
    if (!ok) return;

    await Promise.all([
        fetchTracks(),
        planeLayer.getVisible() ? fetchPlanes() : Promise.resolve(true),
        (binaryLayer.isVisible() && binary.binaryAnyShown()) ? binary.fetchBinary() : Promise.resolve(true),
        range.fetchRange(),
    ]);

    if (settings.setcoord == "true" || settings.setcoord == true) {
        if (station != null && Object.hasOwn(station, "lat") && Object.hasOwn(station, "lon")) {
            settings.setcoord = false;
            let view = map.getView();
            view.setCenter(ol.proj.fromLonLat([station.lon, station.lat]));
            saveSettings();
        }
    }

    if (targetcardVisible()) {
        if (card_type == "ship")
            targetcard.populate();
        else if (card_type == "plane")
            planecard.populate();
    }

    updateMarkerCount();
    redrawMap();
}

function redrawMap() {
    shapeFeatures = {};
    markerFeatures = {};

    markerVector.clear();
    planeVector.clear();
    shapeVector.clear();
    labelVector.clear();
    trackVector.clear();

    labelLayer.declutter_ = settings.labels_declutter;

    const zoom = map.getView().getZoom();
    const showShapeOutlines = zoom > 11.5;
    const includeLabels = (settings.show_labels === "dynamic" && showShapeOutlines) || settings.show_labels === "always";

    for (let [mmsi, entry] of Object.entries(shipsDB)) {
        let ship = entry.raw;
        if (!shipVisible(entry)) continue;
        if (hasValidCoords(ship.lat, ship.lon)) {
            getSprite(ship)

            const lon = ship.lon
            const lat = ship.lat

            const point = new ol.geom.Point(ol.proj.fromLonLat([lon, lat]))
            let feature = new ol.Feature({
                geometry: point
            })

            feature.ship = ship;

            markerFeatures[ship.mmsi] = feature
            markerVector.addFeature(feature)

            if (includeLabels)
                labelVector.addFeature(feature)

            const outline = showShapeOutlines ? markersLib.shipOutlineGeometry(ship) : null;
            if (outline) {
                const shapeFeature = new ol.Feature({ geometry: outline })
                shapeFeature.ship = ship
                shapeFeatures[ship.mmsi] = shapeFeature

                shapeVector.addFeature(shapeFeature)
            }
        }
    }
    measure.refreshMeasures();

    binary.redrawBinaryMessages();

    if (planeLayer.getVisible()) {

        for (let [hexident, entry] of Object.entries(planesDB)) {
            let plane = entry.raw;
            if (hasValidCoords(plane.lat, plane.lon)) {
                getPlaneSprite(plane)

                const lon = plane.lon
                const lat = plane.lat

                const point = new ol.geom.Point(ol.proj.fromLonLat([lon, lat]))
                const feature = new ol.Feature({
                    geometry: point
                })

                feature.plane = plane;

                markerFeatures[plane.hexident] = feature
                planeVector.addFeature(feature)

                if (includeLabels)
                    labelVector.addFeature(feature)
            }
        }
    }

    const cutoff = trackWindowStart();
    const speedMode = settings.track_color_mode === "speed";
    const speedPalette = validPalette(settings.track_speed_palette);
    const speedTop = Number(settings.track_speed_max) || 20;

    // A leg is drawn at the mean of the speeds its two points report, in the
    // tenths of a knot the path carries; a leg with no speed at either end
    // gets a neutral color rather than the bottom of the ramp, which would
    // read as "stopped".
    const legBucket = (newer, older) => {
        if (!speedMode) return 0;
        const a = newer[4], b = older[4];
        const sog = a != null && b != null ? (a + b) / 2 : (a != null ? a : b);
        return sog == null ? -1 : speedBucket(sog / 10, speedTop);
    };

    for (let [mmsi, entry] of Object.entries(paths)) {

        if (shipsDB[mmsi] && !shipVisible(shipsDB[mmsi])) continue;

        if (marker_tracks.has(Number(mmsi)) || settings.show_all_tracks) {
            const path = paths[mmsi];
            const ship = shipsDB[mmsi]?.raw;
            const shipclass = ship?.shipclass;

            // Path: [lat, lon, start_time, end_time, sog, cog, hdg]
            if (path.length > 0 && path[0].length >= 4) {
                const emitSegment = (coords, dashed, bucket) => {
                    if (coords.length < 2) return;
                    const feature = new ol.Feature(new ol.geom.LineString(coords));
                    feature.mmsi = mmsi;
                    feature.isDashed = dashed;
                    feature.shipclass = shipclass;
                    if (speedMode)
                        feature.speedColor = bucket < 0 ? TRACK_SPEED_UNKNOWN_COLOR : bucketColor(speedPalette, bucket);
                    trackVector.addFeature(feature);
                };

                let currentSegment = [];
                let currentDashed = false;
                let currentBucket = 0;

                for (let i = 0; i < path.length; i++) {
                    const point = path[i];
                    if (cutoff && point[3] < cutoff)
                        break;
                    const coord = ol.proj.fromLonLat([point[1], point[0]]);

                    let isDashed = false;
                    let bucket = 0;
                    if (i > 0) {
                        const timeBetweenPoints = path[i - 1][2] - point[3]; // newer start_time - older end_time
                        isDashed = timeBetweenPoints > settings.track_trash_threshold;
                        bucket = legBucket(path[i - 1], point);
                    }

                    if (currentSegment.length === 0) {
                        // First point
                        currentSegment.push(coord);
                        currentDashed = false; // First segment is always solid
                    } else if (currentSegment.length === 1 || (currentDashed === isDashed && currentBucket === bucket)) {
                        // Continue current segment
                        currentSegment.push(coord);
                        currentDashed = isDashed;
                        currentBucket = bucket;
                    } else {
                        emitSegment(currentSegment, currentDashed, currentBucket);
                        // Start new segment
                        currentSegment = [currentSegment[currentSegment.length - 1], coord];
                        currentDashed = isDashed;
                        currentBucket = bucket;
                    }
                }

                emitSegment(currentSegment, currentDashed, currentBucket);
            }
        }
    }

    range.drawRange();
    updateFocusMarker();
    updateHoverMarker();

    updateMarkerCount();
    updateTablecard();

    drawStation();
    applyFixedCenter();
    range.updateDistanceCircles();

}

function updateDarkMode() {
    document.documentElement.classList.toggle("dark", settings.dark_mode);
    document.getElementById("theme-toggle-moon")?.style.setProperty("display", settings.dark_mode ? "none" : "");
    document.getElementById("theme-toggle-sun")?.style.setProperty("display", settings.dark_mode ? "" : "none");
    const themeBtn = document.getElementById("theme-toggle-button");
    if (themeBtn) themeBtn.title = settings.dark_mode ? "Switch to day" : "Switch to night";
    plotsModule?.updateColors();
    updateMapLayer();
    redrawMap();
    syncThemedSettings();
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

function setDarkMode(b) {
    settings.dark_mode = b;
    updateDarkMode();
    saveSettings();
}

function toggleDarkMode() {
    settings.dark_mode = !settings.dark_mode;
    updateDarkMode();
    saveSettings();
}

function refresh_data() {
    if (!document.hidden && !updateInProgress) {
        updateInProgress = true;

        return (async () => {
            try {
                if (settings.tab === "map") {
                    await updateMap();
                } else if (settings.tab === "stat") {
                    await updateStatistics();
                } else if (settings.tab === "plots") {
                    if (!plotsModule) {
                        plotsModule = await import('./tabs/plots.js');
                        plotsModule.setAbsoluteTime(settings.plot_absolute_time);
                    }
                    await plotsModule.update();
                } else if (settings.tab === "ships") {
                    if (!shipTableModule) shipTableModule = await import('./tabs/shiptable.js');
                    await shipTableModule.update();
                }
            } catch (error) {
                console.error("Error updating data:", error);
            } finally {
                updateInProgress = false;
            }
        })();
    }
    return Promise.resolve();
}

async function openFocus(m, z) {
    await fetchShips(false);

    selectMapTab(m);

    let ship = shipsDB[m] && shipsDB[m].raw;
    if (ship && ship.lon && ship.lat) {
        let shipCoords = ol.proj.fromLonLat([ship.lon, ship.lat]);
        let view = map.getView();
        view.setCenter(shipCoords);
    }

    if (z) mapResetView(z);
    else mapResetView(14);

    if (ship && ship.lon && ship.lat) ui.reveal([ship.lon, ship.lat], undefined, { center: true });
}

function updateSettingsTab() {
    document.getElementById("settings_darkmode").checked = settings.dark_mode;
    document.getElementById("settings_map_toolbar").value = settings.map_toolbar;
    document.getElementById("settings_coordinate_format").value = settings.coordinate_format;
    document.getElementById("settings_metric").value = getMetrics().toLowerCase();
    document.getElementById("settings_fading").checked = settings.fading;
    document.getElementById("settings_show_signal_graphs").checked = settings.show_signal_graphs;
    document.getElementById("settings_show_ppm_graphs").checked = settings.show_ppm_graphs;
    document.getElementById("settings_plot_absolute_time").checked = settings.plot_absolute_time;

    document.getElementById("settings_show_range").checked = settings.show_range;
    document.getElementById("settings_distance_circle_color").value = settings.distance_circle_color;

    document.getElementById("settings_show_labels").value = settings.show_labels.toLowerCase();

    document.getElementById("settings_binary_messages").value = settings.binary_messages;
    for (const cat of binary.BINARY_CATEGORIES) {
        const box = document.getElementById("settings_binary_cat_" + cat);
        if (box) box.checked = !settings.binary_exclude.includes(cat);
    }
    document.getElementById("settings_binary_color_class").checked = settings.binary_color_class;
    document.getElementById("settings_binary_id_labels").checked = settings.binary_id_labels;
    document.getElementById("settings_binary_group_areas").checked = settings.binary_group_areas;

    document.getElementById("settings_range_color").value = settings.range_color;
    document.getElementById("settings_range_timeframe").value = settings.range_timeframe;
    document.getElementById("settings_range_color_short").value = settings.range_color_short;
    document.getElementById("settings_range_color_dark").value = settings.range_color_dark;
    document.getElementById("settings_range_color_dark_short").value = settings.range_color_dark_short;

    document.getElementById("settings_icon_scale").value = settings.icon_scale;
    document.getElementById("settings_track_history").value = Math.max(0, TRACK_HISTORY_STOPS.indexOf(settings.track_history));

    // Update all slider display values
    updateSliderDisplay('iconScale', settings.icon_scale);
    updateSliderDisplay('trackHistory', settings.track_history);

    document.getElementById("settings_table_shiptype_use_icon").checked = settings.table_shiptype_use_icon;
    document.getElementById("settings_show_track_on_hover").checked = settings.show_track_on_hover;
    document.getElementById("settings_show_track_on_select").checked = settings.show_track_on_select;
    document.getElementById("settings_track_visibility").value = settings.show_all_tracks ? "all" : "selected";

    document.getElementById("settings_kiosk_mode").checked = settings.kiosk;
    document.getElementById("settings_kiosk_rotation_speed").value = settings.kiosk_rotation_speed;
    document.getElementById("settings_kiosk_pan_map").checked = settings.kiosk_pan_map;

    updateSliderDisplay('kioskSpeed', settings.kiosk_rotation_speed);

    // Update ship class color inputs
    updateTrackColorInputs();

    const paletteSelect = document.getElementById("settings_track_speed_palette");
    if (!paletteSelect.options.length)
        for (const [key, p] of Object.entries(SPEED_PALETTES))
            paletteSelect.add(new Option(p.name, key));

    paletteSelect.value = validPalette(settings.track_speed_palette);
    document.getElementById("settings_track_color_mode").value = settings.track_color_mode === "speed" ? "speed" : "class";
    document.getElementById("settings_track_speed_max").value = settings.track_speed_max;
    updateSliderDisplay('trackSpeedMax', settings.track_speed_max);
    updateTrackColorModeUI();
}

function activateTab(b, a) {
    // Block decoder tab if decoder is disabled
    if (a === "decoder" && !config.features.decoder) {
        return;
    }

    hideMenu();
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

    applyPanels();

    clearInterval(interval);

    refresh_data().then(() => {
        clearInterval(interval);
        interval = setInterval(refresh_data, refreshIntervalMs);
    });

    if (a != "map") fireworks.stop();

    if (a == "log") {
        import('./tabs/log.js').then(({ LogViewer }) => {
            if (settings.tab !== 'log' || logViewer) return;
            logViewer = new LogViewer();
            logViewer.connect();
        }).catch((err) => console.error('Failed to load log tab module:', err));
    }
    if (a != 'log' && logViewer) {
        logViewer.disconnect();
        logViewer = null;
    }

    if (a == "realtime" && config.features.realtime) {
        import('./tabs/realtime.js').then((m) => {
            realtimeModule = m;
            if (settings.tab !== 'realtime') return;
            m.activate();
        }).catch((err) => console.error('Failed to load realtime tab module:', err));
    } else if (a != 'realtime') {
        realtimeModule?.deactivate();
    }
    if (a === "about") {
        import('./tabs/about.js').then(({ setup }) => setup())
            .catch((err) => console.error('Failed to load about tab module:', err));
    }
    if (a === "decoder" && !decoderModule) {
        // Preload so Decode/Clear button clicks resolve from cache.
        import('./tabs/decoder.js').then((m) => { decoderModule = m; })
            .catch((err) => console.error('Failed to load decoder tab module:', err));
    }
}

function selectMapTab(m) {
    document.getElementById("map_tab").click();
    if (m in shipsDB) showTargetcard('ship', m);
}

// split out so the container can be revealed before initMap() runs
function resolveTab() {
    if (settings.tab == "settings") settings.tab = "stat";

    // Check if requested tab is disabled and redirect to map
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

function selectTab() {
    activateTab(null, resolveTab());
}

const androidStyle = document.createElement("style");
document.head.appendChild(androidStyle);

function updateAndroid() {
    const sel = isAndroid() ? ".noandroid" : ".android";
    androidStyle.textContent = sel + " { display: none !important; }";
}

// minutes of track to draw; the last stop means everything held by the server
const TRACK_HISTORY_STOPS = [1, 5, 15, 30, 60, 180, 360, 720, 1440, 0];
// Oldest point to show, in SERVER epoch seconds, 0 for everything. Point times
// and the `since` filter are server-side, so a skewed browser clock must not leak in.
function trackWindowStart() {
    if (!settings.track_history) return 0;
    const now = clock || Math.floor(Date.now() / 1000);
    return now - settings.track_history * 60;
}

function trackHistoryLabel(m) {
    if (!m) return 'All';
    return m < 60 ? m + ' min' : (m / 60) + ' h';
}

/* sliders the settings panel does not bind (they carry their own change handlers) */
const SLIDER_DISPLAYS = {
    kioskSpeed: ["kiosk_rotation_speed_label", (v) => `Rotation Speed (${v}s)`],
    trackHistory: ["track_history_label", (v) => `Length (${trackHistoryLabel(Number(v))})`],
    trackSpeedMax: ["track_speed_max_label", (v) => `Scale Max (${speedWhole(v)} ${getSpeedUnit()})`],
    iconScale: ["icon_scale_label", (v) => `Size (${parseFloat(v).toFixed(2)})`],
};

function updateSliderDisplay(key, value) {
    const [id, format] = SLIDER_DISPLAYS[key];
    document.getElementById(id).textContent = format(value);
}

function showAboutDialog() {
    const message = `
        <div style="display: flex; align-items: center; margin-top: 10px;">
        <span style="text-align: center; margin-right: 10px;"><i style="font-size: 40px" class="directions_aiscatcher_icon"></i></span>
        <span>
        <a href="https://www.aiscatcher.org" target="_blank" rel="noopener"><b style="font-size: 22px;">AIS-catcher</b></a>
        <br>
        <b style="font-size: 11px;">&copy; 2021-2026 jvde.github@gmail.com</b>
        </span>
        </div>
        <p>
        AIS-catcher is a research and educational tool, provided under the
        <a href="https://github.com/jvde-github/AIS-catcher/blob/e66a4481e62d8f1775700e5f51fb7ad9ea569a12/LICENSE" target="_blank" rel="noopener">GNU GPL v3 license</a>.
        It is not reliable for navigation and safety of life or property.
        Radio reception and handling regulations vary by region, so check your local administration's rules. Illegal use is strictly prohibited.
        </p>
        <p>
        The web-interface gratefully uses the following libraries:
        <a href="https://www.chartjs.org/docs/latest/charts/line.html" target="_blank" rel="noopener nofollow">chart.js</a>,
        <a href="https://www.chartjs.org/chartjs-plugin-annotation/latest/" target="_blank" rel="noopener nofollow">chart.js annotation plugin</a>,
        <a href="https://openlayers.org/" target="_blank" rel="noopener nofollow">openlayers</a>,
        <a href="https://fonts.google.com/icons?selected=Material+Icons" target="_blank" rel="noopener nofollow">Material Design Icons</a>,
        <a href="https://tabulator.info/" target="_blank" rel="noopener nofollow">tabulator</a>,
        <a href="https://github.com/markedjs/marked" target="_blank" rel="noopener">marked</a>, and
        <a href="https://github.com/lipis/flag-icons" target="_blank" rel="noopener">flag-icons</a>. Please consult the links for the respective licenses.
        </p>`;

    showDialog("About...", message);
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

function showWelcome() {
    if ((settings.welcome == true || settings.welcome == "true") && !isAndroid()) showAboutDialog();

    settings.welcome = false;
    saveSettings();
}

// for overwrite and insert code where needed

addTileLayer("OpenStreetMap", new ol.layer.Tile({
    source: new ol.source.OSM({ maxZoom: 19 })
}));

const LOCAL_LABEL_FONT = ['Arial'];
const OPENFREEMAP_ATTRIBUTION =
    '<a href="https://openfreemap.org">OpenFreeMap</a> ' +
    '<a href="https://www.openmaptiles.org/">&copy; OpenMapTiles</a> ' +
    'Data from <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a>';

function addVectorBasemap(title, styleUrl, noLabels) {
    const layer = new ol.layer.VectorTile({ declutter: true });
    let applied = false;
    layer.on('change:visible', () => {
        if (!layer.getVisible() || applied) return;
        applied = true;
        Promise.all([import('ol-mapbox-style'), fetch(styleUrl).then((r) => r.json())])
            .then(([{ applyStyle }, glStyle]) => {
                for (const l of glStyle.layers || [])
                    if (l.layout && l.layout['text-font']) l.layout['text-font'] = LOCAL_LABEL_FONT;
                if (noLabels) glStyle.layers = (glStyle.layers || []).filter((l) => l.type !== 'symbol');
                const background = (glStyle.layers || []).find((l) => l.type === 'background')?.paint?.['background-color'];
                return applyStyle(layer, glStyle, { styleUrl }).then(() => {
                    if (background)
                        layer.on('prerender', (evt) => {
                            const ctx = evt.context;
                            ctx.save();
                            ctx.fillStyle = background;
                            ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);
                            ctx.restore();
                        });
                    layer.on('postrender', (evt) => {
                        const dim = 1 - layer.getOpacity();
                        if (dim <= 0) return;
                        const ctx = evt.context;
                        ctx.save();
                        ctx.globalAlpha = dim;
                        ctx.fillStyle = '#000';
                        ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);
                        ctx.restore();
                    });
                    layer.getSource()?.setAttributions(OPENFREEMAP_ATTRIBUTION);
                    if (layer === activeTileLayer)
                        document.getElementById("map_attributions").innerHTML = attributionHTML(layer);
                    refreshBaseMapRows();
                });
            })
            .catch((err) => {
                applied = false;
                console.error('basemap "' + title + '" failed to load:', err);
            });
    });
    addTileLayer(title, layer);
}

addVectorBasemap("OpenFreeMap Positron", 'https://tiles.openfreemap.org/styles/positron');
addVectorBasemap("OpenFreeMap Positron (no labels)", 'https://tiles.openfreemap.org/styles/positron', true);
addVectorBasemap("OpenFreeMap Bright", 'https://tiles.openfreemap.org/styles/bright');
addVectorBasemap("OpenFreeMap Liberty", 'https://tiles.openfreemap.org/styles/liberty');
addVectorBasemap("OpenFreeMap Dark", 'https://tiles.openfreemap.org/styles/dark');
addVectorBasemap("OpenFreeMap Dark (no labels)", 'https://tiles.openfreemap.org/styles/dark', true);

addTileLayer("Satellite", new ol.layer.Tile({
    source: new ol.source.XYZ({
        url: 'https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}',
        attributions: 'Esri et al.'
        // maxZoom is not specified, so it defaults to the OpenLayers default
    })
}));

addOverlayLayer("OpenSeaMap", new ol.layer.Tile({
    source: new ol.source.XYZ({
        url: 'https://tiles.openseamap.org/seamark/{z}/{x}/{y}.png',
        attributions: 'Map data: &copy; <a href="https://www.openseamap.org">OpenSeaMap</a>'
    })
}));

addOverlayLayer("NOAA", new ol.layer.Tile({
    source: new ol.source.TileWMS({
        url: 'https://gis.charttools.noaa.gov/arcgis/rest/services/MCS/ENCOnline/MapServer/exts/MaritimeChartService/WMSServer?',
        params: {
            'LAYERS': '1,2,3,4,5,6,7',
            'FORMAT': 'image/png',
            'TRANSPARENT': 'true',
            'VERSION': '1.3.0'
        },
        serverType: 'geoserver',
        attributions: 'Charts: &copy; <a href="https://nauticalcharts.noaa.gov">NOAA</a>'
    })
}));

initRainRadar(addOverlayLayer);


addOverlayLayer("Aircraft", planeLayer);

let urlParams = new URLSearchParams(window.location.search);
buildStatcard();
restoreDefaultSettings();

community.init({ config, getMap: () => map });
fireworks.init({ config, extraVector, showDialog, showNotification });
kiosk.init({
    getMap: () => map,
    showTargetcard,
    saveSettings,
});
measure.init({
    getShipsDB: () => shipsDB,
    showNotification,
    ensureMeasurecardVisible: () => { if (!measurecardVisible()) toggleMeasurecard(); },
    onMeasuresChanged: () => updateMeasureIndicator(),
});
binary.init({
    map: () => map,
    getStation: () => station,
    getStationName: () => config.station,
    openVessel: (mmsi) => { closeDialog(); closeSettings(); showTargetcard('ship', mmsi); },
    getActiveReceiver: () => activeReceiver,
    getShipsDB: () => shipsDB,
    saveSettings,
    redrawMap,
    // a marker's tooltip re-renders once its members arrive, if still hovered
    isHovered: (feature) => hover_feature === feature,
    isHoveringShip: (mmsi) => hoverType == 'ship' && hoverMMSI == mmsi,
    rehoverShip: (mmsi) => {
        const raw = shipsDB[mmsi]?.raw;
        if (raw && hoverType == 'ship' && hoverMMSI == mmsi) {
            showTooltipShip(hover_info, getTooltipContent(raw), lastHoverPixel, 15, raw.cog);
        }
    },
    rehover: (feature) => {
        if (hover_feature === feature && hoverType == 'tooltip') {
            startHover('tooltip', binary.markerTooltip(feature), lastHoverPixel, feature);
        }
    },
});
range.init({
    getConfig: () => config,
    getStation: () => station,
    getActiveReceiver: () => activeReceiver,
    getHoverFeature: () => hover_feature,
    showDialog,
    saveSettings,
    redrawMap,
});
boxselect.init({
    getMap: () => map,
    getShipsDB: () => shipsDB,
    showTracks: showTracksForMMSIs,
    showNotification,
});
let pluginActionCounter = 0;

targetcard.init({
    fitTargetcard,
    card: ui.card,
    getStyle: () => settings.shipcard_style,
    getTab: () => settings.shipcard_active_tab,
    isCramped: () => mapui.paneCramped(),
    foldOnOpen: () => mapui.paneCramped() && !targetcardVisible(),
    setTab: (t) => { settings.shipcard_active_tab = t; saveSettings(); },
    getReceiver: () => activeReceiver,
    realtimeEnabled: () => config.features.realtime,
    isFollowing,
    updateFocusMarker,
    hoverTrackShown: () => hover_enabled_track,
    selectTrackShown: () => select_enabled_track,
    registerAction: (action) => {
        if (typeof action === 'function') {
            const name = '_plugin_' + ++pluginActionCounter;
            ACTIONS[name] = action;
            return name;
        }
        if (typeof action === 'string' && ACTIONS[action]) return action;

        console.warn('addTargetcardItem: action must be a function or a registered ACTIONS key. Got:', action);
        return null;
    },
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

planecard.init({});

ticker.init({
    buckets: bucketChips(),
    bucketHidden: (b) => filter.isActive() && filter.isHidden("bucket", b),
    openVessel: (m) => openFocus(m, 14),
});

replay.init({
    getReceiver: () => activeReceiver,
    spriteFor,
    iconScale: markersLib.iconScale,
    // always fades: the icon-fading setting only governs the live map
    fadeOpacity: fadeCurve,
    labelText: buildLabelText,
    spriteSheet: SpritesAll,
    getResolution: () => map.getView().getResolution(),
    hullStyle: shapeStyleFunction,
    setLiveLayers: setLiveLayersVisible,
    filterPasses: (cls, knots) => {
        filterProbe.shipclass = cls;
        filterProbe.speed = knots;
        return filter.passesAppearance(filterProbe);
    },
    bucketFor: filter.bucketFor,
    onCounts: (counts) => {
        setShipCounts(counts);
        renderCounts();
    },
    showNotification,
    onStateChange: updateReplaycard,
    onFrame: applyFixedCenter,
});

console.log("Starting plugin code");

window.loadPlugins && window.loadPlugins();

console.log("Plugin loading completed");

console.log("Load settings");
loadSettings();
applyPanels();
updateReceiverSelect(config.receivers);

console.log("Load settings from URL parameters");

loadSettingsFromURL();
targetcard.setStyle(settings.shipcard_style);   // the card was built before the settings were read
applyDynamicStyling();
community.applySharingState();

if (config.features.managed) {
    const pollSharingState = async () => {
        if (document.hidden) return;
        try {
            const r = await fetch("api/sharing_state.json");
            if (!r.ok) return;
            const s = await r.json();
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

for (const [enabled, tab] of [
    [config.features.about_md, "about"],
    [config.features.realtime, "realtime"],
    [config.features.log, "log"],
    [config.features.decoder, "decoder"],
    [config.webcontrol_http, "webcontrol"],
]) {
    if (!enabled) {
        document.getElementById(tab + "_tab").style.display = "none";
        document.getElementById(tab + "_tab_mini").style.display = "none";
    }
}

if (!config.features.replay)
    document.querySelectorAll('.ctx-replay').forEach(el => { el.style.display = "none"; });

showWelcome();
announceStickyState();
kiosk.updateKiosk();
updateAndroid();

if (isAndroid()) showMenu();

// Re-apply chart colors after all stylesheets load (Firefox iframe quirk).
window.addEventListener('load', () => {
    requestAnimationFrame(() => {
        requestAnimationFrame(() => {
            plotsModule?.updateColors();
        });
    });
});

// A resize can cross the breakpoints mapTopInset and the ticker's width rules
// read, so the whole layout has to converge - not just the card. Debounced
// because a window drag fires this continuously and each pass measures.
window.addEventListener('resize', debounce(() => {
    applyPanels({ reposition: false });
    fitTargetcard();
    tablePerPage = 0;
    updateTablecard();
}, 150));
applyTargetcardPinStyling()
