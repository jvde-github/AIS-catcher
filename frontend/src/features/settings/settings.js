// The viewer's settings: their defaults and migrations, loading them from
// storage and the address, saving them, and the settings panel - its pages,
// its controls bound to settings (data-setting) and what a change means
// beyond its value (SETTING_EFFECTS). core/state.js holds the live object.

import { settings, isAndroid } from '../../core/state.js';
import { config } from '../../core/config.js';
import { pathsFrom, setPaths, setPathsFrom } from '../../core/store.js';
import { getSpeedVal, getSpeedUnit } from '../../core/units.js';
import { ShippingClass } from '@aiscatcher/core/constants.js';
import { SPEED_PALETTES, paletteCSS } from '@aiscatcher/core/palette.js';
import * as settingsStorage from '@aiscatcher/ui/settings.js';
import * as panelLib from '@aiscatcher/ui/panel.js';
import { debounce } from '@aiscatcher/ui/components.js';
import { BINARY_CATEGORIES } from '@aiscatcher/ui/binary.js';
import * as markersLib from '@aiscatcher/map/markers.js';
import { toLonLat } from 'ol/proj';
import {
    map, redraw, markerLayer, planeLayer, shapeLayer, trackLayer, labelLayer,
    updateMapLayer, refreshBaseMapRows, refreshOverlayCredits,
} from '../../map.js';
import { receiver, setReceiver, fetchTracks, resetTrackCursor, trackWindowStart } from '../../data.js';
import { showDialog, showNotification } from '../../dialog.js';
import * as community from '../../overlays/community.js';
import * as range from '../range/range.js';
import * as kiosk from '../kiosk/kiosk.js';
import * as mapObjects from '../mapobjects/mapobjects.js';
import { register } from '../../actions.js';

const urlParams = new URLSearchParams(window.location.search);

// ─── defaults and migrations ─────────────────────────────────────────────────

// a track takes the colour of its class's marker until the user picks another
export function getDefaultTrackColors() {
    return { ...markersLib.TRACK_COLORS };
}

// the track colours before the markers were tinted: a saved colour still equal
// to one of these was never chosen, and follows the marker to its new colour
const TRACK_COLORS_BEFORE_TINT = {
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

export const DEFAULT_SETTINGS = {
        counter: true,
        fading: false,
        android: false,
        kiosk: false,
        welcome: true,
        coordinate_format: "decimal",
        icon_scale: 1,
        plane_palette: "classic",
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
        range_color_dark: "#FFA500",
        range_color_dark_short: "#FFDAB9",
        fix_center: false,
        center_point: "station",
        tooltipLabelColor: "#ffffff",
        tooltipLabelColorDark: "#ffffff",
        tooltipLabelShadowColor: "#000000",
        tooltipLabelShadowColorDark: "#000000",
        tooltipLabelFontSize: 9,
        shiphover_color: "#FFA500",
        shipselection_color: "#2563eb",
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
        show_ports: true,
        show_places: true,
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
        menu_labels: "icons",
        show_signal_graphs: true,
        show_ppm_graphs: true,
        plot_absolute_time: true,
        kiosk_rotation_speed: 5,
        kiosk_pan_map: true,
        shiptable_columns: ["shipname", "mmsi", "imo", "callsign", "shipclass", "lat", "lon", "last_signal", "level", "distance", "bearing", "speed", "repeat", "ppm", "status"],
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
    key: config.context,
    defaults: DEFAULT_SETTINGS,
    target: settings,
    version: 2,
    migrate: updateForLegacySettings,
});

