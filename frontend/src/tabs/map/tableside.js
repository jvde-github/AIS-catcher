import { settings } from '../../settings.js';
import { showContextMenu } from '../../ui.js';
import {
    getShipName, getDistanceVal, getDistanceUnit,
    getSpeedVal, getSpeedUnit, getDeltaTimeVal,
    getFlagStyled, getTableShiptype, ShippingClass,
} from '../../format.js';
import { shipsDB } from './state.js';

let deps = {};

// ── Sort helpers ──────────────────────────────────────────────────────────────

function compareNumber(valueA, valueB) {
    if (valueA == null && valueB == null) return settings.tableside_order === "ascending" ? 1 : -1;
    if (valueA == null) return settings.tableside_order === "ascending" ? 1 : -1;
    if (valueB == null) return settings.tableside_order === "ascending" ? -1 : 1;
    return valueA - valueB;
}

function compareString(valueA, valueB) {
    if (valueA == null && valueB == null) return 0;
    if (valueA == null) return 1;
    if (valueB == null) return -1;
    return (valueA + "").localeCompare(valueB + "");
}

// ── Sort column header UI ─────────────────────────────────────────────────────

export function updateTableSort(event) {
    const header = event.currentTarget;
    const column = header.getAttribute("data-column");
    const currentOrder = header.classList.contains("ascending") ? "ascending" : "descending";
    const newOrder = currentOrder === "descending" ? "ascending" : "descending";

    settings.tableside_column = column;
    settings.tableside_order = newOrder;

    deps.saveSettings();
    updateSortMarkers();
    updateTablecard();
}

export function updateSortMarkers() {
    const allHeaders = document.querySelectorAll("[data-column]");
    allHeaders.forEach((header) => {
        header.classList.remove("ascending", "descending");
        if (header.getAttribute("data-column") === settings.tableside_column) {
            header.classList.add(settings.tableside_order);
        }
    });
}

// ── Marker count & statcard ───────────────────────────────────────────────────

function flashNumber(id, newValue) {
    const element = document.getElementById(id);
    const oldValue = parseInt(element.innerText) || 0;
    if (newValue != oldValue) element.classList.add("flash-up");
    element.innerText = newValue;
    setTimeout(() => element.classList.remove("flash-up"), 500);
}

export function updateMarkerCount() {
    const count = shipsDB != null ? Object.keys(shipsDB).length : 0;
    flashNumber("markerCount", count);
    if (document.getElementById("statcard").style.display === "block") updateMarkerCountTooltip();
}

export function updateMarkerCountTooltip() {
    if (shipsDB == null) {
        ["statcard_stationary", "statcard_moving", "statcard_class_b_stationary", "statcard_class_b_moving",
         "statcard_station", "statcard_aton", "statcard_heli", "statcard_sarte"
        ].forEach(id => { document.getElementById(id).innerHTML = ""; });
        return;
    }

    let cStationary = 0, cMoving = 0, cClassBstationary = 0, cClassBmoving = 0,
        cStation = 0, cAton = 0, cHeli = 0, cSarte = 0;

    for (const key of Object.keys(shipsDB)) {
        const ship = shipsDB[key].raw;
        switch (ship.shipclass) {
            case ShippingClass.ATON:     cAton++;    break;
            case ShippingClass.PLANE:    cHeli++;    break;
            case ShippingClass.HELICOPTER: cHeli++;  break;
            case ShippingClass.STATION:  cStation++; break;
            case ShippingClass.SARTEPIRB: cSarte++;  break;
            case ShippingClass.B:
                if (ship.speed != null && ship.speed > 0.5) cClassBmoving++;
                else cClassBstationary++;
                break;
            default:
                if (ship.speed != null && ship.speed > 0.5) cMoving++;
                else cStationary++;
        }
    }

    flashNumber("statcard_stationary", cStationary);
    flashNumber("statcard_moving", cMoving);
    flashNumber("statcard_station", cStation);
    flashNumber("statcard_aton", cAton);
    flashNumber("statcard_heli", cHeli);
    flashNumber("statcard_sarte", cSarte);
    flashNumber("statcard_class_b_stationary", cClassBstationary);
    flashNumber("statcard_class_b_moving", cClassBmoving);
}

export function toggleStatcard() {
    const el = document.getElementById("statcard");
    if (el.style.display === "block") hideStatcard();
    else showStatcard();
}

function showStatcard() {
    updateMarkerCountTooltip();
    document.getElementById("statcard").style.display = "block";
}

function hideStatcard() {
    document.getElementById("statcard").style.display = "none";
}

