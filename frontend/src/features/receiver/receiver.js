// The station the viewer looks through, when the server has several: the card a
// station on the map opens, the filter's choice of one, the header chip that goes
// back to all of them, and what a switch resets.

import { config, severalStations, stationLabel } from '../../core/config.js';
import { stationBand } from '@aiscatcher/ui/stations.js';
import { createPicker } from '@aiscatcher/ui/picker.js';
import { receiver, switchReceiver, refresh } from '../../data.js';
import { ships } from '../../core/store.js';
import { getDistanceVal, getDistanceUnit, getLatValFormat, getLonValFormat } from '../../core/units.js';
import { showDialog, closeDialog, objectToTableHtml } from '../../dialog.js';
import { replaycardVisible, applyMenuLabels } from '../../panels.js';
import * as replaybar from '../replay/replaybar.js';
import * as mapObjects from '../mapobjects/mapobjects.js';
import * as range from '../range/range.js';
import { register } from '../../actions.js';

// { closeSearch(), onFocus(): another station is in focus }
let deps = { closeSearch: () => {}, onFocus: () => {} };

export function init(d) {
    deps = d;
    // the header gives the chip room or not; without it the ticker carries the station
    document.getElementById("header-focus")?.addEventListener("fit", placeFocusChip);
}

// the station shown, as the header chip names it
function receiverLabel() {
    return stationLabel(receiver);
}

// the menu offers a choice only when there is one
export function hasReceiverChoice() {
    return severalStations();
}

// a station the server lets the page look through alone
const offered = (id) => (config.receivers || []).some((r) => r.idx === id);

function selectReceiver(idx) {
    closeDialog();
    onReceiverChange(idx);
}

export function onReceiverChange(idx) {
    deps.closeSearch();
    if (replaycardVisible()) replaybar.toggleReplaycard();
    switchReceiver(idx);
    mapObjects.resetSince();
    range.resetUpdateTime();
    showFocus();
    refresh();
}

// after the settings or the server's list of stations changed: a focus on one
// the server no longer offers goes back to all of them
export function syncFocus(live) {
    if (receiver >= 0 && !offered(receiver)) {
        if (live) onReceiverChange(-1);
        else switchReceiver(-1);
    }
    showFocus();
}

// back to all stations, as the filter's Reset has it
export function resetFocus() {
    if (receiver >= 0) onReceiverChange(-1);
}

// the station in focus, wherever the page shows it
function showFocus() {
    updateFocusChip();
    updateFilterPicker();
    deps.onFocus();
}

// ─── the header chip ─────────────────────────────────────────────────────────

// while one station is in focus the header names it, or gives its number where the
// name does not fit; a click shows all again
function updateFocusChip() {
    const chip = document.getElementById("header-focus");
    if (!chip) return;
    const label = document.getElementById("header-focus-label");
    const hidden = receiver < 0 || !hasReceiverChoice();
    const text = hidden ? "" : receiverLabel();
    if (chip.hidden === hidden && label.textContent === text) return;
    chip.hidden = hidden;
    label.textContent = text;
    document.getElementById("header-focus-id").textContent = hidden ? "" : String(receiver);
    chip.title = "Showing " + (text || "one station") + ": click to show all stations";
    applyMenuLabels(); // the tab pill has less room or more, and the chip with it
}

// the ticker's copy of the chip, for when the header has no room for it
function placeFocusChip() {
    const chip = document.getElementById("header-focus");
    const alt = document.getElementById("ticker_focus");
    if (!chip || !alt) return;
    alt.hidden = chip.hidden || !chip.classList.contains("fit-hidden");
    document.getElementById("ticker_focus_label").textContent = alt.hidden ? "" : receiverLabel();
    alt.title = chip.title;
}

// ─── the filter's station ─────────────────────────────────────────────────────

let picker = null;

// all stations, then each the page offers, in the order of the configuration, with
// what the map knows of it
function pickerOptions() {
    const onMap = new Map(mapObjects.stationsOnMap().map((s) => [s.id, s]));
    const listed = (config.receivers || []).filter((r) => r.idx >= 0);
    return [{ value: -1, name: "All stations", sub: listed.length + " stations", group: "all" },
        ...listed.map((r) => ({ value: r.idx, badge: r.idx, name: r.label, sub: STATUS_TEXT[onMap.get(r.idx)?.status], group: "one" }))];
}

function updateFilterPicker() {
    const section = document.getElementById("filter_station_section");
    if (!section) return;
    section.hidden = !hasReceiverChoice();
    if (section.hidden) return;
    if (!picker) {
        picker = createPicker({ label: "Stations", value: receiver, options: pickerOptions, empty: "No station by that name",
            onPick: (idx) => onReceiverChange(idx) });
        document.getElementById("filter_station").append(picker.el);
    }
    picker.set(receiver);
}

// ─── a station from the map ──────────────────────────────────────────────────

const STATUS_TEXT = { online: "Receiving", connected: "Connected, nothing heard for five minutes", offline: "Offline" };

const count = (n) => (Number(n) || 0).toLocaleString();

// what the map knows of a station and what its own figures say, with the choice
// to look through it alone or, when it is the one in focus, through all again
export async function openStation(id) {
    const s = mapObjects.stationsOnMap().find((x) => x.id === id);
    if (!s) return;
    let stat = null;
    try {
        stat = await (await fetch("api/stat.json?receiver=" + id)).json();
    } catch {
        // the card without figures
    }

    const rows = { Status: STATUS_TEXT[s.status] || STATUS_TEXT.online };
    if (s.mmsi) rows["On board"] = ships[s.mmsi]?.raw?.shipname || "MMSI " + s.mmsi;
    else rows.Position = getLatValFormat(s) + "  " + getLonValFormat(s);
    if (stat) {
        rows["Vessels now"] = count(stat.vessel_count);
        rows["Message rate"] = (Number(stat.msg_rate) || 0).toFixed(1) + " msg/s";
        rows["Messages since start"] = count(stat.session?.count);
        if (stat.session?.dist > 0) rows["Furthest since start"] = getDistanceVal(stat.session.dist) + " " + getDistanceUnit();
    }

    let actions = "";
    if (offered(id) && hasReceiverChoice())
        actions = receiver === id
            ? '<button type="button" class="btn btn-primary" data-action="selectReceiver" data-idx="-1">Show all stations</button>'
            : `<button type="button" class="btn btn-primary" data-action="selectReceiver" data-idx="${Number(id)}">Show only this station</button>`;

    showDialog("Station", '<div class="station-card">' + stationBand(s) + objectToTableHtml(rows) +
        (actions ? '<div class="station-card-actions">' + actions + '</div>' : '') + '</div>');
}

register({
    selectReceiver: (e, d) => selectReceiver(d.idx),
});
