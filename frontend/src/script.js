import { settings, isAndroid } from './core/state.js';
import { ShippingClass } from './core/constants.js';
import { SPEED_PALETTES, palette as validPalette, bucketColor, speedBucket, paletteCSS } from './core/palette.js';
import { debounce, decodeHTMLEntities, deriveLabelBackground, copyToClipboard, hexToRgb } from './core/util.js';
import { calcOffset1M, createShipOutlineGeometry, createDistanceGeometry, hasValidCoords } from './core/geo.js';
import { init as initRainRadar } from './overlays/rainradar.js';
import * as fireworks from './overlays/fireworks.js';
import * as community from './overlays/community.js';
import * as kiosk from './features/kiosk.js';
import * as measure from './features/measure.js';
import * as boxselect from './features/boxselect.js';
import * as replay from './features/replay.js';
import * as binary from './features/binary.js';
import * as range from './features/range.js';
import {
    getDistanceVal, getDistanceUnit,
    getSpeedVal, getSpeedUnit,
    getDimVal, getDimUnit, getDraughtVal, getShipDimension, getShipDimensionSVG, getSpeedHistorySVG,
    getDraughtChartSVG, getChangeListHTML, CHANGE,
    getLatValFormat, getLonValFormat,
    getEtaVal, getDeltaTimeVal,
    getShipName, getCallSign, setShipNameProvider, setCallSignProvider,
    getCountryName, getFlagStyled,
    getStatusVal, getMmsiTypeVal,
    getStringfromMsgType, getStringfromGroup, getStringfromChannels,
    getShipTypeShort, getShipTypeFull,
    sanitizeString, formatBytes, isHttpUrl, formatTime,
} from './core/format.js';

// Named imports (instead of `import * as ...`) so Vite tree-shakes everything
// outside this list. Plugins relying on other OL classes via window.ol will
// need to be updated; the contract is "what script.js + bundled plugins use".
import OlMap from 'ol/Map';
import OlView from 'ol/View';
import OlFeature from 'ol/Feature';
import TileLayer from 'ol/layer/Tile';
import VectorLayer from 'ol/layer/Vector';
import OSMSource from 'ol/source/OSM';
import XYZSource from 'ol/source/XYZ';
import TileWMSSource from 'ol/source/TileWMS';
import VectorSource from 'ol/source/Vector';
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
import { fromLonLat, toLonLat, transformExtent } from 'ol/proj';
import { getLength } from 'ol/sphere';
import { containsCoordinate, getWidth } from 'ol/extent';
import 'ol/ol.css';

const ol = {
    Map: OlMap,
    View: OlView,
    Feature: OlFeature,
    layer: { Tile: TileLayer, Vector: VectorLayer },
    source: { OSM: OSMSource, XYZ: XYZSource, TileWMS: TileWMSSource, Vector: VectorSource },
    geom: { Point, LineString, Polygon, Circle: CircleGeom },
    style: { Style, Stroke, Fill, Icon, Circle: CircleStyle, Text },
    proj: { fromLonLat, toLonLat, transformExtent },
    sphere: { getLength },
    extent: { containsCoordinate, getWidth },
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
//   v4: addShipcardItem() requires a function callback (CSP-clean).
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
    replayToggle: () => replayToggle(),
    replayCycleSpeed: () => replay.cycleSpeed(),
    replayToggleLabels: () => replay.toggleLabels(),
    replaySeek: (e, d, el) => replaySeek(el),

    // tableside / generic close buttons / dialog
    hideTablecard: () => hideTablecard(),
    updateTableSort: (e, dataset, el) => updateTableSort(e, dataset, el),
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
    setKiosk: (e, d, el) => kiosk.setKiosk(el.checked),
    setKioskRotationSpeed: (e, d, el) => kiosk.setKioskRotationSpeed(el.value),
    setKioskPanMap: (e, d, el) => kiosk.setKioskPanMap(el.checked),
    setGraphVisibility: (e, d, el) => setGraphVisibility(d.graph, el.checked),
    setPlotAbsoluteTime: (e, d, el) => setPlotAbsoluteTime(el.checked),
    setMapSetting: (e, d, el) => setMapSetting(d.key,
        el.type === 'checkbox' ? el.checked :
        (el.type === 'range' || el.type === 'number') ? Number(el.value) : el.value),
    setTrackHistory: (e, d, el) => setTrackHistory(TRACK_HISTORY_STOPS[el.value]),
    setBinaryDisplay: (e, d, el) => binary.setBinaryDisplay(el.value),
    setBinaryCategory: (e, d, el) => binary.setBinaryCategory(d.cat, el.checked),
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
    setMapOpacity: (e, d, el) => { settings.map_opacity = Number(el.value); setMapOpacity(); saveSettings(); },
    updateSliderDisplay: (e, d, el) => updateSliderDisplay(d.display, el.value),
    updateTrackHistoryDisplay: (e, d, el) => updateSliderDisplay('trackHistory', TRACK_HISTORY_STOPS[el.value]),

    // context menu (depends on global context_mmsi/card_mmsi)
    toggleShipcardPin: () => toggleShipcardPin(),
    pinStation: () => pinStation(),
    copyTextCtx: () => copyText(context_mmsi),
    copyTextICAO: () => copyText(getICAOFromHexIdent(context_mmsi)),
    downloadCSV: () => shipTableModule?.downloadCSV(),
    unpinCenter: () => unpinCenter(),
    showAllTracks: () => showAllTracks(),
    deleteAllTracks: () => deleteAllTracks(),
    resetTracksFromNow: () => resetTracksFromNow(),
    startBoxSelect: () => { boxselect.start(); showNotification('Drag a rectangle to enable tracks (Esc to cancel)'); },
    ToggleFireworks: () => fireworks.toggle(),
    toggleLabel: () => toggleLabel(),
    toggleKioskMode: () => kiosk.toggleKioskMode(),
    toggleFading: () => toggleFading(),
    toggleRange: () => range.toggleRange(),
    toggleTrackCtx: () => toggleTrack(context_mmsi),
    pinVesselCtx: () => pinVessel(context_mmsi),
    mapResetViewZoomCtx: () => mapResetViewZoom(13, context_mmsi),
    showShipcardCtx: (e, d) => showShipcard(d.kind, context_mmsi),
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
    toggleShipcardSize: () => toggleShipcardSize(),
    shipcardSelectSelf: (e, d, el) => shipcardselect(el),
    shipcardContextMenu: (e) => showContextMenu(e, card_mmsi, card_type, ['object', 'object-map', 'ctx-shipcard']),
    showShipcardClose: () => showShipcard(null, null),
    showBinaryMessageDialogCard: () => binary.showBinaryMessageDialog(card_mmsi),
    openRealtimeForMMSICard: async () => {
        if (!realtimeModule) realtimeModule = await import('./tabs/realtime.js');
        realtimeModule.openForMMSI(card_mmsi);
    },
    openAISCatcherSiteCard: () => openExt('aiscatcher', card_mmsi),
    toggleTrackCard: () => toggleTrack(card_mmsi),
    openFlightAwareCard: () => openExt('flightaware', card_mmsi),
    openPlaneSpottersCard: () => openExt('planespotters', card_mmsi),
    openADSBExchangeCard: () => openExt('adsbexchange', card_mmsi),
    toggleStatcard: () => toggleStatcard(),
    toggleTablecard: () => toggleTablecard(),
    mapSettingsContextMenu: (e, d, el) => showContextMenu(e, '', '', ['settings', 'ctx-map'], el),
    toggleCommunityPane: () => community.toggleCommunityPane(),
    showMapMenu: (e) => showMapMenu(e),
    toggleAttribution: () => toggleAttribution(),
    mainspaceContextMenu: (e) => showContextMenu(e, 0, '', ['settings']),
    plotsContextMenu: (e) => showContextMenu(e, '', 'charts', ['settings', 'ctx-charts']),

    // info panel
    showAboutFromInfoPanel: () => { toggleInfoPanel(); showAboutDialog(); },

    // dynamically-rendered shipcard items
    rotateShipcardIcons: () => rotateShipcardIcons(),
    techInfo: (e) => { e.stopPropagation(); toggleShipcardPopover("tech_popover", "shipcard_tech_info"); },
    shiptypeInfo: (e) => { e.stopPropagation(); toggleShipcardPopover("shiptype_popover", "shipcard_shiptype_info"); },
    shipHistory: (e) => { e.stopPropagation(); openShipcardSection("changes"); },
    toggleShipcardSection: (e, d, el) => { e.stopPropagation(); toggleShipcardSection(el?.dataset.section || d.section); },
    showNMEAContextCopy: (e, d) => showContextMenu(e, d.copy || '', 'ship', ['settings', 'copy-text']),
    removeRealtimeFilter: (e, d) => realtimeModule?.removeFilter(d.kind, d.value),
};

// Public plugin API. Mutable state uses getters so plugins always see the
// live value rather than a snapshot taken at namespace-build time.
window.AISCatcher = {
    PLUGIN_API_VERSION,

    addShipcardItem,
    ACTIONS,

    showDialog,
    closeDialog,
    showNotification,
    showShipcard,
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
// (addShipcardItem, card_mmsi, ...) still resolve. Module top-level no
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
    get shipsSince() { return shipsSince; },
    get receivers() { return config.receivers || []; },
    get setReceiver() { return onReceiverChange; },
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
// the dialog is absent here: its modal component closes itself on Escape
document.addEventListener("keydown", (e) => {
    if (e.key !== "Escape") return;
    if (dialogModal && dialogModal.isOpen()) return;
    if (document.querySelector(".settings_window").classList.contains("active")) closeSettings();
    else if (document.getElementById("menubar").classList.contains("visible")) hideMenu();
});

if (document.body) _bindDelegatedActions();
else document.addEventListener('DOMContentLoaded', _bindDelegatedActions);

let interval,
    activeReceiver = 0,
    lastPathFetch = 0,
    lastFullPathFetch = 0,
    paths = {},
    trackCutoff = 0,
    pathsFrom = -1,
    map,
    basemaps = {},
    overlapmaps = {},
    station = {},
    shipsDB = {},
    shipsSince = 0,
    shipsTimeout = 1800,
    shipsLastCleanup = 0,
    planesDB = {},
    planesSince = 0,
    planesTimeout = 300,
    planesLastCleanup = 0,
    hover_feature = undefined,
    logViewer = null,
    tab_title_station = config.station,
    tab_title_count = null,
    context_mmsi = null,
    context_type = null,
    tab_title = "AIS-catcher";

const baseMapSelector = document.getElementById("baseMapSelector");

let shipcardIconCount = undefined;
const shipcardIconMax = 3;
let shipcardIconOffset = 0;
let card_mmsi = null,
    card_type = null;
// `let` so AISCatcher.setRefreshInterval can mutate.
let refreshIntervalMs = 2500;
let shipFilterOverride = null;
let updateInProgress = false;
let activeTileLayer = undefined;
let hover_enabled_track = false,
    select_enabled_track = false,
    marker_tracks = new Set();

let center, shipcard;
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
        map_night: "Dark Matter (no labels)",
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
        show_circle_outline: false,
        circle_scale: 6.0,
        dark_mode: false,
        center_radius: 0,
        show_station: true,
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
        show_track_on_hover: false,
        show_track_on_select: false,
        show_all_tracks: false,
        shipcard_pinned: false,
        shipcard_top_left: false,
        show_signal_graphs: true,
        show_ppm_graphs: true,
        plot_absolute_time: true,
        shipcard_pinned_x: null,
        shipcard_pinned_y: null,
        kiosk_rotation_speed: 5,
        kiosk_pan_map: true,
        shiptable_columns: ["shipname", "mmsi", "imo", "callsign", "shipclass", "lat", "lon", "last_signal", "level", "distance", "bearing", "speed", "repeat", "ppm", "status"],
        realtime_background_streaming: false,
        realtime_filters: [],
        binary_messages: "highlight",
        binary_exclude: []
};

function restoreDefaultSettings() {
    // In-place mutation: `settings` is imported, the binding is read-only here.
    for (const k of Object.keys(settings)) delete settings[k];
    Object.assign(settings, JSON.parse(JSON.stringify(DEFAULT_SETTINGS)));
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
    restoreDefaultSettings();

    settings.android = android;
    settings.dark_mode = darkmode;

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

const getICAOFromHexIdent = (h) => h.toString(16).toUpperCase().padStart(6, '0')
const getICAO = (plane) => getICAOFromHexIdent(plane.hexident)
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
    { title: "System", subs: [["System", "General"]] },
    {
        title: "Map", subs: [
            ["Map", "General"], ["Ship Labels", "Labels"], ["Ship Outline", "Ships"],
            ["Binary Messages", "Binary"], ["Station Range", "Range"], ["Tracks", "Tracks"],
            ["Kiosk", "Kiosk"]
        ]
    },
    { title: "Plots", subs: [["Graphs", "Plots"]] },
    { title: "Table", subs: [["Table", "Table"]] },
];

let settingsGroups = [];
let settingsSubNav = null;
let settingsSubWrap = null;
let settingsScrollers = [];

function makeSettingsTab(label, activate) {
    const tab = document.createElement("div");
    tab.className = "settings_tab";
    tab.textContent = label;
    tab.setAttribute("role", "tab");
    tab.tabIndex = 0;
    tab.onclick = activate;
    tab.onkeydown = (e) => {
        if (e.key === "Enter" || e.key === " ") {
            e.preventDefault();
            activate();
        }
    };
    return tab;
}

