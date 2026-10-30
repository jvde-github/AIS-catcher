// Which vessels show a track: the ones picked (markerTracks in core/store.js),
// every vessel, the selected vessel's while the settings ask for that, the
// hovered one's likewise, and from what time on. The points themselves are
// fetched by data.js. Also the Tracks page of the settings panel: how far back
// they reach and what colour they are.

import { settings } from './core/state.js';
import {
    paths, pathsFrom, clock, cardMmsi as card_mmsi, cardType as card_type, hoverMmsi, hoverType,
    markerTracks as marker_tracks, setMarkerTracks, setPaths, setPathsFrom,
} from './core/store.js';
import { getSpeedVal, getSpeedUnit } from './core/units.js';
import { ShippingClass } from '@aiscatcher/core/constants.js';
import { SPEED_PALETTES, paletteCSS } from '@aiscatcher/core/palette.js';
import { fetchTracks, trackCutoff, setTrackCutoff, resetTrackCursor, trackWindowStart } from './data.js';
import { redraw } from './fleet.js';
import { saveSettings, effects, defaultTrackColors } from './settings-store.js';
import { showNotification } from './dialog.js';
import { updateTrackOption } from './features/targetcard/card.js';
import { register } from './actions.js';

// the hovered vessel's track is on the map because of the hover, the
// selected vessel's because of the selection: each goes when that ends
let hoverTrackShown = false;
let selectTrackShown = false;

async function ToggleTrackOnMap(m) {

    if (marker_tracks.has(Number(m))) {
        marker_tracks.delete(Number(m));
        redraw();
    } else {
        marker_tracks.add(Number(m));
        await fetchTracks();
        redraw();
    }
}

export async function toggleTrack(m) {
    if (settings.show_track_on_select && card_mmsi == m && card_type == 'ship') {
        selectTrackShown = !selectTrackShown;
    }
    else {
        ToggleTrackOnMap(m);
    }
    updateTrackOption();

}

async function showTrack(m) {
    if (!marker_tracks.has(Number(m))) {
        ToggleTrackOnMap(m);
    }
    updateTrackOption();

}

async function hideTrack(m) {
    if (marker_tracks.has(Number(m))) {
        ToggleTrackOnMap(m);
    }
    updateTrackOption();

}

export function trackIsShown(m) {
    return marker_tracks.has(Number(m));
}

// ─── following the hover and the selection ───────────────────────────────────

// a moment into a hover: the vessel under the pointer shows its track
export function showHoverTrack(mmsi) {
    if (mmsi) {
        hoverTrackShown = !trackIsShown(hoverMmsi);
        if (hoverTrackShown) {
            showTrack(mmsi);
        }
    }
}

export function hoverEnded(mmsi) {
    if (hoverTrackShown) hideTrack(mmsi);
    hoverTrackShown = false;
}

// the card leaves vessel `prev` for `next` (null: it closes); the track
// the selection put up goes, unless the hover still wants it
export function selectionLeaving(prev, next) {
    if (selectTrackShown && (prev != next || next == null)) {
        selectTrackShown = false;

        if (!(prev == hoverMmsi && hoverTrackShown && hoverType == 'ship')) {
            hideTrack(prev);
        }
    }
}

// a card that was closed opens: no track is the selection's yet
export function selectionOpened() {
    selectTrackShown = false;
}

// the card shows vessel `m`: its track comes up, or the hover's becomes the selection's
export function selectionShown(m) {
    if (hoverMmsi === m && hoverTrackShown && hoverType == 'ship') {
        hoverTrackShown = false;
        selectTrackShown = true;
    }
    else if (!trackIsShown(m)) {
        selectTrackShown = true;
        showTrack(m);
    }
}

// ─── all of them, a box of them, none ────────────────────────────────────────

function setTrackVisibility(mode) {
    if (mode === 'all') return showAllTracks();
    if (mode === 'none') return deleteAllTracks();
    settings.show_all_tracks = false;
    saveSettings();
    redraw();
    updateTrackOption();
}

async function showAllTracks() {
    settings.show_all_tracks = true;
    setTrackCutoff(0);
    resetTrackCursor();
    selectTrackShown = hoverTrackShown = false;
    await fetchTracks();
    redraw();
    updateTrackOption();
    saveSettings();
}

export async function showTracksForMMSIs(mmsis) {
    let added = 0;
    for (const m of mmsis) {
        if (!marker_tracks.has(Number(m))) {
            marker_tracks.add(Number(m));
            added++;
        }
    }
    if (added > 0) {
        resetTrackCursor();
        await fetchTracks();
        redraw();
        updateTrackOption();
    }
    return mmsis.length;
}

function deleteAllTracks() {
    settings.show_all_tracks = false;
    setTrackCutoff(0);
    resetTrackCursor();
    setMarkerTracks(new Set());
    let p = {};

    if (card_type == 'ship' && card_mmsi && settings.show_track_on_select) {
        marker_tracks.add(Number(card_mmsi));
        selectTrackShown = true;

        if (paths[card_mmsi]) {
            p[card_mmsi] = paths[card_mmsi];
        }
    }

    setPaths(p);

    redraw(); updateTrackOption();
    saveSettings();
}

