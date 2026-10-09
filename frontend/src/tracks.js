// Which vessels show a track: the ones picked (markerTracks in core/store.js),
// every vessel, the selected vessel's while the settings ask for that, the
// hovered one's likewise, and from what time on. The points themselves are
// fetched by data.js.

import { settings } from './core/state.js';
import {
    paths, clock, cardMmsi as card_mmsi, cardType as card_type, hoverMmsi, hoverType, markerTracks as marker_tracks,
    setMarkerTracks, setPaths, setPathsFrom,
} from './core/store.js';
import { fetchTracks, trackCutoff, setTrackCutoff, resetTrackCursor } from './data.js';
import { redraw } from './map.js';
import { saveSettings } from './features/settings/settings.js';
import { showNotification } from './dialog.js';
import * as targetcard from './features/targetcard/targetcard.js';
import { targetcardMinIfMaxonMobile } from './features/targetcard/card.js';
import { register } from './actions.js';

// the hovered vessel's track is on the map because of the hover, the
// selected vessel's because of the selection: each goes when that ends
export let hoverTrackShown = false;
export let selectTrackShown = false;

async function ToggleTrackOnMap(m) {

    if (marker_tracks.has(Number(m))) {
        marker_tracks.delete(Number(m));
        redraw();
    } else {
        marker_tracks.add(Number(m));
        await fetchTracks();
        targetcardMinIfMaxonMobile();
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

export function setTrackVisibility(mode) {
    if (mode === 'all') return showAllTracks();
    if (mode === 'none') return deleteAllTracks();
    settings.show_all_tracks = false;
    saveSettings();
    redraw();
    targetcard.updateTrackOption();
}

export async function showAllTracks() {
    settings.show_all_tracks = true;
    setTrackCutoff(0);
    resetTrackCursor();
    selectTrackShown = hoverTrackShown = false;
    await fetchTracks();
    redraw();
    targetcard.updateTrackOption();
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
        targetcard.updateTrackOption();
    }
    return mmsis.length;
}

export function deleteAllTracks() {
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

    redraw(); targetcard.updateTrackOption();
    saveSettings();
}

// ─── from when ───────────────────────────────────────────────────────────────

export async function toggleTrackCutoff() {
    setTrackCutoff(trackCutoff ? 0 : (clock || Math.floor(Date.now() / 1000)));
    setPaths({});
    setPathsFrom(-1);
    resetTrackCursor();
    await fetchTracks();
    redraw();
    targetcard.updateTrackOption();
    showNotification(trackCutoff ? "Tracks now start from " + trackCutoffLabel()
        : "Tracks restored to the full history", "success");
}

export function trackCutoffLabel() {
    const ago = Math.max(0, (clock || 0) - trackCutoff);
    return new Date(Date.now() - ago * 1000)
        .toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
}

register({
    setTrackVisibility: (e, d, el) => setTrackVisibility(el.value),
    toggleAllTracks: () => settings.show_all_tracks ? deleteAllTracks() : showAllTracks(),
    deleteAllTracks: () => deleteAllTracks(),
    toggleTrackCutoff: () => toggleTrackCutoff(),
});