function buildSettingsTabs() {
    const main = document.querySelector(".settings_main");

    const parsed = {};
    let currentName = null;
    for (const child of Array.from(main.children)) {
        if (child.tagName === "HEADER") {
            currentName = child.textContent.trim();
            parsed[currentName] = { header: child, sections: [] };
        } else if (child.tagName === "SECTION" && currentName) {
            parsed[currentName].sections.push(child);
        }
    }
    if (Object.keys(parsed).length < 2) return;

    settingsGroups = SETTINGS_TAB_GROUPS
        .map((g) => ({
            title: g.title,
            subs: g.subs.filter(([name]) => name in parsed).map(([name, label]) => ({ label, ...parsed[name] })),
        }))
        .filter((g) => g.subs.length > 0);

    const mapped = new Set(SETTINGS_TAB_GROUPS.flatMap((g) => g.subs.map(([name]) => name)));
    for (const name of Object.keys(parsed)) {
        if (!mapped.has(name)) settingsGroups.push({ title: name, subs: [{ label: name, ...parsed[name] }] });
    }

    const nav = document.createElement("nav");
    nav.className = "settings_tabs";
    nav.setAttribute("role", "tablist");
    settingsGroups.forEach((g, i) => {
        nav.appendChild(makeSettingsTab(g.title, () => selectSettingsGroup(i)));
    });
    const headerSlot = document.querySelector("#settings .settings_header .hdr-slot");
    headerSlot.appendChild(makeTabScroller(nav).wrap);

    settingsSubNav = document.createElement("nav");
    settingsSubNav.className = "settings_tabs settings_subtabs";
    settingsSubNav.setAttribute("role", "tablist");
    settingsSubWrap = makeTabScroller(settingsSubNav, "settings_nav").wrap;
    main.prepend(settingsSubWrap);

    selectSettingsGroup(0);
}

function makeTabScroller(nav, extraClass) {
    const { wrap, update } = window.AISComponents.tabScroller(nav, extraClass);
    settingsScrollers.push(update);
    return { wrap, sync: update };
}

function updateSettingsChevrons() {
    settingsScrollers.forEach((sync) => sync());
}

function selectSettingsGroup(idx) {
    const group = settingsGroups[idx];
    document.querySelectorAll(".settings_tabs:not(.settings_subtabs) .settings_tab")
        .forEach((t, i) => {
            t.classList.toggle("active", i === idx);
            t.setAttribute("aria-selected", i === idx);
        });

    settingsSubNav.innerHTML = "";
    settingsSubWrap.style.display = group.subs.length > 1 ? "" : "none";
    group.subs.forEach((sub, i) => {
        settingsSubNav.appendChild(makeSettingsTab(sub.label, () => selectSettingsSub(idx, i)));
    });
    selectSettingsSub(idx, 0);
    settingsSubNav.scrollLeft = 0;
    updateSettingsChevrons();
}

function selectSettingsSub(groupIdx, subIdx) {
    const shown = new Set(settingsGroups[groupIdx].subs[subIdx].sections);
    const main = document.querySelector(".settings_main");
    let first = true;
    for (const child of main.children) {
        if (child.tagName === "HEADER") child.style.display = "none";
        else if (child.tagName === "SECTION") {
            const show = shown.has(child);
            child.style.display = show ? "" : "none";
            child.classList.toggle("st-first", show && first);
            if (show) first = false;
        }
    }
    settingsSubNav.querySelectorAll(".settings_tab").forEach((t, i) => {
        t.classList.toggle("active", i === subIdx);
        t.setAttribute("aria-selected", i === subIdx);
    });
    main.scrollTop = 0;
}

function openSettings() {
    updateSettingsTab();
    document.querySelector(".settings_window").classList.add("active");
}

function closeSettings() {
    document.querySelector(".settings_window").classList.remove("active");
}

function closeTableSide() {
    document.querySelector(".tableside_window").classList.remove("active");
}

function setCoordinateFormat(format) {
    settings.coordinate_format = format;
    saveSettings();

    refresh_data();
    shipTableModule?.reset();
}


async function copyCoordinates(m) {
    const raw = shipsDB[m]?.raw;
    if (!raw) {
        showNotification("Ship not found", "error");
        return;
    }
    if (await copyClipboard(raw.lat + "," + raw.lon)) showNotification("Coordinates copied to clipboard", "success");
}

let hoverMMSI = undefined;
let hoverType = undefined;

const shapeStyleFunction = function (feature) {

    const [r, g, b] = hexToRgb(settings.shipoutline_inner);
    const o = settings.shipoutline_opacity;

    return new ol.style.Style({
        fill: new ol.style.Fill({
            color: `rgba(${r}, ${g}, ${b}, ${o})`
        }),
        stroke: new ol.style.Stroke({
            color: hoverMMSI && hoverType == 'ship' && feature.ship.mmsi == hoverMMSI ? settings.shiphover_color : settings.shipoutline_border,
            width: 2
        }),
    });
}

const trackStyleFunction = function (feature) {
    let w = Number(settings.track_weight);
    let c = '#12a5ed'; // Default fallback color
    let highlighted = false;

    // Speed colors are baked per segment when the track is built; otherwise
    // the whole track takes the color of its shipping class
    if (feature.speedColor) {
        c = feature.speedColor;
    } else if (feature.shipclass && settings.track_class_colors[feature.shipclass]) {
        c = settings.track_class_colors[feature.shipclass];
    }

    if (feature.mmsi == hoverMMSI && hoverType == 'ship') {
        c = settings.shiphover_color;
        w = w + 2;
        highlighted = true;
    }
    else if (feature.mmsi == card_mmsi && card_type == 'ship') {
        c = settings.shipselection_color;
        w = w + 2;
        highlighted = true;
    }

    const o = Number(settings.track_opacity);
    if (!highlighted && o < 1) {
        const [r, g, b] = hexToRgb(c);
        c = `rgba(${r}, ${g}, ${b}, ${o})`;
    }

    return new ol.style.Style({
        stroke: new ol.style.Stroke({
            color: c,
            width: w,
            lineDash: feature.isDashed ? [6, 6] : undefined
        })
    });
}

// icon scale bucket by vessel length, shared with replay
function iconScale(length) {
    return length >= 100 && length <= 200 ? 0.9 : length > 200 ? 1.1 : 0.75;
}

const markerStyle = function (feature) {

    const mult = iconScale((feature.ship.to_bow || 0) + (feature.ship.to_stern || 0));
    const highlighted = (feature.ship.mmsi == hoverMMSI && hoverType == 'ship') || (feature.ship.mmsi == card_mmsi && card_type == 'ship');

    return new ol.style.Style({
        image: new ol.style.Icon({
            src: SpritesAll,
            rotation: feature.ship.rot,
            offset: [feature.ship.cx, feature.ship.cy],
            size: [feature.ship.imgSize, feature.ship.imgSize],
            scale: settings.icon_scale * mult,
            opacity: highlighted ? 1 : getShipOpacity(feature.ship)
        })
    });
};


const planeStyle = function (feature) {
    return [
        new ol.style.Style({
            image: new ol.style.Icon({
                src: SpritesAll,
                rotation: feature.plane.rot,
                offset: [feature.plane.cx, feature.plane.cy],
                size: [feature.plane.imgSize, feature.plane.imgSize],
                scale: settings.icon_scale * feature.plane.scaling,
                opacity: 1
            })
        })
    ];
};

// Text portion of a map label, shared with the replay layer via replay.init.
// `cls` drives the class-colored background mode, `op` fades with the vessel.
function buildLabelText(txt, op, cls) {
    const text = new ol.style.Text({
        text: txt,
        overflow: true,
        offsetY: 25,
        offsetX: 25,
        font: settings.tooltipLabelFontSize + "px Arial"
    });

    if (settings.label_class_background) {
        const base = settings.track_class_colors[cls] || '#12a5ed';
        text.setFill(new ol.style.Fill({ color: `rgba(255, 255, 255, ${op})` }));
        text.setBackgroundFill(new ol.style.Fill({ color: deriveLabelBackground(base, 0.88 * op) }));
        text.setBackgroundStroke(new ol.style.Stroke({ color: `rgba(0, 0, 0, ${0.35 * op})`, width: 1 }));
        text.setPadding([2, 4, 2, 4]);
    } else {
        const [lr, lg, lb] = hexToRgb(settings.dark_mode ? settings.tooltipLabelColorDark : settings.tooltipLabelColor);
        const [sr, sg, sb] = hexToRgb(settings.dark_mode ? settings.tooltipLabelShadowColorDark : settings.tooltipLabelShadowColor);
        text.setFill(new ol.style.Fill({ color: `rgba(${lr}, ${lg}, ${lb}, ${op})` }));
        text.setStroke(new ol.style.Stroke({ color: `rgba(${sr}, ${sg}, ${sb}, ${op})`, width: 5 }));
    }
    return text;
}

const labelStyle = function (feature) {
    const isActive = (card_type === 'ship' && 'ship' in feature && feature.ship.mmsi == card_mmsi) ||
                     (card_type === 'plane' && 'plane' in feature && feature.plane.hexident == card_mmsi);

    if (settings.labels_active_only && !isActive) return new ol.style.Style({});

    const isHovered = 'ship' in feature && hoverType === 'ship' && feature.ship.mmsi == hoverMMSI;
    const op = !('ship' in feature) || isActive || isHovered ? 1 : getShipOpacity(feature.ship);

    const obj = 'ship' in feature ? feature.ship : feature.plane;
    const text = buildLabelText(
        decodeHTMLEntities('ship' in feature ?
            (feature.ship.shipname || feature.ship.mmsi.toString()) :
            (feature.plane.callsign || getICAO(feature.plane))),
        op, obj.shipclass);

    const isSelected = (settings.labels_prioritize_active ?? true) && isActive;

    return new ol.style.Style({ text: text, zIndex: isSelected ? 1000 : 0 });
};

const hoverCircleStyleFunction = function (feature) {
    const iconScale = settings.icon_scale || 1.0;
    const circleScale = settings.circle_scale || 6.0;
    const radiusScale = 1 + (circleScale - 2.0) * 0.08; // Scale radius slightly with line width
    return new ol.style.Style({
        image: new ol.style.Circle({
            radius: 16 * iconScale * radiusScale,
            stroke: new ol.style.Stroke({
                color: settings.shiphover_color,
                width: circleScale * iconScale
            })
        })
    });
}

const selectCircleStyleFunction = function (feature) {
    const iconScale = settings.icon_scale || 1.0;
    const circleScale = settings.circle_scale || 6.0;
    const radiusScale = 1 + (circleScale - 2.0) * 0.08; // Scale radius slightly with line width
    const styles = [new ol.style.Style({
        image: new ol.style.Circle({
            radius: 13 * iconScale * radiusScale,
            stroke: new ol.style.Stroke({
                color: settings.shipselection_color,
                width: circleScale * iconScale
            })
        })
    })];

    if (card_type === 'ship') {
        const ship = shipsDB[card_mmsi]?.raw;
        if (ship && ship.imgSize) styles.push(markerStyle({ ship }));
    } else if (card_type === 'plane') {
        const plane = planesDB[card_mmsi]?.raw;
        if (plane && plane.imgSize) styles.push(...planeStyle({ plane }));
    }

    return styles;
}

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

const TOOLTIP_FLAG_STYLE = "padding: 0px; margin: 0px; margin-right: 10px; margin-left: 3px; box-shadow: 1px 1px 2px rgba(0, 0, 0, 0.2); font-size: 26px; opacity: 70%";
const CARD_FLAG_STYLE = "padding: 0px; margin: 0px; margin-right: 5px; box-shadow: 2px 2px 3px rgba(0, 0, 0, 0.5); font-size: 26px;";

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

const mapMenu = document.getElementById("map-menu");

function hideMapMenu(event) {

    if (!mapMenu.contains(event.target)) {
        mapMenu.style.display = "none";
        document.removeEventListener("click", hideMapMenu);
    }
}

function showMapMenu(event) {

    hideContextMenu();
    if (event && event.preventDefault) {
        event.preventDefault();
        event.stopPropagation();
    }

    baseMapSelector.value = settings.dark_mode ? settings.map_night : settings.map_day;
    refreshOverlayCredits();

    mapMenu.style.display = "block";

    mapMenu.style.left = "50%";
    mapMenu.style.top = "50%";
    mapMenu.style.transform = "translate(-50%, -50%)";

    document.addEventListener("click", hideMapMenu);
}

const contextMenu = document.getElementById("context-menu");

function hideContextMenu(event) {
    contextMenu.style.display = "none";
    document.removeEventListener("click", hideContextMenu);
}