export function restoreDefaultSettings() {
    settingsStore.applyDefaults();
    settings.track_class_colors = getDefaultTrackColors();
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
    if (!markersLib.PLANE_PALETTES[settings.plane_palette]) settings.plane_palette = DEFAULT_SETTINGS.plane_palette;

    if (!settings.tinted_markers) {
        settings.tinted_markers = true;
        const now = getDefaultTrackColors(), saved = settings.track_class_colors || {};
        for (const cls of Object.keys(now)) {
            if (!saved[cls] || String(saved[cls]).toLowerCase() === TRACK_COLORS_BEFORE_TINT[cls]) saved[cls] = now[cls];
        }
        settings.track_class_colors = saved;
    }

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

export function loadSettings() {
    settingsStore.load(urlParams.has("reset"));
    if (settingsStore.upgraded()) showNotification("Settings were reset for this version of the viewer");
    if (settings.activeReceiver) setReceiver(settings.activeReceiver);
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

export function loadSettingsFromURL() {
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

// everything back to the defaults but the platform, the theme and the vessel filter
export function applyDefaultSettings() {
    const t = settings.tab;

    let android = settings.android;
    let darkmode = settings.dark_mode;
    const ship_filter = settings.ship_filter;
    restoreDefaultSettings();

    settings.android = android;
    settings.dark_mode = darkmode;
    if (ship_filter) settings.ship_filter = ship_filter;

    setMetrics(settings.metric, false);
    updateMapLayer();
    range.removeDistanceCircles();

    settings.tab = t;
    settings.welcome = false;
    hooks.defaults();
    saveSettings();

    redraw();
    updateSettingsTab();
    showNotification("Settings restored to defaults", "success");
}

// ─── saving ──────────────────────────────────────────────────────────────────

export function saveSettings() {
    for (const fn of hooks.save) fn();
    settings.activeReceiver = receiver;

    persistSettings();

    if (settingsPanel.isOpen())
        updateSettingsTab();
}

export function persistSettings() {
    if (map !== undefined) {
        const center = toLonLat(map.getView().getCenter());
        settings.lat = center[1];
        settings.lon = center[0];
        settings.zoom = map.getView().getZoom();
    }
    settingsStore.save();
    community.notifyCommunityPopupView();
    updateMapURL();
}

// the map moved: its view is kept once it rests
export const saveMapView = debounce(persistSettings, 250);

export function updateMapURL() {
    if (isAndroid()) return;

    let view = map.getView();
    let center = toLonLat(view.getCenter()); // Converts the center coordinates to [lon, lat]
    let newURL = window.location.href.split("?")[0] + "?lat=" + center[1].toFixed(4) + "&lon=" + center[0].toFixed(4) + "&zoom=" + view.getZoom().toFixed(2) + "&tab=" + settings.tab;
    history.replaceState(null, null, newURL);
}

// ─── the panel ───────────────────────────────────────────────────────────────

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

/* What a changed setting means beyond its value; every change also redraws the
   map (see onChange below). The effects that need the entry it registers with
   effects(). A control that needs more than this keeps its own data-on-change
   action. */
const SETTING_EFFECTS = {
    track_speed_max: () => updateSpeedLegend(),
    track_speed_palette: () => updateSpeedLegend(),
    track_color_mode: () => updateTrackColorModeUI(),
    distance_circle_color: () => range.removeDistanceCircles(),
    label_class_background: () => updateLabelColorRows(),
    binary_messages: () => mapObjects.restyle(),
    binary_color_class: () => mapObjects.restyle(),
    binary_id_labels: () => mapObjects.restyle(),
    binary_group_areas: () => mapObjects.restyle(),
    range_timeframe: () => range.reload(),
    kiosk_rotation_speed: () => kiosk.rotationChanged(),
};

export function effects(table) {
    Object.assign(SETTING_EFFECTS, table);
}

// what the entry does around the settings: before a save, on opening the
// panel, after a reset to the defaults
const hooks = { save: [], open: () => {}, defaults: () => {} };

export function onSave(fn) {
    hooks.save.push(fn);
}

export function onOpen(fn) {
    hooks.open = fn;
}

export function onDefaults(fn) {
    hooks.defaults = fn;
}

export const settingsPanel = panelLib.create({
    window: document.getElementById("settings"),
    groups: SETTINGS_TAB_GROUPS,
    skipGrouping: ".st-group, .filter-checks, #overlayContainer",
    onOpen: () => {
        hooks.open();
        updateSettingsTab();
        syncThemedSettings();
        refreshOverlayCredits();
    },
});

export function buildSettingsTabs() {
    settingsPanel.build();
    const paletteSelect = document.getElementById("settings_track_speed_palette");
    if (!paletteSelect.options.length)
        for (const [key, p] of Object.entries(SPEED_PALETTES))
            paletteSelect.add(new Option(p.name, key));
    settingsPanel.bind(
        { get: (k) => settings[k], set: (k, v) => { settings[k] = v; saveSettings(); }, stage: (k, v) => { settings[k] = v; } },
        { captions: {
            icon_scale: (v) => `Size (${parseFloat(v).toFixed(2)})`,
            track_speed_max: (v) => `Scale Max (${speedWhole(v)} ${getSpeedUnit()})`,
            circle_scale: (v) => `Width (${parseFloat(v).toFixed(1)})`,
            shipoutline_opacity: (v) => `Opacity (${parseFloat(v).toFixed(2)})`,
            track_opacity: (v) => `Opacity (${Math.round(parseFloat(v) * 100)}%)`,
          },
          onChange: (k, v, el, staged) => {
            SETTING_EFFECTS[k]?.(v);
            /* while a slider is dragged the styles re-run; the release rebuilds the features */
            if (staged) [markerLayer, planeLayer, shapeLayer, trackLayer, labelLayer].forEach((l) => l.changed());
            else redraw();
        } });
    panelLib.adoptClasses(document);   // column headers outside the window too
    document.querySelectorAll(".tablecard_inner table thead > tr > th, .signal-table thead th")
        .forEach((el) => el.classList.add("col-header"));
}

export function syncThemedSettings() {
    refreshBaseMapRows();
    updateLabelColorRows();
}

// the header icon and the context menu land on the first page, not wherever the panel was left
export function openSettings() {
    if (settingsPanel.isOpen()) settingsPanel.close();
    else settingsPanel.open("General");
}

export function closeSettings() {
    settingsPanel.close();
}

export function openSettingsTab(title) {
    settingsPanel.open(title);
}

export function updateSettingsTab() {
    document.getElementById("settings_metric").value = getMetrics().toLowerCase();

    document.getElementById("settings_show_range").checked = settings.show_range;

    for (const cat of BINARY_CATEGORIES) {
        const box = document.getElementById("settings_binary_cat_" + cat);
        if (box) box.checked = !settings.binary_exclude.includes(cat);
    }
    document.getElementById("settings_track_history").value = Math.max(0, TRACK_HISTORY_STOPS.indexOf(settings.track_history));

    updateSliderDisplay('trackHistory', settings.track_history);

    document.getElementById("settings_track_visibility").value = settings.show_all_tracks ? "all" : "selected";

    updateTrackColorInputs();
    updateTrackColorModeUI();
    settingsPanel.sync();
}

export function setMetrics(s, notify = true) {
    if (s.toUpperCase() == "DEFAULT") settings.metric = "DEFAULT";
    else if (s.toUpperCase() == "METRIC") settings.metric = "SI";
    else if (s.toUpperCase() == "IMPERIAL") settings.metric = "IMPERIAL";
    else settings.metric = "DEFAULT";

    if (notify) showNotification("Switched units to " + s);
    saveSettings();

    SETTING_EFFECTS.metric?.();
}

function getMetrics() {
    if (settings.metric == "DEFAULT") return "Default";
    if (settings.metric == "SI") return "Metric";
    if (settings.metric == "IMPERIAL") return "Imperial";
    return "Default";
}

export function updateLabelColorRows() {
    document.querySelectorAll(".label-colors").forEach((s) => { s.hidden = !!settings.label_class_background; });
}

export function setTrackHistory(minutes) {
    settings.track_history = minutes;
    saveSettings();

    // deltas only move forwards, so reaching further back needs a full refetch
    const want = trackWindowStart();
    const covered = pathsFrom === 0 || (pathsFrom > 0 && want >= pathsFrom);

    if (covered) {
        redraw();
        return;
    }

    setPaths({});
    resetTrackCursor();
    setPathsFrom(-1);
    fetchTracks().then(redraw);
}

export function setTrackClassColor(shipClass, color) {
    settings.track_class_colors[ShippingClass[shipClass]] = color;
    saveSettings();
    redraw();
}

export function applyColorToAllTracks(color) {
    // Use default blue color if no color provided
    const colorToApply = color || '#12a5ed';
    for (let classKey in ShippingClass) {
        settings.track_class_colors[ShippingClass[classKey]] = colorToApply;
    }
    updateTrackColorInputs();
    saveSettings();
    redraw();
}

function updateTrackColorInputs() {
    for (const key of Object.keys(ShippingClass)) {
        document.getElementById(`settings_track_${key.toLowerCase()}_color`).value = settings.track_class_colors[ShippingClass[key]];
    }
}

// Only one of the two coloring blocks is on screen at a time: the per-class
// pickers, or the palette with its scale.
export function updateTrackColorModeUI() {
    const speed = settings.track_color_mode === "speed";
    document.querySelectorAll(".track-class-color").forEach((el) => el.classList.toggle("hidden", speed));
    document.querySelectorAll(".track-speed-color").forEach((el) => el.classList.toggle("hidden", !speed));
    updateSpeedLegend();
}

// the scale is read at a glance, so its ticks carry no decimals
const speedWhole = (knots) => Math.round(Number(getSpeedVal(knots)));

export function updateSpeedLegend() {
    const bar = document.getElementById("track_speed_legend");
    if (!bar) return;
    bar.style.background = paletteCSS(settings.track_speed_palette);
    document.getElementById("track_speed_scale_label").textContent =
        `Preview (0 - ${speedWhole(settings.track_speed_max)} ${getSpeedUnit()})`;
}

export function resetTrackColorsToDefault() {
    settings.track_class_colors = getDefaultTrackColors();
    updateTrackColorInputs();
    saveSettings();
    redraw();
}

// minutes of track to draw; the last stop means everything held by the server
export const TRACK_HISTORY_STOPS = [1, 5, 15, 30, 60, 180, 360, 720, 1440, 0];

function trackHistoryLabel(m) {
    if (!m) return 'All';
    return m < 60 ? m + ' min' : (m / 60) + ' h';
}

/* sliders the settings panel does not bind (they carry their own change handlers) */
const SLIDER_DISPLAYS = {
    trackHistory: ["track_history_label", (v) => `Length (${trackHistoryLabel(Number(v))})`],
};

export function updateSliderDisplay(key, value) {
    const [id, format] = SLIDER_DISPLAYS[key];
    document.getElementById(id).textContent = format(value);
}

// labels on the map: off, or always on
export function toggleLabel() {
    if (settings.show_labels == "never") {
        settings.show_labels = "always";
    } else
        settings.show_labels = "never";

    saveSettings();
    redraw();
}

register({
    toggleLabel: () => toggleLabel(),
    openSettings: () => openSettings(),
    openMapSettings: () => openSettingsTab("Map"),
    closeSettings: () => closeSettings(),
    applyDefaultSettings: () => applyDefaultSettings(),
    setMetrics: (e, d, el) => setMetrics(el.value),
    // the track-length slider: dragged (the caption) and released (saved)
    setTrackHistory: (e, d, el) => setTrackHistory(TRACK_HISTORY_STOPS[el.value]),
    updateTrackHistoryDisplay: (e, d, el) => updateSliderDisplay('trackHistory', TRACK_HISTORY_STOPS[el.value]),
    applyColorToAllTracks: (e, d, el) => applyColorToAllTracks(el.value),
    resetTrackColorsToDefault: () => resetTrackColorsToDefault(),
    setTrackClassColor: (e, d, el) => setTrackClassColor(d.shipclass, el.value),
});
