import { registerActions, registerChanges, registerInputs } from '../events.js';
import { settings, persist, notifyChange, onSettingsChange, restoreDefaultSettings } from '../settings.js';
import { showDialog, showNotification, isKiosk } from '../ui.js';
import { updateAllChartColors } from './charts.js';

let deps = {};

// ── Dark mode ─────────────────────────────────────────────────────────────────

export function updateDarkMode() {
    document.documentElement.classList.toggle("dark", settings.dark_mode);
    updateAllChartColors();
    deps.updateMapLayer();
    deps.redrawMap();
}

export function setDarkMode(b) {
    settings.dark_mode = b;
    updateDarkMode();
    deps.saveSettings();
}

function toggleDarkMode() {
    settings.dark_mode = !settings.dark_mode;
    updateDarkMode();
    deps.saveSettings();
}

// ── Coordinate / units ────────────────────────────────────────────────────────

export function setCoordinateFormat(format) {
    settings.coordinate_format = format;
    deps.saveSettings();
    deps.refresh_data();
    deps.resetTable();
}

export function setMetrics(s) {
    const upper = s.toUpperCase();
    settings.metric = upper === "METRIC" ? "SI" : upper === "IMPERIAL" ? "IMPERIAL" : "DEFAULT";
    showNotification("Switched units to " + s);
    deps.saveSettings();
    deps.refresh_data();
    deps.resetTable();
}

export function getMetrics() {
    if (settings.metric === "SI")        return "Metric";
    if (settings.metric === "IMPERIAL")  return "Imperial";
    return "Default";
}

// ── Table icon ────────────────────────────────────────────────────────────────

export function setTableIcon(s) {
    settings.table_shiptype_use_icon = s;
    deps.saveSettings();
    deps.refresh_data();
    deps.resetTable();
}

// ── Map settings ──────────────────────────────────────────────────────────────

export function setMapSetting(a, v) {
    settings[a] = v;
    deps.saveSettings();
    deps.redrawMap();
}

export function applyIconScale(v) {
    settings.icon_scale = v;
    deps.redrawMap();
    deps.saveSettings();
}

export function applyMapOpacity(v) {
    settings.map_opacity = v;
    deps.setMapOpacity();
    deps.saveSettings();
}

export function applyTrackOnSelect(v) { settings.show_track_on_select = v; deps.saveSettings(); }
export function applyTrackOnHover(v)  { settings.show_track_on_hover  = v; deps.saveSettings(); }

export function applyDistanceCircleColor(v) {
    deps.removeDistanceCircles();
    setMapSetting('distance_circle_color', v);
}

export function applyRangeColor(key, val) {
    deps.setRangeColor(val, key);
}

// ── Range ─────────────────────────────────────────────────────────────────────

export function setRangeSwitch(b) {
    if (b != settings.show_range) deps.toggleRange();
}

export function setRangeTimePeriod(v) {
    settings.range_timeframe = v;
    deps.saveSettings();
    deps.fetchRange(true).then(() => deps.drawRange());
}

// ── Fading ────────────────────────────────────────────────────────────────────

export function setFading(b) {
    if (b != settings.fading) deps.toggleFading();
}

// ── Track colors ──────────────────────────────────────────────────────────────

export function setTrackClassColor(shipClass, color) {
    settings.track_class_colors[deps.ShippingClass[shipClass]] = color;
    deps.saveSettings();
    deps.redrawMap();
}

export function applyColorToAllTracks(color) {
    const colorToApply = color || '#12a5ed';
    for (const classKey in deps.ShippingClass) {
        settings.track_class_colors[deps.ShippingClass[classKey]] = colorToApply;
    }
    updateTrackColorInputs();
    deps.saveSettings();
    deps.redrawMap();
}

export function resetTrackColorsToDefault() {
    settings.track_class_colors = deps.getDefaultTrackColors();
    updateTrackColorInputs();
    deps.saveSettings();
    deps.redrawMap();
}

export function updateTrackColorInputs() {
    const SC = deps.ShippingClass;
    const tc = settings.track_class_colors;
    document.getElementById("settings_track_cargo_color").value      = tc[SC.CARGO];
    document.getElementById("settings_track_b_color").value          = tc[SC.B];
    document.getElementById("settings_track_passenger_color").value  = tc[SC.PASSENGER];
    document.getElementById("settings_track_tanker_color").value     = tc[SC.TANKER];
    document.getElementById("settings_track_fishing_color").value    = tc[SC.FISHING];
    document.getElementById("settings_track_highspeed_color").value  = tc[SC.HIGHSPEED];
    document.getElementById("settings_track_special_color").value    = tc[SC.SPECIAL];
    document.getElementById("settings_track_aton_color").value       = tc[SC.ATON];
    document.getElementById("settings_track_station_color").value    = tc[SC.STATION];
    document.getElementById("settings_track_sartepirb_color").value  = tc[SC.SARTEPIRB];
    document.getElementById("settings_track_plane_color").value      = tc[SC.PLANE];
    document.getElementById("settings_track_helicopter_color").value = tc[SC.HELICOPTER];
    document.getElementById("settings_track_other_color").value      = tc[SC.OTHER];
    document.getElementById("settings_track_unknown_color").value    = tc[SC.UNKNOWN];
}

