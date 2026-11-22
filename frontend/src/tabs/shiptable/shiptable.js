import { getShipDimension, getDimUnit, getDraughtVal, getDistanceVal, getDistanceUnit, getLatValFormat, getLonValFormat, getSpeedVal, getSpeedUnit } from '../../core/units.js';
import { getCountryName, getStatusVal, getMmsiTypeVal, getDeltaTimeVal, getEtaVal, getStringfromChannels, getStringfromMsgType, getStringfromGroup, sanitizeString } from '@aiscatcher/core/text.js';
import { getShipName, getCallSign } from '../../core/names.js';
import { stationNames } from '../../core/config.js';
import { flagHTML } from '@aiscatcher/ui/components.js';
import * as filter from '../../core/filter.js';
import { ships, clock } from '../../core/store.js';
import { fetchShips, fetchTableRows } from '../../data.js';
import { getTableShiptype } from '../../map.js';
import { saveSettings } from '../../settings-store.js';
import { settings } from '../../core/state.js';
import { selectMapTab } from '../index.js';
import { register } from '../../actions.js';
import { showContextMenu } from '../../features/contextmenu/contextmenu.js';

const SIZES = [20, 50, 100];
const DEFAULT_COLUMNS = ["shipname", "mmsi", "imo", "callsign", "shipclass", "lat", "lon", "last_signal", "level", "distance", "bearing", "speed", "repeat", "ppm", "status"];
const RESET_ICON = '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="1 4 1 10 7 10"/><path d="M3.51 15a9 9 0 1 0 2.13-9.36L1 10"/></svg>';

let host = null, head, body, foot, menu;
let rows = [], shown = [];
let page = 0, size = 50;

const valid = (ship, yes, no, pending) => (ship.validated == 1 ? yes : ship.validated == -1 ? no : pending);

function matches(data, query) {
    return (data.shipname || "").toLowerCase().includes(query) ||
        String(data.mmsi ?? "").includes(query) ||
        (data.callsign || "").toLowerCase().includes(query) ||
        String(data.shipclass ?? "").includes(query) ||
        (data.last_signal != null && getDeltaTimeVal(clock - data.last_signal).includes(query)) ||
        (data.count != null && data.count.toString().includes(query)) ||
        (data.ppm != null && data.ppm.toString().includes(query)) ||
        (data.level != null && data.level.toString().includes(query)) ||
        (data.distance != null && getDistanceVal(data.distance).includes(query)) ||
        (data.bearing != null && data.bearing.toString().includes(query)) ||
        (data.lat != null && getLatValFormat(data).includes(query)) ||
        (data.lon != null && getLonValFormat(data).includes(query)) ||
        (data.speed != null && getSpeedVal(data.speed).toString().includes(query)) ||
        (data.validated != null && valid(data, "yes", "no", "pending").includes(query)) ||
        (data.channels != null && getStringfromChannels(data.channels).includes(query)) ||
        (data.group_mask != null && getStringfromGroup(data.group_mask).includes(query)) ||
        stationNames(data.stations).toLowerCase().includes(query);
}

function padEni(v) {
    v = (v || "").trim();
    return /^\d{1,7}$/.test(v) ? v.padStart(8, "0") : v;
}

function etaSortValue(ship) {
    return ship.eta_month != null && ship.eta_day != null && ship.eta_hour != null && ship.eta_minute != null
        ? ((ship.eta_month * 31 + ship.eta_day) * 24 + ship.eta_hour) * 60 + ship.eta_minute
        : -1;
}

function dimSortValue(ship) {
    return ship.to_bow != null && ship.to_stern != null ? ship.to_bow + ship.to_stern : -1;
}

const yes = (b) => (b ? "Yes" : "No");
const fixed = (v, n) => (v != null ? Number(v).toFixed(n) : "");
const position = (ship, text) => `<div class="${valid(ship, "status-ok", "status-bad", "")}">${text}</div>`;

const draught = (s) => (s.draught ? getDraughtVal(s.draught) : "");
const distance = (s) => (s.distance != null ? getDistanceVal(s.distance) : "");
const speed = (s) => (s.speed != null ? getSpeedVal(s.speed) : "");

