/* Shared vessel rows and pagination. Hosts supply display values from their
   own data sources; map visibility and server queries stay with the host. */
import { flagHTML } from './components.js';
import { sanitizeString, getDeltaTimeVal } from './core/text.js';

const text = value => sanitizeString(String(value ?? ''));

/* Two units fit a table cell; the tooltip keeps the complete duration. */
export function durationHTML(seconds, prefix = '') {
    if (!Number.isFinite(seconds)) return '—';
    const full = getDeltaTimeVal(Math.max(0, Math.floor(seconds)));
    const short = full.split(' ').slice(0, 2).join(' ');
    return `<span class="table-duration" title="${text(prefix + full)}">${text(prefix + short)}</span>`;
}

export function vesselNameCell(ship) {
    const name = text(ship.name);
    const title = name + (ship.mmsi ? ` · MMSI ${text(ship.mmsi)}` : '');
    return `<td class="col-name" title="${title}"><span class="table-name">` +
        flagHTML(ship.country, '', ship.countryName) +
        `<button type="button" class="vessel-link port-ship-link" translate="no">${name}` +
        (ship.relay ? '<span class="row-relay" title="Received through another station">&#8635;</span>' : '') +
        '</button></span></td>';
}

/* typeHTML and lastHTML are trusted sprite/date markup, never raw feed text. */
export function renderVesselRows(ships, { distance = false, idAttribute = 'data-mmsi' } = {}) {
    if (!['data-mmsi', 'data-id'].includes(idAttribute)) throw new Error('Invalid vessel identifier attribute');
    return ships.map(ship => {
        const classes = [ship.selected && 'selected', ship.stale && 'stale'].filter(Boolean).join(' ');
        return `<tr ${idAttribute}="${text(ship.id ?? ship.mmsi)}" class="${classes}">` + vesselNameCell(ship) +
            (distance ? `<td class="num col-dist">${text(ship.distance ?? '—')}</td>` : '') +
            `<td class="num col-spd">${text(ship.speed ?? '—')}</td>` +
            `<td class="col-type">${ship.typeHTML || ''}</td>` +
            `<td class="num col-last">${ship.lastHTML ?? text(ship.last ?? '—')}</td></tr>`;
    }).join('');
}

/* Moving between cells of one row keeps the map highlight steady. The host
   resolves the row identifier against its map data; a server list may also
   contain vessels the map has not loaded. */
export function bindVesselHover(body, { enter, leave, idAttribute = 'data-mmsi' }) {
    let hovered = null;
    const clear = () => {
        if (hovered === null) return;
        leave(hovered);
        hovered = null;
    };
    const over = event => {
        const row = event.target.closest(`tr[${idAttribute}]`);
        if (!row || !body.contains(row)) return;
        const id = row.getAttribute(idAttribute);
        if (id === hovered) return;
        clear();
        hovered = id;
        enter(id);
    };
    const out = event => {
        const row = event.target.closest(`tr[${idAttribute}]`);
        if (row && event.relatedTarget && row.contains(event.relatedTarget)) return;
        clear();
    };
    body.addEventListener('mouseover', over);
    body.addEventListener('mouseout', out);
    return {
        clear,
        sync() {
            if (![...body.children].some(row => row.getAttribute(idAttribute) === hovered)) clear();
        },
        destroy() {
            clear();
            body.removeEventListener('mouseover', over);
            body.removeEventListener('mouseout', out);
        },
    };
}

/* How many rows fit, measured from a row that is actually on screen: an
   assumed row height is wrong the moment a column wraps or the type scale
   changes. Returns 0 while the panel has no height yet, which is the case on
   the frame it opens. */
export function pageSize(inner, head, sampleRow) {
    var declared = inner ? parseFloat(getComputedStyle(inner).getPropertyValue('--vessel-row-height')) : NaN;
    var rowHeight = Math.max(18, sampleRow ? sampleRow.getBoundingClientRect().height : declared || 26);
    var space = (inner ? inner.getBoundingClientRect().height : 0) -
        (head ? head.getBoundingClientRect().height : 0);

    return space < rowHeight ? 0 : Math.max(1, Math.floor(space / rowHeight));
}

/* Sub-pixel row heights make pageSize optimistic by a row, so count the rows
   that really landed inside the box. */
export function rowsFitting(inner, rows) {
    if (!inner) return 0;

    var bottom = inner.getBoundingClientRect().bottom;
    return Array.prototype.filter.call(rows, function (tr) {
        return tr.getBoundingClientRect().bottom <= bottom + 1;
    }).length;
}

/* A stable footer: the visible range at left, page controls at right. */
export function renderPager(pager, { page, perPage, total, shown = Math.min(perPage, total - page * perPage), loading = false }) {
    if (!pager) return;

    const pages = Math.max(1, Math.ceil(total / perPage));
    pager.hidden = false;
    const count = pager.parentElement.querySelector('.table-count');
    if (count) count.textContent = shown > 0
        ? `${page * perPage + 1}–${page * perPage + shown} of ${total.toLocaleString()}`
        : `0 of ${total.toLocaleString()}`;
    var buttons = pager.querySelectorAll("button");
    var label = pager.querySelector("span");

    if (label) label.textContent = (page + 1) + " / " + pages;
    if (buttons[0]) buttons[0].disabled = loading || page === 0;
    if (buttons[1]) buttons[1].disabled = loading || page >= pages - 1;
}

/* Move the ▲/▼ to the column now being sorted on. */
export function markSort(root, column, order) {
    if (!root) return;

    root.querySelectorAll("[data-column]").forEach(function (th) {
        th.classList.remove("ascending", "descending");
        if (th.getAttribute("data-column") === column) th.classList.add(order);
    });
}

/* The order a head takes when clicked: the same column flips, a new one starts
   ascending. */
export function nextOrder(th) {
    return th.classList.contains("ascending") ? "descending" : "ascending";
}

/* Blanks sort last in both directions — a vessel with no speed is not the
   slowest one, it is one we cannot place. */
export function compareNumber(a, b, order) {
    if (a == null) return order === "ascending" ? 1 : -1;
    if (b == null) return order === "ascending" ? -1 : 1;
    return a - b;
}

export function compareString(a, b) {
    if (a == null && b == null) return 0;
    if (a == null) return 1;
    if (b == null) return -1;
    return (a + "").localeCompare(b + "");
}
