// The viewer's settings as a store: their defaults and migrations, loading them
// from storage and the address, and writing them, with what a write takes along
// (beforeWrite, afterWrite) and what a save tells (onSaved); what a changed
// setting means beyond its value (effects). core/state.js holds the live object.
// The panel is features/settings/settings.js.

import { settings } from './core/state.js';
import { config } from './core/config.js';
import { ShippingClass } from '@aiscatcher/core/constants.js';
import * as settingsStorage from '@aiscatcher/ui/settings.js';
import { debounce } from '@aiscatcher/ui/components.js';
import * as markersLib from '@aiscatcher/map/markers.js';
import { showDialog, showNotification } from './dialog.js';

const urlParams = new URLSearchParams(window.location.search);

// ─── defaults and migrations ─────────────────────────────────────────────────

// a track takes the colour of its class's marker until the user picks another
export function defaultTrackColors() {
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

const DEFAULT_SETTINGS = {
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
        map_opacity: 0.5,
        layer_opacity: { Aircraft: 1 },
        basemap_opacity: {
            "AIS-catcher Light": 1, "AIS-catcher Light (no labels)": 1, "AIS-catcher Dark": 1,
            "OpenFreeMap Positron": 1, "OpenFreeMap Positron (no labels)": 1,
            "OpenFreeMap Bright": 1, "OpenFreeMap Liberty": 1,
            "OpenFreeMap Dark": 1, "OpenFreeMap Dark (no labels)": 1,
            "OpenStreetMap": 0.6, "Satellite": 0.6,
        },
        show_track_on_hover: false,
        show_track_on_select: false,
        show_all_tracks: false,
        targetcard_top_left: true,
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
    settings.track_class_colors = defaultTrackColors();
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
        const now = defaultTrackColors(), saved = settings.track_class_colors || {};
        for (const cls of Object.keys(now)) {
            if (!saved[cls] || String(saved[cls]).toLowerCase() === TRACK_COLORS_BEFORE_TINT[cls]) saved[cls] = now[cls];
        }
        settings.track_class_colors = saved;
    }

    if (!("showPlanesAtFirst" in settings)) {
        settings.showPlanesAtFirst = true;
        settings.map_overlay.push("Aircraft");
    }

    for (const key of ["pinned", "pinned_x", "pinned_y", "top_left"]) {
        const was = "shipcard_" + key;
        if (was in settings) {
            settings["targetcard_" + key] = settings[was];
            delete settings[was];
        }
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
    settings.android = false;
}

function convertStringSettingsToActual() {
    for (const key of Object.keys(DEFAULT_SETTINGS)) {
        const v = settings[key];
        if (typeof v !== 'string') continue;
        const want = typeof DEFAULT_SETTINGS[key];
        if (want === 'boolean')
            settings[key] = v === "true";
        else if (want === 'number')
            settings[key] = v !== '' && !isNaN(v) ? Number(v) : DEFAULT_SETTINGS[key];
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

// ─── writing ─────────────────────────────────────────────────────────────────

const before = [], after = [], saved = [];

// fn brings what is kept outside the settings into them, before every write
export function beforeWrite(fn) {
    before.push(fn);
}

// fn follows every write
export function afterWrite(fn) {
    after.push(fn);
}

// fn follows a save of the settings; not a resting map writing its view
export function onSaved(fn) {
    saved.push(fn);
}

function write() {
    for (const fn of before) fn();
    settingsStore.save();
    for (const fn of after) fn();
}

export function saveSettings() {
    write();
    for (const fn of saved) fn();
}

// the map moved: its view is kept once it rests
export const saveMapView = debounce(write, 250);

// ─── what a change means ─────────────────────────────────────────────────────

/* What a changed setting means beyond its value; the panel also redraws the map
   after every change. Each owner registers its own with effects(), beside its
   actions. A control that needs more than this keeps its own data-on-change
   action. */
const SETTING_EFFECTS = {};

export function effects(table) {
    Object.assign(SETTING_EFFECTS, table);
}

export function runEffect(key, value) {
    SETTING_EFFECTS[key]?.(value);
}

// everything as the settings now say, after they load and after a reset to the
// defaults; an effect registered for several settings runs once
export function runAllEffects() {
    const done = new Set();
    for (const [key, fn] of Object.entries(SETTING_EFFECTS)) {
        if (done.has(fn)) continue;
        done.add(fn);
        fn(settings[key]);
    }
}