export function openSettings()  { document.querySelector(".settings_window").classList.add("active"); }
export function closeSettings() { document.querySelector(".settings_window").classList.remove("active"); }

// ── Kiosk ─────────────────────────────────────────────────────────────────────

export function setKiosk(enabled) {
    settings.kiosk = enabled;
    deps.updateKiosk();
    deps.saveSettings();
}

export function setKioskRotationSpeed(speed) {
    settings.kiosk_rotation_speed = parseInt(speed);
    deps.saveSettings();
    if (isKiosk() && deps.isKioskAnimating()) deps.startKioskAnimation();
}

export function setKioskPanMap(enabled) {
    settings.kiosk_pan_map = enabled;
    deps.saveSettings();
}

// ── Slider display labels ─────────────────────────────────────────────────────

export function updateKioskSpeedDisplay(value) {
    document.getElementById("kiosk_rotation_speed_label").textContent = `Rotation Speed (${value}s)`;
}

export function updateTrackWeightDisplay(value) {
    document.getElementById("track_weight_label").textContent = `Track Weight (${value})`;
}

export function updateTrackTrashThresholdDisplay(value) {
    document.getElementById("track_trash_threshold_label").textContent = `Track Dash Threshold (${value}s)`;
}

export function updateIconScaleDisplay(value) {
    document.getElementById("icon_scale_label").textContent = `Ship icon size (${parseFloat(value).toFixed(2)})`;
}

export function updateMapOpacityDisplay(value) {
    document.getElementById("map_opacity_label").textContent = `Map dimming (${Math.round(parseFloat(value) * 100)}%)`;
}

export function updateTooltipFontSizeDisplay(value) {
    document.getElementById("tooltip_font_size_label").textContent = `Font Size (${value})`;
}

export function updateShipoutlineOpacityDisplay(value) {
    document.getElementById("shipoutline_opacity_label").textContent = `Opacity (${parseFloat(value).toFixed(2)})`;
}

export function updateCircleScaleDisplay(value) {
    document.getElementById("circle_scale_label").textContent = `Selector line width (${parseFloat(value).toFixed(1)})`;
}

// ── Misc dialogs ──────────────────────────────────────────────────────────────

function showPlugins() {
    showDialog("Plugins", "<pre>Loaded plugins:\n" + deps.getPlugins() + "</pre>");
}

function showServerErrors() {
    showDialog("Server Errors", deps.getServerMessage() === "" ? "None" : ("<pre>" + deps.getServerMessage() + "</pre>"));
}

// ── Defaults ──────────────────────────────────────────────────────────────────

function applyDefaultSettings() {
    const t        = settings.tab;
    const android  = settings.android;
    const darkmode = settings.dark_mode;

    restoreDefaultSettings(deps.getDefaultTrackColors());

    settings.android = android;
    if (deps.isAndroid()) settings.dark_mode = darkmode;

    deps.updateSortMarkers();
    setDarkMode(settings.dark_mode);
    setMetrics(settings.metric === "SI" ? "metric" : settings.metric === "IMPERIAL" ? "imperial" : "default");
    deps.updateMapLayer();
    setFading(settings.fading);
    deps.updateFocusMarker();
    deps.removeDistanceCircles();

    settings.tab     = t;
    settings.welcome = false;

    deps.redrawMap();
}

// ── Settings panel DOM sync ───────────────────────────────────────────────────

