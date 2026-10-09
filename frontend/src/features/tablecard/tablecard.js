// The side table beside the map: the vessels sorted by a column, filtered by
// name or MMSI, a page at a time sized to the panel, the selected vessel
// marked, a hover on a row ringing the vessel on the map.

import { settings } from '../../core/state.js';
import * as filter from '../../core/filter.js';
import { ships as shipsDB, clock, cardMmsi as card_mmsi, cardType as card_type, hoverMmsi as hoverMMSI, hoverType } from '../../core/store.js';
import { getShipName } from '../../core/names.js';
import { getDistanceVal, getDistanceUnit, getSpeedVal, getSpeedUnit } from '../../core/units.js';
import { getCountryName, compactCount } from '@aiscatcher/core/text.js';
import { decodeHTMLEntities } from '@aiscatcher/ui/components.js';
import * as tableLib from '@aiscatcher/ui/table.js';
import { getTableShiptype } from '../../map.js';
import { saveSettings } from '../settings/settings.js';
import { showTargetcard } from '../targetcard/card.js';
import { register } from '../../actions.js';
import { showContextMenu } from '../contextmenu/contextmenu.js';
import { startHover, stopHover } from '../hover/hover.js';

export function updateTableSort(event, dataset, header) {
    settings.tableside_column = header.getAttribute("data-column");
    settings.tableside_order = tableLib.nextOrder(header);

    saveSettings();
    tablePage = 0;
    updateSortMarkers();
    updateTablecard();
}

export function updateSortMarkers() {
    tableLib.markSort(document.getElementById("tableside"),
        settings.tableside_column, settings.tableside_order);
}

const compareString = tableLib.compareString;
const compareNumber = (a, b) => tableLib.compareNumber(a, b, settings.tableside_order);

const TABLE_STALE_SEC = 600;

let tablePage = 0;
let tablePerPage = 0;
let tableRedrawing = false;
export const tableHover = tableLib.bindVesselHover(document.getElementById('tablecardBody'), {
    enter: id => { if (shipsDB?.[id]) startHover('ship', Number(id)); },
    leave: id => { if (hoverType === 'ship' && hoverMMSI === Number(id)) stopHover(); },
});

function tablePageSize() {
    return tableLib.pageSize(
        document.querySelector(".tablecard_inner"),
        document.querySelector(".mytable thead"),
        document.querySelector("#tablecardBody tr"));
}

function tableRowsFitting() {
    return tableLib.rowsFitting(
        document.querySelector(".tablecard_inner"),
        document.querySelectorAll("#tablecardBody tr"));
}

function renderTablePager(total, perPage) {
    const pager = document.getElementById("tablePager");
    tableLib.renderPager(pager, { page: tablePage, perPage, total });
    // one page needs no paging: just how many vessels
    if (total <= perPage) {
        pager.hidden = true;
        const count = pager.parentElement.querySelector('.table-count');
        if (count) count.textContent = total === 1 ? '1 vessel' : total.toLocaleString() + ' vessels';
    }
}

function turnTablePage(step) {
    tablePage += step;
    updateTablecard();
}

