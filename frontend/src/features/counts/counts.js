// The vessel counts: the counter on the map, the counts card with a chip per
// kind of sender, the ticker's tally and the filter button's summary.

import * as filter from '../../core/filter.js';
import { ships as shipsDB, counts as shipCounts, setCounts as setShipCounts } from '../../core/store.js';
import { compactCount } from '@aiscatcher/core/text.js';
import { bucketChip } from '@aiscatcher/ui/components.js';
import * as replay from '../replay/replay.js';
import * as ticker from '../ticker/ticker.js';
import { panels } from '../../panels.js';

const BUCKET_ELEMENT = {
    am: "statcard_moving",
    as: "statcard_stationary",
    bm: "statcard_class_b_moving",
    bs: "statcard_class_b_stationary",
    aton: "statcard_aton",
    base: "statcard_station",
    sarte: "statcard_sarte",
    air: "statcard_heli",
};

export function bucketChips() {
    return filter.BUCKETS.map((b) => ({
        ...b,
        label: b.label + " \u2014 click to show or hide",
        action: "toggleFilterBucket",
        countId: BUCKET_ELEMENT[b.id],
    }));
}

export function buildStatcard() {
    const box = document.querySelector("#statcard .statcard_inner");
    if (!box || box.children.length) return;

    for (const chip of bucketChips()) box.appendChild(bucketChip(chip, chip.countId).item);
}

function countShips() {
    const buckets = {};
    let total = 0, shown = 0;

    for (const key in shipsDB) {
        const entry = shipsDB[key];
        const b = filter.bucketOf(entry.raw);
        buckets[b] = (buckets[b] || 0) + 1;
        total++;
        if (filter.visible(entry)) shown++;
    }
    setShipCounts({ total, shown, buckets });
}

export function updateMarkerCountTooltip() {
    for (const [id, elementId] of Object.entries(BUCKET_ELEMENT)) {
        if (shipsDB == null) {
            document.getElementById(elementId).innerHTML = "";
            continue;
        }
        const v = shipCounts.buckets[id] || 0;
        setCount(elementId, v);
        const item = document.getElementById(elementId)?.closest(".stat-item");
        if (item) {
            item.dataset.zero = v === 0 ? "true" : "false";
            item.classList.toggle("stat-off", filter.isActive() && filter.isHidden("bucket", id));
        }
    }
}

function setCount(id, value) {
    document.getElementById(id).innerText = compactCount(value);
}

export function updateMarkerCount() {
    if (shipsDB == null) {
        setCount("markerCount", 0);
        return;
    }
    if (replay.isActive()) return;

    countShips();
    renderCounts();
}

export function renderCounts() {
    const active = filter.isActive();
    setCount("markerCount", active ? shipCounts.shown : shipCounts.total);
    const el = document.getElementById("markerCount");
    if (el) {
        el.classList.toggle("count-filtered", active);
        el.parentElement.title = active
            ? shipCounts.shown + " of " + shipCounts.total + " vessels shown"
            : shipCounts.total + " vessels";
    }

    updateFilterIndicator();
    ticker.setCounts({ shown: shipCounts.shown, total: shipCounts.total, filtered: active, buckets: shipCounts.buckets });

    if (panels.statcard) updateMarkerCountTooltip();
}

function updateFilterIndicator() {
    const btn = document.getElementById("filter-btn");
    if (!btn) return;

    const active = filter.isActive();
    btn.classList.toggle("is-active", active);
    btn.title = active
        ? "Filter: " + filter.describe().join(" \u00b7 ") + " (" + shipCounts.shown + " of " + shipCounts.total + ")"
        : "Filter vessels";

    const tableBtn = document.getElementById("shipTableFilterBtn");
    if (tableBtn) {
        tableBtn.classList.toggle("filter-on", active);
        tableBtn.title = btn.title;
    }

    const note = document.getElementById("filter_empty");
    if (note) note.classList.toggle("visible", active && shipCounts.total > 0 && shipCounts.shown === 0);
}
