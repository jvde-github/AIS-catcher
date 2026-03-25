import { registerActions } from '../events.js';
import { settings, persist, onSettingsChange } from '../settings.js';
import { showContextMenu } from '../ui.js';
import {
    sanitizeString, includeShip,
    getDistanceVal, getDistanceUnit, getSpeedVal, getSpeedUnit,
    getDimVal, getDimUnit, getShipDimension,
    getDeltaTimeVal, getLatValFormat, getLonValFormat,
    getEtaVal, getShipName, getCallSign,
    getFlag, getCountryName,
    getTableShiptype, getStringfromChannels, getStringfromGroup, getStringfromMsgType,
    getStatusVal, getTypeVal,
} from '../format.js';

let deps = {};
let table = null;
let tableFirstTime = true;

// ── Ship data fetch (independent of map's shipsDB) ───────────────────────────

const SHIP_KEYS = [
    "mmsi", "lat", "lon", "distance", "bearing", "level", "count", "ppm",
    "approx", "heading", "cog", "speed", "to_bow", "to_stern", "to_starboard", "to_port",
    "last_group", "group_mask", "shiptype", "mmsi_type", "shipclass", "msg_type",
    "country", "status", "draught", "eta_month", "eta_day", "eta_hour", "eta_minute",
    "imo", "callsign", "shipname", "destination", "last_signal",
    "flags", "validated", "channels", "altitude", "received_stations",
];

async function fetchShipData() {
    try {
        const response = await fetch("api/ships_array.json?receiver=" + deps.getActiveReceiver());
        const ships = await response.json();

        const data = [];
        ships.values.forEach((v) => {
            const s = Object.fromEntries(SHIP_KEYS.map((k, i) => [k, v[i]]));

            const flags = s.flags;
            s.validated2   = (flags & 3) === 2 ? -1 : flags & 3;
            s.repeat       = (flags >> 2)  & 3;
            s.virtual_aid  = (flags >> 4)  & 1;
            s.approximate  = (flags >> 5)  & 1;
            s.channels2    = (flags >> 6)  & 0b1111;
            s.cs_unit      = (flags >> 10) & 3;
            s.raim         = (flags >> 12) & 3;
            s.dte          = (flags >> 14) & 3;
            s.assigned     = (flags >> 16) & 3;
            s.display      = (flags >> 18) & 3;
            s.dsc          = (flags >> 20) & 3;
            s.band         = (flags >> 22) & 3;
            s.msg22        = (flags >> 24) & 3;
            s.off_position = (flags >> 26) & 3;
            s.maneuver     = (flags >> 28) & 3;

            if (includeShip(s)) {
                s.shipname = sanitizeString(s.shipname);
                s.callsign = sanitizeString(s.callsign);
                data.push(s);
            }
        });
        return data;
    } catch (e) {
        console.error("Failed fetching ships for table:", e);
        return null;
    }
}

// ── Tabulator table ───────────────────────────────────────────────────────────