// ─── from when ───────────────────────────────────────────────────────────────

// what was fetched goes: the next fetch asks for the whole window
function dropTracks() {
    setPaths({});
    setPathsFrom(-1);
    resetTrackCursor();
}

async function toggleTrackCutoff() {
    setTrackCutoff(trackCutoff ? 0 : (clock || Math.floor(Date.now() / 1000)));
    dropTracks();
    await fetchTracks();
    redraw();
    updateTrackOption();
    showNotification(trackCutoff ? "Tracks now start from " + trackCutoffLabel()
        : "Tracks restored to the full history", "success");
}

export function trackCutoffLabel() {
    const ago = Math.max(0, (clock || 0) - trackCutoff);
    return new Date(Date.now() - ago * 1000)
        .toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
}

// ─── the settings panel's Tracks page ────────────────────────────────────────

function setTrackHistory(minutes) {
    settings.track_history = minutes;
    saveSettings();

    // deltas only move forwards, so reaching further back needs a full refetch
    const want = trackWindowStart();
    const covered = pathsFrom === 0 || (pathsFrom > 0 && want >= pathsFrom);

    if (covered) {
        redraw();
        return;
    }

    dropTracks();
    fetchTracks().then(redraw);
}

function setTrackClassColor(shipClass, color) {
    settings.track_class_colors[ShippingClass[shipClass]] = color;
    saveSettings();
    redraw();
}

function applyColorToAllTracks(color) {
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
function updateTrackColorModeUI() {
    const speed = settings.track_color_mode === "speed";
    document.querySelectorAll(".track-class-color").forEach((el) => el.classList.toggle("hidden", speed));
    document.querySelectorAll(".track-speed-color").forEach((el) => el.classList.toggle("hidden", !speed));
    updateSpeedLegend();
}

// the scale is read at a glance, so its ticks carry no decimals
export const speedWhole = (knots) => Math.round(Number(getSpeedVal(knots)));

function updateSpeedLegend() {
    const bar = document.getElementById("track_speed_legend");
    if (!bar) return;
    bar.style.background = paletteCSS(settings.track_speed_palette);
    document.getElementById("track_speed_scale_label").textContent =
        `Preview (0 - ${speedWhole(settings.track_speed_max)} ${getSpeedUnit()})`;
}

function resetTrackColorsToDefault() {
    settings.track_class_colors = defaultTrackColors();
    updateTrackColorInputs();
    saveSettings();
    redraw();
}

// minutes of track to draw; the last stop means everything held by the server
const TRACK_HISTORY_STOPS = [1, 5, 15, 30, 60, 180, 360, 720, 1440, 0];

function trackHistoryLabel(m) {
    if (!m) return 'All';
    return m < 60 ? m + ' min' : (m / 60) + ' h';
}

/* the track-length slider carries its own change handlers, so its caption is set here */
function updateTrackHistoryLabel(minutes) {
    document.getElementById("track_history_label").textContent = `Length (${trackHistoryLabel(Number(minutes))})`;
}

// the page's controls that are not bound to a setting one to one
export function syncTrackControls() {
    document.getElementById("settings_track_history").value = Math.max(0, TRACK_HISTORY_STOPS.indexOf(settings.track_history));
    updateTrackHistoryLabel(settings.track_history);
    document.getElementById("settings_track_visibility").value = settings.show_all_tracks ? "all" : "selected";
    updateTrackColorInputs();
    updateTrackColorModeUI();
}

export function buildTrackControls() {
    const paletteSelect = document.getElementById("settings_track_speed_palette");
    if (!paletteSelect.options.length)
        for (const [key, p] of Object.entries(SPEED_PALETTES))
            paletteSelect.add(new Option(p.name, key));
}

effects({
    track_speed_max: () => updateSpeedLegend(),
    track_speed_palette: () => updateSpeedLegend(),
    track_color_mode: () => updateTrackColorModeUI(),
});

register({
    setTrackVisibility: (e, d, el) => setTrackVisibility(el.value),
    toggleAllTracks: () => settings.show_all_tracks ? deleteAllTracks() : showAllTracks(),
    deleteAllTracks: () => deleteAllTracks(),
    toggleTrackCutoff: () => toggleTrackCutoff(),
    // the track-length slider: dragged (the caption) and released (saved)
    setTrackHistory: (e, d, el) => setTrackHistory(TRACK_HISTORY_STOPS[el.value]),
    updateTrackHistoryDisplay: (e, d, el) => updateTrackHistoryLabel(TRACK_HISTORY_STOPS[el.value]),
    applyColorToAllTracks: (e, d, el) => applyColorToAllTracks(el.value),
    resetTrackColorsToDefault: () => resetTrackColorsToDefault(),
    setTrackClassColor: (e, d, el) => setTrackClassColor(d.shipclass, el.value),
});
