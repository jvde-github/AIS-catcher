// The settings panel: its pages, its controls bound to settings (data-setting)
// and resetting them to the defaults. The settings themselves, with their effects,
// are settings-store.js's.

import { settings } from '../../core/state.js';
import { getSpeedUnit } from '../../core/units.js';
import * as panelLib from '@aiscatcher/ui/panel.js';
import { BINARY_CATEGORIES } from '@aiscatcher/ui/binary.js';
import { markerLayer, planeLayer, shapeLayer, trackLayer, labelLayer, refreshBaseMapRows, refreshOverlayCredits } from '../../map.js';
import { redraw } from '../../fleet.js';
import { showNotification } from '../../dialog.js';
import { restoreDefaultSettings, saveSettings, onSaved, effects, runEffect, runAllEffects } from '../../settings-store.js';
import * as tracks from '../../tracks.js';
import { closeSearch } from '../search/search-panel.js';
import { updateFilterUI } from '../filter/filter.js';
import { register } from '../../actions.js';

// everything back to the defaults but the platform, the theme and the vessel filter
function applyDefaultSettings() {
    const { tab, android, dark_mode, ship_filter } = settings;
    restoreDefaultSettings();

    settings.tab = tab;
    settings.android = android;
    settings.dark_mode = dark_mode;
    if (ship_filter) settings.ship_filter = ship_filter;
    settings.welcome = false;

    runAllEffects();
    saveSettings();

    redraw();
    updateSettingsTab();
    showNotification("Settings restored to defaults", "success");
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

effects({
    label_class_background: () => updateLabelColorRows(),
});

export const settingsPanel = panelLib.create({
    window: document.getElementById("settings"),
    groups: SETTINGS_TAB_GROUPS,
    skipGrouping: ".st-group, .filter-checks, #overlayContainer",
    onOpen: () => {
        closeSearch();
        updateFilterUI();
        updateSettingsTab();
        syncThemedSettings();
        refreshOverlayCredits();
    },
});

onSaved(() => {
    if (settingsPanel.isOpen()) updateSettingsTab();
});

export function buildSettingsTabs() {
    settingsPanel.build();
    tracks.buildTrackControls();
    settingsPanel.bind(
        { get: (k) => settings[k], set: (k, v) => { settings[k] = v; saveSettings(); }, stage: (k, v) => { settings[k] = v; } },
        { captions: {
            icon_scale: (v) => `Size (${parseFloat(v).toFixed(2)})`,
            track_speed_max: (v) => `Scale Max (${tracks.speedWhole(v)} ${getSpeedUnit()})`,
            circle_scale: (v) => `Width (${parseFloat(v).toFixed(1)})`,
            shipoutline_opacity: (v) => `Opacity (${parseFloat(v).toFixed(2)})`,
            track_opacity: (v) => `Opacity (${Math.round(parseFloat(v) * 100)}%)`,
          },
          onChange: (k, v, el, staged) => {
            runEffect(k, v);
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
function openSettings() {
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
    tracks.syncTrackControls();
    settingsPanel.sync();
}

function setMetrics(s) {
    if (s.toUpperCase() == "DEFAULT") settings.metric = "DEFAULT";
    else if (s.toUpperCase() == "METRIC") settings.metric = "SI";
    else if (s.toUpperCase() == "IMPERIAL") settings.metric = "IMPERIAL";
    else settings.metric = "DEFAULT";

    showNotification("Switched units to " + s);
    saveSettings();

    runEffect("metric");
}

function getMetrics() {
    if (settings.metric == "DEFAULT") return "Default";
    if (settings.metric == "SI") return "Metric";
    if (settings.metric == "IMPERIAL") return "Imperial";
    return "Default";
}

function updateLabelColorRows() {
    document.querySelectorAll(".label-colors").forEach((s) => { s.hidden = !!settings.label_class_background; });
}

// labels on the map: off, or always on
function toggleLabel() {
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
});