export async function update() {
    const data = await fetchShipData();
    if (!data) return;

    if (table === null) {
        table = new Tabulator("#shipTable", {
            index: "mmsi",
            rowFormatter: function (row) {
                const ship = row.getData();
                const borderColor = ship.validated === 1 ? "#7CFC00" : ship.validated === -1 ? "red" : "lightgrey";
                row.getElement().style.borderLeft = `10px solid ${borderColor}`;
            },
            data,
            layout: "fitDataTable",
            persistence: { sort: true },
            pagination: "local",
            paginationSize: 50,
            rowHeight: 37,
            paginationSizeSelector: [20, 50, 100],
            columns: [
                {
                    title: "Shipname", field: "shipname", sorter: "string",
                    formatter: (cell) => {
                        const ship = cell.getRow().getData();
                        return getFlag(ship.country) + getShipName(ship);
                    },
                },
                { title: "MMSI",  field: "mmsi",        sorter: "number" },
                { title: "IMO",   field: "imo",         sorter: "number" },
                { title: "Dest",  field: "destination", sorter: "string" },
                {
                    title: "ETA", field: "eta", sorter: "string",
                    formatter: (cell) => {
                        const s = cell.getRow().getData();
                        return s.eta_month != null && s.eta_hour != null && s.eta_day != null && s.eta_minute != null
                            ? getEtaVal(s) : null;
                    },
                },
                {
                    title: "Callsign", field: "callsign", sorter: "string",
                    formatter: (cell) => { const s = cell.getRow().getData(); return s ? getCallSign(s) : ""; },
                },
                {
                    title: "Flag", field: "country", sorter: "string",
                    formatter: (cell) => { const s = cell.getRow().getData(); return s ? getCountryName(s.country) : ""; },
                },
                {
                    title: "Status", field: "status", sorter: "string",
                    formatter: (cell) => { const s = cell.getRow().getData(); return s ? getStatusVal(s) : ""; },
                },
                {
                    title: "Type", field: "shipclass", sorter: "number",
                    formatter: (cell) => getTableShiptype(cell.getRow().getData()),
                },
                {
                    title: "Class", field: "class", sorter: "string",
                    formatter: (cell) => getTypeVal(cell.getRow().getData()),
                },
                {
                    title: "Dim", field: "dimension", sorter: "number",
                    formatter: (cell) => { const s = cell.getRow().getData(); return s ? getShipDimension(s) || "" : ""; },
                },
                {
                    title: "Draught", field: "draught", sorter: "number",
                    formatter: (cell) => {
                        const s = cell.getRow().getData();
                        return s.draught ? getDimVal(s.draught) + " " + getDimUnit() : null;
                    },
                },
                {
                    title: "Last", field: "last_signal", sorter: "number",
                    formatter: (cell) => { const v = cell.getValue(); return v != null ? getDeltaTimeVal(v) : ""; },
                },
                { title: "Count", field: "count", sorter: "number" },
                {
                    title: "PPM", field: "ppm", sorter: "number",
                    formatter: (cell) => { const v = cell.getValue(); return v != null ? Number(v).toFixed(1) : ""; },
                },
                {
                    title: "RSSI", field: "level", sorter: "number",
                    formatter: (cell) => { const v = cell.getValue(); return v != null ? Number(v).toFixed(1) : ""; },
                },
                {
                    title: "Dist", field: "distance", sorter: "number",
                    formatter: (cell) => {
                        const s = cell.getRow().getData();
                        return s != null && s.distance != null
                            ? getDistanceVal(s.distance) + (s.repeat > 0 ? " (R)" : "") : "";
                    },
                },
                {
                    title: "Brg", field: "bearing", sorter: "number",
                    formatter: (cell) => { const v = cell.getValue(); return v != null ? Number(v).toFixed(0) + "&deg;" : ""; },
                },
                {
                    title: "Lat", field: "lat", sorter: "number",
                    formatter: (cell) => {
                        const s = cell.getRow().getData();
                        const color = s.validated === 1 ? "green" : s.validated === -1 ? "red" : "inherited";
                        return `<div style='color:${color}'>${s.lat != null ? getLatValFormat(s) : ""}</div>`;
                    },
                },
                {
                    title: "Lon", field: "lon", sorter: "number",
                    formatter: (cell) => {
                        const s = cell.getRow().getData();
                        const color = s.validated === 1 ? "green" : s.validated === -1 ? "red" : "inherited";
                        return `<div style='color:${color}'>${s.lon != null ? getLonValFormat(s) : ""}</div>`;
                    },
                },
                {
                    title: "Spd", field: "speed", sorter: "number",
                    formatter: (cell) => { const v = cell.getValue(); return v ? getSpeedVal(v) + " " + getSpeedUnit() : null; },
                },
                {
                    title: "Valid", field: "validated", sorter: "string",
                    formatter: (cell) => {
                        const v = cell.getValue();
                        return v === 1 ? "Yes" : v === -1 ? "No" : "Pending";
                    },
                },
                {
                    title: "Repeated", field: "repeat", sorter: "string",
                    formatter: (cell) => cell.getValue() === 1 ? "Yes" : "No",
                },
                {
                    title: "Ch", field: "channels", sorter: "number",
                    formatter: (cell) => getStringfromChannels(cell.getValue()),
                },
                {
                    title: "Msg Type", field: "msg_type", sorter: "number",
                    formatter: (cell) => getStringfromMsgType(cell.getValue()),
                },
                {
                    title: "MSG6",  field: "MSG6",  sorter: "number",
                    formatter: (cell) => cell.getValue() & (1 << 6)  ? "Yes" : "No",
                },
                {
                    title: "MSG8",  field: "MSG8",  sorter: "number",
                    formatter: (cell) => cell.getValue() & (1 << 8)  ? "Yes" : "No",
                },
                {
                    title: "MSG27", field: "MSG27", sorter: "number",
                    formatter: (cell) => cell.getValue() & (1 << 27) ? "Yes" : "No",
                },
                {
                    title: "Src", field: "group_mask", sorter: "number",
                    formatter: (cell) => getStringfromGroup(cell.getValue()),
                },
            ],
        });

        table.on("rowContext", (e, row) => {
            showContextMenu(e, row.getData().mmsi, "ship", ["settings", "ship", "table-menu"]);
        });
        table.on("rowClick", (_e, row) => {
            const d = row.getData();
            if (d.lat != null && d.lon != null) deps.selectMapTab(d.mmsi);
        });
        table.on("tableBuilt", () => {
            if (tableFirstTime) { setShipTableColumns(); tableFirstTime = false; }
        });
    } else {
        const sorters = table.getSorters();
        table.replaceData(data);
        table.setSort(sorters);
    }
}

// ── Column visibility ─────────────────────────────────────────────────────────