function showContextMenu(event, mmsi, type, context, anchorEl) {

    if (event && event.preventDefault) {
        event.preventDefault();
        event.stopPropagation();
    }

    hideMapMenu(event);

    document.getElementById("ctx_labelswitch").textContent = settings.show_labels != "never" ? "Hide ship labels" : "Show ship labels";
    document.getElementById("ctx_range").textContent = settings.show_range ? "Hide station range" : "Show station range";
    document.getElementById("ctx_fading").textContent = settings.fading ? "Show icons without fading" : "Show icons with fading";
    document.getElementById("ctx_fireworks").textContent = fireworks.isRunning() ? "Stop Fireworks Mode" : "Start Fireworks Mode";
    document.getElementById("ctx_label_shipcard_pin").textContent = settings.shipcard_pinned ? "Unpin Shipcard position" : "Pin Shipcard position";
    document.getElementById("ctx_signal_graphs").textContent = settings.show_signal_graphs ? "Hide signal level graphs" : "Show signal level graphs";
    document.getElementById("ctx_ppm_graphs").textContent = settings.show_ppm_graphs ? "Hide frequency shift graphs" : "Show frequency shift graphs";

    context_mmsi = mmsi;
    context_type = type;

    const classList = ["station", "settings", "plane-map", "ship-map", "plane", "ship", "ctx-map", "copy-text", "table-menu", "ctx-shipcard", "ctx-charts"];

    if (context.includes('object')) {
        context.push(type);
    }
    if (context.includes('object-map')) {
        context.push(type + "-map");
    }

    classList.forEach((className) => {
        const shouldDisplay = context.includes(className);
        const elements = document.querySelectorAll("." + className);
        elements.forEach((element) => {
            element.style.display = shouldDisplay ? "flex" : "none";
        });
    });

    // Hide realtime menu items if realtime is disabled
    if (!config.features.realtime) {
        document.querySelectorAll('.ctx-realtime').forEach((element) => {
            element.style.display = "none";
        });
    }

    if (!config.features.replay) {
        document.querySelectorAll('.ctx-replay').forEach((element) => {
            element.style.display = "none";
        });
    }

    // we might have made non-android items visible in the context menu, so hide non-android items if needed
    updateAndroid();
    kiosk.updateKiosk();

    if (settings.show_all_tracks) {
        document.querySelectorAll(".ctx-noalltracks").forEach(function (element) {
            element.style.display = "none";
        });
    }
    if (settings.show_all_tracks || marker_tracks.size > 0) {
        document.querySelectorAll(".ctx-removealltracks").forEach(function (element) {
            element.style.display = "flex";
        });
    } else {
        document.querySelectorAll(".ctx-removealltracks").forEach(function (element) {
            element.style.display = "none";
        });
    }

    document.getElementById("ctx_menu_unpin").style.display = settings.fix_center && context.includes("ctx-map") ? "flex" : "none";
    document.getElementById("ctx_track").innerText = trackOptionString(context_mmsi);

    contextMenu.style.display = "block";

    if (anchorEl) {
        // Anchor above the control button (it sits near the bottom edge), so the
        // menu unfurls upward instead of running off-screen.
        contextMenu.style.transform = "none";
        const btn = anchorEl.getBoundingClientRect();
        const rect = contextMenu.getBoundingClientRect();
        let left = Math.max(8, btn.right - rect.width);
        let top = btn.top - rect.height - 8;
        if (top < 8) top = Math.min(btn.bottom + 8, window.innerHeight - rect.height - 8);
        contextMenu.style.left = left + "px";
        contextMenu.style.top = top + "px";
    } else if (context.includes("center")) {
        contextMenu.style.left = "50%";
        contextMenu.style.top = "50%";
        contextMenu.style.transform = "translate(-50%, -50%)";
    } else {
        contextMenu.style.left = event.pageX + 5 + "px";
        contextMenu.style.top = event.pageY + 5 + "px";
        contextMenu.style.transform = "none";

        const contextMenuRect = contextMenu.getBoundingClientRect();
        let viewportWidth = window.innerWidth && window.outerWidth ? Math.min(window.innerWidth, window.outerWidth) : document.documentElement.clientWidth;
        let viewportHeight = window.innerHeight && window.outerHeight ? Math.min(window.innerHeight, window.outerHeight) : document.documentElement.clientHeight;

        const maxX = viewportWidth - contextMenuRect.width;
        const maxY = viewportHeight - contextMenuRect.height;

        const adjustedX = Math.max(0, Math.min(event.pageX + 5, maxX));
        const adjustedY = Math.max(0, Math.min(event.pageY + 5, maxY));

        contextMenu.style.left = adjustedX + "px";
        contextMenu.style.top = adjustedY + "px";
    }

    document.addEventListener("click", hideContextMenu);
}

let dialogModal = null;

