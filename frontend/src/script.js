import { settings, isAndroid } from './core/state.js';
import { ShippingClass } from './core/constants.js';
import { debounce, decodeHTMLEntities, deriveLabelBackground, copyToClipboard } from './core/util.js';
import { calcOffset1M, createShipOutlineGeometry, createDistanceGeometry } from './core/geo.js';
import { init as initRainRadar } from './overlays/rainradar.js';
import * as fireworks from './overlays/fireworks.js';
import * as community from './overlays/community.js';
import * as kiosk from './features/kiosk.js';
import * as measure from './features/measure.js';
import {
    getDistanceVal, getDistanceUnit,
    getSpeedVal, getSpeedUnit,
    getDimVal, getDimUnit, getShipDimension,
    getLatValFormat, getLonValFormat,
    getEtaVal, getDeltaTimeVal,
    getShipName, getCallSign, setShipNameProvider, setCallSignProvider,
    getCountryName, getFlagStyled,
    getStatusVal, getMmsiTypeVal,
    getStringfromMsgType, getStringfromGroup, getStringfromChannels,
    getShipTypeShort, getShipTypeFull,
    sanitizeString, formatBytes,
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
        share_location: false, save_messages: false,
        realtime: false, log: false, decoder: false,
        about_md: false,
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
    headerClick: () => headerClick(),
    activateTab: (e, d) => activateTab(e, d.tab),
    openWebControl: () => openWebControl(),
    toggleInfoPanel: () => toggleInfoPanel(),
    headerSettings: (e) => showContextMenu(e, '', '', ['settings', 'center']),
    toggleScreenSize: () => toggleScreenSize(),
    toggleReceiverDropdown: (e) => toggleReceiverDropdown(e),

    // tableside / generic close buttons / dialog
    hideTablecard: () => hideTablecard(),
    updateTableSort: (e) => updateTableSort(e),
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
    setRangeSwitch: (e, d, el) => setRangeSwitch(el.checked),
    setRangeTimePeriod: (e, d, el) => setRangeTimePeriod(el.value),
    setKiosk: (e, d, el) => kiosk.setKiosk(el.checked),
    setKioskRotationSpeed: (e, d, el) => kiosk.setKioskRotationSpeed(el.value),
    setKioskPanMap: (e, d, el) => kiosk.setKioskPanMap(el.checked),
    setGraphVisibility: (e, d, el) => setGraphVisibility(d.graph, el.checked),
    setMapSetting: (e, d, el) => setMapSetting(d.key, el.type === 'checkbox' ? el.checked : el.value),
    setRangeColor: (e, d, el) => setRangeColor(el.value, d.field),
    setMapSettingDistanceColor: (e, d, el) => { removeDistanceCircles(); setMapSetting('distance_circle_color', el.value); },
    setShowTrackOnSelect: (e, d, el) => { settings.show_track_on_select = el.checked; saveSettings(); },
    setShowTrackOnHover: (e, d, el) => { settings.show_track_on_hover = el.checked; saveSettings(); },
    applyColorToAllTracks: (e, d, el) => applyColorToAllTracks(el.value),
    resetTrackColorsToDefault: () => resetTrackColorsToDefault(),
    setTrackClassColor: (e, d, el) => setTrackClassColor(d.shipclass, el.value),

    // settings: range/icon-scale/opacity sliders (oninput = display only, onchange = persist)
    setIconScale: (e, d, el) => { settings.icon_scale = el.value; redrawMap(); saveSettings(); },
    updateIconScaleDisplay: (e, d, el) => updateIconScaleDisplay(el.value),
    setMapOpacity: (e, d, el) => { settings.map_opacity = el.value; setMapOpacity(); saveSettings(); },
    updateMapOpacityDisplay: (e, d, el) => updateMapOpacityDisplay(el.value),
    updateCircleScaleDisplay: (e, d, el) => updateCircleScaleDisplay(el.value),
    updateTooltipFontSizeDisplay: (e, d, el) => updateTooltipFontSizeDisplay(el.value),
    updateShipoutlineOpacityDisplay: (e, d, el) => updateShipoutlineOpacityDisplay(el.value),
    updateTrackWeightDisplay: (e, d, el) => updateTrackWeightDisplay(el.value),
    updateTrackTrashThresholdDisplay: (e, d, el) => updateTrackTrashThresholdDisplay(el.value),
    updateKioskSpeedDisplay: (e, d, el) => updateKioskSpeedDisplay(el.value),

    // context menu (depends on global context_mmsi/card_mmsi)
    toggleShipcardPin: () => toggleShipcardPin(),
    pinStation: () => pinStation(),
    copyTextCtx: () => copyText(context_mmsi),
    copyTextICAO: () => copyText(getICAOFromHexIdent(context_mmsi)),
    downloadCSV: () => shipTableModule?.downloadCSV(),
    unpinCenter: () => unpinCenter(),
    showAllTracks: () => showAllTracks(),
    deleteAllTracks: () => deleteAllTracks(),
    ToggleFireworks: () => fireworks.toggle(),
    toggleLabel: () => toggleLabel(),
    toggleKioskMode: () => kiosk.toggleKioskMode(),
    toggleFading: () => toggleFading(),
    toggleRange: () => toggleRange(),
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

    // realtime / decoder controls
    promptFilterMMSI: () => realtimeModule?.promptFilter(),
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
    showBinaryMessageDialogCard: () => showBinaryMessageDialog(card_mmsi),
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
    mapSettingsContextMenu: (e) => showContextMenu(e, '', '', ['settings', 'center', 'ctx-map']),
    toggleCommunityPane: () => community.toggleCommunityPane(),
    showMapMenu: (e) => showMapMenu(e),
    mainspaceContextMenu: (e) => showContextMenu(e, 0, '', ['settings']),
    plotsContextMenu: (e) => showContextMenu(e, '', 'charts', ['settings', 'ctx-charts']),

    // info panel
    showAboutFromInfoPanel: () => { toggleInfoPanel(); showAboutDialog(); },

    // dynamically-rendered shipcard items
    rotateShipcardIcons: () => rotateShipcardIcons(),
    techInfo: (e) => { e.stopPropagation(); toggleShipcardPopover("tech_popover", "shipcard_tech_info"); },
    shiptypeInfo: (e) => { e.stopPropagation(); toggleShipcardPopover("shiptype_popover", "shipcard_shiptype_info"); },
    showNMEAContextCopy: (e, d) => showContextMenu(e, d.copy || '', 'ship', ['settings', 'copy-text']),
    removeRealtimeFilterMMSI: (e, d) => realtimeModule?.removeFilter(d.mmsi),
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
    paths = {},
    map,
    basemaps = {},
    overlapmaps = {},
    station = {},
    shipsDB = {},
    shipsSince = 0,
    shipsTimeout = 1800,
    shipsLastCleanup = 0,
    binaryDB = {},
    binarySince = 0,
    binaryTimeout = 1800,
    planesDB = {},
    planesSince = 0,
    planesTimeout = 300,
    planesLastCleanup = 0,
    hover_feature = undefined,
    show_all_tracks = false,
    logViewer = null,
    range_outline = undefined,
    range_outline_short = undefined,
    tab_title_station = config.station,
    tab_title_count = null,
    context_mmsi = null,
    context_type = null,
    tab_title = "AIS-catcher";

const baseMapSelector = document.getElementById("baseMapSelector");

let shipcardIconCount = undefined;
const shipcardIconMax = 3;
let shipcardIconOffset = 0;
const plugins_main = [];
let card_mmsi = null,
    card_type = null;
// `let` so AISCatcher.setRefreshInterval can mutate.
let refreshIntervalMs = 2500;
let shipFilterOverride = null;
let range_update_time = null;
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

function restoreDefaultSettings() {
    // In-place mutation: `settings` is imported, the binding is read-only here.
    for (const k of Object.keys(settings)) delete settings[k];
    Object.assign(settings, {
        counter: true,
        fading: false,
        android: false,
        kiosk: false,
        welcome: true,
        coordinate_format: "decimal",
        icon_scale: 1,
        track_weight: 1,
        track_trash_threshold: 30,
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
        label_class_background: true,
        eri: true,
        loadURL: true,
        map_opacity: 0.5,
        show_track_on_hover: false,
        show_track_on_select: false,
        shipcard_pinned: false,
        show_signal_graphs: true,
        show_ppm_graphs: true,
        shipcard_pinned_x: null,
        shipcard_pinned_y: null,
        kiosk_rotation_speed: 5,
        kiosk_pan_map: true,
        shiptable_columns: ["shipname", "mmsi", "imo", "callsign", "shipclass", "lat", "lon", "last_signal", "level", "distance", "bearing", "speed", "repeat", "ppm", "status"],
        realtime_background_streaming: false,
        realtime_filter_mmsis: []
    });

    // Set default track colors
    settings.track_class_colors = getDefaultTrackColors();
}


// NMEA Decoder Functions
function toggleInfoPanel() {
    const overlay = document.querySelector('.overlay');
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
    setMetrics(settings.metric);
    updateMapLayer();
    setFading(settings.fading);

    updateFocusMarker();
    removeDistanceCircles();

    settings.tab = t;
    settings.welcome = false;

    redrawMap();
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
        if (typeof settings.map_opacity !== 'undefined') {
            layer.setOpacity(Number(settings.map_opacity));
        }
        addOverlayCheckbox(title);
    }
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

    overlayContainer.appendChild(checkbox);
    overlayContainer.appendChild(label);
    overlayContainer.appendChild(document.createElement('br'));

    checkbox.addEventListener('change', function () {
        overlapmaps[title].setVisible(this.checked);
        if (this.checked) {
            if (!settings.map_overlay.includes(title)) settings.map_overlay.push(title);
        } else {
            const i = settings.map_overlay.indexOf(title);
            if (i > -1) settings.map_overlay.splice(i, 1);
        }
        saveSettings();
        redrawMap();
    });
}
function removeOverlayLayer(title) {
    delete overlapmaps[title];
}
function removeOverlayLayerAll() {
    overlapmaps = {};
}
function addControlToMap(c) {
    c.addTo(map);
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

function openSettings() {
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


function copyCoordinates(m) {
    const raw = shipsDB[m]?.raw;
    const coords = raw ? raw.lat + "," + raw.lon : "not found";
    if (copyClipboard(coords)) showNotification("Coordinates copied to clipboard");
}

let hoverMMSI = undefined;
let hoverType = undefined;

const rangeStyleFunction = function (feature) {
    let clr = undefined;

    if (feature.short) {
        clr = settings.dark_mode ? settings.range_color_dark_short : settings.range_color_short;
    } else {
        clr = settings.dark_mode ? settings.range_color_dark : settings.range_color;
    }

    return new ol.style.Style({
        stroke: new ol.style.Stroke({
            color: clr,
            width: feature === hover_feature ? 4 : 2
        })
    });
}

const shapeStyleFunction = function (feature) {

    const c = settings.shipoutline_inner;
    const o = settings.shipoutline_opacity;

    return new ol.style.Style({
        fill: new ol.style.Fill({
            color: `rgba(${parseInt(c.slice(-6, -4), 16)}, ${parseInt(c.slice(-4, -2), 16)}, ${parseInt(c.slice(-2), 16)}, ${o})`
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

    // Use shipping class color if available
    if (feature.shipclass && settings.track_class_colors[feature.shipclass]) {
        c = settings.track_class_colors[feature.shipclass];
    }

    if (feature.mmsi == hoverMMSI && hoverType == 'ship') {
        c = settings.shiphover_color;
        w = w + 2;
    }
    else if (feature.mmsi == card_mmsi && card_type == 'ship') {
        c = settings.shipselection_color;
        w = w + 2;
    }

    return new ol.style.Style({
        stroke: new ol.style.Stroke({
            color: c,
            width: w,
            lineDash: feature.isDashed ? [6, 6] : undefined
        })
    });
}

const markerStyle = function (feature) {

    const length = (feature.ship.to_bow || 0) + (feature.ship.to_stern || 0);
    const mult = length >= 100 && length <= 200 ? 0.9 : length > 200 ? 1.1 : 0.75;

    return new ol.style.Style({
        image: new ol.style.Icon({
            src: SpritesAll,
            rotation: feature.ship.rot,
            offset: [feature.ship.cx, feature.ship.cy],
            size: [feature.ship.imgSize, feature.ship.imgSize],
            scale: settings.icon_scale * mult,
            opacity: 1
        })
    });
};


const planeStyle = function (feature) {
    const altitude = feature.plane.altitude || 0;
    const shadowScale = Math.min(Math.max(altitude / 40000, 0.1), 1); // 0.1-1 scale based on height up to 40000ft

    return [
        /* Shadow/border layer
        new ol.style.Style({
            image: new ol.style.Icon({
                src: "https://www.aiscatcher.org/hub_test/sprites_hub.png",
                rotation: feature.plane.rot,
                offset: [feature.plane.cx, feature.plane.cy],
                size: [feature.plane.imgSize, feature.plane.imgSize],
                scale: (settings.icon_scale * feature.plane.scaling) * (1 + shadowScale * 1),
                opacity: 0.25
            })
        }), */
        // Main aircraft layer
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

const labelStyle = function (feature) {
    const font = settings.tooltipLabelFontSize + "px Arial";
    const text = new ol.style.Text({
        text: decodeHTMLEntities('ship' in feature ?
            (feature.ship.shipname || feature.ship.mmsi.toString()) :
            (feature.plane.callsign || getICAO(feature.plane))),
        overflow: true,
        offsetY: 25,
        offsetX: 25,
        font: font
    });

    if (settings.label_class_background) {
        const obj = 'ship' in feature ? feature.ship : feature.plane;
        const base = (obj && settings.track_class_colors[obj.shipclass]) || '#12a5ed';
        text.setFill(new ol.style.Fill({ color: '#ffffff' }));
        text.setBackgroundFill(new ol.style.Fill({ color: deriveLabelBackground(base) }));
        text.setBackgroundStroke(new ol.style.Stroke({ color: 'rgba(0, 0, 0, 0.35)', width: 1 }));
        text.setPadding([2, 4, 2, 4]);
    } else {
        text.setFill(new ol.style.Fill({
            color: settings.dark_mode ? settings.tooltipLabelColorDark : settings.tooltipLabelColor
        }));
        text.setStroke(new ol.style.Stroke({
            color: settings.dark_mode ? settings.tooltipLabelShadowColorDark : settings.tooltipLabelShadowColor,
            width: 5
        }));
    }

    return new ol.style.Style({ text: text });
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
    return new ol.style.Style({
        image: new ol.style.Circle({
            radius: 13 * iconScale * radiusScale,
            stroke: new ol.style.Stroke({
                color: settings.shipselection_color,
                width: circleScale * iconScale
            })
        })
    });
}

const binaryAssociatedOutline = new ol.style.Style({
    image: new ol.style.Circle({
        radius: 12,
        fill: new ol.style.Fill({ color: 'rgba(255, 255, 255, 0)' }),
        stroke: new ol.style.Stroke({ color: 'white', width: 2 })
    }),
    zIndex: 200
});
const binaryStyleCache = new Map();
const binaryStyle = function (feature) {
    const count = feature.get('binary_count') || feature.binary_count || 1;
    const isAssociated = feature.get('is_associated') || feature.is_associated;
    const key = (isAssociated ? 'a:' : 'n:') + count;

    let cached = binaryStyleCache.get(key);
    if (cached) return cached;

    if (isAssociated) {
        const badge = new ol.style.Style({
            image: new ol.style.Circle({
                radius: 8,
                fill: new ol.style.Fill({ color: 'rgba(220, 0, 0, 0.9)' }),
                stroke: new ol.style.Stroke({ color: 'white', width: 1 }),
                displacement: [10, 10]
            }),
            text: new ol.style.Text({
                text: count.toString(),
                font: 'bold 9px Arial',
                fill: new ol.style.Fill({ color: 'white' }),
                offsetX: 10,
                offsetY: -10,
                textAlign: 'center',
                textBaseline: 'middle'
            }),
            zIndex: 201
        });
        cached = [binaryAssociatedOutline, badge];
    } else {
        const circle = new ol.style.Style({
            image: new ol.style.Circle({
                radius: 10,
                fill: new ol.style.Fill({ color: 'rgba(220, 0, 0, 0.9)' }),
                stroke: new ol.style.Stroke({ color: 'white', width: 1.5 })
            }),
            text: new ol.style.Text({
                text: count.toString(),
                font: 'bold 9px Arial',
                fill: new ol.style.Fill({ color: 'white' }),
                textAlign: 'center',
                textBaseline: 'middle'
            }),
            zIndex: 100
        });
        cached = [circle];
    }
    binaryStyleCache.set(key, cached);
    return cached;
};


const markerVector = new ol.source.Vector({
    features: []
})

const binaryVector = new ol.source.Vector({
    features: []
})


const rangeVector = new ol.source.Vector({
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

const binaryLayer = new ol.layer.Vector({
    source: binaryVector,
    style: binaryStyle
})

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

const rangeLayer = new ol.layer.Vector({
    source: rangeVector,
    style: rangeStyleFunction
});

const labelLayer = new ol.layer.Vector({
    source: labelVector,
    style: labelStyle,
    declutter: settings.labels_declutter || true
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
    }
    return response.text();
}


async function showNMEA(m) {
    if (config.features.save_messages) {
        const s = await fetchJSON("api/message", m);
        const obj = JSON.parse(s);

        let tableHtml = '<table class="mytable">';
        for (let key in obj) {
            let value = obj[key];
            if (Array.isArray(value) || (value !== null && typeof value === 'object')) value = JSON.stringify(value);
            if (value == null) value = '';
            const safeKey = sanitizeString(String(key));
            const safeVal = sanitizeString(String(value));
            tableHtml += `<tr><td>${safeKey}</td><td data-on-contextmenu="showNMEAContextCopy" data-copy="${safeVal}">${safeVal}</td></tr>`;
        }
        tableHtml += "</table>";

        showDialog("Message " + m, tableHtml);
    } else {
        showDialog("Error", 'Please enable "-N MSG on" in AIS-catcher settings.');
    }
}

async function showVesselDetail(m) {
    let s = await fetchJSON("api/vessel", m);
    let obj = JSON.parse(s);

    let tableHtml = '<table class="mytable">';
    for (let key in obj) {
        tableHtml += "<tr><td>" + key + "</td><td>" + obj[key] + "</td></tr>";
    }
    tableHtml += "</table>";

    showDialog("Vessel " + m, tableHtml);
}

function showBinaryMessageDialog(featureOrMmsi) {
    let title, content;

    if (typeof featureOrMmsi === 'object') {
        if (!featureOrMmsi.binary_messages || featureOrMmsi.binary_messages.length === 0) {
            showDialog("Binary Message", "No message content available");
            return;
        }

        title = "Binary Messages";
        content = getBinaryMessageList(featureOrMmsi.binary_messages);

    } else if (typeof featureOrMmsi === 'number' || typeof featureOrMmsi === 'string') {
        const mmsi = Number(featureOrMmsi);

        if (!binaryDB[mmsi] || !binaryDB[mmsi].ship_messages || binaryDB[mmsi].ship_messages.length === 0) {
            showDialog("Binary Messages", "No binary messages available for this vessel");
            return;
        }

        const shipName = shipsDB[mmsi]?.raw?.shipname || `MMSI ${mmsi}`;
        title = `Binary Messages for ${shipName}`;
        content = getBinaryMessageList(binaryDB[mmsi].ship_messages);
    }

    showDialog(title, content);
    document.getElementById("dialog-box").style.maxWidth = "500px";
}

function getBinaryMessageList(messages) {
    if (!messages || messages.length === 0) {
        return "<p>No messages available</p>";
    }

    const sortedMessages = [...messages].sort((a, b) => b.timestamp - a.timestamp);

    let content = '<div class="binary-messages-list">';

    content += `<div class="binary-message-count">${sortedMessages.length} message${sortedMessages.length > 1 ? 's' : ''} available</div>`;

    sortedMessages.forEach((msg, index) => {
        if (index > 0) {
            content += '<hr style="margin: 15px 0; border: 0; border-top: 1px solid rgba(0,0,0,0.1);">';
        }

        content += '<div class="binary-message-item">';

        content += `<div class="binary-message-header">
                      <span class="binary-message-time">${msg.formattedTime || new Date(msg.timestamp * 1000).toLocaleTimeString()}</span>`;

        if (msg.message && msg.message.mmsi) {
            const shipName = msg.message.mmsi in shipsDB ?
                (shipsDB[msg.message.mmsi].raw.shipname || `MMSI ${msg.message.mmsi}`) :
                `MMSI ${msg.message.mmsi}`;

            content += `<span class="binary-message-source"> from ${shipName}</span>`;
        }

        content += '</div>';

        if (msg.message && msg.message.dac == 1 && (msg.message.fid == 31 || msg.message.fi == 31)) {
            content += getBinaryMessageContent(msg, true);
        } else if (isInlandMessage(msg)) {
            content += getInlandMessageContent(msg);
        } else if (isTextMessage(msg)) {
            content += getTextMessageContent(msg);
        } else {
            content += '<div class="binary-message-details">';
            content += `<div><strong>Message Type:</strong> ${msg.message ? `DAC ${msg.message.dac}, FI ${msg.message.fid || msg.message.fi}` : 'Unknown'}</div>`;

            // Add a collapsible section for raw data
            content += `<details class="binary-raw-data">
                          <summary>Show Raw Data</summary>
                          <pre>${JSON.stringify(msg.message, null, 2)}</pre>
                        </details>`;

            content += '</div>';
        }

        content += '</div>';
    });

    content += '</div>';

    return content;
}

function copyText(m) {
    if (copyClipboard(m)) showNotification("Content copied to clipboard");
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

function showContextMenu(event, mmsi, type, context) {

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

    classList.forEach((className) => {
        if (context.includes('object')) {
            context.push(type);
        }
        if (context.includes('object-map')) {
            context.push(type + "-map");
        }
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

    // we might have made non-android items visible in the context menu, so hide non-android items if needed
    updateAndroid();
    kiosk.updateKiosk();

    if (show_all_tracks) {
        document.querySelectorAll(".ctx-noalltracks").forEach(function (element) {
            element.style.display = "none";
        });
    }

    if (show_all_tracks || marker_tracks.size > 0) {
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

    if (context.includes("center")) {
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

function showDialog(title, message) {
    let dialogBox = document.getElementById("dialog-box");
    const dialogTitle = dialogBox.querySelector(".dialog-title");
    const dialogMessage = dialogBox.querySelector(".dialog-message");

    dialogTitle.innerText = title;
    dialogMessage.innerHTML = message;
    dialogBox.classList.remove("hidden");
}

function closeDialog() {
    let dialogBox = document.getElementById("dialog-box");
    dialogBox.classList.add("hidden");
    dialogBox.style.maxWidth = "";
}

function showNotification(message) {
    console.log("[notification] " + message);
    const notificationElement = document.createElement("div");
    notificationElement.classList.add("notification");
    notificationElement.textContent = message;

    notificationContainer.appendChild(notificationElement);

    setTimeout(() => {
        notificationElement.style.opacity = 0;
        setTimeout(() => {
            notificationContainer.removeChild(notificationElement);
        }, 500);
    }, 2000);
}

function checkLatestVersion() {
    const buildVersion = config.build.version;
    if (!buildVersion || buildVersion === 'unknown') {
        console.log('AIS-catcher: build version not available, skipping version check');
        return;
    }

    fetch('https://www.aiscatcher.org/api/version')
        .then(response => {
            if (!response.ok) {
                throw new Error('Failed to fetch latest release info');
            }
            return response.json();
        })
        .then(data => {
            const latestVersion = data.tag_name;
            if (latestVersion && latestVersion !== buildVersion) {
                console.log('AIS-catcher: New version available! Current: ' + buildVersion + ', Latest: ' + latestVersion);
                showDialog('Update Available',
                    'A new version of AIS-catcher is available!<br><br>' +
                    'Current version: <b>' + buildVersion + '</b><br>' +
                    'Latest version: <b>' + latestVersion + '</b><br><br>' +
                    'Updating ensures you have the latest features and security updates.<br><br>' +
                    'Please visit <a href="https://docs.aiscatcher.org" target="_blank">docs.aiscatcher.org</a> for installation instructions or ' +
                    '<a href="https://github.com/jvde-github/AIS-catcher/releases/latest" target="_blank">GitHub Releases</a> to download the latest version.');
            } else {
                console.log('AIS-catcher: You are running the latest version (' + buildVersion + ')');
            }
        })
        .catch(error => {
            console.log('AIS-catcher: Could not check for updates - ' + error.message);
        });
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

        const overlays = JSON.parse(JSON.stringify(settings.map_overlay));
        settings.map_overlay = JSON.parse(JSON.stringify(overlays));

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

    if (settings.dark_mode) {
        activeTileLayer = settings.map_night in basemaps ? basemaps[settings.map_night] : basemaps[Object.keys(basemaps)[0]];

    } else {
        activeTileLayer = settings.map_day in basemaps ? basemaps[settings.map_day] : basemaps[Object.keys(basemaps)[0]];
    }

    activeTileLayer.setVisible(true);

    if (settings.map_overlay.length > 0) {
        for (let i = 0; i < settings.map_overlay.length; i++) {
            if (settings.map_overlay[i] in overlapmaps)
                overlapmaps[settings.map_overlay[i]].setVisible(settings.map_overlay[i] in overlapmaps);
        }
    }

    const attributions = activeTileLayer.getSource().getAttributions();
    const mapAttributions = document.getElementById("map_attributions");
    if (typeof attributions === 'function') {
        const currentAttributions = attributions();
        mapAttributions.innerHTML = currentAttributions.join(', ');
    } else if (Array.isArray(attributions)) {
        mapAttributions.innerHTML = attributions.join(', ');
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
                    border-color: var(--menu-border-color);
                    border-radius: 5px;
                    box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1), 0 4px 6px -2px rgba(0, 0, 0, 0.05);
                }
            }
    `;
    else style += " .settings_window { top: 72px; }\n";

    dynamicStyle.innerHTML = style;
}

function setMapOpacity() {
    for (let key in basemaps)
        basemaps[key].setOpacity(Number(settings.map_opacity));

    for (let key in overlapmaps) {
        if (key !== "Aircraft") {
            overlapmaps[key].setOpacity(Number(settings.map_opacity));
        }
    }
}

let clickTimeout = undefined;
const handleClick = function (pixel, target, event) {
    if (clickTimeout) {
        clearTimeout(clickTimeout);
        clickTimeout = null;
        return;
    }

    const feature = target.closest('.ol-control') ? undefined : map.forEachFeatureAtPixel(pixel,
        function (feature) { if ('ship' in feature || 'plane' in feature || 'link' in feature || 'binary' in feature) { return feature; } }, { hitTolerance: 10 });

    let included = feature && 'ship' in feature && feature.ship.mmsi in shipsDB;
    let included_plane = feature && 'plane' in feature && feature.plane.hexident in planesDB;

    if (event.originalEvent.shiftKey || measure.isActive()) {
        const shipMmsi = (feature && 'ship' in feature && feature.ship.mmsi in shipsDB) ? feature.ship.mmsi : null;
        measure.handleMapClick(shipMmsi, () => ol.proj.toLonLat(map.getCoordinateFromPixel(pixel)));
        return;
    }



    if (feature && 'link' in feature && !included) {
        window.open(feature.link, '_blank');
    } else if (feature && feature.binary === true && !feature.is_associated) {
        closeDialog();
        closeSettings();
        showBinaryMessageDialog(feature);
        return;
    } else if (feature && 'ship' in feature || included) {

        closeDialog();
        closeSettings();
        showShipcard('ship', feature.ship.mmsi, pixel);
    }
    else if (feature && 'plane' in feature || included_plane) {

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

    [rangeLayer, shapeLayer, trackLayer, markerLayer, labelLayer, extraLayer, binaryLayer, measure.measureVector].forEach(layer => {
        map.addLayer(layer);
    });

    triggerMapLayer();

    map.on('movestart', function () {
        stopHover();
    });

    map.on('moveend', function (evt) {
        debouncedSaveSettings();
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
        option.value = key;
        option.textContent = key;
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

function setMetrics(s) {
    if (s.toUpperCase() == "DEFAULT") settings.metric = "DEFAULT";
    else if (s.toUpperCase() == "METRIC") settings.metric = "SI";
    else if (s.toUpperCase() == "IMPERIAL") settings.metric = "IMPERIAL";
    else settings.metric = "DEFAULT";

    showNotification("Switched units to " + s);
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
                    cHeli++;
                    break;
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

function updateTableSort(event) {
    const header = event.currentTarget;

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
    if (valueA == null && valueB == null) return settings.tableside_order === "ascending" ? 1 : -1;
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

        const dist = ship.distance ? (getDistanceVal(ship.distance) + (ship.repeat > 0 ? " (R)" : "")) : "";
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

function setRangeSwitch(b) {
    if (b != settings.show_range) {
        toggleRange();
    }
}

function setRangeColor(v, f) {
    settings[f] = v;
    redrawMap();
}

function setRangeTimePeriod(v) {
    settings.range_timeframe = v;
    saveSettings();
    fetchRange(true).then(() => drawRange());
}

async function toggleRange() {
    const isSationSet = station && Object.hasOwn(station, "lat") && Object.hasOwn(station, "lon") && !(station.lat == 0 && station.lon == 0);

    if (!config.features.share_location || !isSationSet) {
        showDialog("Error", "Unable to show range as station location not available");
        settings.show_range = false;
    } else settings.show_range = !settings.show_range;

    saveSettings();

    await fetchRange();
    drawRange();
}

function setFading(b) {
    if (b != settings.fading) {
        toggleFading();
    }
}

function toggleFading() {
    settings.fading = !settings.fading;
}

function setGraphVisibility(type, show, save = true) {
    if (type === 'signal') {
        settings.show_signal_graphs = show;
        // Toggle signal level graphs
        document.querySelectorAll('.graph-panel').forEach(panel => {
            const header = panel.querySelector('header');
            if (header && header.textContent.includes('Signal Level')) {
                panel.style.display = show ? '' : 'none';
            }
        });
    } else if (type === 'ppm') {
        settings.show_ppm_graphs = show;
        // Toggle frequency shift graphs
        document.querySelectorAll('.graph-panel').forEach(panel => {
            const header = panel.querySelector('header');
            if (header && header.textContent.includes('Frequency Shift')) {
                panel.style.display = show ? '' : 'none';
            }
        });
    }
    if (save) saveSettings();
}

function toggleGraphVisibility(type) {
    if (type === 'signal') {
        setGraphVisibility('signal', !settings.show_signal_graphs);
    } else if (type === 'ppm') {
        setGraphVisibility('ppm', !settings.show_ppm_graphs);
    }
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

async function fetchRange(forcefetch = false) {
    const isSationSet = station && Object.hasOwn(station, "lat") && Object.hasOwn(station, "lon") && !(station.lat == 0 && station.lon == 0);

    if (!isSationSet || !settings.show_range) {
        settings.show_range = false;
        range_update_time = undefined;
        return;
    }

    const now = new Date();

    if (!forcefetch && range_update_time && Math.floor((now - range_update_time) / 1000 / 60) < 15) return;

    range_update_time = now;

    let response, h;
    try {
        response = await fetch("api/history_full.json?receiver=" + activeReceiver);
        h = await response.json();
    } catch (error) {
        settings.show_range = false;

        return;
    }



    range_outline = [];
    range_outline_short = [];

    const N = h.day.stat[0].radar_a.length;

    const range = [];
    const range_short = [];

    for (let i = 0; i < N; i++) {
        let m = 0;
        for (let j = 0; j < h.minute.stat.length; j++) {
            m = Math.max(m, h.minute.stat[j].radar_a[i]);
            m = Math.max(m, h.minute.stat[j].radar_b[i]);
        }

        range_short.push(m);

        for (let j = 0; j < h.hour.stat.length; j++) {
            m = Math.max(m, h.hour.stat[j].radar_a[i]);
            m = Math.max(m, h.hour.stat[j].radar_b[i]);
        }

        const additionalDays = settings.range_timeframe == "7d" ? 7 : settings.range_timeframe == "30d" ? 30 : 0;

        for (let j = 0; j < Math.min(additionalDays, h.day.stat.length); j++) {
            m = Math.max(m, h.day.stat[j].radar_a[i]);
            m = Math.max(m, h.day.stat[j].radar_b[i]);
        }

        range.push(m);
    }

    const deltaNorth = calcOffset1M([station.lon, station.lat], 0)[0];
    const deltaEast = calcOffset1M([station.lon, station.lat], 90)[1];

    for (let i = 0; i < N; i++) {
        range_outline.push([
            station.lon + ((range[i] * deltaEast * 1000) / 0.5399568) * Math.sin(((i * 2) / N) * Math.PI),
            station.lat + ((range[i] * deltaNorth * 1000) / 0.5399568) * Math.cos(((i * 2) / N) * Math.PI),
        ]);

        range_outline.push([
            station.lon + ((range[i] * deltaEast * 1000) / 0.5399568) * Math.sin((((i + 1) * 2) / N) * Math.PI),
            station.lat + ((range[i] * deltaNorth * 1000) / 0.5399568) * Math.cos((((i + 1) * 2) / N) * Math.PI),
        ]);

        range_outline_short.push([
            station.lon + ((range_short[i] * deltaEast * 1000) / 0.5399568) * Math.sin(((i * 2) / N) * Math.PI),
            station.lat + ((range_short[i] * deltaNorth * 1000) / 0.5399568) * Math.cos(((i * 2) / N) * Math.PI),
        ]);

        range_outline_short.push([
            station.lon + ((range_short[i] * deltaEast * 1000) / 0.5399568) * Math.sin((((i + 1) * 2) / N) * Math.PI),
            station.lat + ((range_short[i] * deltaNorth * 1000) / 0.5399568) * Math.cos((((i + 1) * 2) / N) * Math.PI),
        ]);
    }

    range_outline = range_outline.map(point => ol.proj.fromLonLat(point));
    range_outline_short = range_outline_short.map(point => ol.proj.fromLonLat(point));
}

let rangeFeature = undefined;
let rangeShortFeature = undefined;

function drawRange() {
    if (rangeFeature) {
        rangeVector.removeFeature(rangeFeature);
        rangeFeature = undefined;
    }

    if (rangeShortFeature) {
        rangeVector.removeFeature(rangeShortFeature);
        rangeShortFeature = undefined;
    }

    if (!settings.show_range) return;

    if (range_outline) {
        rangeFeature = new ol.Feature({
            geometry: new ol.geom.Polygon([range_outline])
        });

        rangeFeature.short = false;
        rangeFeature.rangering = true;
        rangeFeature.tooltip = "Station Range " + settings.range_timeframe;
        rangeVector.addFeature(rangeFeature);
    }

    if (range_outline_short) {
        rangeShortFeature = new ol.Feature({
            geometry: new ol.geom.Polygon([range_outline_short])
        });

        rangeShortFeature.tooltip = "Station Range 1h";
        rangeShortFeature.rangering = true;
        rangeShortFeature.short = true;
        rangeVector.addFeature(rangeShortFeature);
    }
}

let distanceFeatures = undefined;
let distanceLat = undefined;
let distanceLon = undefined;
let distanceMetric = undefined;

function removeDistanceCircles() {
    if (distanceFeatures) {
        for (var i = 0; i < distanceFeatures.length; i++)
            rangeVector.removeFeature(distanceFeatures[i]);
    }
    distanceFeatures = undefined;
}


function distanceCircleStyleFunction(feature) {
    let clr, width;

    if (feature === hover_feature) {
        clr = 'orange';
        clr = settings.distance_circle_color;

        width = 5;
    } else {
        clr = settings.distance_circle_color;
        width = 1;
    }

    return new ol.style.Style({
        stroke: new ol.style.Stroke({
            color: clr,
            width: width
        })
    });
}


function updateDistanceCircles() {
    const lat = station.lat;
    const lon = station.lon;

    if (!settings.distance_circles) {
        removeDistanceCircles();
        return;
    }

    if (!lat || !lon) return;

    if (!distanceFeatures || distanceLat != lat || distanceLon != lon || distanceMetric != settings.metric) {

        removeDistanceCircles();

        distanceLat = lat;
        distanceLon = lon;
        distanceMetric = settings.metric;

        distanceFeatures = [];

        const conv = settings.metric === "DEFAULT" ? 1.852 : settings.metric === "SI" ? 1 : 1.609344;

        const range = [5000, 10000, 25000, 50000, 100000];

        for (var i = 0; i < range.length; i++) {

            let distanceCircle = new ol.Feature({
                geometry: createDistanceGeometry(lat, lon, range[i] * conv)
            });

            distanceCircle.tooltip = range[i] / 1000 + " " + getDistanceUnit().toUpperCase();
            distanceCircle.distancecircle = true;
            distanceCircle.setStyle(distanceCircleStyleFunction);
            rangeVector.addFeature(distanceCircle);
            distanceFeatures.push(distanceCircle);
        }
    }
}

function formatTime(timestamp) {
    const date = new Date(timestamp * 1000);
    return date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
}

async function fetchBinary() {
    const isIncremental = binarySince > 0;

    if (!isIncremental) binaryDB = {};

    let response;
    try {
        response = await fetch("api/binmsgs.json?receiver=" + activeReceiver + (isIncremental ? "&since=" + binarySince : ""));
        const data = await response.json();

        const messages = data.messages || [];
        const serverTime = data.time || 0;
        if (data.timeout) binaryTimeout = data.timeout;

        messages.forEach((msg) => {
            const hasLocation = msg.message && msg.message.lat && msg.message.lon;
            if (msg.message && msg.message.mmsi && (hasLocation || isInlandMessage(msg) || isTextMessage(msg))) {
                const mmsi = msg.message.mmsi;

                if (!binaryDB[mmsi]) {
                    binaryDB[mmsi] = {
                        ship_messages: [],
                        standalone_messages: []
                    };
                }

                const exists = binaryDB[mmsi].ship_messages.some(m =>
                    m.timestamp === msg.timestamp && m.dac === msg.dac && m.fi === msg.fi);
                if (exists) return;

                msg.formattedTime = formatTime(msg.timestamp);
                if (hasLocation) {
                    msg.message_lat = msg.message.lat;
                    msg.message_lon = msg.message.lon;
                }

                binaryDB[mmsi].ship_messages.push(msg);
            }
        });

        if (serverTime > 0) {
            binarySince = serverTime;
            const cutoff = serverTime - binaryTimeout;
            for (const mmsi in binaryDB) {
                binaryDB[mmsi].ship_messages = binaryDB[mmsi].ship_messages.filter(m => m.timestamp > cutoff);
                if (binaryDB[mmsi].ship_messages.length === 0) delete binaryDB[mmsi];
            }
        }

        let idx = 0;
        for (const mmsi in binaryDB) {
            binaryDB[mmsi].ship_messages.forEach(m => { m.index = ++idx; });
        }

        return true;
    } catch (error) {

        console.log("Failed loading binary:", error);
        return false;
    }
}

async function fetchShips(noDoubleFetch = true) {
    if (isFetchingShips && noDoubleFetch) {
        console.log("A fetch operation is already running.");
        return false;
    }

    let ships = {};

    isFetchingShips = true;
    let response;
    try {
        response = await fetch("api/ships_array.json?receiver=" + activeReceiver + (shipsSince > 0 ? "&since=" + shipsSince : ""));
    } catch (error) {

        console.log("failed loading ships: " + error);
        return false;
    } finally {
        isFetchingShips = false;
    }
    ships = await response.json();



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
            s.shipname = sanitizeString(s.shipname);
            s.callsign = sanitizeString(s.callsign);
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
            s.validated = flags & 3;
            s.validated2 = (flags & 3) == 2 ? -1 : flags & 3;
            s.repeat = (flags >> 2) & 3;
            s.virtual_aid = (flags >> 4) & 1;
            s.approx = (flags >> 5) & 1;
            s.approximate = s.approx;
            s.channels = (flags >> 6) & 0b1111;
            s.channels2 = (flags >> 6) & 0b1111;
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

function hideMenuifSmall() {
    hideMenu();
}

function toggleMenu() {
    document.getElementById("menubar").classList.toggle("visible");
    document.getElementById("menubar_mini").classList.toggle("showflex");
    document.getElementById("menubar_mini").classList.toggle("hide");

    const menuButton = document.getElementById("header_menu_button");
    menuButton.classList.toggle("menu_icon");
    menuButton.classList.toggle("close_icon");
}

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
    document.getElementById("settings_track_cargo_color").value = settings.track_class_colors[ShippingClass.CARGO];
    document.getElementById("settings_track_b_color").value = settings.track_class_colors[ShippingClass.B];
    document.getElementById("settings_track_passenger_color").value = settings.track_class_colors[ShippingClass.PASSENGER];
    document.getElementById("settings_track_tanker_color").value = settings.track_class_colors[ShippingClass.TANKER];
    document.getElementById("settings_track_fishing_color").value = settings.track_class_colors[ShippingClass.FISHING];
    document.getElementById("settings_track_highspeed_color").value = settings.track_class_colors[ShippingClass.HIGHSPEED];
    document.getElementById("settings_track_special_color").value = settings.track_class_colors[ShippingClass.SPECIAL];
    document.getElementById("settings_track_aton_color").value = settings.track_class_colors[ShippingClass.ATON];
    document.getElementById("settings_track_station_color").value = settings.track_class_colors[ShippingClass.STATION];
    document.getElementById("settings_track_sartepirb_color").value = settings.track_class_colors[ShippingClass.SARTEPIRB];
    document.getElementById("settings_track_plane_color").value = settings.track_class_colors[ShippingClass.PLANE];
    document.getElementById("settings_track_helicopter_color").value = settings.track_class_colors[ShippingClass.HELICOPTER];
    document.getElementById("settings_track_other_color").value = settings.track_class_colors[ShippingClass.OTHER];
    document.getElementById("settings_track_unknown_color").value = settings.track_class_colors[ShippingClass.UNKNOWN];
}


function shipcardismax() {
    return document.getElementById("shipcard").classList.contains("shipcard-ismax");
}

function shipcardselect(e) {
    if (shipcardismax()) {
        e.classList.toggle("shipcard-max-only");
        e.classList.toggle("shipcard-row-selected");
    } else toggleShipcardSize();

    saveSettings();
}

function toggleShipcardSize() {
    Array.from(document.getElementsByClassName("shipcard-min-only")).forEach((e) => e.classList.toggle("visible"));
    Array.from(document.getElementsByClassName("shipcard-max-only")).forEach((e) => e.classList.toggle("hide"));

    document.getElementById("shipcard").classList.toggle("shipcard-ismax");
    document.getElementById("shipcard_minmax_button").classList.toggle("keyboard_arrow_down_icon");
    document.getElementById("shipcard_minmax_button").classList.toggle("keyboard_arrow_up_icon");

    let e = document.getElementById("shipcard_content").children;

    if (shipcardismax()) {
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

function updateReceiverSelect(receivers) {
    const wrap = document.getElementById("receiver-btn-wrap");
    if (!wrap) return;
    if (!receivers || receivers.length <= 1) {
        wrap.style.display = "none";
        return;
    }
    wrap.style.display = "flex";
    const btn = document.getElementById("receiver-btn");
    if (btn) btn.classList.toggle("active", activeReceiver !== 0);
    const dd = document.getElementById("receiver-dropdown");
    if (!dd) return;
    if (dd.children.length !== receivers.length) {
        dd.innerHTML = "";
        for (const r of receivers) {
            const item = document.createElement("div");
            item.className = "receiver-dropdown-item" + (r.idx === activeReceiver ? " active" : "");
            item.dataset.idx = r.idx;
            item.textContent = r.label;
            item.onclick = () => { onReceiverChange(r.idx); closeReceiverDropdown(); };
            dd.appendChild(item);
        }
    } else {
        for (const item of dd.children) {
            item.classList.toggle("active", parseInt(item.dataset.idx) === activeReceiver);
        }
    }
}

function toggleReceiverDropdown(event) {
    event.stopPropagation();
    const dd = document.getElementById("receiver-dropdown");
    if (!dd) return;
    dd.style.display = dd.style.display === "none" ? "block" : "none";
}

function closeReceiverDropdown() {
    const dd = document.getElementById("receiver-dropdown");
    if (dd) dd.style.display = "none";
}

function onReceiverChange(idx) {
    activeReceiver = parseInt(idx, 10) || 0;
    shipsSince = 0;
    binarySince = 0;
    const btn = document.getElementById("receiver-btn");
    if (btn) btn.classList.toggle("active", activeReceiver !== 0);
    const dd = document.getElementById("receiver-dropdown");
    if (dd) {
        for (const item of dd.children) {
            item.classList.toggle("active", parseInt(item.dataset.idx) === activeReceiver);
        }
    }
    refresh_data();
}



// fetches main statistics from the server
async function fetchStatistics() {
    let response;
    try {
        response = await fetch("api/stat.json?receiver=" + activeReceiver);
    } catch (error) {

        return;
    }
    let statistics = await response.json();

    return statistics;
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

async function updateStatistics() {
    const stat = await fetchStatistics();

    if (stat) {
        // in bulk....
        ["os", "tcp_clients", "hardware", "build_describe", "build_date", "station", "product", "vendor", "serial", "model", "sample_rate"].forEach(
            (e) => (document.getElementById("stat_" + e).innerHTML = stat[e]),
        );

        if (stat.station_link != "") document.getElementById("stat_station").innerHTML = "<a href='" + stat.station_link + "'>" + stat.station + "</a>";

        const statSharingElement = document.getElementById("stat_sharing");
        const [sharingText, sharingColor] = community.sharingDisplay();
        statSharingElement.innerHTML = `<a href="${stat.sharing_link}" target="_blank" style="color: ${sharingColor}">${sharingText}</a>`;


        document.getElementById("stat_update_time").textContent = Number(refreshIntervalMs / 1000).toFixed(1) + " s";
        let title = document.getElementById("stat_station").textContent;
        if (title != "" && title != null) {
            tab_title_station = title;
            updateTitle();
        }
        document.getElementById("stat_memory").innerText = stat.memory ? Number(stat.memory / 1000000).toFixed(1) + " MB" : "N/A";
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

                html += `<div><span>Output</span><span>${o.description || o.type}</span></div>`;
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
                html += "</section>";
            }
            outputSection.innerHTML = html;
        }
    }
}

function tableRowClick(m) {
    let ship = shipsDB[m].raw;
    if (ship.lat == null || ship.lon == null) return;

    selectMapTab(m);
}

// Receiver-dropdown close-on-outside-click.
document.addEventListener('click', function (event) {
    const wrap = document.getElementById('receiver-btn-wrap');
    if (wrap && !wrap.contains(event.target)) {
        closeReceiverDropdown();
    }
});

function getTooltipContent(ship) {
    let content = '<div class="tooltip-card">' +
        getFlagStyled(ship.country, "padding: 0px; margin: 0px; margin-right: 10px; margin-left: 3px; box-shadow: 1px 1px 2px rgba(0, 0, 0, 0.2); font-size: 26px; opacity: 70%") +
        '<div>' +
        (getShipName(ship) || ship.mmsi) + ' at ' + getSpeedVal(ship.speed) + ' ' + getSpeedUnit() + '<br>' +
        'Received ' + getDeltaTimeVal(shipsSince - ship.last_signal) + ' ago' +
        '</div>' +
        '</div>';

    if (ship.mmsi in binaryDB && binaryDB[ship.mmsi].ship_messages &&
        binaryDB[ship.mmsi].ship_messages.length > 0) {

        const messages = binaryDB[ship.mmsi].ship_messages;

        const meteoMessages = messages.filter(msg =>
            msg.message && msg.message.dac == 1 &&
            (msg.message.fid == 31 || msg.message.fi == 31)
        );

        if (meteoMessages.length > 0) {
            content += getBinaryMessageContent(meteoMessages);
        }

        const inlandMessages = messages.filter(msg => isInlandMessage(msg));

        if (inlandMessages.length > 0) {
            content += getInlandMessageContent(inlandMessages);
        }

        const textMessages = messages.filter(msg => isTextMessage(msg));

        if (textMessages.length > 0) {
            content += getTextMessageTooltip(textMessages);
        }
    }

    return content;
}

function getTextMessageTooltip(messages) {
    const sorted = [...messages].sort((a, b) => b.timestamp - a.timestamp);
    const m = sorted[0];

    let content = '<div class="meteo-tooltip">';
    content += `<div style="font-size: 11px; color: #FFA500; padding: 4px 0 3px; margin-bottom: 2px; display: flex; justify-content: space-between; align-items: center;">`;
    content += `<span style="font-size: 11px;">${m.formattedTime} - Text Message</span>`;
    if (sorted.length > 1) {
        content += `<span style="font-size: 10px; opacity: 0.5; font-style: italic;">+${sorted.length - 1} more</span>`;
    }
    content += '</div>';
    content += `<div style="font-size: 11px; font-weight: bold; white-space: pre-wrap;">${sanitizeString(m.message.text || '')}</div>`;
    content += '</div>';
    return content;
}

function getTooltipContentPlane(plane) {
    const altitude = plane.airborne == 1 ? (plane.altitude ? Math.round(plane.altitude) + ' ft' : '-') : 'ground';
    const speed = plane.speed ? Math.round(plane.speed) : '-';
    return '<div class="tooltip-card">' +
        getFlagStyled(plane.country, "padding: 0px; margin: 0px; margin-right: 10px; margin-left: 3px; box-shadow: 1px 1px 2px rgba(0, 0, 0, 0.2); font-size: 26px; opacity: 70%") +
        '<div>' +
        (plane.callsign || plane.hexident) + ' at ' + altitude + '/' + speed + ' kts<br>' +
        'Received ' + getDeltaTimeVal(planesSince - plane.last_signal) + ' ago' +
        '</div>' +
        '</div>';
}

function getShipOpacity(ship) {
    if (settings.fading == false) return 1;

    let opacity = 1 - ((shipsSince - ship.last_signal) / 1800) * 0.8;
    return Math.max(0.2, Math.min(1, opacity));
}


function getShipCSSClassAndStyle(ship, opacity = 1) {
    getSprite(ship);
    let style = `opacity: ${opacity};`;
    let scale = settings.icon_scale;

    style += `background-position: -${ship.cx - 0}px -${ship.cy - 0}px; width: 20px; height: 20px; transform: rotate(${ship.rot}rad) scale(${scale});`;

    return { class: "sprites", style: style, hint: ship.hint };
}

function getIconCSS(ship, opacity = 1) {
    const { class: classValue, style, hint } = getShipCSSClassAndStyle(ship, opacity);
    return `class="${classValue}" style="${style}" title="${hint}"`;
}

function getTableShiptype(ship, opacity = 1) {
    if (ship == null) return "";

    const { class: classValue, style, hint } = getShipCSSClassAndStyle(ship, opacity);
    return settings.table_shiptype_use_icon
        ? `<span class="table-shiptype-icon"><span class="${classValue}" style="${style}" title="${hint}"></span></span>`
        : hint;
}


function notImplemented() {
    showDialog("Warning", "Not implemented yet");
}



function updateFocusMarker() {
    const shipRaw = card_type == 'ship' ? shipsDB[card_mmsi]?.raw : null;
    const planeRaw = card_type == 'plane' ? planesDB[card_mmsi]?.raw : null;

    if (selectCircleFeature) {
        if (shipRaw && shipRaw.lon && shipRaw.lat) {
            selectCircleFeature.setGeometry(new ol.geom.Point(ol.proj.fromLonLat([shipRaw.lon, shipRaw.lat])));
            return;
        }
        else if (planeRaw && planeRaw.lon && planeRaw.lat) {
            selectCircleFeature.setGeometry(new ol.geom.Point(ol.proj.fromLonLat([planeRaw.lon, planeRaw.lat])));
            return;
        }
        else {
            extraVector.removeFeature(selectCircleFeature);
            selectCircleFeature = undefined;
            return;
        }
    }

    if (shipRaw && shipRaw.lon && shipRaw.lat) {
        selectCircleFeature = new ol.Feature(new ol.geom.Point(ol.proj.fromLonLat([shipRaw.lon, shipRaw.lat])));
        selectCircleFeature.setStyle(selectCircleStyleFunction);
        selectCircleFeature.mmsi = card_mmsi;
        extraVector.addFeature(selectCircleFeature);
    }
    else if (planeRaw && planeRaw.lon && planeRaw.lat) {
        selectCircleFeature = new ol.Feature(new ol.geom.Point(ol.proj.fromLonLat([planeRaw.lon, planeRaw.lat])));
        selectCircleFeature.setStyle(selectCircleStyleFunction);
        selectCircleFeature.mmsi = card_mmsi;
        extraVector.addFeature(selectCircleFeature);
    }
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

function toggleAttribution() {
    const attribution = document.getElementById('map_attributions');
    const currentDisplay = attribution.style.display;
    attribution.style.display = currentDisplay === 'none' ? 'block' : 'none';
}

function getTooltipContentBinary(mmsiOrBinary) {
    if (typeof mmsiOrBinary === 'number' || typeof mmsiOrBinary === 'string') {
        const mmsi = Number(mmsiOrBinary);
        if (mmsi in binaryDB && binaryDB[mmsi].ship_messages && binaryDB[mmsi].ship_messages.length > 0) {
            const meteoMessages = binaryDB[mmsi].ship_messages.filter(msg =>
                msg.message &&
                msg.message.dac == 1 &&
                (msg.message.fid == 31 || msg.message.fi == 31)
            );

            if (meteoMessages.length > 0) {
                meteoMessages.sort((a, b) => b.timestamp - a.timestamp);

                let content = '';

                meteoMessages.forEach((meteoMsg, index) => {
                    if (index > 0) {
                        content += '<hr style="margin: 12px 0; border: 0; border-top: 1px dashed rgba(255,255,255,0.8);">';
                    }

                    // Add the individual message content
                    content += getBinaryMessageContent(meteoMsg);
                });

                return content;
            }
        }
        return ''; // No meteorological messages found
    }

    return getBinaryMessageContent(mmsiOrBinary);
}

function getBinaryMessageContent(binary, includeRaw = false) {
    const messages = Array.isArray(binary) ? binary : [binary];

    if (messages.length === 0) return '';

    messages.sort((a, b) => b.timestamp - a.timestamp);

    const msg = messages[0].message;

    const hasHydroData =
        ('watercurrent' in msg && msg.watercurrent != null) ||
        ('currentspeed' in msg && msg.currentspeed != null) ||
        ('currentdir' in msg && msg.currentdir != null) ||
        ('watertemp' in msg && msg.watertemp != null) ||
        ('waterlevel' in msg && msg.waterlevel != null);

    let content = '<div class="meteo-tooltip">';
    content += `<div style="font-size: 11px; color: #FFA500; padding: 4px 0 3px; margin-bottom: 2px; display: flex; justify-content: space-between; align-items: center;">`;
    content += `<span style="font-size: 11px;">${messages[0].formattedTime} - ${hasHydroData ? 'Meteo & Hydro' : 'Meteo'}</span>`;
    if (messages.length > 1) {
        content += `<span style="font-size: 10px; opacity: 0.5; font-style: italic;">+${messages.length - 1} more</span>`;
    }
    content += '</div>';

    const row = (label, value) =>
        `<div style="display: flex; justify-content: space-between; padding: 1px 0; white-space: nowrap;">` +
        `<span style="font-size: 11px; opacity: 0.6; margin-right: 12px;">${label}</span>` +
        `<span style="font-size: 11px; font-weight: bold;">${value}</span></div>`;

    // Wind
    if ('wspeed' in msg && msg.wspeed != null) {
        let val = msg.wspeed.toFixed(1) + ' kts';
        if ('wdir' in msg && msg.wdir != null && msg.wdir !== 360) val += ' / ' + msg.wdir + '&deg;';
        content += row('Wind', val);
    }

    // Air temperature
    if ('airtemp' in msg && msg.airtemp != null) {
        content += row('Air', msg.airtemp.toFixed(1) + '&deg;C');
    }

    // Pressure
    if ('pressure' in msg && msg.pressure != null) {
        let val = msg.pressure.toFixed(1) + ' hPa';
        if ('pressuretend' in msg && msg.pressuretend != null) {
            val += ' (' + ['steady', 'decreasing', 'increasing'][msg.pressuretend] + ')';
        }
        content += row('Pressure', val);
    }

    // Water current
    const currentSpeed = msg.watercurrent || msg.currentspeed;
    const currentDir = msg.currentdir || msg.currentdirection;
    if (currentSpeed != null) {
        let val = currentSpeed.toFixed(1) + ' kts';
        if (currentDir != null && currentDir !== 360) val += ' / ' + currentDir + '&deg;';
        content += row('Current', val);
    }

    // Water level
    if ('waterlevel' in msg && msg.waterlevel != null) {
        content += row('Water Level', msg.waterlevel.toFixed(2) + ' m');
    }

    // Water temperature
    if ('watertemp' in msg && msg.watertemp != null) {
        content += row('Water', msg.watertemp.toFixed(1) + '&deg;C');
    }

    // Waves
    if ('waveheight' in msg && msg.waveheight != null) {
        let val = msg.waveheight.toFixed(1) + ' m';
        if ('wavedir' in msg && msg.wavedir != null && msg.wavedir !== 360) val += ' / ' + msg.wavedir + '&deg;';
        if ('waveperiod' in msg && msg.waveperiod != null) val += ', ' + msg.waveperiod + 's';
        content += row('Wave', val);
    }

    // Swell
    if ('swellheight' in msg && msg.swellheight != null) {
        let val = msg.swellheight.toFixed(1) + ' m';
        if ('swelldir' in msg && msg.swelldir != null && msg.swelldir !== 360) val += ' / ' + msg.swelldir + '&deg;';
        if ('swellperiod' in msg && msg.swellperiod != null) val += ', ' + msg.swellperiod + 's';
        content += row('Swell', val);
    }

    // Visibility
    if ('visibility' in msg && msg.visibility != null) {
        content += row('Visibility', msg.visibility.toFixed(1) + ' nm');
    }

    if (includeRaw) {
        content += `<details class="binary-raw-data">
                      <summary>Show Raw Data</summary>
                      <pre>${sanitizeString(JSON.stringify(msg, null, 2))}</pre>
                    </details>`;
    }

    content += '</div>';
    return content;
}

function isInlandMessage(msg) {
    if (!msg.message) return false;
    const fi = msg.message.fid != null ? msg.message.fid : msg.message.fi;
    return msg.message.dac == 200 && fi == 55;
}

function isTextMessage(msg) {
    if (!msg.message) return false;
    const fi = msg.message.fid != null ? msg.message.fid : msg.message.fi;
    return msg.message.dac == 1 && (fi == 0 || fi == 29 || fi == 30);
}

function getTextMessageContent(msg) {
    const m = msg.message;
    const fi = m.fid != null ? m.fid : m.fi;
    const kind = m.type == 6 || fi == 30 ? "Text message (addressed)" : "Text message (broadcast)";

    let content = '<div class="binary-message-details">';
    content += `<div><strong>${kind}</strong></div>`;
    content += `<div style="font-size: 1.1em; margin: 6px 0; white-space: pre-wrap;">${sanitizeString(m.text || '')}</div>`;

    if (m.dest_mmsi) {
        const destName = m.dest_mmsi in shipsDB ?
            (shipsDB[m.dest_mmsi].raw.shipname || `MMSI ${m.dest_mmsi}`) :
            `MMSI ${m.dest_mmsi}`;
        content += `<div><strong>To:</strong> ${sanitizeString(String(destName))}</div>`;
    }
    if (m.ack_required) content += '<div>Acknowledgement requested</div>';

    content += `<details class="binary-raw-data">
                  <summary>Show Raw Data</summary>
                  <pre>${sanitizeString(JSON.stringify(m, null, 2))}</pre>
                </details>`;
    content += '</div>';
    return content;
}

function getInlandMessageContent(binary) {
    const messages = Array.isArray(binary) ? binary : [binary];
    if (messages.length === 0) return '';

    // Keep only the latest persons-on-board message
    let entry = null;
    messages.forEach(m => {
        if (!isInlandMessage(m)) return;
        if (!entry || m.timestamp > entry.timestamp) entry = m;
    });
    if (!entry) return '';

    const msg = entry.message;

    const row = (label, value) =>
        `<div style="display: flex; justify-content: space-between; padding: 1px 0; white-space: nowrap;">` +
        `<span style="font-size: 11px; opacity: 0.6; margin-right: 12px;">${label}</span>` +
        `<span style="font-size: 11px; font-weight: bold;">${value}</span></div>`;

    let content = '<div class="meteo-tooltip">';
    content += `<div style="font-size: 11px; color: #FFA500; padding: 4px 0 3px; margin-bottom: 2px;">`;
    content += `<span style="font-size: 11px;">${entry.formattedTime} - Persons on Board</span>`;
    content += '</div>';

    if (msg.crew_count != null) content += row('Crew', msg.crew_count);
    if (msg.passenger_count != null) content += row('Passengers', msg.passenger_count);
    if (msg.shipboard_personnel_count != null) content += row('Personnel', msg.shipboard_personnel_count);

    content += '</div>';
    return content;
}

const startHover = function (type, mmsi, pixel, feature) {

    if (type != 'ship' && type != 'tooltip' && type != 'plane' && type != 'binary') return;

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

    const shipRaw = hoverType == 'ship' ? shipsDB[hoverMMSI]?.raw : null;
    const planeRaw = hoverType == 'plane' ? planesDB[hoverMMSI]?.raw : null;

    if (hoverCircleFeature) {
        if (shipRaw && shipRaw.lon && shipRaw.lat) {
            hoverCircleFeature.setGeometry(new ol.geom.Point(ol.proj.fromLonLat([shipRaw.lon, shipRaw.lat])));
            return;
        }
        else if (planeRaw && planeRaw.lon && planeRaw.lat) {
            hoverCircleFeature.setGeometry(new ol.geom.Point(ol.proj.fromLonLat([planeRaw.lon, planeRaw.lat])));
            return;
        }
        else {
            extraVector.removeFeature(hoverCircleFeature);
            hoverCircleFeature = undefined;
            hoverMMSI = undefined;
            hoverType = undefined;
            return;
        }
    }

    if (shipRaw && shipRaw.lon && shipRaw.lat) {
        hoverCircleFeature = new ol.Feature(new ol.geom.Point(ol.proj.fromLonLat([shipRaw.lon, shipRaw.lat])));
        hoverCircleFeature.setStyle(hoverCircleStyleFunction);
        hoverCircleFeature.mmsi = hoverMMSI;
        extraVector.addFeature(hoverCircleFeature);
    }
    else if (planeRaw && planeRaw.lon && planeRaw.lat) {
        hoverCircleFeature = new ol.Feature(new ol.geom.Point(ol.proj.fromLonLat([planeRaw.lon, planeRaw.lat])));
        hoverCircleFeature.setStyle(hoverCircleStyleFunction);
        hoverCircleFeature.mmsi = hoverMMSI;
        extraVector.addFeature(hoverCircleFeature);
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

    if (feature) return feature;
    return undefined;
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

            // Add meteo data if available
            const meteoMessages = feature.binary_messages.filter(msg =>
                msg.message && msg.message.dac == 1 &&
                (msg.message.fid == 31 || msg.message.fi == 31)
            );

            if (meteoMessages.length > 0) {
                tooltipContent += getBinaryMessageContent(meteoMessages);
            }

            // Add inland data if available
            const inlandMessages = feature.binary_messages.filter(msg => isInlandMessage(msg));

            if (inlandMessages.length > 0) {
                tooltipContent += getInlandMessageContent(inlandMessages);
            }

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

const debouncedSaveSettings = debounce(saveSettings, 250);
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
    if (map !== undefined) {
        let view = map.getView();
        let center = ol.proj.toLonLat(view.getCenter()); // Convert the center coordinate to longitude and latitude
        settings.lat = center[1]; // Latitude
        settings.lon = center[0]; // Longitude
        settings.zoom = view.getZoom(); // Zoom level
    }


    const scRows = document.querySelectorAll(".shipcard-content-row");

    const selectedRows = [];
    scRows.forEach((row) => {
        const rowClasses = row.getAttribute("class");
        selectedRows.push(rowClasses.includes("shipcard-max-only") ? 0 : 1);
    });

    settings.shipcard_max = isShipcardMax();
    settings.shipcard_rows = selectedRows;
    settings.activeReceiver = activeReceiver;

    const filters = realtimeModule?.getFilterMMSIs();
    if (filters !== null && filters !== undefined) settings.realtime_filter_mmsis = filters;
    const bg = realtimeModule?.getBackgroundStreaming();
    if (bg !== null && bg !== undefined) settings.realtime_background_streaming = bg;

    localStorage[context] = JSON.stringify(settings);

    community.notifyCommunityPopupView();
    updateMapURL();
    updateSettingsTab();

    // Apply graph visibility settings (without saving during initialization)
    setGraphVisibility('signal', settings.show_signal_graphs, false);
    setGraphVisibility('ppm', settings.show_ppm_graphs, false);
}

function updateForLegacySettings() {
    if ('latlon_in_dms' in settings) {

        settings.coordinate_format = settings.latlon_in_dms ? "dms" : "decimal";
        delete settings.latlon_in_dms;
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
    if (!shipcardismax()) toggleShipcardSize();

    if (settings.shipcard_rows && settings.shipcard_rows.length > 0) {
        const rows = document.querySelectorAll(".shipcard-content-row");

        rows.forEach((row, index) => {
            if (settings.shipcard_rows[index] == 1) {
                row.setAttribute("class", "mapcard-content-row shipcard-content-row shipcard-row-selected");
            } else {
                row.setAttribute("class", "mapcard-content-row shipcard-content-row shipcard-max-only");
            }
        });
        if (settings.shipcard_max != shipcardismax()) {
            toggleShipcardSize();
        }
    }

    settings.android = false;
}

function convertStringBooleansToActual() {
    const booleanSettings = [
        'counter', 'fading', 'android', 'kiosk', 'welcome', 'show_range',
        'distance_circles', 'table_shiptype_use_icon', 'fix_center',
        'show_circle_outline', 'dark_mode', 'setcoord', 'eri', 'loadURL',
        'show_station', 'labels_declutter', 'label_class_background', 'show_track_on_hover',
        'show_track_on_select', 'shipcard_max', 'kiosk_pan_map',
        'show_signal_graphs', 'show_ppm_graphs'
    ];

    booleanSettings.forEach(key => {
        if (Object.hasOwn(settings, key)) {
            if (typeof settings[key] === 'string') {
                settings[key] = settings[key] === "true";
            }
        }
    });
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

    convertStringBooleansToActual();
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


async function showAllTracks() {
    show_all_tracks = true;
    lastPathFetch = 0;
    select_enabled_track = hover_enabled_track = false;
    await fetchTracks();
    redrawMap();
    updateShipcardTrackOption()
}

function deleteAllTracks() {
    show_all_tracks = false;
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
}


async function fetchTracks() {
    if (marker_tracks.size == 0 && show_all_tracks == false) return true;

    let a;
    let isDelta = false;
    try {
        if (show_all_tracks) {
            const sinceParam = lastPathFetch > 0 ? "&since=" + lastPathFetch : "";
            isDelta = sinceParam !== "";
            a = await fetch("api/allpath.json?receiver=" + activeReceiver + sinceParam);
        } else {
            for (var mmsi of marker_tracks) {
                if (!(mmsi in shipsDB)) {
                    ToggleTrackOnMap(mmsi);
                }
            }
            const mmsi_str = Array.from(marker_tracks).join(",");
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
                const newer = newPaths[mmsi];
                if (!paths[mmsi]) {
                    paths[mmsi] = newer;
                    continue;
                }
                const older = paths[mmsi];
                const merged = [];
                let i = 0, j = 0;
                while (i < newer.length && j < older.length) {
                    const a = newer[i], b = older[j];
                    if (a[2] === b[2]) {
                        b[0] = a[0]; b[1] = a[1]; b[3] = a[3];
                        merged.push(b);
                        i++; j++;
                    } else if (a[2] > b[2]) {
                        merged.push(a); i++;
                    } else {
                        merged.push(b); j++;
                    }
                }
                while (i < newer.length) merged.push(newer[i++]);
                while (j < older.length) merged.push(older[j++]);
                paths[mmsi] = merged;
            }
            for (const mmsi in paths) {
                if (!(mmsi in shipsDB)) delete paths[mmsi];
                else if (paths[mmsi].length > 250) paths[mmsi] = paths[mmsi].slice(0, 250);
            }
        }
    } catch (error) {
        console.log("Error loading path: " + error);
        if (!isDelta) paths = {};
        lastPathFetch = 0;
        return false;
    }

    return true;
}

function trackOptionString(mmsi) {
    const hover_track = mmsi == hoverType == 'ship' && hoverMMSI && hover_enabled_track;
    const select_track = card_type == 'ship' && mmsi == card_mmsi && select_enabled_track;
    const track_shown = marker_tracks.has(Number(mmsi));

    if (hover_track || select_track) return "Show Track";
    return track_shown ? "Hide Track" : "Show Track";
}

function updateShipcardTrackOption() {
    const trackOptionElement = document.getElementById("shipcard_track_option");

    if (show_all_tracks || card_type == 'plane') {
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

    const hasBinaryMsgs = card_mmsi &&
        binaryDB &&
        card_mmsi in binaryDB &&
        binaryDB[card_mmsi].ship_messages &&
        binaryDB[card_mmsi].ship_messages.length > 0;

    const existingBadge = iconElement.querySelector('.message-badge');
    if (existingBadge) {
        existingBadge.remove();
    }

    if (hasBinaryMsgs) {
        const count = binaryDB[card_mmsi].ship_messages.length;
        messageButton.style.display = '';

        const badge = document.createElement('span');
        badge.className = 'message-badge';
        badge.textContent = count;
        iconElement.appendChild(badge);
    } else {
        messageButton.style.display = 'none';
    }
}

function populateShipcard() {

    if (card_type != 'ship') return;

    if (!(card_mmsi in shipsDB)) {
        document
            .getElementById("shipcard_content")
            .querySelectorAll("span:nth-child(2)")
            .forEach((e) => (e.innerHTML = null));
        document.getElementById("shipcard_header_title").innerHTML = "<b style='color:red;'>Out of range</b>";
        document.getElementById("shipcard_header_flag").innerHTML = "";
        document.getElementById("shipcard_mmsi").innerHTML = card_mmsi;

        updateFocusMarker();
        return;
    }

    let ship = shipsDB[card_mmsi].raw;

    document.getElementById("shipcard_header_flag").innerHTML = getFlagStyled(ship.country, "padding: 0px; margin: 0px; margin-right: 5px; box-shadow: 2px 2px 3px rgba(0, 0, 0, 0.5); font-size: 26px;");
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
        { id: "cog", u: "&deg", d: 0 },
        { id: "bearing", u: "&deg", d: 0 },
        { id: "heading", u: "&deg", d: 0 },
        { id: "level", u: "dB", d: 1 },
        { id: "ppm", u: "ppm", d: 1 },
    ].forEach((el) => (document.getElementById("shipcard_" + el.id).innerHTML = ship[el.id] ? Number(ship[el.id]).toFixed(el.d) + " " + el.u : null));

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
    document.getElementById("shipcard_lat").innerHTML = ship.lat ? getLatValFormat(ship) : null;
    document.getElementById("shipcard_lon").innerHTML = ship.lon ? getLonValFormat(ship) : null;
    document.getElementById("shipcard_altitude").innerHTML = ship.altitude ? ship.altitude + " m" : null;

    document.getElementById("shipcard_speed").innerHTML = ship.speed ? getSpeedVal(ship.speed) + " " + getSpeedUnit() : null;
    document.getElementById("shipcard_distance").innerHTML = ship.distance ? (getDistanceVal(ship.distance) + " " + getDistanceUnit() + (ship.repeat > 0 ? " (R)" : "")) : null;
    document.getElementById("shipcard_draught").innerHTML = ship.draught ? getDimVal(ship.draught) + " " + getDimUnit() : null;
    document.getElementById("shipcard_dimension").innerHTML = getShipDimension(ship);
    document.getElementById("shipcard_bluesign").innerHTML = ship.maneuver === 2 ? "Set" : (ship.maneuver === 1 ? "Not set" : null);

    updateShipcardTrackOption(card_mmsi);
    updateMessageButton();
    updateTechDetails(ship);

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

    return categories[plane.category] || plane.category.toString() || "-";
}

function populatePlanecard() {

    if (card_type != 'plane') return;

    document
        .getElementById("shipcard_content")
        .querySelectorAll("span:nth-child(2)")
        .forEach((e) => (e.innerHTML = null));

    if (!(card_mmsi in planesDB)) {
        document
            .getElementById("shipcard_content")
            .querySelectorAll("span:nth-child(2)")
            .forEach((e) => (e.innerHTML = null));
        document.getElementById("shipcard_header_title").innerHTML = "<b style='color:red;'>Out of range</b>";
        document.getElementById("shipcard_header_flag").innerHTML = "";
        document.getElementById("shipcard_mmsi").innerHTML = card_mmsi;

        updateFocusMarker();
        return;
    }

    let plane = planesDB[card_mmsi].raw;

    setShipcardValidation(plane.validated);

    // Set header
    document.getElementById("shipcard_header_title").textContent = (plane.callsign || getICAO(plane));
    document.getElementById("shipcard_header_flag").innerHTML = getFlagStyled(plane.country, "padding: 0px; margin: 0px; margin-right: 5px; box-shadow: 2px 2px 3px rgba(0, 0, 0, 0.5); font-size: 26px;");

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
    document.getElementById("shipcard_plane_distance").innerHTML = plane.distance ? (getDistanceVal(plane.distance) + " " + getDistanceUnit()) : null;

    document.getElementById("shipcard_plane_last_group").innerHTML = getStringfromGroup(plane.last_group);
    document.getElementById("shipcard_plane_sources").innerHTML = getStringfromGroup(plane.group_mask);

    [
        { id: "heading", u: "&deg", d: 0 },
        { id: "level", u: "dB", d: 1 },
        { id: "bearing", u: "&deg", d: 0 }
    ].forEach((el) => (document.getElementById("shipcard_plane_" + el.id).innerHTML = plane[el.id] ? Number(plane[el.id]).toFixed(el.d) + " " + el.u : null));

    updateShipcardTrackOption(card_mmsi);
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
            color: 'white', //getComputedStyle(document.documentElement).getPropertyValue('--secondary-color'),
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
                color: getComputedStyle(document.documentElement).getPropertyValue('--tertiary-color')
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
            let newPixel = [...pixel];
            let margin = 10; // Margin in pixels

            if (shipcardRect.bottom + margin + 20 <= mapRect.bottom) { // 20 is an approximate marker height
                newPixel[1] = shipcardRect.bottom + margin;
            }
            else if (shipcardRect.right + margin + 20 <= mapRect.right) { // 20 is an approximate marker width
                newPixel[0] = shipcardRect.right + margin;
            }
            newPixel = pixel;

            moveMapCenter(newPixel);
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
        document.getElementById("shipcard_drag_handle").classList.add("opacity-25");
    }
    else {
        shipcard.classList.remove("pinned");
        document.getElementById("shipcard_drag_handle").classList.remove("opacity-25");
    }
}

function toggleShipcardPin() {
    if (settings.shipcard_pinned) {
        unpinShipcard();
    } else {
        pinShipcard();
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
        return;
    }

    aside.style.left = "";
    aside.style.top = "";

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
        }
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

function getSprite(ship) {
    let shipClass = ship.shipclass;
    let sprite = shippingMappings[shipClass] || {
        cx: 120,
        cy: 20,
        imgSize: 20,
        hint: ''
    }

    ship.rot = 0
    ship.cx = sprite.cx
    ship.cy = sprite.cy
    ship.imgSize = sprite.imgSize
    ship.hint = sprite.hint

    if (sprite.cy === 20) {
        if (ship.speed != null && ship.speed > 0.5 && ship.cog != null) {
            ship.cy = 0
            ship.rot = ship.cog * 3.1415926 / 180;
        }
    } else if ((shipClass == ShippingClass.HELICOPTER || shipClass == ShippingClass.PLANE) && ship.cog != null) {
        ship.rot = ship.cog * 3.1415926 / 180;
    }

    return
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
    const ok = await fetchShips();
    if (!ok) return;

    await Promise.all([
        fetchTracks(),
        planeLayer.isVisible() ? fetchPlanes() : Promise.resolve(true),
        binaryLayer.isVisible() ? fetchBinary() : Promise.resolve(true),
        fetchRange(),
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

function redrawBinaryMessages() {
    binaryVector.clear();

    const gridCells = {}; // For clustering standalone messages
    const gridSize = 0.01; // Grid size in degrees (approx. 1km)

    // Process messages for vessels
    for (const [mmsi, msgData] of Object.entries(binaryDB)) {
        if (!msgData.ship_messages || msgData.ship_messages.length === 0) continue;

        // Cache all messages
        const allMessages = msgData.ship_messages;

        // Check if the ship exists in shipsDB
        const shipExists = mmsi in shipsDB;
        const hasValidShipCoordinates = shipExists &&
            shipsDB[mmsi].raw.lat !== null &&
            shipsDB[mmsi].raw.lon !== null &&
            shipsDB[mmsi].raw.lat !== 0 &&
            shipsDB[mmsi].raw.lon !== 0 &&
            shipsDB[mmsi].raw.lat < 90 &&
            shipsDB[mmsi].raw.lon < 180;

        if (hasValidShipCoordinates) {
            // Prepare arrays for messages to show at ship or at their own locations
            const shipMessages = [];
            const standaloneMessages = [];

            const ship = shipsDB[mmsi].raw;
            const shipLat = ship.lat;
            const shipLon = ship.lon;

            // Sort messages by proximity to the ship
            allMessages.forEach(msg => {
                // Messages without their own location (e.g. inland FID 10/55) badge on the ship
                if (!msg.message_lat || !msg.message_lon) {
                    shipMessages.push(msg);
                    return;
                }

                const msgLat = msg.message_lat;
                const msgLon = msg.message_lon;

                // Skip invalid coordinates
                if (msgLat === 0 || msgLon === 0 || msgLat > 90 || msgLon > 180) return;

                // Simple distance calculation
                const distanceThreshold = 0.05; // Approx 5km
                const distance = Math.sqrt(
                    Math.pow(msgLat - shipLat, 2) +
                    Math.pow(msgLon - shipLon, 2)
                );

                if (distance <= distanceThreshold) {
                    // Message is close to ship, show at ship location
                    shipMessages.push(msg);
                } else {
                    // Message is far from ship, show at its own location
                    standaloneMessages.push(msg);
                    addMessageToGridCell(msg, mmsi, gridCells, gridSize);
                }
            });

            // Create badge at ship location if there are messages to show there
            if (shipMessages.length > 0) {
                const point = new ol.geom.Point(ol.proj.fromLonLat([shipLon, shipLat]));
                const feature = new ol.Feature({
                    geometry: point
                });

                feature.binary = true;
                feature.ship = ship;
                feature.binary_mmsi = mmsi;
                feature.binary_count = shipMessages.length;
                feature.binary_messages = shipMessages;
                feature.is_associated = true;
                feature.setId(`binary-ship-${mmsi}`);

                binaryVector.addFeature(feature);
            }

        } else {
            // No associated ship with valid coordinates - add all messages to grid cells for clustering
            allMessages.forEach(msg => {
                if (!msg.message_lat || !msg.message_lon) return;
                addMessageToGridCell(msg, mmsi, gridCells, gridSize);
            });
        }
    }

    // Create features for grid cells (clustered standalone messages)
    createGridCellFeatures(gridCells);
}

// Helper function to add a message to the appropriate grid cell
function addMessageToGridCell(msg, mmsi, gridCells, gridSize) {
    const msgLat = msg.message_lat;
    const msgLon = msg.message_lon;

    // Skip invalid coordinates
    if (msgLat === 0 || msgLon === 0 || msgLat > 90 || msgLon > 180) return;

    // Create grid cell key for clustering
    const gridX = Math.floor(msgLon / gridSize);
    const gridY = Math.floor(msgLat / gridSize);
    const gridKey = `${gridX},${gridY}`;

    if (!gridCells[gridKey]) {
        gridCells[gridKey] = {
            messages: [],
            totalLat: 0,
            totalLon: 0,
            mmsiCounts: {}
        };
    }

    // Add to grid cell for clustering
    gridCells[gridKey].messages.push(msg);
    gridCells[gridKey].totalLat += msgLat;
    gridCells[gridKey].totalLon += msgLon;
    gridCells[gridKey].mmsiCounts[mmsi] =
        (gridCells[gridKey].mmsiCounts[mmsi] || 0) + 1;
}

// Helper function to create features for grid cells
function createGridCellFeatures(gridCells) {
    for (const [gridKey, gridData] of Object.entries(gridCells)) {
        if (gridData.messages.length === 0) continue;

        // Calculate average position for this grid cell
        const avgLat = gridData.totalLat / gridData.messages.length;
        const avgLon = gridData.totalLon / gridData.messages.length;

        const point = new ol.geom.Point(ol.proj.fromLonLat([avgLon, avgLat]));
        const feature = new ol.Feature({
            geometry: point
        });

        feature.binary = true;
        feature.binary_count = gridData.messages.length;
        feature.binary_messages = gridData.messages;
        feature.binary_mmsi_counts = gridData.mmsiCounts;
        feature.is_associated = false;
        feature.tooltip = `${gridData.messages.length} binary messages`;
        feature.setId(`binary-standalone-${gridKey}`);

        binaryVector.addFeature(feature);
    }
}

function redrawMap() {
    shapeFeatures = {};
    markerFeatures = {};

    markerVector.clear();
    binaryVector.clear();
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
        if (ship.lat != null && ship.lon != null && ship.lat != 0 && ship.lon != 0 && ship.lat < 90 && ship.lon < 180) {
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

    redrawBinaryMessages();

    if (planeLayer.isVisible()) {

        for (let [hexident, entry] of Object.entries(planesDB)) {
            let plane = entry.raw;
            if (plane.lat != null && plane.lon != null && plane.lat != 0 && plane.lon != 0 && plane.lat < 90 && plane.lon < 180) {
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

    for (let [mmsi, entry] of Object.entries(paths)) {

        if (marker_tracks.has(Number(mmsi)) || show_all_tracks) {
            const path = paths[mmsi];
            const ship = shipsDB[mmsi]?.raw;
            const shipclass = ship?.shipclass;

            // Path: [lat, lon, start_time, end_time]
            if (path.length > 0 && path[0].length >= 4) {
                let currentSegment = [];
                let currentDashed = false;

                for (let i = 0; i < Math.min(path.length, 250); i++) {
                    const point = path[i];
                    const coord = ol.proj.fromLonLat([point[1], point[0]]);

                    let isDashed = false;
                    if (i > 0) {
                        const timeBetweenPoints = path[i - 1][2] - point[3]; // newer start_time - older end_time
                        isDashed = timeBetweenPoints > settings.track_trash_threshold;
                    }

                    if (currentSegment.length === 0) {
                        // First point
                        currentSegment.push(coord);
                        currentDashed = false; // First segment is always solid
                    } else if (currentDashed === isDashed) {
                        // Continue current segment
                        currentSegment.push(coord);
                    } else {
                        // Create feature for current segment
                        if (currentSegment.length > 1) {
                            const lineString = new ol.geom.LineString(currentSegment);
                            const feature = new ol.Feature(lineString);
                            feature.mmsi = mmsi;
                            feature.isDashed = currentDashed;
                            feature.shipclass = shipclass;
                            trackVector.addFeature(feature);
                        }
                        // Start new segment
                        currentSegment = [currentSegment[currentSegment.length - 1], coord];
                        currentDashed = isDashed;
                    }
                }

                // Add final segment
                if (currentSegment.length > 1) {
                    const lineString = new ol.geom.LineString(currentSegment);
                    const feature = new ol.Feature(lineString);
                    feature.mmsi = mmsi;
                    feature.isDashed = currentDashed;
                    feature.shipclass = shipclass;
                    trackVector.addFeature(feature);
                }
            }
        }
    }

    drawRange();
    updateFocusMarker();
    updateHoverMarker();

    updateMarkerCount();
    updateTablecard();

    drawStation(station);
    updateDistanceCircles();

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
                    if (!plotsModule) plotsModule = await import('./tabs/plots.js');
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

    let ship = shipsDB[m].raw;
    if (ship && ship.lon && ship.lat) {
        let shipCoords = ol.proj.fromLonLat([ship.lon, ship.lat]);
        let view = map.getView();
        view.setCenter(shipCoords);
    }

    if (z) mapResetView(z);
    else mapResetView(14);

}

function updateSettingsTab() {
    document.getElementById("settings_darkmode").checked = settings.dark_mode;
    document.getElementById("settings_coordinate_format").value = settings.coordinate_format;
    document.getElementById("settings_metric").value = getMetrics().toLowerCase();
    document.getElementById("settings_show_station").checked = settings.show_station;
    document.getElementById("settings_fading").checked = settings.fading;
    document.getElementById("settings_show_signal_graphs").checked = settings.show_signal_graphs;
    document.getElementById("settings_show_ppm_graphs").checked = settings.show_ppm_graphs;
    document.getElementById("settings_shiphover_color").value = settings.shiphover_color;
    document.getElementById("settings_shipselection_color").value = settings.shipselection_color;

    document.getElementById("settings_show_range").checked = settings.show_range;
    document.getElementById("settings_distance_circles").checked = settings.distance_circles;
    document.getElementById("settings_distance_circle_color").value = settings.distance_circle_color;

    document.getElementById("settings_labels_declutter").checked = settings.labels_declutter;
    document.getElementById("settings_label_class_background").checked = settings.label_class_background;
    document.getElementById("settings_tooltipLabelFontsize").value = settings.tooltipLabelFontSize;

    document.getElementById("settings_show_labels").value = settings.show_labels.toLowerCase();

    document.getElementById("settings_shipoutline_border").value = settings.shipoutline_border;
    document.getElementById("settings_shipoutline_inner").value = settings.shipoutline_inner;
    document.getElementById("settings_shipoutline_opacity").value = settings.shipoutline_opacity;
    document.getElementById("settings_show_circle_outline").checked = settings.show_circle_outline;
    document.getElementById("settings_circle_scale").value = settings.circle_scale;

    document.getElementById("settings_range_color").value = settings.range_color;
    document.getElementById("settings_range_timeframe").value = settings.range_timeframe;
    document.getElementById("settings_range_color_short").value = settings.range_color_short;
    document.getElementById("settings_range_color_dark").value = settings.range_color_dark;
    document.getElementById("settings_range_color_dark_short").value = settings.range_color_dark_short;

    document.getElementById("settings_map_opacity").value = settings.map_opacity;
    document.getElementById("settings_icon_scale").value = settings.icon_scale;
    document.getElementById("settings_track_weight").value = settings.track_weight;
    document.getElementById("settings_track_trash_threshold").value = settings.track_trash_threshold;

    // Update all slider display values
    updateIconScaleDisplay(settings.icon_scale);
    updateMapOpacityDisplay(settings.map_opacity);
    updateTrackWeightDisplay(settings.track_weight);
    updateTrackTrashThresholdDisplay(settings.track_trash_threshold);
    updateTooltipFontSizeDisplay(settings.tooltipLabelFontSize);
    updateShipoutlineOpacityDisplay(settings.shipoutline_opacity);
    updateCircleScaleDisplay(settings.circle_scale || 6.0);

    document.getElementById("settings_tooltipLabelColor").value = settings.tooltipLabelColor;
    document.getElementById("settings_tooltipLabelShadowColor").value = settings.tooltipLabelShadowColor;

    document.getElementById("settings_tooltipLabelColorDark").value = settings.tooltipLabelColorDark;
    document.getElementById("settings_tooltipLabelShadowColorDark").value = settings.tooltipLabelShadowColorDark;
    document.getElementById("settings_table_shiptype_use_icon").checked = settings.table_shiptype_use_icon;
    document.getElementById("settings_show_track_on_hover").checked = settings.show_track_on_hover;
    document.getElementById("settings_show_track_on_select").checked = settings.show_track_on_select;

    document.getElementById("settings_kiosk_mode").checked = settings.kiosk;
    document.getElementById("settings_kiosk_rotation_speed").value = settings.kiosk_rotation_speed;
    document.getElementById("settings_kiosk_pan_map").checked = settings.kiosk_pan_map;

    updateKioskSpeedDisplay(settings.kiosk_rotation_speed);

    // Update ship class color inputs
    updateTrackColorInputs();
}


function activateTab(b, a) {
    // Block decoder tab if decoder is disabled
    if (a === "decoder" && !config.features.decoder) {
        return;
    }

    hideMenuifSmall();

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
        interval = setInterval(refresh_data, refreshIntervalMs);
    });

    if (a != "map") fireworks.stop();
    if (a == "settings") updateSettingsTab();

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

function updateAndroid() {
    const sel = isAndroid() ? ".noandroid" : ".android";
    document.querySelectorAll(sel).forEach((el) => {
        el.style.setProperty("display", "none", "important");
    });
}

function updateKioskSpeedDisplay(value) {
    document.getElementById("kiosk_rotation_speed_label").textContent = `Rotation Speed (${value}s)`;
}

function updateTrackWeightDisplay(value) {
    document.getElementById("track_weight_label").textContent = `Track Weight (${value})`;
}

function updateTrackTrashThresholdDisplay(value) {
    document.getElementById("track_trash_threshold_label").textContent = `Track Dash Threshold (${value}s)`;
}

function updateIconScaleDisplay(value) {
    document.getElementById("icon_scale_label").textContent = `Ship icon size (${parseFloat(value).toFixed(2)})`;
}

function updateMapOpacityDisplay(value) {
    const percentage = Math.round(parseFloat(value) * 100);
    document.getElementById("map_opacity_label").textContent = `Map dimming (${percentage}%)`;
}

function updateTooltipFontSizeDisplay(value) {
    document.getElementById("tooltip_font_size_label").textContent = `Font Size (${value})`;
}

function updateShipoutlineOpacityDisplay(value) {
    document.getElementById("shipoutline_opacity_label").textContent = `Opacity (${parseFloat(value).toFixed(2)})`;
}

function updateCircleScaleDisplay(value) {
    document.getElementById("circle_scale_label").textContent = `Selector line width (${parseFloat(value).toFixed(1)})`;
}

function showAboutDialog() {
    const message = `
        <div style="display: flex; align-items: center; margin-top: 10px;">
        <span style="text-align: center; margin-right: 10px;"><i style="font-size: 40px" class="directions_aiscatcher_icon"></i></span>
        <span>
        <a href="https://www.aiscatcher.org"><b style="font-size: 1.6em;">AIS-catcher</b></a>
        <br>
        <b style="font-size: 0.8em;">&copy; 2021-2026 jvde.github@gmail.com</b>
        </span>
        </div>
        <p>
        AIS-catcher is a research and educational tool, provided under the
        <a href="https://github.com/jvde-github/AIS-catcher/blob/e66a4481e62d8f1775700e5f51fb7ad9ea569a12/LICENSE">GNU GPL v3 license</a>.
        It is not reliable for navigation and safety of life or property.
        Radio reception and handling regulations vary by region, so check your local administration's rules. Illegal use is strictly prohibited.
        </p>
        <p>
        The web-interface gratefully uses the following libraries:
        <a href="https://www.chartjs.org/docs/latest/charts/line.html" rel="nofollow">chart.js</a>,
        <a href="https://www.chartjs.org/chartjs-plugin-annotation/latest/" rel="nofollow">chart.js annotation plugin</a>,
        <a href="https://openlayers.org/" rel="nofollow">openlayers</a>,
        <a href="https://fonts.google.com/icons?selected=Material+Icons" rel="nofollow">Material Design Icons</a>,
        <a href="https://tabulator.info/" rel="nofollow">tabulator</a>,
        <a href="https://github.com/markedjs/marked">marked</a>, and
        <a href="https://github.com/lipis/flag-icons">flag-icons</a>. Please consult the links for the respective licenses.
        </p>`;

    showDialog("About...", message);
}

function showWelcome() {
    if (settings.welcome == true || (settings.welcome == "true" && !isAndroid())) showAboutDialog();

    settings.welcome = false;
    saveSettings();
}

// for overwrite and insert code where needed
function main() {
    plugins_main.forEach(function (p) {
        p();
    });
}

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
        serverType: 'geoserver'
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

addOverlayLayer("Aircraft", planeLayer);


console.log("Starting plugin code");


window.loadPlugins && window.loadPlugins();

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

console.log("Plugin loading completed");

console.log("Load settings");
loadSettings();
updateReceiverSelect(config.receivers);

console.log("Load settings from URL parameters");

loadSettingsFromURL();
updateForLegacySettings();

applyDynamicStyling();
community.applySharingState();

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
prepareShipcard();

if (!config.features.about_md) {
    document.getElementById("about_tab").style.display = "none";
    document.getElementById("about_tab_mini").style.display = "none";
}

if (!config.features.realtime) {
    document.getElementById("realtime_tab").style.display = "none";
    document.getElementById("realtime_tab_mini").style.display = "none";

    // Hide realtime context menu items
    const realtimeMenuItems = document.querySelectorAll('.ctx-realtime');
    realtimeMenuItems.forEach(item => {
        item.style.display = 'none';
    });

    // Hide realtime option in shipcard
    const shipcardRealtime = document.getElementById('shipcard_realtime_option');
    if (shipcardRealtime) {
        shipcardRealtime.style.display = 'none';
    }
}

if (!config.features.log) {
    document.getElementById("log_tab").style.display = "none";
    document.getElementById("log_tab_mini").style.display = "none";
}

if (!config.features.decoder) {
    document.getElementById("decoder_tab").style.display = "none";
    document.getElementById("decoder_tab_mini").style.display = "none";
}

if (!config.webcontrol_http) {
    document.getElementById("webcontrol_tab").style.display = "none";
    document.getElementById("webcontrol_tab_mini").style.display = "none";
}

showWelcome();
kiosk.updateKiosk();
applyShipcardPinStyling()
updateAndroid();

if (isAndroid()) showMenu();

main();

// Re-apply chart colors after all stylesheets load (Firefox iframe quirk).
window.addEventListener('load', () => {
    requestAnimationFrame(() => {
        requestAnimationFrame(() => {
            plotsModule?.updateColors();
        });
    });
});

//checkLatestVersion();