function populateShipTableColumnVisibilityMenu() {
    const menu = document.getElementById("shipTableColumnVisibilityMenu");
    menu.innerHTML = "";
    table.getColumns().forEach((column) => {
        const checkboxId = `checkbox-${column.getField()}`;
        const item = document.createElement("div");
        item.classList.add("ship-table-column-dropdown-item");

        const checkbox = document.createElement("input");
        checkbox.type    = "checkbox";
        checkbox.id      = checkboxId;
        checkbox.value   = column.getField();
        checkbox.checked = column.isVisible();
        checkbox.addEventListener("change", updateShipTableColumnVisibility);

        const label = document.createElement("label");
        label.setAttribute("for", checkboxId);
        label.textContent = column.getDefinition().title;

        item.appendChild(checkbox);
        item.appendChild(label);
        menu.appendChild(item);
    });
}

function updateShipTableColumnVisibility() {
    settings.shiptable_columns = Array.from(
        document.querySelectorAll("#shipTableColumnVisibilityMenu input[type='checkbox']")
    ).filter(cb => cb.checked).map(cb => cb.value);
    setShipTableColumns();
}

export function resetShipTableColumns() {
    settings.shiptable_columns = ["shipname", "mmsi", "imo", "callsign", "shipclass", "lat", "lon",
        "last_signal", "level", "distance", "bearing", "speed", "repeat", "ppm", "status"];
    setShipTableColumns();
}

function setShipTableColumns() {
    deps.saveSettings();
    const updatedColumns = table.getColumns().map(col => ({
        ...col.getDefinition(),
        visible: settings.shiptable_columns.includes(col.getField()),
    }));
    table.setColumns(updatedColumns);
    populateShipTableColumnVisibilityMenu();
}

// ── Column dropdown ───────────────────────────────────────────────────────────

function toggleDropdown() {
    const dropdownToggle = document.getElementById("shipTableColumnDropdownToggle");
    const dropdownMenu   = document.getElementById("shipTableColumnVisibilityMenu");
    if (dropdownMenu.style.display === "block") {
        dropdownMenu.style.display = "none";
        return;
    }
    if (table) populateShipTableColumnVisibilityMenu();
    const rect = dropdownToggle.getBoundingClientRect();
    dropdownMenu.style.top     = (rect.bottom + window.scrollY) + "px";
    dropdownMenu.style.right   = (window.innerWidth - rect.right) + "px";
    dropdownMenu.style.display = "block";
}

// ── CSV download ──────────────────────────────────────────────────────────────

function downloadCSV() {
    if (table) table.download("csv", "data.csv");
}

// ── Search filter ─────────────────────────────────────────────────────────────

function customShipFilter(data, { query }) {
    const q = query.toLowerCase();
    return data.shipname.toLowerCase().includes(q) ||
        data.mmsi.toString().includes(q) ||
        data.callsign.toLowerCase().includes(q) ||
        data.shipclass.toString().includes(q) ||
        (data.last_signal != null && getDeltaTimeVal(data.last_signal).includes(q)) ||
        (data.count       != null && data.count.toString().includes(q)) ||
        (data.ppm         != null && data.ppm.toString().includes(q)) ||
        (data.level       != null && data.level.toString().includes(q)) ||
        (data.distance    != null && getDistanceVal(data.distance).includes(q)) ||
        (data.bearing     != null && data.bearing.toString().includes(q)) ||
        (data.lat         != null && getLatValFormat(data).includes(q)) ||
        (data.lon         != null && getLonValFormat(data).includes(q)) ||
        (data.speed       != null && getSpeedVal(data.speed).toString().includes(q)) ||
        (data.validated   != null && (data.validated === 1 ? "yes" : data.validated === -1 ? "no" : "pending").includes(q)) ||
        (data.channels    != null && getStringfromChannels(data.channels).includes(q)) ||
        (data.group_mask  != null && getStringfromGroup(data.group_mask).includes(q));
}

export function refreshTable() {
    if (table) table.redraw(true);
}

// ── Public API ────────────────────────────────────────────────────────────────

export function init(dependencies) {
    deps = dependencies;

    registerActions({ downloadCSV, resetShipTableColumns });

    const dropdownToggle = document.getElementById("shipTableColumnDropdownToggle");
    const dropdownMenu   = document.getElementById("shipTableColumnVisibilityMenu");
    document.body.appendChild(dropdownMenu);

    dropdownToggle.addEventListener("click", (e) => { e.stopPropagation(); toggleDropdown(); });
    document.getElementById("shipTableColumnReset").addEventListener("click", resetShipTableColumns);

    document.getElementById("shipSearch").addEventListener("input", (e) => {
        if (table) table.setFilter(customShipFilter, { query: e.target.value });
    });

    // Re-run filter when settings change (metric/coordinate_format may affect filter output)
    onSettingsChange(() => {
        if (table) {
            const query = document.getElementById("shipSearch").value;
            if (query) table.setFilter(customShipFilter, { query });
        }
    });
}
