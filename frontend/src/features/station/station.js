// The station the page looks through, when the server has several: the card a
// station on the map opens, the filter's choice of one, the header chip that goes
// back to all of them, and what a switch resets.

import { config, severalStations, stationLabel } from '../../core/config.js';
import { createPicker } from '@aiscatcher/ui/picker.js';
import * as cardTabs from '@aiscatcher/ui/stationcard-tabs.js';
import { flagHTML } from '@aiscatcher/ui/components.js';
import { stepChartSVG } from '@aiscatcher/core/spark.js';
import { getCountryName, getDeltaTimeVal } from '@aiscatcher/core/text.js';
import { paneCramped } from '@aiscatcher/map/mapui.js';
import { focus, switchFocus, refresh, onRefresh } from '../../data.js';
import { ships, clock } from '../../core/store.js';
import { settings } from '../../core/state.js';
import { getDistanceVal, getDistanceUnit, getLatValFormat, getLonValFormat } from '../../core/units.js';
import { closeDialog } from '../../dialog.js';
import { replaycardVisible, applyMenuLabels } from '../../panels.js';
import * as replaybar from '../replay/replaybar.js';
import * as mapObjects from '../mapobjects/mapobjects.js';
import * as range from '../range/range.js';
import { register } from '../../actions.js';

// { closeSearch(), onFocus(): another station is in focus, ui: the map UI the
//   card is placed by, cardLayout: the ship card's layout options, closeTargetcard(),
//   openStatistics(): the statistics tab }
let deps = { closeSearch: () => {}, onFocus: () => {}, closeTargetcard: () => {}, openStatistics: () => {} };