export function updateTablecard() {
    if (!document.getElementById("tableside").classList.contains("active")) return;
    if (document.getElementById("tableside").dataset.context) return;

    const tableBody = document.getElementById("tablecardBody");
    if (shipsDB == null) return;

    let shipKeys = Object.keys(shipsDB);

    let column = settings.tableside_column;
    let order = settings.tableside_order;

    const sortFunctions = {
        flag: (a, b) => compareString(shipsDB[a].raw.country, shipsDB[b].raw.country),
        shipname: (a, b) => compareString(getShipName(shipsDB[a].raw), getShipName(shipsDB[b].raw)),
        distance: (a, b) => compareNumber(shipsDB[a].raw.distance, shipsDB[b].raw.distance),
        speed: (a, b) => compareNumber(shipsDB[a].raw.speed, shipsDB[b].raw.speed),
        type: (a, b) => compareNumber(shipsDB[a].raw.shipclass, shipsDB[b].raw.shipclass),
        last_signal: (a, b) => compareNumber(shipsDB[b].raw.last_signal, shipsDB[a].raw.last_signal),
    };

    if (column in sortFunctions) {
        shipKeys.sort((keyA, keyB) => {
            const comparisonResult = sortFunctions[column](keyA, keyB);
            return order === "ascending" ? comparisonResult : -comparisonResult;
        });
    }

    document.getElementById("table_dist_unit").textContent = getDistanceUnit();
    document.getElementById("table_spd_unit").textContent = getSpeedUnit();

    // the filter field narrows the list by name or MMSI
    const query = (document.getElementById('tableside_filter')?.value || '').trim().toLowerCase();
    const matches = (key) => {
        if (!query) return true;
        const ship = shipsDB[key].raw;
        return String(ship.mmsi).startsWith(query) || (decodeHTMLEntities(getShipName(ship)) || '').toLowerCase().includes(query);
    };
    const shown = shipKeys.filter(key => key in shipsDB && filter.visible(shipsDB[key]) && matches(key));
    document.getElementById('tableside_count').textContent = compactCount(shown.length);
    // without a station position every distance is a dash: drop the column, the names get the room
    document.getElementById('tableside').classList.toggle('no-dist', !shown.some(k => shipsDB[k].raw.distance != null));

    const perPage = tablePerPage || tablePageSize();
    if (!perPage) {
        if (!tableRedrawing) {
            tableRedrawing = true;
            // re-enter only if a height exists by then; a panel that stays too
            // short waits for the next resize instead of retrying every frame
            requestAnimationFrame(() => { tableRedrawing = false; if (tablePageSize()) updateTablecard(); });
        }
        return;
    }

    const pages = Math.max(1, Math.ceil(shown.length / perPage));
    tablePage = Math.min(Math.max(0, tablePage), pages - 1);

    const rows = shown.slice(tablePage * perPage, (tablePage + 1) * perPage).map(key => {
        const ship = shipsDB[key].raw;
        const age = clock - ship.last_signal;
        return {
            mmsi: ship.mmsi, name: decodeHTMLEntities(getShipName(ship)) || ship.mmsi,
            country: ship.country, countryName: getCountryName(ship.country),
            distance: ship.distance != null ? getDistanceVal(ship.distance) : '—',
            speed: ship.speed != null ? getSpeedVal(ship.speed) : '—',
            typeHTML: getTableShiptype(ship), lastHTML: tableLib.durationHTML(age),
            selected: isSelectedShip(ship.mmsi), stale: age > TABLE_STALE_SEC, relay: ship.repeat > 0,
        };
    });
    tableBody.innerHTML = tableLib.renderVesselRows(rows, { distance: true });
    tableHover.sync();
    renderTablePager(shown.length, perPage);

    const fits = tableRowsFitting();
    if (fits && fits !== perPage && rows.length === perPage && !tableRedrawing) {
        tablePerPage = fits;
        tableRedrawing = true;
        updateTablecard();
        tableRedrawing = false;
    }

    tableBody.onclick = function(e) {
        const tr = e.target.closest('tr[data-mmsi]');
        if (tr) showTargetcard('ship', parseInt(tr.dataset.mmsi));
    };
    tableBody.oncontextmenu = function(e) {
        const tr = e.target.closest('tr[data-mmsi]');
        if (tr) showContextMenu(e, parseInt(tr.dataset.mmsi), "ship", ["object", "object-map"]);
    };
}

document.getElementById('tableside_filter')?.addEventListener('input', () => { tablePage = 0; updateTablecard(); });

function isSelectedShip(mmsi) {
    return card_type === "ship" && mmsi == card_mmsi;
}

export function syncTableSelection() {
    document.querySelectorAll('#tablecardBody tr[data-mmsi]').forEach(tr =>
        tr.classList.toggle('selected', isSelectedShip(tr.dataset.mmsi)));
}


// the panel was resized: the page size is measured again
export function relayoutTablecard() {
    tablePerPage = 0;
    updateTablecard();
}

document.getElementById('tableside').addEventListener('table:overview', () => relayoutTablecard());

register({
    updateTableSort: (e, dataset, el) => updateTableSort(e, dataset, el),
    tablePagePrev: () => turnTablePage(-1),
    tablePageNext: () => turnTablePage(1),
});