export function updateSettingsTab() {
    document.getElementById("settings_darkmode").checked                  = settings.dark_mode;
    document.getElementById("settings_coordinate_format").value           = settings.coordinate_format;
    document.getElementById("settings_metric").value                      = getMetrics().toLowerCase();
    document.getElementById("settings_show_station").checked              = settings.show_station;
    document.getElementById("settings_fading").checked                    = settings.fading;
    document.getElementById("settings_show_signal_graphs").checked        = settings.show_signal_graphs;
    document.getElementById("settings_show_ppm_graphs").checked           = settings.show_ppm_graphs;
    document.getElementById("settings_shiphover_color").value             = settings.shiphover_color;
    document.getElementById("settings_shipselection_color").value         = settings.shipselection_color;
    document.getElementById("settings_show_range").checked                = settings.show_range;
    document.getElementById("settings_distance_circles").checked          = settings.distance_circles;
    document.getElementById("settings_distance_circle_color").value       = settings.distance_circle_color;
    document.getElementById("settings_labels_declutter").checked          = settings.labels_declutter;
    document.getElementById("settings_tooltipLabelFontsize").value        = settings.tooltipLabelFontSize;
    document.getElementById("settings_show_labels").value                 = settings.show_labels.toLowerCase();
    document.getElementById("settings_shipoutline_border").value          = settings.shipoutline_border;
    document.getElementById("settings_shipoutline_inner").value           = settings.shipoutline_inner;
    document.getElementById("settings_shipoutline_opacity").value         = settings.shipoutline_opacity;
    document.getElementById("settings_show_circle_outline").checked       = settings.show_circle_outline;
    document.getElementById("settings_circle_scale").value                = settings.circle_scale;
    document.getElementById("settings_range_color").value                 = settings.range_color;
    document.getElementById("settings_range_timeframe").value             = settings.range_timeframe;
    document.getElementById("settings_range_color_short").value           = settings.range_color_short;
    document.getElementById("settings_range_color_dark").value            = settings.range_color_dark;
    document.getElementById("settings_range_color_dark_short").value      = settings.range_color_dark_short;
    document.getElementById("settings_map_opacity").value                 = settings.map_opacity;
    document.getElementById("settings_icon_scale").value                  = settings.icon_scale;
    document.getElementById("settings_track_weight").value                = settings.track_weight;
    document.getElementById("settings_track_trash_threshold").value       = settings.track_trash_threshold;

    updateIconScaleDisplay(settings.icon_scale);
    updateMapOpacityDisplay(settings.map_opacity);
    updateTrackWeightDisplay(settings.track_weight);
    updateTrackTrashThresholdDisplay(settings.track_trash_threshold);
    updateTooltipFontSizeDisplay(settings.tooltipLabelFontSize);
    updateShipoutlineOpacityDisplay(settings.shipoutline_opacity);
    updateCircleScaleDisplay(settings.circle_scale || 6.0);

    document.getElementById("settings_tooltipLabelColor").value           = settings.tooltipLabelColor;
    document.getElementById("settings_tooltipLabelShadowColor").value     = settings.tooltipLabelShadowColor;
    document.getElementById("settings_tooltipLabelColorDark").value       = settings.tooltipLabelColorDark;
    document.getElementById("settings_tooltipLabelShadowColorDark").value = settings.tooltipLabelShadowColorDark;
    document.getElementById("settings_table_shiptype_use_icon").checked   = settings.table_shiptype_use_icon;
    document.getElementById("settings_show_track_on_hover").checked       = settings.show_track_on_hover;
    document.getElementById("settings_show_track_on_select").checked      = settings.show_track_on_select;
    document.getElementById("settings_kiosk_mode").checked                = settings.kiosk;
    document.getElementById("settings_kiosk_rotation_speed").value        = settings.kiosk_rotation_speed;
    document.getElementById("settings_kiosk_pan_map").checked             = settings.kiosk_pan_map;

    updateKioskSpeedDisplay(settings.kiosk_rotation_speed);
    updateTrackColorInputs();
}

// ── Public API ────────────────────────────────────────────────────────────────

export function init(dependencies) {
    deps = dependencies;

    registerActions({
        applyDefaultSettings, showPlugins, showServerErrors,
        resetTrackColorsToDefault,
        openSettings,
        closeSettings,
        toggleDarkMode,
    });

    registerChanges({
        setDarkMode, setCoordinateFormat, setMetrics, setTableIcon, setMapSetting,
        applyIconScale, applyMapOpacity, setFading, setRangeSwitch, setRangeTimePeriod,
        applyRangeColor, applyDistanceCircleColor, applyTrackOnSelect, applyTrackOnHover,
        applyColorToAllTracks, setTrackClassColor,
        setKiosk, setKioskRotationSpeed, setKioskPanMap,
        // setGraphVisibility — registered by initCharts()
    });

    registerInputs({
        updateIconScaleDisplay, updateMapOpacityDisplay, updateCircleScaleDisplay,
        updateTooltipFontSizeDisplay, updateShipoutlineOpacityDisplay,
        updateTrackWeightDisplay, updateTrackTrashThresholdDisplay, updateKioskSpeedDisplay,
    });

    // Sync panel when settings change externally.
    onSettingsChange(updateSettingsTab);
}