// ── Tableside panel ───────────────────────────────────────────────────────────

export function updateTablecard() {
    if (!document.getElementById("tableside").classList.contains("active")) return;

    const tableBody = document.getElementById("tablecardBody");
    tableBody.innerHTML = "";

    if (shipsDB == null) return;

    const shipKeys = Object.keys(shipsDB);
    const column = settings.tableside_column;
    const order = settings.tableside_order;

    const sortFunctions = {
        flag:        (a, b) => compareString(shipsDB[a].raw.country, shipsDB[b].raw.country),
        shipname:    (a, b) => compareString(getShipName(shipsDB[a].raw), getShipName(shipsDB[b].raw)),
        distance:    (a, b) => compareNumber(shipsDB[a].raw.distance, shipsDB[b].raw.distance),
        speed:       (a, b) => compareNumber(shipsDB[a].raw.speed, shipsDB[b].raw.speed),
        type:        (a, b) => compareNumber(shipsDB[a].raw.shipclass, shipsDB[b].raw.shipclass),
        last_signal: (a, b) => compareNumber(shipsDB[a].raw.last_signal, shipsDB[b].raw.last_signal),
    };

    if (column in sortFunctions) {
        shipKeys.sort((a, b) => {
            const result = sortFunctions[column](a, b);
            return order === "ascending" ? result : -result;
        });
    }

    const filter = document.getElementById('shipSearchSide').value.toLowerCase();
    const rows = [];
    let addedRows = 0;

    for (let i = 0; i < shipKeys.length && addedRows < 200; i++) {
        const key = shipKeys[i];
        if (!(key in shipsDB)) continue;
        const ship = shipsDB[key].raw;
        const shipName = String(getShipName(ship) || ship.mmsi);
        if (filter && !shipName.toLowerCase().includes(filter)) continue;

        const dist      = ship.distance ? (getDistanceVal(ship.distance) + (ship.repeat > 0 ? " (R)" : "")) : "";
        const distTitle = ship.distance != null ? getDistanceVal(ship.distance) + " " + getDistanceUnit() + (ship.repeat > 0 ? " (R)" : "") : "";
        const spd       = ship.speed != null ? getSpeedVal(ship.speed) : "";
        const spdTitle  = ship.speed != null ? getSpeedVal(ship.speed) + " " + getSpeedUnit() : "";

        rows.push(`<tr data-mmsi="${ship.mmsi}">` +
            `<td>${getFlagStyled(ship.country, "padding:0;margin:0;box-shadow:1px 1px 2px rgba(0,0,0,0.2);font-size:16px;")}</td>` +
            `<td>${shipName}</td>` +
            `<td title="${distTitle}">${dist}</td>` +
            `<td title="${spdTitle}">${spd}</td>` +
            `<td>${getTableShiptype(ship)}</td>` +
            `<td>${getDeltaTimeVal(ship.last_signal)}</td>` +
            `</tr>`);
        addedRows++;
    }

    tableBody.innerHTML = rows.join('');

    tableBody.onmouseover = function (e) {
        const tr = e.target.closest('tr[data-mmsi]');
        if (tr) deps.startHover('ship', parseInt(tr.dataset.mmsi));
    };
    tableBody.onmouseout = function () { deps.stopHover(); };
    tableBody.onclick = function (e) {
        const tr = e.target.closest('tr[data-mmsi]');
        if (tr) deps.showShipcard('ship', parseInt(tr.dataset.mmsi));
    };
    tableBody.oncontextmenu = function (e) {
        const tr = e.target.closest('tr[data-mmsi]');
        if (tr) showContextMenu(e, parseInt(tr.dataset.mmsi), "ship", ["object", "object-map"]);
    };
}

export function toggleTablecard() {
    if (!document.getElementById("tableside").classList.contains("active") && window.innerWidth < 800) {
        settings.tab = "ships";
        deps.selectTab();
        return;
    }

    document.getElementById("tableside").classList.toggle("active");
    document.querySelectorAll(".map-button-box").forEach(el => el.classList.toggle("active"));
    updateTablecard();
}

export function hideTablecard() {
    if (document.getElementById("tableside").classList.contains("active")) {
        toggleTablecard();
    }
}

export function closeTableSide() {
    document.querySelector(".tableside_window").classList.remove("active");
}

// ── Public API ────────────────────────────────────────────────────────────────

export function init(dependencies) {
    deps = dependencies;

    document.getElementById('shipSearchSide').addEventListener('input', updateTablecard);

    document.querySelectorAll("[data-column]").forEach(header => {
        header.addEventListener("click", updateTableSort);
    });
}