// [title, field, shown, sorted by, exported]: without the last the CSV takes the field as it is
const columns = () => [
    ["Shipname", "shipname", (s) => flagHTML(s.country, 'flag-inline', getCountryName(s.country)) + sanitizeString(String(getShipName(s) || s.mmsi))],
    ["MMSI", "mmsi"],
    ["IMO", "imo"],
    ["ENI", "eni", (s) => sanitizeString(padEni(s.eni)), (s) => padEni(s.eni)],
    ["Dest", "destination", (s) => sanitizeString(s.destination || "")],
    ["ETA", "eta", (s) => (etaSortValue(s) >= 0 ? getEtaVal(s) : ""), etaSortValue],
    ["Callsign", "callsign", (s) => sanitizeString(getCallSign(s) || "")],
    ["Flag", "country", (s) => getCountryName(s.country)],
    ["Status", "status", getStatusVal],
    ["Type", "shipclass", (s) => `<div class="cell-center">${getTableShiptype(s)}</div>`],
    ["Class", "class", getMmsiTypeVal, getMmsiTypeVal],
    ["Dim", "dimension", (s) => getShipDimension(s) || "", dimSortValue],
    [`Draught (${getDimUnit()})`, "draught", draught, undefined, draught],
    ["Last", "last_signal", (s) => (s.last_signal != null ? getDeltaTimeVal(clock - s.last_signal) : "")],
    ["Count", "count"],
    ["PPM", "ppm", (s) => fixed(s.ppm, 1)],
    ["RSSI (dB)", "level", (s) => fixed(s.level, 1)],
    [`Dist (${getDistanceUnit()})`, "distance", (s) => (s.distance != null ? distance(s) + (s.repeat > 0 ? " (R)" : "") : ""), undefined, distance],
    ["Brg", "bearing", (s) => (s.bearing != null ? Number(s.bearing).toFixed(0) + "&deg;" : "")],
    ["Station", "stations", (s) => sanitizeString(stationNames(s.stations)), (s) => stationNames(s.stations)],
    ["Lat", "lat", (s) => position(s, s.lat != null ? getLatValFormat(s) : "")],
    ["Lon", "lon", (s) => position(s, s.lon != null ? getLonValFormat(s) : "")],
    [`Spd (${getSpeedUnit()})`, "speed", (s) => (s.speed ? speed(s) : ""), undefined, speed],
    ["Valid", "validated", (s) => valid(s, "Yes", "No", "Pending")],
    ["Repeated", "repeat", (s) => yes(s.repeat == 1)],
    ["Ch", "channels", (s) => getStringfromChannels(s.channels)],
    ["Msg Type", "msg_type", (s) => getStringfromMsgType(s.msg_type)],
    ...[6, 8, 27].map((t) => ["MSG" + t, "MSG" + t, (s) => yes(s.msg_type & (1 << t)), (s) => (s.msg_type >> t) & 1]),
    ["Src", "group_mask", (s) => getStringfromGroup(s.group_mask)],
];

const visible = () => columns().filter(([, field]) => settings.shiptable_columns.includes(field));
const text = ([, field, show], ship) => (show ? show(ship) : ship[field]) ?? "";

function compare(a, b) {
    const none = (v) => v == null || v === "";
    if (none(a) || none(b)) return none(b) - none(a);
    return typeof a === "number" && typeof b === "number" ? a - b : String(a).localeCompare(String(b));
}

function listed() {
    const query = document.getElementById("shipSearch").value.toLowerCase(), sort = settings.shiptable_sort;
    const list = query ? rows.filter((s) => matches(s, query)) : rows.slice();
    const [, field, , by] = (sort && columns().find((c) => c[1] === sort.field)) || [];
    if (field) list.sort((a, b) => sort.dir * compare(by ? by(a) : a[field], by ? by(b) : b[field]));
    return list;
}

const drawn = new WeakMap();
const fill = (el, html) => {
    if (drawn.get(el) === html) return;
    drawn.set(el, html);
    el.innerHTML = html;
};

function render() {
    if (!host) return;
    const cols = visible(), list = listed(), sort = settings.shiptable_sort || {};
    const pages = Math.max(1, Math.ceil(list.length / size));
    page = Math.min(page, pages - 1);
    shown = list.slice(page * size, (page + 1) * size);

    fill(head, "<tr>" + cols.map(([title, field]) =>
        `<th class="col-header${sort.field === field ? (sort.dir > 0 ? " sort-up" : " sort-down") : ""}" data-field="${field}">${title}</th>`).join("") + "</tr>");
    body.innerHTML = shown.map((ship, i) =>
        `<tr data-row="${i}" class="${valid(ship, "row-ok", "row-bad", "")}">` + cols.map((c) => `<td>${text(c, ship)}</td>`).join("") + "</tr>").join("");

    const from = Math.max(0, Math.min(page - 2, pages - 5)), last = pages - 1;
    const button = (to, label, attr) => `<button type="button" data-page="${to}"${attr}>${label}</button>`;
    const off = (there) => (page === there ? " disabled" : "");
    fill(foot, `<label>Page Size <select>${SIZES.map((n) => `<option${n === size ? " selected" : ""}>${n}</option>`).join("")}</select></label>` +
        button(0, "First", off(0)) + button(page - 1, "Prev", off(0)) +
        Array.from({ length: Math.min(5, pages) }, (x, i) => button(from + i, from + i + 1, from + i === page ? ' class="active"' : "")).join("") +
        button(page + 1, "Next", off(last)) + button(last, "Last", off(last)));
}

