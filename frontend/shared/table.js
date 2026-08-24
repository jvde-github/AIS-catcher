/* table.js — the mechanics a side table needs whatever it happens to list.

   The rows themselves stay with the host: the viewer lists its own ship
   database and the website lists what the tiles gave it, and those records do
   not look alike. What is identical is the arithmetic around them — how many
   rows a panel can show, the pager in the band, the sort marker on a column
   head, and comparators that keep blanks at the bottom whichever way the sort
   points. */

/* How many rows fit, measured from a row that is actually on screen: an
   assumed row height is wrong the moment a column wraps or the type scale
   changes. Returns 0 while the panel has no height yet, which is the case on
   the frame it opens. */
export function pageSize(inner, head, sampleRow) {
    var rowHeight = Math.max(18, sampleRow ? sampleRow.getBoundingClientRect().height : 26);
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

/* ‹ 8 / 12 · 224 › — hidden while everything fits on one page. The pager holds
   two buttons around a label, which is all this needs to know about it. */
export function renderPager(pager, page, pages, total) {
    if (!pager) return;

    pager.hidden = pages < 2;
    if (pager.hidden) return;

    var buttons = pager.querySelectorAll("button");
    var label = pager.querySelector("span");

    if (label) label.textContent = (page + 1) + " / " + pages + " · " + total;
    if (buttons[0]) buttons[0].disabled = page === 0;
    if (buttons[1]) buttons[1].disabled = page >= pages - 1;
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