function showDialog(title, message, hideTitle) {
    if (!dialogModal) {
        dialogModal = window.AISComponents.modal({
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
    return window.AISComponents.toast(type, message, duration, "notification-container");
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

function setMap(key) {
    if (settings.dark_mode)
        settings.map_night = key;
    else
        settings.map_day = key;

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

function getLayerOpacity(title) {
    const v = settings.layer_opacity[title];
    return Number(v === undefined ? settings.map_opacity : v);
}

function setLayerOpacity(title, value) {
    settings.layer_opacity[title] = Number(value);
    overlapmaps[title]?.setOpacity(Number(value));
}

function setMapOpacity() {
    for (let key in basemaps)
        basemaps[key].setOpacity(Number(settings.map_opacity));

    for (let key in overlapmaps)
        overlapmaps[key].setOpacity(getLayerOpacity(key));

    syncOverlayDimmers();
}

let clickTimeout = undefined;
const handleClick = function (pixel, target, event) {
    const feature = target.closest('.ol-control') ? undefined : map.forEachFeatureAtPixel(pixel,
        function (feature) { if ('ship' in feature || 'plane' in feature || 'link' in feature || 'binary' in feature) { return feature; } }, { hitTolerance: 10 });

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
    } else if (feature && feature.binary === true && !feature.is_associated) {
        closeDialog();
        closeSettings();
        binary.showBinaryMessageDialog(feature);
        return;
    } else if (feature && 'ship' in feature) {
        closeDialog();
        closeSettings();
        showShipcard('ship', feature.ship.mmsi, pixel);
    }
    else if (feature && 'plane' in feature) {
        closeDialog();
        closeSettings();
        showShipcard('plane', feature.plane.hexident, pixel);
    }
    else {
        clickTimeout = setTimeout(function () {
            showShipcard(null, null);
            clickTimeout = null;
        }, 300);
    }
};

function initMap() {

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

    [trackLayer, rangeLayer, shapeLayer, markerLayer, labelLayer, extraLayer, binaryLayer, measure.measureVector,
     replay.hullLayer, replay.markerLayer].forEach(layer => {
        map.addLayer(layer);
    });

    triggerMapLayer();

    map.on('movestart', function () {
        stopHover();
    });

    map.on('moveend', function (evt) {
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

        const f = getFeature(map.getEventPixel(evt), map.getTargetElement())

        if (!f)
            showContextMenu(evt, 0, null, ['settings', 'ctx-map']);
        else if ('station' in f) {
            showContextMenu(evt, null, null, ["station", "ctx-map"]);
        }
        else if ('ship' in f)
            showContextMenu(evt, f.ship.mmsi, 'ship', ["ship", "ship-map"]);
        else if ('plane' in f)
            showContextMenu(evt, f.plane.hexident, 'plane', ["plane", "plane-map"]);
    });

    baseMapSelector.innerHTML = '';

    Object.keys(basemaps).forEach(key => {
        const option = document.createElement("option");
        const credit = attributionPlain(basemaps[key]);
        option.value = key;
        option.textContent = credit ? `${key} — ${credit}` : key;
        option.title = option.textContent;
        baseMapSelector.appendChild(option);
    });
    baseMapSelector.value = settings.dark_mode ? settings.map_night : settings.map_day;

    baseMapSelector.addEventListener('change', function () { setMap(this.value); });

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

function updateMarkerCountTooltip() {
    if (shipsDB == null) {
        ["statcard_stationary", "statcard_moving", "statcard_class_b_stationary", "statcard_class_b_moving", "statcard_station", "statcard_aton", "statcard_heli", "statcard_sarte"].forEach(function (id) {
            document.getElementById(id).innerHTML = "";
        });

        return;
    }

    let cStationary = 0,
        cMoving = 0,
        cClassBstationary = 0,
        cClassBmoving = 0,
        cStation = 0,
        cAton = 0,
        cHeli = 0,
        cSarte = 0;

    for (let key of Object.keys(shipsDB)) {
        let ship = shipsDB[key].raw;
        switch (ship.shipclass) {
                case ShippingClass.ATON:
                    cAton++;
                    break;
                case ShippingClass.PLANE:
                case ShippingClass.HELICOPTER:
                    cHeli++;
                    break;
                case ShippingClass.STATION:
                    cStation++;
                    break;
                case ShippingClass.SARTEPIRB:
                    cSarte++;
                    break;
                case ShippingClass.B:
                    if (ship.speed != null && ship.speed > 0.5) cClassBmoving++;
                    else cClassBstationary++;
                    break;
                default:
                    if (ship.speed != null && ship.speed > 0.5) cMoving++;
                    else cStationary++;
                    break;
            }
    }

    const counts = {
        statcard_moving: cMoving,
        statcard_stationary: cStationary,
        statcard_class_b_moving: cClassBmoving,
        statcard_class_b_stationary: cClassBstationary,
        statcard_aton: cAton,
        statcard_station: cStation,
        statcard_sarte: cSarte,
        statcard_heli: cHeli,
    };
    for (const [id, v] of Object.entries(counts)) {
        flashNumber(id, v);
        const item = document.getElementById(id)?.closest('.stat-item');
        if (item) item.dataset.zero = v === 0 ? 'true' : 'false';
    }
}

function updateTableSort(event, dataset, header) {
    const column = header.getAttribute("data-column");
    const currentOrder = header.classList.contains("ascending") ? "ascending" : "descending";

    const newOrder = currentOrder === "descending" ? "ascending" : "descending";

    settings.tableside_column = column;
    settings.tableside_order = newOrder;

    saveSettings();
    updateSortMarkers();
    updateTablecard();
}

function updateSortMarkers() {
    const allHeaders = document.querySelectorAll("[data-column]");

    allHeaders.forEach((otherHeader) => {
        otherHeader.classList.remove("ascending");
        otherHeader.classList.remove("descending");

        if (otherHeader.getAttribute("data-column") === settings.tableside_column) {
            otherHeader.classList.add(settings.tableside_order);
        }
    });
}

function compareNumber(valueA, valueB) {
    if (valueA == null) return settings.tableside_order === "ascending" ? 1 : -1;
    if (valueB == null) return settings.tableside_order === "ascending" ? -1 : 1;
    return valueA - valueB;
}

function compareString(valueA, valueB) {
    if (valueA == null && valueB == null) return 0;
    if (valueA == null) return 1;
    if (valueB == null) return -1;

    return (valueA + "").localeCompare(valueB + "");
}

document.getElementById('shipSearchSide').addEventListener('input', updateTablecard);

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

    const filter = document.getElementById('shipSearchSide').value.toLowerCase();

    const rows = [];
    let addedRows = 0;

    for (let i = 0; i < shipKeys.length && addedRows < 200; i++) {
        const key = shipKeys[i];
        if (!(key in shipsDB)) continue;
        const ship = shipsDB[key].raw;
        const shipName = String(getShipName(ship) || ship.mmsi);
        if (filter && !shipName.toLowerCase().includes(filter)) continue;

        const dist = ship.distance != null ? (getDistanceVal(ship.distance) + (ship.repeat > 0 ? " (R)" : "")) : "";
        const distTitle = ship.distance != null ? getDistanceVal(ship.distance) + " " + getDistanceUnit() + (ship.repeat > 0 ? " (R)" : "") : "";
        const spd = ship.speed != null ? getSpeedVal(ship.speed) : "";
        const spdTitle = ship.speed != null ? getSpeedVal(ship.speed) + " " + getSpeedUnit() : "";

        rows.push(`<tr data-mmsi="${ship.mmsi}">` +
            `<td>${getFlagStyled(ship.country, "padding:0;margin:0;box-shadow:1px 1px 2px rgba(0,0,0,0.2);font-size:16px;")}</td>` +
            `<td>${shipName}</td>` +
            `<td title="${distTitle}">${dist}</td>` +
            `<td title="${spdTitle}">${spd}</td>` +
            `<td>${getTableShiptype(ship)}</td>` +
            `<td>${getDeltaTimeVal(shipsSince - ship.last_signal)}</td>` +
            `</tr>`);
        addedRows++;
    }

    tableBody.innerHTML = rows.join('');

    tableBody.onmouseover = function(e) {
        const tr = e.target.closest('tr[data-mmsi]');
        if (tr) startHover('ship', parseInt(tr.dataset.mmsi));
    };
    tableBody.onmouseout = function(e) { stopHover(); };
    tableBody.onclick = function(e) {
        const tr = e.target.closest('tr[data-mmsi]');
        if (tr) showShipcard('ship', parseInt(tr.dataset.mmsi));
    };
    tableBody.oncontextmenu = function(e) {
        const tr = e.target.closest('tr[data-mmsi]');
        if (tr) showContextMenu(e, parseInt(tr.dataset.mmsi), "ship", ["object", "object-map"]);
    };
}

function flashNumber(id, newValue) {

    const element = document.getElementById(id);
    const oldValue = parseInt(element.innerText) || 0;

    if (newValue != oldValue) {
        element.classList.add("flash-up");
    }

    element.innerText = newValue;

    setTimeout(() => {
        element.classList.remove("flash-up");
    }, 500);
}

function updateMarkerCount() {

    let count = 0;
    if (shipsDB != null) {
        count = Object.keys(shipsDB).length;
    }

    flashNumber("markerCount", count);

    if (document.getElementById("statcard").style.display == "block") updateMarkerCountTooltip();
}

function toggleStatcard() {
    if (document.getElementById("statcard").style.display == "block") hideStatcard();
    else showStatcard();
}

function showStatcard() {
    updateMarkerCountTooltip();
    document.getElementById("statcard").style.display = "block";
}

function hideStatcard() {
    document.getElementById("statcard").style.display = "none";
}

function toggleTablecard() {
    if (!document.getElementById("tableside").classList.contains("active") && window.innerWidth < 800) {
        settings.tab = "ships";
        selectTab();
        return;
    }

    document.getElementById("tableside").classList.toggle("active");
    let elements = document.querySelectorAll(".map-button-box");
    elements.forEach(function (element) {
        element.classList.toggle("active");
    });

    updateTablecard();
}

function hideTablecard() {
    if (document.getElementById("tableside").classList.contains("active")) {
        toggleTablecard();
    }
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
        "mmsi", "lat", "lon", "distance", "bearing",
        "heading", "cog", "speed", "status", "level", "ppm",
        "count", "msg_type", "last_signal", "last_group", "group_mask",
        "flags", "altitude", "received_stations",
        "mmsi_type", "shipclass", "country"
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
        shipsDB = {};
        station = {};
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
            } else {
                shipsDB[mmsi] = { raw: s };
            }
        });
    }

    // Process dynamic data (position/signal)
    if (ships.dynamic) {
        ships.dynamic.forEach((v) => {
            const s = Object.fromEntries(dynamicKeys.map((k, i) => [k, v[i]]));

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
    shipsSince = serverTime;

    // periodically expire ships older than timeout
    if (isIncremental && serverTime - shipsLastCleanup > shipsTimeout / 2) {
        for (const mmsi in shipsDB) {
            if (serverTime - shipsDB[mmsi].raw.last_signal > shipsTimeout)
                delete shipsDB[mmsi];
        }
        shipsLastCleanup = serverTime;
    }

    if (Object.hasOwn(ships, "station")) station = ships.station;

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

    if (!isIncremental) planesDB = {};

    if (planes.values) {
        planes.values.forEach((v) => {
            const p = Object.fromEntries(keys.map((k, i) => [k, v[i]]));

            p.shipclass = ShippingClass.PLANE;
            p.validated = 1;
            p.name = p.callsign || p.hexident;

            const hex = p.hexident;
            if (hex in planesDB) {
                Object.assign(planesDB[hex].raw, p);
            } else {
                planesDB[hex] = { raw: p };
            }
        });
    }

    planesSince = serverTime;

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
function addShipcardItem(icon, txt, title, action, contextType = 'ship') {
    const div = document.createElement("div");
    div.title = title;
    div.setAttribute("data-context-type", contextType);
    if (icon.startsWith("fa")) {
        icon = "question_mark";
    }
    const i = document.createElement("i");
    i.className = icon + "_icon";
    const span = document.createElement("span");
    span.textContent = txt;
    div.appendChild(i);
    div.appendChild(span);

    let name;
    if (typeof action === 'function') {
        name = '_plugin_' + (addShipcardItem._counter = (addShipcardItem._counter || 0) + 1);
        ACTIONS[name] = action;
    } else if (typeof action === 'string' && ACTIONS[action]) {
        name = action;
    } else {
        console.warn('addShipcardItem: action must be a function or a registered ACTIONS key. Got:', action);
        return;
    }
    div.dataset.action = name;
    document.getElementById("shipcard_footer").appendChild(div);
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
    let icon = document.getElementById("screentoggle-id");
    if (document.fullscreenElement) {
        icon.innerHTML = "fullscreen_exit";
    } else {
        icon.innerHTML = "fullscreen";
    }
}

// we calculate the lat/lon for 1m move in direction of heading
// underlying calculation uses an offset of 100m and then scales down to 1.

function setMapSetting(a, v) {
    settings[a] = v;
    saveSettings();
    redrawMap();
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

    paths = {};
    lastPathFetch = 0;
    pathsFrom = -1;
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
        `Scale (0 - ${speedWhole(settings.track_speed_max)} ${getSpeedUnit()})`;
}

function shipcardselect(e) {
    if (isShipcardMax()) {
        e.classList.toggle("shipcard-max-only");
        e.classList.toggle("shipcard-row-selected");
    } else toggleShipcardSize();

    saveSettings();
}

function toggleShipcardSize() {
    Array.from(document.getElementsByClassName("shipcard-min-only")).forEach((e) => e.classList.toggle("visible"));
    Array.from(document.getElementsByClassName("shipcard-max-only")).forEach((e) => e.classList.toggle("hidden"));

    document.getElementById("shipcard").classList.toggle("shipcard-ismax");
    document.getElementById("shipcard_minmax_button").classList.toggle("keyboard_arrow_down_icon");
    document.getElementById("shipcard_minmax_button").classList.toggle("keyboard_arrow_up_icon");

    let e = document.getElementById("shipcard_content").children;

    if (isShipcardMax()) {
        for (let i = 0; i < e.length; i++) {
            if (
                (e[i].classList.contains("shipcard-max-only") && e[i].classList.contains("shipcard-row-selected")) ||
                (!e[i].classList.contains("shipcard-max-only") && !e[i].classList.contains("shipcard-row-selected"))
            )
                e[i].classList.toggle("shipcard-row-selected");

            const aside = document.getElementById("shipcard");
            if (aside.style.top && aside.getBoundingClientRect().bottom > window.innerHeight) {

                if (card_mmsi in shipsDB && card_type == "ship") {
                    let pixel = map.getPixelFromCoordinate(ol.proj.fromLonLat([shipsDB[card_mmsi].raw.lon, shipsDB[card_mmsi].raw.lat]));
                    positionAside(pixel, aside);
                }
                else if (card_mmsi in planesDB && card_type == "plane") {
                    let pixel = map.getPixelFromCoordinate(ol.proj.fromLonLat([planesDB[card_mmsi].raw.lon, planesDB[card_mmsi].raw.lat]));
                    positionAside(pixel, aside);
                }
            }
        }
    } else {
        for (let i = 0; i < e.length; i++) {
            if (e[i].classList.contains("shipcard-row-selected")) e[i].classList.toggle("shipcard-row-selected");
        }
    }
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
    shipsSince = 0;
    binary.resetSince();
    range.resetUpdateTime();
    lastPathFetch = 0;
    paths = {};
    pathsFrom = -1;
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
        const [sharingText, sharingColor] = community.sharingDisplay();
        statSharingElement.innerHTML = `<a href="${stat.sharing_link}" target="_blank" style="color: ${sharingColor}">${sharingText}</a>`;


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
                    const connectedColor = s.connected ? "green" : "red";
                    html += `<div><span>Status</span><span style="color:${connectedColor}">${connected}</span></div>`;
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
    let content = '<div class="tooltip-card">' +
        getFlagStyled(ship.country, TOOLTIP_FLAG_STYLE) +
        '<div>' +
        (getShipName(ship) || ship.mmsi) + ' at ' + getSpeedVal(ship.speed) + ' ' + getSpeedUnit() + '<br>' +
        (ship.shiptype ? getShipTypeShort(ship.shiptype) + '<br>' : '') +
        'Received ' + getDeltaTimeVal(shipsSince - ship.last_signal) + ' ago' +
        '</div>' +
        '</div>';

    content += binary.tooltipSections(binary.shipBinaryMessages(ship.mmsi), true);

    return content;
}

function getTooltipContentPlane(plane) {
    const altitude = plane.airborne == 1 ? (plane.altitude ? Math.round(plane.altitude) + ' ft' : '-') : 'ground';
    const speed = plane.speed ? Math.round(plane.speed) : '-';
    return '<div class="tooltip-card">' +
        getFlagStyled(plane.country, TOOLTIP_FLAG_STYLE) +
        '<div>' +
        sanitizeString(plane.callsign || plane.hexident || '') + ' at ' + altitude + '/' + speed + ' kts<br>' +
        'Received ' + getDeltaTimeVal(planesSince - plane.last_signal) + ' ago' +
        '</div>' +
        '</div>';
}

// age-based fade shared with replay, so a replayed moment fades like the
// moment itself did
function fadeCurve(age) {
    return Math.max(0.2, Math.min(1, 1 - (age / 1800) * 0.8));
}

function fadeOpacity(age) {
    if (settings.fading == false) return 1;
    return fadeCurve(age);
}

function getShipOpacity(ship) {
    return fadeOpacity(shipsSince - ship.last_signal);
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

const showTooltipShip = (tooltip, mmsi, pixel, distance, angle = 0) => {

    tooltip.innerHTML = mmsi;

    if (pixel) {
        const [mapW, mapH] = map.getSize();
        const { offsetWidth: tw, offsetHeight: th } = tooltip;
        const dist = distance;

        // we position the tooltip top-right of the ship in the direction of the ship's course to minimize overlap with path
        const calculatePosition = (a) => {
            const rad = (a + 45) * Math.PI / 180;
            const s = Math.sin(rad);
            const c = -Math.cos(rad);
            let x = pixel[0] + dist * s + (s < 0 ? -tw : 0);
            let y = pixel[1] + dist * c + (c < 0 ? -th : 0);
            return { x, y };
        };

        let pos = calculatePosition(angle);
        // if it is off the screen, we try to move it to the opposite side
        if (pos.x + tw > mapW - 50 || pos.x < 0 || pos.y + th > mapH - 10 || pos.y < 0) {
            pos = calculatePosition(angle + 180);
        }

        // avoid that it is on top of controls or off screen
        pos.x = Math.min(Math.max(pos.x, 10), mapW - tw - 50);
        pos.y = Math.min(Math.max(pos.y, 10), mapH - th - 10);

        Object.assign(tooltip.style, {
            left: `${pos.x}px`,
            top: `${pos.y}px`,
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

    hoverMMSI = undefined;
    hoverType = undefined;
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

function showAttribution(on) {
    const foldout = document.getElementById('map-attribution-foldout');
    clearTimeout(attributionTimer);
    foldout.classList.toggle('visible', on);
}

function flashAttribution() {
    showAttribution(true);
    attributionTimer = setTimeout(() => showAttribution(false), 3000);
}

function toggleAttribution() {
    const foldout = document.getElementById('map-attribution-foldout');
    showAttribution(!foldout.classList.contains('visible'));
}


const startHover = function (type, mmsi, pixel, feature) {

    if (type != 'ship' && type != 'tooltip' && type != 'plane') return;

    if (mmsi !== hoverMMSI || hoverType !== type) {
        stopHover();

        hoverMMSI = mmsi;
        hoverType = type;
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

function updateHoverMarker() {
    const raw = hoverType == 'ship' ? shipsDB[hoverMMSI]?.raw : hoverType == 'plane' ? planesDB[hoverMMSI]?.raw : null;
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
    else if (feature && feature.binary === true) {
        // Handle hover for binary features (both ship-associated and standalone)
        if (feature.is_associated && feature.binary_mmsi && feature.binary_mmsi in shipsDB) {
            // For ship-associated binary features, redirect to ship hover
            startHover('ship', feature.binary_mmsi, pixel, feature);
            return;
        } else if (feature.binary_messages && feature.binary_messages.length > 0) {
            // For standalone binary clusters
            let tooltipContent = `<div class="tooltip-card">`;

            // Find MMSI counts to show in tooltip
            if (feature.binary_mmsi_counts) {
                const mmsiEntries = Object.entries(feature.binary_mmsi_counts);
                if (mmsiEntries.length > 0) {
                    tooltipContent += '<div style="margin-top: 5px; font-size: 0.9em;">From: ';
                    mmsiEntries.slice(0, 3).forEach(([mmsi, count], index) => {
                        const shipName = shipsDB[mmsi]?.raw?.shipname || `MMSI ${mmsi}`;
                        tooltipContent += `${index > 0 ? ', ' : ''}${shipName} (${count})`;
                    });
                    if (mmsiEntries.length > 3) {
                        tooltipContent += ` and ${mmsiEntries.length - 3} more`;
                    }
                    tooltipContent += '</div>';
                }
            }
            tooltipContent += '</div>';

            tooltipContent += binary.tooltipSections(feature.binary_messages);

            startHover('tooltip', tooltipContent, pixel, feature);
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
    localStorage[context] = JSON.stringify(settings);
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
    const scRows = document.querySelectorAll(".shipcard-content-row");

    const selectedRows = [];
    scRows.forEach((row) => {
        const rowClasses = row.getAttribute("class");
        selectedRows.push(rowClasses.includes("shipcard-max-only") ? 0 : 1);
    });

    settings.shipcard_max = isShipcardMax();
    settings.shipcard_rows = selectedRows;
    settings.activeReceiver = activeReceiver;

    const filters = realtimeModule?.getFilters();
    if (filters !== null && filters !== undefined) settings.realtime_filters = filters;
    const bg = realtimeModule?.getBackgroundStreaming();
    if (bg !== null && bg !== undefined) settings.realtime_background_streaming = bg;

    persistSettings();

    if (document.querySelector(".settings_window").classList.contains("active"))
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

    if (!("showPlanesAtFirst" in settings)) {
        settings.showPlanesAtFirst = true;
        settings.map_overlay.push("Aircraft");
    }

    if (Array.isArray(settings.map_overlay) && settings.map_overlay.includes("Community Feed")) {
        settings.map_overlay = settings.map_overlay.filter(t => t !== "Community Feed");
        queueMicrotask(() => showDialog("Community feed has moved",
            "The on-map Community Feed overlay has been replaced by a new <b>Community Pane</b> that opens the full aiscatcher.org map in a popup window.<br><br>Right-click the map and choose <b>Toggle Community Pane</b>, or use the network button in the map controls."));
    }
}


function loadSettings() {
    if (!urlParams.has("reset")) {
        try {
            const localStorageSettings = localStorage.getItem(context);
            if (localStorageSettings !== null) {
                const ls = JSON.parse(localStorageSettings);
                Object.assign(settings, ls);
            }
        } catch (error) {
            console.log(error);
            return;
        }
    }
    if (settings.activeReceiver) activeReceiver = settings.activeReceiver;
    if (!isShipcardMax()) toggleShipcardSize();

    if (settings.shipcard_rows && settings.shipcard_rows.length > 0) {
        const rows = document.querySelectorAll(".shipcard-content-row");

        rows.forEach((row, index) => {
            if (settings.shipcard_rows[index] == 1) {
                row.setAttribute("class", "mapcard-content-row shipcard-content-row shipcard-row-selected");
            } else {
                row.setAttribute("class", "mapcard-content-row shipcard-content-row shipcard-max-only");
            }
        });
        if (settings.shipcard_max != isShipcardMax()) {
            toggleShipcardSize();
        }
    }

    settings.android = false;
}

function convertStringSettingsToActual() {
    // shipcard_max is written by saveSettings and absent from the defaults
    const extraBooleans = ['shipcard_max'];

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

function mapResetViewZoom(z, m) {
    if (m && m in shipsDB) {
        let ship = shipsDB[m].raw;
        let view = map.getView();
        view.setCenter(ol.proj.fromLonLat([ship.lon, ship.lat]));
        view.setZoom(Math.min(view.getMaxZoom(), Math.max(z, view.getZoom() + 1)));
    }

    shipcardMinIfMaxonMobile();
}

function mapResetView(z) {

    let view = map.getView();
    view.setZoom(Math.min(view.getMaxZoom(), Math.max(z, view.getZoom() + 1)));
    shipcardMinIfMaxonMobile();
}

function shipcardVisible() {
    return document.getElementById("shipcard").classList.contains("visible");
}

function measurecardVisible() {
    return document.getElementById("measurecard").classList.contains("visible");
}

function toggleMeasurecard() {
    if (shipcardVisible() && !measurecardVisible()) showShipcard(null, null);
    document.getElementById("measurecard").classList.toggle("visible");
}

function replaycardVisible() {
    return document.getElementById("replaybar").classList.contains("visible");
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
    const bar = document.getElementById("replaybar");
    bar.classList.toggle("visible");
    // the phone layout hangs off this: the bar spans the full width there and
    // the map controls step aside for it
    document.body.classList.toggle("replay-open", bar.classList.contains("visible"));

    if (replaycardVisible()) {
        // replay owns the screen: the live panels and tools step aside
        closeSettings();
        closeTableSide();
        if (shipcardVisible()) showShipcard(null, null);
        if (measurecardVisible()) toggleMeasurecard();
        measure.cancel();

        // before anything is loaded the scrubber spans the server's whole
        // history, so dragging it chooses where playback will start
        replay.refreshBounds().then(updateReplaycard);
    } else {
        replayLoadAt.cancel();
        stopReplay();
    }
}

// The scrubber means "start here" until a load has happened, and "seek here"
// after — so Play is the only control needed to get from opening the bar to
// watching the fleet move.
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
        shipcardMinIfMaxonMobile();
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
    updateShipcardTrackOption(m);

}

async function showTrack(m) {
    if (!marker_tracks.has(Number(m))) {
        ToggleTrackOnMap(m);
    }
    updateShipcardTrackOption(m);

}

async function hideTrack(m) {
    if (marker_tracks.has(Number(m))) {
        ToggleTrackOnMap(m);
    }
    updateShipcardTrackOption(m);

}

function trackIsShown(m) {
    return marker_tracks.has(Number(m));
}

function pinStation() {
    pinVessel("STATION");
}

function pinVessel(m) {
    settings.center_point = m;
    settings.fix_center = true;
    saveSettings();
    drawStation();
}

function unpinCenter() {
    settings.fix_center = false;
    saveSettings();
    drawStation();
}


function setTrackVisibility(mode) {
    if (mode === 'all') return showAllTracks();
    if (mode === 'none') return deleteAllTracks();
    settings.show_all_tracks = false;
    saveSettings();
    redrawMap();
    updateShipcardTrackOption();
}

async function showAllTracks() {
    settings.show_all_tracks = true;
    trackCutoff = 0;
    lastPathFetch = 0;
    select_enabled_track = hover_enabled_track = false;
    await fetchTracks();
    redrawMap();
    updateShipcardTrackOption();
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
        updateShipcardTrackOption();
    }
    return mmsis.length;
}

function deleteAllTracks() {
    settings.show_all_tracks = false;
    trackCutoff = 0;
    lastPathFetch = 0;
    marker_tracks = new Set();
    let p = {};

    if (card_type == 'ship' && card_mmsi && settings.show_track_on_select) {
        marker_tracks.add(Number(card_mmsi));
        select_enabled_track = true;

        if (paths[card_mmsi]) {
            p[card_mmsi] = paths[card_mmsi];
        }
    }

    paths = p;

    redrawMap(); updateShipcardTrackOption();
    saveSettings();
}

async function resetTracksFromNow() {
    trackCutoff = shipsSince || Math.floor(Date.now() / 1000);
    paths = {};
    pathsFrom = -1;
    lastPathFetch = 0;
    await fetchTracks();
    redrawMap();
    updateShipcardTrackOption();
    showNotification("Tracks reset — showing from now", "success");
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
                pathsFrom = trackWindowStart();
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
            const mmsi_str = Array.from(marker_tracks).join(",");
            pathsFrom = 0;
            a = await fetch("api/path.json?" + mmsi_str + "&receiver=" + activeReceiver);
        }

        const newPaths = await a.json();

        let maxTs = lastPathFetch;
        for (const mmsi in newPaths)
            for (const pt of newPaths[mmsi])
                if (pt[3] > maxTs) maxTs = pt[3];
        if (maxTs > lastPathFetch) lastPathFetch = maxTs;

        if (!isDelta) {
            paths = newPaths;
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
        if (!isDelta) { paths = {}; pathsFrom = -1; }
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

function trackOptionString(mmsi) {
    const hover_track = hoverType == 'ship' && mmsi == hoverMMSI && hover_enabled_track;
    const select_track = card_type == 'ship' && mmsi == card_mmsi && select_enabled_track;
    const track_shown = marker_tracks.has(Number(mmsi));

    if (hover_track || select_track) return "Show Track";
    return track_shown ? "Hide Track" : "Show Track";
}

function updateShipcardTrackOption() {
    const trackOptionElement = document.getElementById("shipcard_track_option");

    if (settings.show_all_tracks || card_type == 'plane') {
        trackOptionElement.style.opacity = "0.5";
        trackOptionElement.style.pointerEvents = "none";
    } else {
        trackOptionElement.style.opacity = "1";
        trackOptionElement.style.pointerEvents = "auto";
    }

    if (card_mmsi && card_type == 'ship') {
        document.getElementById("shipcard_track").innerText = trackOptionString(card_mmsi);
    }
}

function isShipcardMax() {
    let e = document.getElementById("shipcard").classList;
    return e.contains("shipcard-ismax");
}

function setShipcardValidation(v) {
    document.getElementById("shipcard_header").classList.remove("shipcard-validated", "shipcard-not-validated", "shipcard-dubious");

    switch (v) {
        case 1:
            document.getElementById("shipcard_header").classList.add("shipcard-validated");
            break;
        case -1:
            document.getElementById("shipcard_header").classList.add("shipcard-dubious");
            break;
        default:
            document.getElementById("shipcard_header").classList.add("shipcard-not-validated");
    }
}

function updateMessageButton() {
    const messageButton = document.querySelector('#shipcard_footer [data-action="showBinaryMessageDialogCard"]');
    if (!messageButton) return;

    const iconElement = messageButton.querySelector('i.mail_icon');
    if (!iconElement) return;

    const count = binary.shipBinaryMessages(card_mmsi).length;

    const existingBadge = iconElement.querySelector('.message-badge');
    if (existingBadge) {
        existingBadge.remove();
    }

    if (count > 0) {
        messageButton.style.display = '';

        const badge = document.createElement('span');
        badge.className = 'message-badge';
        badge.textContent = count;
        iconElement.appendChild(badge);
    } else {
        messageButton.style.display = 'none';
    }
}

function showCardOutOfRange() {
    document
        .getElementById("shipcard_content")
        .querySelectorAll("span:nth-child(2)")
        .forEach((e) => (e.innerHTML = null));
    document.getElementById("shipcard_header_title").innerHTML = "<b style='color:red;'>Out of range</b>";
    document.getElementById("shipcard_header_flag").innerHTML = "";
    document.getElementById("shipcard_mmsi").innerHTML = card_mmsi;

    updateFocusMarker();
}

function populateShipcard() {

    if (card_type != 'ship') return;

    if (!(card_mmsi in shipsDB)) {
        showCardOutOfRange();
        return;
    }

    let ship = shipsDB[card_mmsi].raw;

    document.getElementById("shipcard_header_flag").innerHTML = getFlagStyled(ship.country, CARD_FLAG_STYLE);
    document.getElementById("shipcard_header_title").innerHTML = (getShipName(ship) || ship.mmsi);

    setShipcardValidation(ship.validated);

    // verbatim copies
    ["destination", "mmsi", "count", "received_stations"].forEach((e) => (document.getElementById("shipcard_" + e).innerHTML = ship[e] != null ? ship[e] : "-"));

    if (ship.imo != null) {
        document.getElementById("shipcard_imo_label").innerHTML = "IMO";
        document.getElementById("shipcard_imo").innerHTML = ship.imo;
    } else {
        document.getElementById("shipcard_imo_label").innerHTML = ship.eni ? "ENI" : "IMO";
        document.getElementById("shipcard_imo").innerHTML = ship.eni || "-";
    }

    // round and add units
    [
        { id: "cog", u: "&deg", d: 1 },
        { id: "bearing", u: "&deg", d: 0 },
        { id: "heading", u: "&deg", d: 0 },
        { id: "level", u: "dB", d: 1 },
        { id: "ppm", u: "ppm", d: 1 },
    ].forEach((el) => (document.getElementById("shipcard_" + el.id).innerHTML = ship[el.id] != null ? Number(ship[el.id]).toFixed(el.d) + " " + el.u : null));

    document.getElementById("shipcard_country").innerHTML = getCountryName(ship.country);
    document.getElementById("shipcard_callsign").innerHTML = getCallSign(ship);
    document.getElementById("shipcard_msgtypes").innerHTML = getStringfromMsgType(ship.msg_type);

    document.getElementById("shipcard_last_group").innerHTML = getStringfromGroup(ship.last_group);
    document.getElementById("shipcard_sources").innerHTML = getStringfromGroup(ship.group_mask);

    document.getElementById("shipcard_channels").innerHTML = getStringfromChannels(ship.channels);
    document.getElementById("shipcard_type").innerHTML = getMmsiTypeVal(ship) + ' <i class="info_icon shipcard-tech-icon" id="shipcard_tech_info" data-action="techInfo" title="Technical details"></i>';
    document.getElementById("shipcard_shiptype").innerHTML = ship.shiptype != null
        ? getShipTypeShort(ship.shiptype) + ' <i class="info_icon shipcard-tech-icon" id="shipcard_shiptype_info" data-action="shiptypeInfo" title="Ship type details"></i>'
        : getShipTypeShort(ship.shiptype);
    document.getElementById("shiptype_code").textContent = ship.shiptype != null ? ship.shiptype : "-";
    document.getElementById("shiptype_desc").textContent = ship.shiptype != null
        ? getShipTypeFull(ship.shiptype)
        : "-";
    document.getElementById("shipcard_status").innerHTML = getStatusVal(ship);
    document.getElementById("shipcard_last_signal").innerHTML = getDeltaTimeVal(shipsSince - ship.last_signal);
    document.getElementById("shipcard_eta").innerHTML = ship.eta_month != null && ship.eta_hour != null && ship.eta_day != null && ship.eta_minute != null ? getEtaVal(ship) : null;
    document.getElementById("shipcard_lat").innerHTML = ship.lat != null ? getLatValFormat(ship) : null;
    document.getElementById("shipcard_lon").innerHTML = ship.lon != null ? getLonValFormat(ship) : null;
    document.getElementById("shipcard_altitude").innerHTML = ship.altitude != null ? ship.altitude + " m" : null;

    document.getElementById("shipcard_speed").innerHTML = ship.speed != null ? getSpeedVal(ship.speed) + " " + getSpeedUnit() : null;
    document.getElementById("shipcard_distance").innerHTML = ship.distance != null ? (getDistanceVal(ship.distance) + " " + getDistanceUnit() + (ship.repeat > 0 ? " (R)" : "")) : null;
    document.getElementById("shipcard_draught").innerHTML = ship.draught ? getDraughtVal(ship.draught) + " " + getDimUnit() : null;
    document.getElementById("shipcard_dimension").innerHTML = getShipDimension(ship);
    document.getElementById("shipcard_bluesign").innerHTML = ship.maneuver === 2 ? "Set" : (ship.maneuver === 1 ? "Not set" : null);

    updateShipcardTrackOption();
    updateMessageButton();
    updateTechDetails(ship);
    renderHullSection(ship);

}

function updateTechDetails(ship) {
    // Helper to format flag values
    const formatFlag = (value, trueText = "Yes", falseText = "No", unknownText = "-") => {
        if (value === 0) return unknownText;
        if (value === 1) return falseText;
        if (value === 2) return trueText;
        return unknownText;
    };

    // Update RAIM
    document.getElementById("tech_raim").textContent = formatFlag(ship.raim);

    // Update DTE
    document.getElementById("tech_dte").textContent = formatFlag(ship.dte, "Not Ready", "Ready");

    // Update Assigned Mode
    document.getElementById("tech_assigned").textContent = formatFlag(ship.assigned, "Assigned", "Autonomous");

    // Update Display
    document.getElementById("tech_display").textContent = formatFlag(ship.display);

    // Update DSC
    document.getElementById("tech_dsc").textContent = formatFlag(ship.dsc);

    // Update Band
    document.getElementById("tech_band").textContent = formatFlag(ship.band, "Dual", "Single");

    // Update MSG22
    document.getElementById("tech_msg22").textContent = formatFlag(ship.msg22);

    // Update Off Position
    document.getElementById("tech_off_position").textContent = formatFlag(ship.off_position, "Off", "On");

    // Update Maneuver
    const maneuverText = ship.maneuver === 0 ? "-" : ship.maneuver === 1 ? "None" : "Special";
    document.getElementById("tech_maneuver").textContent = maneuverText;

    // Transponder vendor info (type 24 part B)
    document.getElementById("tech_vendor").textContent = ship.vendorid || "-";
    document.getElementById("tech_model").textContent = ship.model != null ? ship.model : "-";
    document.getElementById("tech_serial").textContent = ship.serial != null ? ship.serial : "-";
}

// Everything time-related about a vessel in one window: the track gives speed,
// the change log gives draught and what else was edited. Both are fetched here
// rather than per row, so opening a shipcard still costs nothing.
let historyMMSI = 0;

// Sections collapse independently of the card's own min/max, which toggles
// `hidden` on every shipcard-max-only element — hence a class of their own.
const sectionOpen = { voyage: true, vessel: true, source: false,
                      hull: false, speed: false, draught: false, changes: false };

let historyCache = { mmsi: 0, pts: null, changes: null };

function applyShipcardSection(key) {
    const open = !!sectionOpen[key];
    document.querySelectorAll('#shipcard_content [data-section="' + key + '"]').forEach((el) => {
        if (el.classList.contains("shipcard-section")) {
            const caret = el.querySelector(".shipcard-section-caret");
            if (caret) {
                caret.classList.toggle("keyboard_arrow_up_icon", open);
                caret.classList.toggle("keyboard_arrow_down_icon", !open);
            }
            return;
        }
        el.classList.toggle("sec-collapsed", !open);
    });
}

function toggleShipcardSection(key) {
    if (!key) return;
    sectionOpen[key] = !sectionOpen[key];
    applyShipcardSection(key);
    if (sectionOpen[key]) fillShipcardSection(key);
}

function openShipcardSection(key) {
    if (!sectionOpen[key]) toggleShipcardSection(key);
}

function setSectionAvailable(key, available) {
    const head = document.getElementById("shipcard_" + key + "_head");
    const body = document.getElementById("shipcard_" + key + "_section");
    if (head) head.classList.toggle("sec-empty", !available);
    if (body && !available) body.classList.add("sec-collapsed");
}

function renderHullSection(ship) {
    ship = ship || shipsDB[card_mmsi]?.raw;
    const svg = ship ? getShipDimensionSVG(ship) : "";
    setSectionAvailable("hull", !!svg);
    const body = document.getElementById("shipcard_hull_body");
    if (body) body.innerHTML = svg;
}

async function fillShipcardSection(key) {
    const mmsi = card_mmsi;
    if (!mmsi) return;

    if (key === "hull") return renderHullSection();
    if (!["speed", "draught", "changes"].includes(key)) return;

    const body = document.getElementById("shipcard_" + key + "_body");
    if (!body) return;

    if (historyCache.mmsi !== mmsi) {
        body.innerHTML = '<span class="dim-note">Loading…</span>';
        try {
            const [p, c] = await Promise.all([
                fetch("api/path.json?" + mmsi + "&receiver=" + activeReceiver).then((r) => r.json()),
                fetch("api/changes.json?" + mmsi + "&receiver=" + activeReceiver).then((r) => r.json()),
            ]);
            historyCache = { mmsi, pts: p[mmsi] || [], changes: Array.isArray(c) ? c : [] };
        } catch (err) {
            body.innerHTML = '<span class="dim-note">History unavailable</span>';
            return;
        }
    }
    if (card_mmsi !== mmsi) return;

    const ship = shipsDB[mmsi]?.raw;
    let html = "";
    if (key === "speed") html = getSpeedHistorySVG(historyCache.pts);
    else if (key === "draught") html = getDraughtChartSVG(historyCache.changes, ship ? ship.draught : null);
    else html = getChangeListHTML(historyCache.changes,
        [CHANGE.DESTINATION, CHANGE.ETA, CHANGE.SHIPNAME, CHANGE.CALLSIGN, CHANGE.STATUS],
        (x) => x.f === CHANGE.STATUS ? getStatusVal({ status: x.to })
             : x.f === CHANGE.DRAUGHT ? getDimVal(x.to / 10) + " " + getDimUnit()
             : String(x.to));

    setSectionAvailable(key, !!html);
    body.innerHTML = html || '<span class="dim-note">Nothing recorded yet</span>';
}

function resetShipHistory() {
    historyCache = { mmsi: 0, pts: null, changes: null };
    ["hull", "speed", "draught", "changes"].forEach((k) => {
        const body = document.getElementById("shipcard_" + k + "_body");
        if (body) body.innerHTML = "";
        sectionOpen[k] = false;
    });
    sectionOpen.voyage = true;
    sectionOpen.vessel = true;
    sectionOpen.source = false;
    Object.keys(sectionOpen).forEach(applyShipcardSection);
}

function toggleShipcardPopover(popoverId, iconId) {
    const popover = document.getElementById(popoverId);
    const icon = document.getElementById(iconId);

    const onOutsideClick = (event) => {
        if (popover.contains(event.target)) return;
        event.stopPropagation();
        event.preventDefault();
        popover.style.display = "none";
        document.removeEventListener("click", onOutsideClick, true);
    };

    const iconRect = icon.getBoundingClientRect();
    const shipcardRect = document.getElementById("shipcard").getBoundingClientRect();

    popover.style.display = "block";

    let left = iconRect.left - shipcardRect.left + 20;
    let top = iconRect.bottom - shipcardRect.top + 5;

    const popoverRect = popover.getBoundingClientRect();

    if (iconRect.left + popoverRect.width + 20 > window.innerWidth)
        left = Math.max(5, iconRect.right - shipcardRect.left - popoverRect.width - 5);
    if (iconRect.bottom + popoverRect.height + 5 > window.innerHeight)
        top = Math.max(5, iconRect.top - shipcardRect.top - popoverRect.height - 5);

    left = Math.max(5, Math.min(left, shipcardRect.width - popoverRect.width - 5));
    top = Math.max(5, top);

    popover.style.left = left + "px";
    popover.style.top = top + "px";

    setTimeout(() => document.addEventListener("click", onOutsideClick, true), 0);
}

function getCategory(plane) {
    if (!plane || !plane.category) return "-";

    const categories = {
        41: "< 7 MT",
        42: "7 - 34 MT",
        43: "34 - 136 MT",
        44: "High vortex",
        45: "> 136 MT",
        46: "High perf",
        47: "Rotorcraft",
        31: "Glider",
        32: "LTA",
        33: "Parachutist",
        34: "Ultralight",
        36: "UAV",
        37: "Space",
        21: "Emergency",
        23: "Service"
    };

    return categories[plane.category] || plane.category.toString();
}

function populatePlanecard() {

    if (card_type != 'plane') return;

    if (!(card_mmsi in planesDB)) {
        showCardOutOfRange();
        return;
    }

    document
        .getElementById("shipcard_content")
        .querySelectorAll("span:nth-child(2)")
        .forEach((e) => (e.innerHTML = null));

    let plane = planesDB[card_mmsi].raw;

    setShipcardValidation(plane.validated);

    // Set header
    document.getElementById("shipcard_header_title").textContent = (plane.callsign || getICAO(plane));
    document.getElementById("shipcard_header_flag").innerHTML = getFlagStyled(plane.country, CARD_FLAG_STYLE);

    // Populate plane fields
    document.getElementById("shipcard_plane_country").innerHTML = getCountryName(plane.country);
    document.getElementById("shipcard_plane_type").innerHTML = "ADSB";
    document.getElementById("shipcard_plane_callsign").textContent = plane.callsign || "-";
    document.getElementById("shipcard_plane_hexident").textContent = getICAO(plane);
    document.getElementById("shipcard_plane_category").textContent = getCategory(plane);
    document.getElementById("shipcard_plane_squawk").textContent = plane.squawk || "-";
    document.getElementById("shipcard_plane_speed").innerHTML = plane.speed ? getSpeedVal(plane.speed) + " " + getSpeedUnit() : null;

    document.getElementById("shipcard_plane_altitude").textContent = plane.airborne == 1 ? (plane.altitude ? `${plane.altitude} ft` : "-") : "on ground";
    document.getElementById("shipcard_plane_lat").innerHTML = plane.lat ? getLatValFormat(plane) : null;
    document.getElementById("shipcard_plane_lon").innerHTML = plane.lon ? getLonValFormat(plane) : null;
    document.getElementById("shipcard_plane_vertrate").textContent = plane.vertrate ? `${plane.vertrate} ft/min` : "-";
    document.getElementById("shipcard_plane_last_signal").textContent = getDeltaTimeVal(planesSince - plane.last_signal);
    document.getElementById("shipcard_plane_messages").textContent = plane.nMessages || "-";
    document.getElementById("shipcard_plane_downlink").textContent = getStringfromMsgType(plane.message_types);
    document.getElementById("shipcard_plane_TC").textContent = getStringfromMsgType(plane.message_subtypes);
    document.getElementById("shipcard_plane_distance").innerHTML = plane.distance != null ? (getDistanceVal(plane.distance) + " " + getDistanceUnit()) : null;

    document.getElementById("shipcard_plane_last_group").innerHTML = getStringfromGroup(plane.last_group);
    document.getElementById("shipcard_plane_sources").innerHTML = getStringfromGroup(plane.group_mask);

    [
        { id: "heading", u: "&deg", d: 0 },
        { id: "level", u: "dB", d: 1 },
        { id: "bearing", u: "&deg", d: 0 }
    ].forEach((el) => (document.getElementById("shipcard_plane_" + el.id).innerHTML = plane[el.id] != null ? Number(plane[el.id]).toFixed(el.d) + " " + el.u : null));

    updateShipcardTrackOption();
}

function shipcardMinIfMaxonMobile() {
    if (shipcardVisible() && window.matchMedia("(max-height: 1000px) and (max-width: 500px)").matches && isShipcardMax()) {
        toggleShipcardSize();
    }
}

function drawStation() {
    const hasNoStation = settings.show_station == false || station == null || !Object.hasOwn(station, "lat") || !Object.hasOwn(station, "lon");
    const hasMMSIcenter = settings.center_point && settings.center_point != "STATION" && settings.center_point in shipsDB;

    if (stationFeature) {
        extraVector.removeFeature(stationFeature);
        stationFeature = undefined;
    }

    if (hasNoStation) {
        return;
    }

    const radius = 10;
    let svgIconStyle = new ol.style.Style({
        image: new ol.style.Icon({
            anchor: [0.5, 0.5],
            scale: 0.3,
            color: 'white', //getComputedStyle(document.documentElement).getPropertyValue('--color-secondary'),
            src: 'data:image/svg+xml,%3Csvg xmlns="http://www.w3.org/2000/svg" height="48" viewBox="0 -960 960 960" width="48"%3E%3Cpath fill="white" d="M198-278q-60-58-89-133T80-560q0-74 29-149t89-133l35 35q-50 49-76.5 116.5T130-560q0 63 26.5 130.5T233-313l-35 35Zm92-92q-40-37-59-89.5T212-560q0-48 19-100.5t59-89.5l35 35q-29 29-46 72.5T262-560q0 35 17.5 79.5T325-405l-35 35Zm4 290 133-405q-17-12-27.5-31T389-560q0-38 26.5-64.5T480-651q38 0 64.5 26.5T571-560q0 25-10.5 44T533-485L666-80h-59l-29-90H383l-30 90h-59Zm108-150h156l-78-238-78 238Zm268-140-35-35q29-29 46-72.5t17-82.5q0-35-17.5-79.5T635-715l35-35q39 37 58.5 89.5T748-560q0 47-19.5 100T670-370Zm92 92-35-35q49-49 76-116.5T830-560q0-63-27-130.5T727-807l35-35q60 58 89 133t29 149q0 75-27.5 149.5T762-278Z"/%3E%3C/svg%3E',
        })
    });

    let CircleStyle = new ol.style.Style({
        image: new ol.style.Circle({
            radius: radius,
            stroke: new ol.style.Stroke({
                color: station.gps ? '#2e86ff' : 'white',
                width: 3
            }),
            fill: new ol.style.Fill({
                color: getComputedStyle(document.documentElement).getPropertyValue('--color-station-fill')
            }),
        })
    });

    stationFeature = new ol.Feature({
        geometry: new ol.geom.Point(ol.proj.fromLonLat([station.lon, station.lat]))
    });

    stationFeature.setStyle([CircleStyle, svgIconStyle]);
    stationFeature.tooltip = station.gps ? "Receiving Station (GPS)" : "Receiving Station";
    stationFeature.station = true;
    extraVector.addFeature(stationFeature);

    // MMSI is driving center but does not exist
    if (!hasMMSIcenter && settings.center_point != "STATION") {
        settings.center_point = "STATION";
        settings.fix_center = false;
    }

    if (settings.center_point == "STATION") {
        center = station;
    } else {
        center = {};
        if (hasMMSIcenter) {
            let ship = shipsDB[settings.center_point].raw;
            if (ship.lat != null && ship.lon != null) {
                center = { lat: ship.lat, lon: ship.lon };
            }
        }
    }

    if (settings.fix_center && center != null && Object.hasOwn(center, "lat") && Object.hasOwn(center, "lon")) {

        let view = map.getView();
        view.setCenter(ol.proj.fromLonLat([center.lon, center.lat]));

        settings.lat = center.lat;
        settings.lon = center.lon;
    }
}

function moveMapCenter(px) {
    let view = map.getView();
    let currentCenter = view.getCenter();
    let newCenter = map.getCoordinateFromPixel(px);
    let centerDiff = [newCenter[0] - currentCenter[0], newCenter[1] - currentCenter[1]];

    stopHover();
    view.animate({
        center: [currentCenter[0] + centerDiff[0], currentCenter[1] + centerDiff[1]],
        duration: 1000
    });
}

function adjustMapForShipcard(pixel) {

    let lat = null, lon = null;
    if (card_type == 'ship' && card_mmsi in shipsDB) {
        lat = shipsDB[card_mmsi].raw.lat;
        lon = shipsDB[card_mmsi].raw.lon;
    } else if (card_type == 'plane' && card_mmsi in planesDB) {
        lat = planesDB[card_mmsi].raw.lat;
        lon = planesDB[card_mmsi].raw.lon;
    }

    if (lat && lon) {
        let view = map.getView();

        let currentExtent = view.calculateExtent(map.getSize());
        let shipCoords = ol.proj.fromLonLat([lon, lat]);

        if (!ol.extent.containsCoordinate(currentExtent, shipCoords)) {
            view.animate({
                center: shipCoords,
                duration: 1000
            });
            return;
        }

        // Use the provided pixel if defined, otherwise calculate it
        if (!pixel) {
            pixel = map.getPixelFromCoordinate(shipCoords);
        }

        let shipcard = document.getElementById("shipcard");
        let shipcardRect = shipcard.getBoundingClientRect();
        let mapElement = map.getTargetElement();
        let mapRect = mapElement.getBoundingClientRect();

        let isUnderShipcard = (
            pixel[0] + mapRect.left >= shipcardRect.left &&
            pixel[0] + mapRect.left <= shipcardRect.right &&
            pixel[1] + mapRect.top >= shipcardRect.top &&
            pixel[1] + mapRect.top <= shipcardRect.bottom
        );

        if (isUnderShipcard) {
            moveMapCenter(pixel);
        }
    }
}

function pinShipcard() {
    settings.shipcard_pinned = true;
    settings.shipcard_pinned_x = parseInt(shipcard.style.left) || 0;
    settings.shipcard_pinned_y = parseInt(shipcard.style.top) || 0;

    applyShipcardPinStyling();
    showNotification("Shipcard pinned to current position");
    saveSettings();
}

function unpinShipcard() {
    settings.shipcard_pinned = false;
    settings.shipcard_pinned_x = null;
    settings.shipcard_pinned_y = null;

    applyShipcardPinStyling();

    showNotification("Shipcard unpinned");
    saveSettings();
}

function applyShipcardPinStyling() {
    const shipcard = document.getElementById("shipcard");
    if (settings.shipcard_pinned) {
        shipcard.classList.add("pinned");
        document.getElementById("shipcard_drag_handle").classList.add("opacity-50");
    }
    else {
        shipcard.classList.remove("pinned");
        document.getElementById("shipcard_drag_handle").classList.remove("opacity-50");
    }
}

function toggleShipcardPin() {
    if (settings.shipcard_pinned) {
        unpinShipcard();
    } else {
        pinShipcard();
    }
}

function placeTopLeft(aside) {
    const mapSize = map.getSize();
    const rect = aside.getBoundingClientRect();
    if (mapSize && mapSize[0] >= rect.width + 20 && mapSize[1] >= rect.height + 20) {
        aside.style.left = "10px";
        aside.style.top = "10px";
        aside.classList.add("floating");
    }
}

function positionAside(pixel, aside) {

    stopHover();

    if (settings.kiosk && settings.kiosk_pan_map && card_type == 'ship' && card_mmsi in shipsDB) {
        moveMapCenter(pixel);
        const mapSize = map.getSize();
        pixel = [mapSize[0] / 2, mapSize[1] / 2];
    }

    if (settings.shipcard_pinned && settings.shipcard_pinned_x !== null && settings.shipcard_pinned_y !== null) {
        aside.style.left = `${settings.shipcard_pinned_x}px`;
        aside.style.top = `${settings.shipcard_pinned_y}px`;
        aside.classList.add("floating");
        return;
    }

    aside.style.left = "";
    aside.style.top = "";
    aside.classList.remove("floating");

    if (settings.shipcard_top_left) {
        placeTopLeft(aside);
        adjustMapForShipcard(pixel);
        return;
    }

    let placed = false;

    if (pixel) {
        const margin = 35;
        let marginRight = document.getElementById("tableside").classList.contains("active") ? 592 : 30;

        const mapSize = map.getSize();
        const shipCardRect = aside.getBoundingClientRect();
        const shipCardWidth = shipCardRect.width;
        const shipCardHeight = shipCardRect.height;

        const rightSpace = mapSize[0] - (pixel[0] + shipCardWidth + margin + marginRight);
        const leftSpace = pixel[0] - (shipCardWidth + margin);

        if ((rightSpace > 0 || leftSpace > 0) && mapSize[1] > shipCardHeight + 2 * margin) {
            let topPosition = pixel[1] - (shipCardHeight / 2);
            topPosition = Math.max(margin, Math.min(mapSize[1] - shipCardHeight - margin, topPosition));

            aside.style.top = `${topPosition}px`;

            if (rightSpace >= 0) {
                aside.style.left = `${pixel[0] + margin}px`;
            } else if (leftSpace >= 0) {
                aside.style.left = `${pixel[0] - shipCardWidth - margin}px`;
            } else {
                aside.style.left = `${(mapSize[0] - shipCardWidth) / 2}px`;
            }

            aside.classList.add("floating");
            placed = true;
        }
    }

    if (!placed) {
        placeTopLeft(aside);
    }
    adjustMapForShipcard(pixel);
}

function displayShipcardIcons(type) {
    let icons = document.querySelectorAll('#shipcard_footer > div');
    let idx = 0;

    for (let icon of icons) {
        // Hide icons that don't match current context type
        if (icon.dataset.contextType !== type && icon.dataset.contextType) {
            icon.style.display = "none";
            continue;
        }

        // Check if this is the More button - always show it and don't count it
        const isMoreButton = icon.querySelector('i')?.classList.contains('more_horiz_icon');
        if (isMoreButton) {
            icon.style.display = "flex";
            continue;
        }

        // Check if realtime option should be hidden
        const isRealtimeDisabled = icon.id === 'shipcard_realtime_option' && !config.features.realtime;

        if (isRealtimeDisabled) {
            icon.style.display = "none";
            // Don't increment idx, effectively removing it from the visible count
        } else {
            // Show if within offset range
            const isInRange = idx >= shipcardIconOffset[type] && idx < shipcardIconOffset[type] + shipcardIconMax;
            icon.style.display = isInRange ? "flex" : "none";
            idx++;
        }
    }
}

function rotateShipcardIcons() {
    shipcardIconOffset[card_type] += shipcardIconMax;
    if (shipcardIconOffset[card_type] >= shipcardIconCount[card_type]) {
        shipcardIconOffset[card_type] = 0;
    }
    displayShipcardIcons(card_type);
}

function prepareShipcard() {
    // Initialize offset/count objects if needed
    if (!shipcardIconOffset || typeof shipcardIconOffset !== 'object') {
        shipcardIconOffset = { ship: 0, plane: 0 };
    }
    shipcardIconCount = shipcardIconCount || { ship: 0, plane: 0 };

    // Count icons for each context
    shipcardIconCount.ship = document.querySelectorAll('#shipcard_footer > div[data-context-type="ship"]').length;
    shipcardIconCount.plane = document.querySelectorAll('#shipcard_footer > div[data-context-type="plane"]').length;

    // Adjust count if realtime is disabled (exclude realtime option from count)
    if (!config.features.realtime) {
        const realtimeOption = document.getElementById('shipcard_realtime_option');
        if (realtimeOption && realtimeOption.dataset.contextType === 'ship') {
            shipcardIconCount.ship--;
        }
    }

    // Add More button for each context if needed
    if (shipcardIconCount.ship > shipcardIconMax) {
        addShipcardItem('more_horiz', 'More', 'More options', 'rotateShipcardIcons', 'ship');
    }
    if (shipcardIconCount.plane > shipcardIconMax) {
        addShipcardItem('more_horiz', 'More', 'More options', 'rotateShipcardIcons', 'plane');
    }

    displayShipcardIcons('ship');
}

function showShipcard(type, m, pixel = undefined) {
    resetShipHistory();

    const aside = document.getElementById("shipcard");
    const visible = shipcardVisible();

    let ship = m in shipsDB ? shipsDB[m].raw : null;
    let ship_old = card_mmsi in shipsDB ? shipsDB[card_mmsi].raw : null;


    if (select_enabled_track && (card_mmsi != m || m == null)) {
        select_enabled_track = false;

        if (!(card_mmsi == hoverMMSI && hover_enabled_track && hoverType == 'ship')) {
            hideTrack(card_mmsi);
        }
    }

    if (m != null && !visible) {
        if (measurecardVisible()) toggleMeasurecard();
        aside.classList.toggle("visible");

        select_enabled_track = false;


    } else if (visible && m == null) {
        aside.classList.toggle("visible");
    }


    if (type !== card_type) {
        document.querySelectorAll('#shipcard_content [data-context-type]').forEach(element => {
            if (element.dataset.contextType === type) {
                element.style.display = '';
            } else {
                element.style.display = 'none';
            }
        });

        displayShipcardIcons(type);
    }

    card_mmsi = m;
    card_type = type;


    if (shipcardVisible()) {
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


        if (isShipcardMax()) {
            toggleShipcardSize();
        }
        if (!visible) shipcardMinIfMaxonMobile();
        positionAside(pixel, aside);

        if (card_type == 'ship') populateShipcard();
        else if (card_type == 'plane') populatePlanecard();

        // trigger reflow for iPad Safari
        aside.style.display = 'none';
        aside.offsetHeight;
        aside.style.display = '';
    }

    trackLayer.changed();
    labelLayer.changed();
    updateFocusMarker();
}

const shippingMappings = {
    [ShippingClass.OTHER]: { cx: 120, cy: 20, hint: 'Other', imgSize: 20 },
    [ShippingClass.UNKNOWN]: {
        cx: 120,
        cy: 20,
        hint: 'Unknown',
        imgSize: 20
    },
    [ShippingClass.CARGO]: { cx: 0, cy: 20, hint: 'Cargo', imgSize: 20 },
    [ShippingClass.TANKER]: { cx: 80, cy: 20, hint: 'Tanker', imgSize: 20 },
    [ShippingClass.PASSENGER]: {
        cx: 40,
        cy: 20,
        hint: 'Passenger',
        imgSize: 20
    },
    [ShippingClass.HIGHSPEED]: {
        cx: 100,
        cy: 20,
        hint: 'High Speed',
        imgSize: 20
    },
    [ShippingClass.SPECIAL]: {
        cx: 60,
        cy: 20,
        hint: 'Special',
        imgSize: 20
    },
    [ShippingClass.FISHING]: {
        cx: 140,
        cy: 20,
        hint: 'Fishing',
        imgSize: 20
    },
    [ShippingClass.ATON]: {
        cx: 0,
        cy: 40,
        hint: 'AtoN',
        imgSize: 20
    },
    [ShippingClass.PLANE]: { cx: 0, cy: 60, hint: 'Aircraft', imgSize: 25 },
    [ShippingClass.HELICOPTER]: {
        cx: 0,
        cy: 85,
        hint: 'Helicopter',
        imgSize: 25
    },
    [ShippingClass.B]: { cx: 20, cy: 20, hint: 'Class B', imgSize: 20 },
    [ShippingClass.STATION]: {
        cx: 20,
        cy: 40,
        hint: 'Base Station',
        imgSize: 20
    },
    [ShippingClass.SARTEPIRB]: {
        cx: 40,
        cy: 40,
        hint: 'SART/EPIRB',
        imgSize: 20
    }
}

// Pure sprite pick for a class in motion, shared with replay: rows whose
// undirected variant sits at cy 20 have a directional one at cy 0 that is
// used once the vessel is under way.
function spriteFor(shipClass, speed, cog) {
    const sprite = shippingMappings[shipClass] || {
        cx: 120,
        cy: 20,
        imgSize: 20,
        hint: ''
    };

    let cy = sprite.cy;
    let rot = 0;

    if (sprite.cy === 20) {
        if (speed != null && speed > 0.5 && cog != null) {
            cy = 0;
            rot = cog * 3.1415926 / 180;
        }
    } else if ((shipClass == ShippingClass.HELICOPTER || shipClass == ShippingClass.PLANE) && cog != null) {
        rot = cog * 3.1415926 / 180;
    }

    return { cx: sprite.cx, cy: cy, imgSize: sprite.imgSize, hint: sprite.hint, rot: rot };
}

function getSprite(ship) {
    const s = spriteFor(ship.shipclass, ship.speed, ship.cog);

    ship.rot = s.rot
    ship.cx = s.cx
    ship.cy = s.cy
    ship.imgSize = s.imgSize
    ship.hint = s.hint
}
function getPlaneSprite(plane) {
    let sprite = shippingMappings[ShippingClass.PLANE];

    // Handle rotorcraft (TC=4, CA=7)
    if (plane.category && plane.category == 47) {
        sprite = shippingMappings[ShippingClass.HELICOPTER];
    }

    plane.scaling = 0.75; // Default scaling

    // Set scaling based on wake vortex category
    if (plane.category) {

        const ca = plane.category % 10;  // Extract CA value
        switch (ca) {
            case 1: // Light
                plane.scaling = 0.5;
                break;
            case 2: // Medium 1
            case 3: // Medium 2
                plane.scaling = 0.75;
                break;
            case 4: // High vortex
            case 5: // Heavy
                plane.scaling = 1.0;
                break;
            case 6: // High performance
                plane.scaling = 0.8;
                break;
            case 7: // Rotorcraft
                plane.scaling = 0.6;
                break;
        }
    }

    plane.scaling = plane.scaling * 1.2
    plane.rot = plane.heading * 3.1415926 / 180;
    plane.cx = sprite.cx;

    if (plane.airborne == 0)
        plane.cx = 25;
    else
        plane.cx = 50;

    plane.cy = sprite.cy;
    plane.imgSize = sprite.imgSize;
    plane.hint = sprite.hint;

    return sprite;
}

const SpritesAll = 'icons.png'

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

    if (shipcardVisible()) {
        if (card_type == "ship")
            populateShipcard();
        else if (card_type == "plane")
            populatePlanecard();
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

            if (showShapeOutlines && (ship.heading != null || settings.show_circle_outline)) {
                const shapeFeature = new ol.Feature({
                    geometry: createShipOutlineGeometry(ship)
                })
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
    range.updateDistanceCircles();

}

function updateDarkMode() {
    document.documentElement.classList.toggle("dark", settings.dark_mode);
    plotsModule?.updateColors();
    updateMapLayer();
    redrawMap();
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

}

function updateSettingsTab() {
    document.querySelectorAll('[data-on-change="setMapSetting"][data-key]').forEach(el => {
        if (el.type === 'checkbox') el.checked = settings[el.dataset.key];
        else el.value = settings[el.dataset.key];
    });

    document.getElementById("settings_darkmode").checked = settings.dark_mode;
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
        document.getElementById("settings_binary_cat_" + cat).checked = !settings.binary_exclude.includes(cat);
    }

    document.getElementById("settings_range_color").value = settings.range_color;
    document.getElementById("settings_range_timeframe").value = settings.range_timeframe;
    document.getElementById("settings_range_color_short").value = settings.range_color_short;
    document.getElementById("settings_range_color_dark").value = settings.range_color_dark;
    document.getElementById("settings_range_color_dark_short").value = settings.range_color_dark_short;

    document.getElementById("settings_map_opacity").value = settings.map_opacity;
    document.getElementById("settings_icon_scale").value = settings.icon_scale;
    document.getElementById("settings_track_history").value = Math.max(0, TRACK_HISTORY_STOPS.indexOf(settings.track_history));

    // Update all slider display values
    updateSliderDisplay('iconScale', settings.icon_scale);
    updateSliderDisplay('mapOpacity', settings.map_opacity);
    updateSliderDisplay('trackWeight', settings.track_weight);
    updateSliderDisplay('trackOpacity', settings.track_opacity);
    updateSliderDisplay('trackHistory', settings.track_history);
    updateSliderDisplay('trackTrashThreshold', settings.track_trash_threshold);
    updateSliderDisplay('tooltipFontSize', settings.tooltipLabelFontSize);
    updateSliderDisplay('shipoutlineOpacity', settings.shipoutline_opacity);
    updateSliderDisplay('circleScale', settings.circle_scale || 6.0);

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
    if (a === "map") document.getElementById("tableside").style.display = "flex";

    const tabElement = document.getElementById(a + "_tab");
    if (tabElement) tabElement.className += " active";

    const tabMiniElement = document.getElementById(a + "_tab_mini");
    if (tabMiniElement) tabMiniElement.className += " active";

    settings.tab = a;
    saveSettings();

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
    if (m in shipsDB) showShipcard('ship', m);
}

function selectTab() {
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
    activateTab(null, settings.tab);
    //document.getElementById(settings.tab + "_tab").click();
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
    const now = shipsSince || Math.floor(Date.now() / 1000);
    return now - settings.track_history * 60;
}

function trackHistoryLabel(m) {
    if (!m) return 'All';
    return m < 60 ? m + ' min' : (m / 60) + ' h';
}

const SLIDER_DISPLAYS = {
    kioskSpeed: ["kiosk_rotation_speed_label", (v) => `Rotation Speed (${v}s)`],
    trackWeight: ["track_weight_label", (v) => `Weight (${v})`],
    trackOpacity: ["track_opacity_label", (v) => `Opacity (${Math.round(parseFloat(v) * 100)}%)`],
    trackHistory: ["track_history_label", (v) => `History (${trackHistoryLabel(Number(v))})`],
    trackTrashThreshold: ["track_trash_threshold_label", (v) => `Dash Threshold (${v}s)`],
    trackSpeedMax: ["track_speed_max_label", (v) => `Scale Max (${speedWhole(v)} ${getSpeedUnit()})`],
    iconScale: ["icon_scale_label", (v) => `Marker Size (${parseFloat(v).toFixed(2)})`],
    mapOpacity: ["map_opacity_label", (v) => `Map Dimming (${Math.round(parseFloat(v) * 100)}%)`],
    tooltipFontSize: ["tooltip_font_size_label", (v) => `Font Size (${v})`],
    shipoutlineOpacity: ["shipoutline_opacity_label", (v) => `Opacity (${parseFloat(v).toFixed(2)})`],
    circleScale: ["circle_scale_label", (v) => `Width (${parseFloat(v).toFixed(1)})`],
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

function showWelcome() {
    if ((settings.welcome == true || settings.welcome == "true") && !isAndroid()) showAboutDialog();

    settings.welcome = false;
    saveSettings();
}

// for overwrite and insert code where needed

addTileLayer("OpenStreetMap", new ol.layer.Tile({
    source: new ol.source.OSM({ maxZoom: 19 })
}));

addTileLayer("Positron", new ol.layer.Tile({
    source: new ol.source.XYZ({
        url: 'https://{a-d}.basemaps.cartocdn.com/light_all/{z}/{x}/{y}{r}.png',
        attributions: '&copy; <a href="https://www.openstreetmap.org/copyright">OSM</a> &copy; <a href="https://carto.com/attributions">CARTO</a>',
        maxZoom: 20
    })
}));

addTileLayer("Positron (no labels)", new ol.layer.Tile({
    source: new ol.source.XYZ({
        url: 'https://{a-d}.basemaps.cartocdn.com/light_nolabels/{z}/{x}/{y}{r}.png',
        attributions: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors &copy; <a href="https://carto.com/attributions">CARTO</a>',
        maxZoom: 20
    })
}));

addTileLayer("Dark Matter", new ol.layer.Tile({
    source: new ol.source.XYZ({
        url: 'https://{a-d}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png',
        attributions: '&copy; <a href="https://www.openstreetmap.org/copyright">OSM</a> &copy; <a href="https://carto.com/attributions">CARTO</a>',
        maxZoom: 20
    })
}));

addTileLayer("Dark Matter (no labels)", new ol.layer.Tile({
    source: new ol.source.XYZ({
        url: 'https://{a-d}.basemaps.cartocdn.com/dark_nolabels/{z}/{x}/{y}{r}.png',
        attributions: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors &copy; <a href="https://carto.com/attributions">CARTO</a>',
        maxZoom: 20
    })
}));

addTileLayer("Voyager", new ol.layer.Tile({
    source: new ol.source.XYZ({
        url: 'https://{a-d}.basemaps.cartocdn.com/rastertiles/voyager/{z}/{x}/{y}{r}.png',
        attributions: '&copy; <a href="https://www.openstreetmap.org/copyright">OSM</a> &copy; <a href="https://carto.com/attributions">CARTO</a>',
        maxZoom: 20
    })
}));

addTileLayer("Voyager (no labels)", new ol.layer.Tile({
    source: new ol.source.XYZ({
        url: 'https://{a-d}.basemaps.cartocdn.com/rastertiles/voyager_nolabels/{z}/{x}/{y}{r}.png',
        attributions: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors &copy; <a href="https://carto.com/attributions">CARTO</a>',
        maxZoom: 20
    })
}));

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

function makeDraggable(dragHandle, dragTarget) {
    const moveThreshold = 15;
    let isDragging = false;
    let startX, startY, offsetX, offsetY;

    // add class cursor-move to dragHandle
    dragHandle.classList.add('cursor-move');

    dragHandle.addEventListener('pointerdown', (e) => {
        e.preventDefault();

        startX = e.clientX;
        startY = e.clientY;
        offsetX = startX - dragTarget.offsetLeft;
        offsetY = startY - dragTarget.offsetTop;

        const onPointerMove = (e) => {
            const dx = e.clientX - startX;
            const dy = e.clientY - startY;

            if (!isDragging && (dx * dx + dy * dy) > moveThreshold * moveThreshold) {
                isDragging = true;
                dragTarget.classList.add('dragging');
            }

            if (isDragging) {
                const newX = e.clientX - offsetX;
                const newY = e.clientY - offsetY;

                // clear right/bottom anchor so a right-anchored element moves instead of stretching
                dragTarget.style.right = 'auto';
                dragTarget.style.bottom = 'auto';
                dragTarget.style.left = `${newX}px`;
                dragTarget.style.top = `${newY}px`;

                // Update pinned position if shipcard is pinned
                if (settings.shipcard_pinned && dragTarget.id === 'shipcard') {
                    settings.shipcard_pinned_x = newX;
                    settings.shipcard_pinned_y = newY;
                    saveSettings();
                }
            }
        };

        const onPointerUp = (e) => {
            if (isDragging) {
                isDragging = false;
                dragTarget.classList.remove('dragging');
            }

            dragHandle.releasePointerCapture(e.pointerId);
            dragHandle.removeEventListener('pointermove', onPointerMove);
            dragHandle.removeEventListener('pointerup', onPointerUp);
        };

        dragHandle.setPointerCapture(e.pointerId);
        dragHandle.addEventListener('pointermove', onPointerMove);
        dragHandle.addEventListener('pointerup', onPointerUp);
    });
}

if (!window.matchMedia('(max-width: 500px), (max-height: 800px)').matches) {
    document.querySelectorAll('aside').forEach((aside) => {
        const dragHandle = aside.querySelector('.draggable');
        if (dragHandle) {
            makeDraggable(dragHandle, aside);
        }
    });
}
else {
    // hide all spans with class "draggable-hide-if-not-active"
    document.querySelectorAll('.draggable-hide-if-not-active').forEach((span) => {
        span.style.display = 'none';
    });
}

console.log("Starting plugin code");


window.loadPlugins && window.loadPlugins();

addOverlayLayer("Aircraft", planeLayer);

let urlParams = new URLSearchParams(window.location.search);
restoreDefaultSettings();

community.init({ config, getMap: () => map });
fireworks.init({ config, extraVector, showDialog, showNotification });
kiosk.init({
    getMap: () => map,
    getShipsDB: () => shipsDB,
    getShipsSince: () => shipsSince,
    getCardMmsi: () => card_mmsi,
    getHoverMmsi: () => hoverMMSI,
    showShipcard,
    saveSettings,
});
measure.init({
    getShipsDB: () => shipsDB,
    showNotification,
    ensureMeasurecardVisible: () => { if (!measurecardVisible()) toggleMeasurecard(); },
});
binary.init({
    getActiveReceiver: () => activeReceiver,
    getShipsDB: () => shipsDB,
    getDialogModal: () => dialogModal,
    showDialog,
    saveSettings,
    redrawMap,
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
replay.init({
    getReceiver: () => activeReceiver,
    spriteFor,
    iconScale,
    // always fades: the icon-fading setting only governs the live map
    fadeOpacity: fadeCurve,
    labelText: buildLabelText,
    spriteSheet: SpritesAll,
    getResolution: () => map.getView().getResolution(),
    hullStyle: shapeStyleFunction,
    setLiveLayers: setLiveLayersVisible,
    showNotification,
    onStateChange: updateReplaycard,
});

console.log("Plugin loading completed");

console.log("Load settings");
loadSettings();
updateReceiverSelect(config.receivers);

console.log("Load settings from URL parameters");

loadSettingsFromURL();
updateForLegacySettings();
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
prepareShipcard();
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
kiosk.updateKiosk();
applyShipcardPinStyling()
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

