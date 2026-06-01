import Tabulator from 'tabulator-tables/src/js/core/Tabulator.js';
import SortModule from 'tabulator-tables/src/js/modules/Sort/Sort.js';
import FilterModule from 'tabulator-tables/src/js/modules/Filter/Filter.js';
import PageModule from 'tabulator-tables/src/js/modules/Page/Page.js';
import PersistenceModule from 'tabulator-tables/src/js/modules/Persistence/Persistence.js';
import DownloadModule from 'tabulator-tables/src/js/modules/Download/Download.js';
import ExportModule from 'tabulator-tables/src/js/modules/Export/Export.js';
import FormatModule from 'tabulator-tables/src/js/modules/Format/Format.js';
import InteractionModule from 'tabulator-tables/src/js/modules/Interaction/Interaction.js';
import 'tabulator-tables/dist/css/tabulator.min.css';

Tabulator.registerModule([
    SortModule, FilterModule, PageModule, PersistenceModule,
    DownloadModule, ExportModule, FormatModule, InteractionModule,
]);
import {
    getFlag, getShipName, getCallSign, getCountryName,
    getStatusVal, getTypeVal,
    getShipDimension, getDimVal, getDimUnit,
    getDeltaTimeVal, getEtaVal,
    getDistanceVal, getLatValFormat, getLonValFormat,
    getSpeedVal, getSpeedUnit,
    getStringfromChannels, getStringfromMsgType, getStringfromGroup,
} from '../core/format.js';

let table = null;
let tableFirstTime = true;

function customShipFilter(data, filterParams) {
    const { shipsSince } = window.__app__;
    const query = filterParams.query.toLowerCase();
    return data.shipname.toLowerCase().includes(query) ||
        data.mmsi.toString().includes(query) ||
        data.callsign.toLowerCase().includes(query) ||
        data.shipclass.toString().includes(query) ||
        (data.last_signal != null && getDeltaTimeVal(shipsSince - data.last_signal).includes(query)) ||
        (data.count != null && data.count.toString().includes(query)) ||
        (data.ppm != null && data.ppm.toString().includes(query)) ||
        (data.level != null && data.level.toString().includes(query)) ||
        (data.distance != null && getDistanceVal(data.distance).includes(query)) ||
        (data.bearing != null && data.bearing.toString().includes(query)) ||
        (data.lat != null && getLatValFormat(data).includes(query)) ||
        (data.lon != null && getLonValFormat(data).includes(query)) ||
        (data.speed != null && getSpeedVal(data.speed).toString().includes(query)) ||
        (data.validated != null && (data.validated == 1 ? "yes" : data.validated == -1 ? "no" : "pending").includes(query)) ||
        (data.channels != null && getStringfromChannels(data.channels).includes(query)) ||
        (data.group_mask != null && getStringfromGroup(data.group_mask).includes(query));
}

// ENI is canonically 8 digits; transmitters often drop a leading zero.
// Left-pad numeric values so they display and sort consistently.
function padEni(v) {
    return /^\d{1,7}$/.test(v) ? v.padStart(8, "0") : (v || "");
}