function drawMenu() {
    menu.innerHTML = '<div class="col-header column-menu-header">Show columns</div><div class="column-menu-list">' +
        columns().map(([title, field]) =>
            `<label class="ship-table-column-dropdown-item"><input type="checkbox" value="${field}"${settings.shiptable_columns.includes(field) ? " checked" : ""}><span>${title}</span></label>`).join("") +
        `</div><div class="column-menu-footer"><button type="button" class="btn-quiet column-menu-reset">${RESET_ICON}<span>Reset to default</span></button></div>`;
}

function setColumns(fields) {
    settings.shiptable_columns = fields;
    saveSettings();
    render();
}

function build() {
    host = document.getElementById("shipTable");
    host.innerHTML = '<div class="ship-table-scroll"><table class="ship-table"><thead></thead><tbody></tbody></table></div><div class="ship-table-foot"></div>';
    [head, body, foot] = ["thead", "tbody", ".ship-table-foot"].map((q) => host.querySelector(q));
    const ship = (e) => shown[e.target.closest("tr[data-row]")?.dataset.row];

    head.addEventListener("click", (e) => {
        const field = e.target.closest("th")?.dataset.field, sort = settings.shiptable_sort;
        if (!field) return;
        settings.shiptable_sort = { field, dir: sort && sort.field === field ? -sort.dir : 1 };
        saveSettings();
        render();
    });
    body.addEventListener("click", (e) => {
        const s = ship(e);
        if (s && s.lat != null && s.lon != null) selectMapTab(s.mmsi);
    });
    body.addEventListener("contextmenu", (e) => {
        const s = ship(e);
        if (s) showContextMenu(e, s.mmsi, "ship", ["settings", "ship", "table-menu"]);
    });
    foot.addEventListener("click", (e) => {
        const to = e.target.closest("button")?.dataset.page;
        if (to === undefined) return;
        page = Number(to);
        render();
    });
    foot.addEventListener("change", (e) => {
        size = Number(e.target.value);
        page = 0;
        render();
    });
    document.getElementById("shipSearch").addEventListener("input", () => {
        page = 0;
        render();
    });

    const toggle = document.getElementById("shipTableColumnDropdownToggle");
    menu = document.getElementById("shipTableColumnVisibilityMenu");
    // Reparent the menu so the table container doesn't clip it.
    document.body.appendChild(menu);
    toggle.addEventListener("click", (e) => {
        e.stopPropagation();
        const open = menu.style.display !== "block", at = toggle.getBoundingClientRect();
        if (open) drawMenu();
        menu.style.top = at.bottom + window.scrollY + "px";
        menu.style.right = window.innerWidth - at.right + "px";
        menu.style.display = open ? "block" : "none";
    });
    document.addEventListener("click", (e) => {
        if (!menu.contains(e.target)) menu.style.display = "none";
    });
    menu.addEventListener("change", () => setColumns([...menu.querySelectorAll("input:checked")].map((c) => c.value)));
    menu.addEventListener("click", (e) => {
        if (!e.target.closest(".column-menu-reset")) return;
        setColumns(DEFAULT_COLUMNS);
        drawMenu();
    });
}

export async function update() {
    const ok = await fetchShips();
    if (!ok) return;
    await fetchTableRows();

    rows = Object.values(ships).filter(filter.visible).map((e) => e.raw);
    if (!host) build();
    render();
}

function downloadCSV() {
    const cols = visible(), plain = document.createElement("div");
    const cell = (v) => '"' + String(v ?? "").replace(/"/g, '""') + '"';
    const shownAs = (c, ship) => {
        plain.innerHTML = text(c, ship);
        return plain.textContent;
    };
    const value = (c, ship) => (c[4] ? c[4](ship) : ship[c[1]] !== undefined ? ship[c[1]] : shownAs(c, ship));
    const lines = [cols.map((c) => cell(c[0])), ...listed().map((ship) => cols.map((c) => cell(value(c, ship))))];
    const link = document.createElement("a");
    link.href = URL.createObjectURL(new Blob([lines.map((l) => l.join(",")).join("\n")], { type: "text/csv" }));
    link.download = "data.csv";
    link.click();
    URL.revokeObjectURL(link.href);
}

export const reset = render;

register({ downloadCSV });
