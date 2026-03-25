// Shared settings module.
// - `settings`         — mutable object, live ES-module binding (all importers
//                        see the same object after restoreDefaultSettings() replaces it)
// - `persist()`        — write to localStorage only, no broadcast (use for
//                        background saves like realtime filter updates)
// - `notifyChange()`   — broadcast to all registered tab listeners
// - `onSettingsChange` — each tab registers one callback for display updates
//
// Call init(storageKey) once at app startup with the localStorage key.

let storageKey = 'aiscatcher';
const listeners = [];

export let settings = {};

export function init(key) {
    storageKey = key;
}

export function onSettingsChange(fn) {
    listeners.push(fn);
}

export function notifyChange() {
    listeners.forEach(fn => fn());
}

export function persist() {
    localStorage[storageKey] = JSON.stringify(settings);
}

// trackColors is passed in because ShippingClass lives in script.js for now.
export function restoreDefaultSettings(trackColors = {}) {
    settings = {
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
        show_labels: "never",
        labels_declutter: true,
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
        realtime_filter_mmsis: [],
        track_class_colors: trackColors,
    };
}