function buildColumns() {
    return [
        {
            title: "Shipname", field: "shipname", sorter: "string",
            formatter: (cell) => {
                const ship = cell.getRow().getData();
                return getFlag(ship.country) + (getShipName(ship) || ship.mmsi);
            },
        },
        { title: "MMSI", field: "mmsi", sorter: "number" },
        { title: "IMO", field: "imo", sorter: "number", formatter: (cell) => { const v = cell.getValue(); return v != null ? v : ""; } },
        { title: "ENI", field: "eni", sorter: (a, b) => padEni(a).localeCompare(padEni(b)), formatter: (cell) => padEni(cell.getValue()) },
        { title: "Dest", field: "destination", sorter: "string", formatter: (cell) => { const v = cell.getValue(); return v != null ? v : ""; } },
        {
            title: "ETA", field: "eta", sorter: "string",
            formatter: (cell) => {
                const ship = cell.getRow().getData();
                return ship.eta_month != null && ship.eta_hour != null && ship.eta_day != null && ship.eta_minute != null ? getEtaVal(ship) : null;
            },
        },
        {
            title: "Callsign", field: "callsign", sorter: "string",
            formatter: (cell) => {
                const ship = cell.getRow().getData();
                return ship != null ? getCallSign(ship) : "";
            },
        },
        {
            title: "Flag", field: "country", sorter: "string",
            formatter: (cell) => {
                const ship = cell.getRow().getData();
                return ship != null ? getCountryName(ship.country) : "";
            },
        },
        {
            title: "Status", field: "status", sorter: "string",
            formatter: (cell) => {
                const ship = cell.getRow().getData();
                return ship != null ? getStatusVal(ship) : "";
            },
        },
        {
            title: "Type", field: "shipclass", sorter: "number",
            formatter: (cell) => window.__app__.getTableShiptype(cell.getRow().getData()),
        },
        {
            title: "Class", field: "class", sorter: "string",
            formatter: (cell) => getTypeVal(cell.getRow().getData()),
        },
        {
            title: "Dim", field: "dimension", sorter: "number",
            formatter: (cell) => {
                const ship = cell.getRow().getData();
                return ship ? getShipDimension(ship) || "" : "";
            },
        },
        {
            title: "Draught", field: "draught", sorter: "number",
            formatter: (cell) => {
                const ship = cell.getRow().getData();
                return ship.draught ? getDimVal(ship.draught) + " " + getDimUnit() : null;
            },
        },
        {
            title: "Last", field: "last_signal", sorter: "number",
            formatter: (cell) => {
                const value = cell.getValue();
                return value != null ? getDeltaTimeVal(window.__app__.shipsSince - value) : "";
            },
        },
        { title: "Count", field: "count", sorter: "number" },
        {
            title: "PPM", field: "ppm", sorter: "number",
            formatter: (cell) => {
                const value = cell.getValue();
                return value != null ? Number(value).toFixed(1) : "";
            },
        },
        {
            title: "RSSI", field: "level", sorter: "number",
            formatter: (cell) => {
                const value = cell.getValue();
                return value != null ? Number(value).toFixed(1) : "";
            },
        },
        {
            title: "Dist", field: "distance", sorter: "number",
            formatter: (cell) => {
                const ship = cell.getRow().getData();
                return (ship != null && ship.distance != null)
                    ? getDistanceVal(ship.distance) + (ship.repeat > 0 ? " (R)" : "")
                    : "";
            },
        },
        {
            title: "Brg", field: "bearing", sorter: "number",
            formatter: (cell) => {
                const value = cell.getValue();
                return value != null ? Number(value).toFixed(0) + "&deg;" : "";
            },
        },
        {
            title: "Lat", field: "lat", sorter: "number",
            formatter: (cell) => {
                const ship = cell.getRow().getData();
                const color = ship.validated == 1 ? "green" : ship.validated == -1 ? "red" : "inherited";
                return "<div style='color:" + color + "'>" + (ship.lat != null ? getLatValFormat(ship) : "") + "</div>";
            },
        },
        {
            title: "Lon", field: "lon", sorter: "number",
            formatter: (cell) => {
                const ship = cell.getRow().getData();
                const color = ship.validated == 1 ? "green" : ship.validated == -1 ? "red" : "inherited";
                return "<div style='color:" + color + "'>" + (ship.lon != null ? getLonValFormat(ship) : "") + "</div>";
            },
        },
        {
            title: "Spd", field: "speed", sorter: "number",
            formatter: (cell) => {
                const value = cell.getValue();
                return value ? getSpeedVal(value) + " " + getSpeedUnit() : null;
            },
        },
        {
            title: "Valid", field: "validated", sorter: "string",
            formatter: (cell) => {
                const value = cell.getValue();
                return value == 1 ? "Yes" : value == -1 ? "No" : "Pending";
            },
        },
        {
            title: "Repeated", field: "repeat", sorter: "string",
            formatter: (cell) => cell.getValue() == 1 ? "Yes" : "No",
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
            title: "MSG6", field: "MSG6", sorter: "number",
            formatter: (cell) => cell.getValue() & (1 << 6) ? "Yes" : "No",
        },
        {
            title: "MSG8", field: "MSG8", sorter: "number",
            formatter: (cell) => cell.getValue() & (1 << 8) ? "Yes" : "No",
        },
        {
            title: "MSG27", field: "MSG27", sorter: "number",
            formatter: (cell) => cell.getValue() & (1 << 27) ? "Yes" : "No",
        },
        {
            title: "Src", field: "group_mask", sorter: "number",
            formatter: (cell) => getStringfromGroup(cell.getValue()),
        },
    ];
}

function populateColumnVisibilityMenu() {
    const menu = document.getElementById("shipTableColumnVisibilityMenu");
    menu.innerHTML = "";

    table.getColumns().forEach((column) => {
        const checkboxId = `checkbox-${column.getField()}`;
        const item = document.createElement("div");
        item.classList.add("ship-table-column-dropdown-item");

        const checkbox = document.createElement("input");
        checkbox.type = "checkbox";
        checkbox.id = checkboxId;
        checkbox.value = column.getField();
        checkbox.checked = column.isVisible();
        checkbox.addEventListener("change", updateColumnVisibility);

        const label = document.createElement("label");
        label.setAttribute("for", checkboxId);
        label.textContent = column.getDefinition().title;

        item.appendChild(checkbox);
        item.appendChild(label);
        menu.appendChild(item);
    });
}