export function init(d) {
    deps = d;
    // the header gives the chip room or not; without it the ticker carries the station
    document.getElementById("header-focus")?.addEventListener("fit", placeFocusChip);
    if (d.ui) buildCard();
    onRefresh(refreshStationcard);
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
    shipsItem();
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

// a dropdown bound to the focus, in `host`: a pick there is a pick everywhere;
// `scope` draws it small, as the scope at the end of a tab's strip
export function mountFocusPicker(host, { scope = false } = {}) {
    const p = createPicker({ label: "Stations", value: focus, options: focusOptions, empty: "No station by that name",
        onPick: (id) => onStationChange(id) });
    if (scope) p.el.classList.add("pick--scope");
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
const HEADER_CLASS = { online: "card-validated", connected: "card-warning", offline: "card-dubious" };

const count = (n) => (Number(n) || 0).toLocaleString();
const set = (id, text) => { const el = document.getElementById(id); if (el) el.textContent = text; };
const onMap = (s) => Number.isFinite(s.lat) && Number.isFinite(s.lon) && Math.abs(s.lat) <= 90 && Math.abs(s.lon) <= 180;
const stationRow = (id) => mapObjects.stationsAll().find((x) => x.id === id);

/* The card a station opens, as the website's station card: its figures, a day
   of activity and the last half hour of messages, in the ship card's style.
   While it is open the page looks through that station, as the website shows
   that station's ships; the close goes back to what was in focus before. */
let card = null, cardStation = null, focusBefore = -1, cardLayout = null, cardTab = "summary", figuresAt = 0, shipsEl = null;

export function stationcardVisible() {
    return cardStation !== null;
}

function sectionDefaults() {
    const open = cardLayout !== null || !paneCramped();
    return { stats: true, activity: open, msgrate: open };
}

function buildCard() {
    card = deps.ui.createCard({ mount: document.getElementById("stationcard"), sectionAction: "toggleStationcardSection", layout: deps.cardLayout });
    card.el.addEventListener("card:section", () => card.fit());
    const item = (icon, label, title, action) => card.footer.add({ icon, label, title, action, group: "station" });
    shipsEl = item("directions_boat", "Ships", "Show only the vessels this station hears", "stationcardShips");
    item("leaderboard", "Statistics", "This station's figures", "stationcardStatistics");
    item("near_me", "Nearby", "Ships, stations and ports nearest this station", "stationcardNearby");
    card.footer.show("station");
    setStationcardStyle(settings.shipcard_style);
}

// the ship card's style, tabs or the classic fold, on this card too
export function setStationcardStyle(style) {
    if (!card) return;
    cardLayout?.destroy();
    cardLayout = null;
    const tabbed = style === "tabs";
    card.el.classList.toggle("card-tabs", tabbed);
    if (tabbed) {
        cardLayout = cardTabs.build(document.getElementById("stationcard_content"), "stationcard_", {
            tab: paneCramped() ? null : cardTab,
            foldable: () => paneCramped(),
            onTab: (t) => { if (t) cardTab = t; card.fit(); },
        });
        card.setMax(true);
    }
    card.section.reset(sectionDefaults());
    if (cardStation !== null) card.fit();
}

export async function openStation(id, pixel) {
    const s = stationRow(id);
    if (!s || !card) return;
    deps.ui.dismiss();
    deps.closeTargetcard();
    if (cardStation === null) focusBefore = focus;
    cardStation = id;
    fillRows(s);
    card.el.classList.add("visible");
    card.section.reset(sectionDefaults());
    cardLayout?.select(paneCramped() ? null : cardTab, false);
    card.open(pixel);
    if (offered(id) && hasStationChoice() && focus !== id) onStationChange(id);
    shipsItem();
    await fillFigures(id);
}

export function closeStationcard() {
    if (cardStation === null) return;
    const was = cardStation;
    cardStation = null;
    card.el.classList.remove("visible");
    if (focus === was && focusBefore !== was) onStationChange(focusBefore);
    focusBefore = -1;
}

// the name and the flag over the figures the map objects carry
function fillRows(s) {
    const header = document.getElementById("stationcard_header");
    document.getElementById("stationcard_header_title").textContent = (s.name || "Station " + s.id).toUpperCase();
    document.getElementById("stationcard_header_flag").innerHTML = s.country ? flagHTML(s.country, "flag-card", getCountryName(s.country)) : "";
    header.classList.remove("card-validated", "card-warning", "card-dubious");
    header.classList.add(HEADER_CLASS[s.status] || "card-dubious");
    set("stationcard_station_id", s.id);
    set("stationcard_status", STATUS_TEXT[s.status] || "No data yet");
    set("stationcard_ago", s.heard > 0 ? getDeltaTimeVal(clock - s.heard) + " ago" : "-");
    const aboard = s.mmsi ? ships[s.mmsi]?.raw?.shipname || "MMSI " + s.mmsi : null;
    set("stationcard_where_label", aboard ? "On board" : "Position");
    set("stationcard_where", aboard || (onMap(s) ? getLatValFormat(s) + "  " + getLonValFormat(s) : "-"));
}

const json = (url) => fetch(url).then((r) => (r.ok ? r.json() : null)).catch(() => null);

// the station's own figures and histories from the server
async function fillFigures(id) {
    figuresAt = Date.now();
    const [stat, history] = await Promise.all([json("api/stat.json?stations=" + id), json("api/history_full.json?stations=" + id)]);
    if (cardStation !== id) return;
    set("stationcard_ships", stat ? count(stat.vessel_count) : "-");
    set("stationcard_messages", stat ? count(stat.session?.count) : "-");
    set("stationcard_rate", stat ? (Number(stat.msg_rate) || 0).toFixed(1) + " msg/s" : "-");
    set("stationcard_furthest", stat?.session?.dist > 0 ? getDistanceVal(stat.session.dist) + " " + getDistanceUnit() : "-");
    chart("activity", "stationcard_timeline", history?.hour, 24, "msg/h", "24 h of messages", "Messages per hour over the last 24 hours");
    chart("msgrate", "stationcard_msgrate", history?.minute, 30, "msg/min", "30 min of messages", "Messages per minute over the last 30 minutes");
    card.fit();
}

// the last `n` buckets of a history as the step chart the website draws
function chart(section, id, src, n, unit, foot, aria) {
    const pts = src && Array.isArray(src.stat) && Array.isArray(src.time)
        ? src.time.map((t, i) => ({ t: src.now + t * src.interval, v: Number(src.stat[i]?.count) || 0 })).sort((a, b) => a.t - b.t).slice(-n)
        : [];
    const ok = pts.length >= 2;
    card.section.available(section, ok);
    document.getElementById(id).innerHTML = ok
        ? stepChartSVG(pts, unit, foot, "now \u00b7 " + Math.round(pts[pts.length - 1].v) + " " + unit, aria, (v) => String(Math.round(v)))
        : "";
}

// every refresh: the rows from the map objects, the figures every half minute
function refreshStationcard() {
    if (cardStation === null) return;
    const s = stationRow(cardStation);
    if (!s) { closeStationcard(); return; }
    fillRows(s);
    if (Date.now() - figuresAt > 30000) fillFigures(cardStation);
}

// the footer's Ships item: the station's vessels alone, or all of them again
function shipsItem() {
    if (!shipsEl) return;
    const can = cardStation !== null && offered(cardStation) && hasStationChoice();
    shipsEl.style.display = can ? "" : "none";
    const span = shipsEl.querySelector("span");
    if (span) span.textContent = focus === cardStation ? "All ships" : "Ships";
}

register({
    selectStation: (e, d) => selectStation(d.id),
    closeStationcard: () => closeStationcard(),
    toggleStationcardSection: (e, dataset, el) => { e.stopPropagation(); card?.section.toggle(el?.dataset.section || dataset.section); },
    stationcardShips: () => {
        if (cardStation === null) return;
        onStationChange(focus === cardStation ? (focusBefore === cardStation ? -1 : focusBefore) : cardStation);
    },
    stationcardStatistics: () => {
        if (cardStation !== null && focus !== cardStation && offered(cardStation)) onStationChange(cardStation);
        deps.openStatistics();
    },
    stationcardNearby: () => {
        const s = cardStation !== null ? stationRow(cardStation) : null;
        if (s && onMap(s)) mapObjects.openNearby({ lat: s.lat, lon: s.lon, title: s.name || "Station " + s.id, country: s.country });
    },
});
