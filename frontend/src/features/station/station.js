// The station the page looks through, when the server has several: the card a
// station on the map opens, the filter's choice of one, the header chip that goes
// back to all of them, and what a switch resets.

import { config, severalStations, stationLabel } from '../../core/config.js';
import { stationBand } from '@aiscatcher/ui/stations.js';
import { createPicker } from '@aiscatcher/ui/picker.js';
import { focus, switchFocus, refresh } from '../../data.js';
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
function focusName() {
    return stationLabel(focus);
}

// the menu offers a choice only when there is one
export function hasStationChoice() {
    return severalStations();
}

// a station the server lets the page look through alone
const offered = (id) => (config.stations || []).some((s) => s.id === id);

function selectStation(id) {
    closeDialog();
    onStationChange(id);
}

export function onStationChange(id) {
    deps.closeSearch();
    if (replaycardVisible()) replaybar.toggleReplaycard();
    switchFocus(id);
    mapObjects.resetSince();
    range.resetUpdateTime();
    showFocus();
    refresh();
}

// after the settings or the server's list of stations changed: a focus on one
// the server no longer offers goes back to all of them
export function syncFocus(live) {
    if (focus >= 0 && !offered(focus)) {
        if (live) onStationChange(-1);
        else switchFocus(-1);
    }
    showFocus();
}

// back to all stations, as the filter's Reset has it
export function resetFocus() {
    if (focus >= 0) onStationChange(-1);
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
    const hidden = focus < 0 || !hasStationChoice();
    const text = hidden ? "" : focusName();
    if (chip.hidden === hidden && label.textContent === text) return;
    chip.hidden = hidden;
    label.textContent = text;
    document.getElementById("header-focus-id").textContent = hidden ? "" : String(focus);
    chip.title = "Showing " + (text || "one station") + ": click to show all stations";
    applyMenuLabels(); // the tab pill has less room or more, and the chip with it
}

// the ticker's copy of the chip, for when the header has no room for it
function placeFocusChip() {
    const chip = document.getElementById("header-focus");
    const alt = document.getElementById("ticker_focus");
    if (!chip || !alt) return;
    alt.hidden = chip.hidden || !chip.classList.contains("fit-hidden");
    document.getElementById("ticker_focus_label").textContent = alt.hidden ? "" : focusName();
    alt.title = chip.title;
}

// ─── the station picked from a list ───────────────────────────────────────────

const pickers = []; // every dropdown bound to the focus: the filter's, the statistics', the plots'

// all stations, then each the page offers, in the order of the configuration, with
// what the map knows of it
export function focusOptions() {
    const onMap = new Map(mapObjects.stationsAll().map((s) => [s.id, s]));
    const listed = (config.stations || []).filter((s) => s.id >= 0);
    return [{ value: -1, name: "All stations", sub: listed.length + " stations", group: "all" },
        ...listed.map((s) => ({ value: s.id, badge: s.id, name: s.name, sub: STATUS_TEXT[onMap.get(s.id)?.status], group: "one" }))];
}

// a dropdown bound to the focus, in `host`: a pick there is a pick everywhere
export function mountFocusPicker(host) {
    const p = createPicker({ label: "Stations", value: focus, options: focusOptions, empty: "No station by that name",
        onPick: (id) => onStationChange(id) });
    host.append(p.el);
    pickers.push(p);
    return p;
}

let filterPicker = null;

function updateFilterPicker() {
    const section = document.getElementById("filter_station_section");
    if (!section) return;
    section.hidden = !hasStationChoice();
    if (!section.hidden && !filterPicker)
        filterPicker = mountFocusPicker(document.getElementById("filter_station"));
    for (const p of pickers)
        p.set(focus);
}

// ─── a station from the map ──────────────────────────────────────────────────

export const STATUS_TEXT = { online: "Receiving", connected: "Connected, nothing heard for five minutes", offline: "Offline" };

const count = (n) => (Number(n) || 0).toLocaleString();

// what the map knows of a station and what its own figures say, with the choice
// to look through it alone or, when it is the one in focus, through all again
export async function openStation(id) {
    const s = mapObjects.stationsOnMap().find((x) => x.id === id);
    if (!s) return;
    let stat = null;
    try {
        stat = await (await fetch("api/stat.json?stations=" + id)).json();
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
    if (offered(id) && hasStationChoice())
        actions = focus === id
            ? '<button type="button" class="btn btn-primary" data-action="selectStation" data-id="-1">Show all stations</button>'
            : `<button type="button" class="btn btn-primary" data-action="selectStation" data-id="${Number(id)}">Show only this station</button>`;

    showDialog("Station", '<div class="station-card">' + stationBand(s) + objectToTableHtml(rows) +
        (actions ? '<div class="station-card-actions">' + actions + '</div>' : '') + '</div>');
}

register({
    selectStation: (e, d) => selectStation(d.id),
});