function updateColumnVisibility() {
    const settings = window.AISCatcher.settings;
    const checkboxes = document.querySelectorAll("#shipTableColumnVisibilityMenu input[type='checkbox']");
    settings.shiptable_columns = Array.from(checkboxes)
        .filter((c) => c.checked)
        .map((c) => c.value);
    setColumns();
}

function resetColumns() {
    const settings = window.AISCatcher.settings;
    settings.shiptable_columns = ["shipname", "mmsi", "imo", "callsign", "shipclass", "lat", "lon", "last_signal", "level", "distance", "bearing", "speed", "repeat", "ppm", "status"];
    setColumns();
}

function setColumns() {
    const settings = window.AISCatcher.settings;
    window.__app__.saveSettings();

    const updatedColumns = table.getColumns().map((column) => {
        const field = column.getField();
        return {
            ...column.getDefinition(),
            visible: settings.shiptable_columns.includes(field),
        };
    });
    table.setColumns(updatedColumns);
    populateColumnVisibilityMenu();
}

function search(query) {
    if (table) table.setFilter(customShipFilter, { query });
}

function wireUIEvents() {
    const searchInput = document.getElementById("shipSearch");
    if (searchInput) {
        searchInput.addEventListener("input", (e) => search(e.target.value));
    }

    const dropdownToggle = document.getElementById('shipTableColumnDropdownToggle');
    const dropdownMenu = document.getElementById('shipTableColumnVisibilityMenu');
    if (dropdownToggle && dropdownMenu) {
        // Reparent the menu so the table container doesn't clip it.
        document.body.appendChild(dropdownMenu);

        dropdownToggle.addEventListener('click', (event) => {
            event.stopPropagation();
            const isOpen = dropdownMenu.style.display === 'block';
            if (isOpen) {
                dropdownMenu.style.display = 'none';
                return;
            }
            if (table) populateColumnVisibilityMenu();
            const rect = dropdownToggle.getBoundingClientRect();
            dropdownMenu.style.top = (rect.bottom + window.scrollY) + 'px';
            dropdownMenu.style.right = (window.innerWidth - rect.right) + 'px';
            dropdownMenu.style.display = 'block';
        });

        document.addEventListener('click', (event) => {
            if (!dropdownMenu.contains(event.target) && event.target !== dropdownToggle) {
                dropdownMenu.style.display = 'none';
            }
        });
    }

    const resetButton = document.getElementById('shipTableColumnReset');
    if (resetButton) {
        resetButton.addEventListener('click', () => resetColumns());
    }
}

export async function update() {
    const ok = await window.__app__.fetchShips();
    if (!ok) return;

    const data = Object.values(window.AISCatcher.shipsDB).map((ship) => ship.raw);

    if (table == null) {
        table = new Tabulator("#shipTable", {
            index: "mmsi",
            rowFormatter: (row) => {
                const ship = row.getData();
                const borderColor = ship.validated == 1 ? "#7CFC00" : ship.validated == -1 ? "red" : "lightgrey";
                row.getElement().style.borderLeft = `10px solid ${borderColor}`;
            },
            data,
            layout: "fitDataTable",
            persistence: { sort: true },
            pagination: "local",
            paginationSize: 50,
            rowHeight: 37,
            paginationSizeSelector: [20, 50, 100],
            columns: buildColumns(),
        });
        table.on("rowContext", (e, row) => {
            window.AISCatcher.showContextMenu(e, row.getData().mmsi, "ship", ["settings", "ship", "table-menu"]);
        });
        table.on("rowClick", (e, row) => {
            window.__app__.tableRowClick(row.getData().mmsi);
        });
        table.on("tableBuilt", () => {
            if (tableFirstTime) {
                setColumns();
                tableFirstTime = false;
            }
        });
        wireUIEvents();
    } else {
        const sorters = table.getSorters();
        table.replaceData(data);
        table.setSort(sorters);
    }
}

export function downloadCSV() {
    if (table) table.download("csv", "data.csv");
}

// Drop the Tabulator instance; next update() rebuilds it. Called when a
// setting that affects column rendering changes (units, coord format, ...).
export function reset() {
    if (table != null) {
        table.destroy();
        table = null;
    }
}
