// The vessel filter's panel page: its lists, its two dual-range sliders and
// its summary, and what a changed filter means - every view is drawn again.

import * as filter from '../../core/filter.js';
import { ships as shipsDB, station, counts as shipCounts } from '../../core/store.js';
import { getDistanceConversion, getDistanceUnit, getSpeedConversion, getSpeedUnit } from '../../core/units.js';
import * as components from '@aiscatcher/ui/components.js';
import { redraw as redrawMap } from '../../map.js';
import { saveSettings, settingsPanel, openSettingsTab } from '../settings/settings.js';
import { loaded as loadedTab } from '../../tabs/index.js';
import * as replay from '../replay/replay.js';
import { updateMarkerCount } from '../counts/counts.js';
import { updateTablecard } from '../tablecard/tablecard.js';
import { register } from '../../actions.js';

export function applyFilter() {
    saveSettings();
    if (shipsDB != null)
        for (const key in shipsDB) shipsDB[key].show = undefined;

    // whichever view is on screen owns the counters: replay reports its own
    // fleet as it redraws, and updateMarkerCount stands down while it does
    replay.refresh();
    updateMarkerCount();
    redrawMap();
    updateTablecard();
    loadedTab('ships')?.update();
    loadedTab('realtime')?.applyVesselFilter();
    updateFilterUI();
}

function openFilterPanel() {
    if (settingsPanel.isOpen("Filter")) settingsPanel.close();
    else openSettingsTab("Filter");
}

export function updateFilterUI() {
    const set = (id, value) => {
        const el = document.getElementById(id);
        if (el) {
            if (el.type === "checkbox") el.checked = !!value;
            else el.value = value ?? "";
        }
    };
    set("filter_seen", filter.get("seen"));
    set("filter_validated", filter.get("validated"));
    set("filter_repeated", filter.get("repeated"));

    const seenLabel = document.getElementById("filter_seen_label");
    if (seenLabel) {
        const v = Number(filter.get("seen")) || 0;
        seenLabel.textContent = "Seen within" + (v > 0 ? " (" + v + " min)" : " (any)");
    }

    const summary = document.getElementById("filter_summary");
    if (summary) {
        summary.textContent = filter.isActive()
            ? shipCounts.shown + " of " + shipCounts.total + " vessels shown"
            : "Showing every vessel";
        summary.classList.toggle("filter-on", filter.isActive());
    }

    for (const [kind, box] of Object.entries(FILTER_LISTS)) {
        buildFilterList(box, kind);
        document.querySelectorAll("#" + box + " input").forEach((el) => {
            const id = kind === "bucket" ? el.dataset.id : Number(el.dataset.id);
            el.checked = !filter.isHidden(kind, id);
        });
    }

    syncDualRange("filter_speed", "speed_min", "speed_max", getSpeedConversion, getSpeedUnit());
    syncDualRange("filter_distance", "distance_min", "distance_max", getDistanceConversion, getDistanceUnit());

    const known = station != null && station.lat != null && station.lon != null;
    document.getElementById("filter_distance")?.closest("section")?.classList.toggle("st-off", !known);
}

const FILTER_LISTS = { bucket: "filter_senders", class: "filter_classes", status: "filter_statuses" };

function buildFilterList(boxId, kind) {
    const box = document.getElementById(boxId);
    if (!box || box.dataset.built) return;
    box.dataset.built = "1";
    box.innerHTML = filter.LISTS[kind].items().map((item) => {
        const icon = item.icon
            ? '<span class="' + item.icon + '" style="' +
              components.chipStyle({ ...item, scale: item.scale || 1.15 }) + '"></span>'
            : "";
        return '<label class="filter-check"><input type="checkbox" data-on-change="toggleFilterItem"' +
            ' data-kind="' + kind + '" data-id="' + item.id + '">' + icon +
            "<span>" + item.label + "</span></label>";
    }).join("");
}

function buildDualRange(el) {
    if (el.dataset.built) return;
    el.dataset.built = "1";
    const { min, max, step } = el.dataset;
    el.innerHTML =
        '<div class="dr-track"><div class="dr-fill"></div>' +
        '<input type="range" class="dr-min" min="' + min + '" max="' + max + '" step="' + step + '">' +
        '<input type="range" class="dr-max" min="' + min + '" max="' + max + '" step="' + step + '"></div>';

    const lo = el.querySelector(".dr-min"), hi = el.querySelector(".dr-max");
    const commit = () => {
        const key = el.dataset.key;
        const a = Number(lo.value), b = Number(hi.value);
        setFilterValue(key + "_min", a <= Number(min) ? null : a);
        setFilterValue(key + "_max", b >= Number(max) ? null : b);
    };
    lo.addEventListener("input", () => { if (Number(lo.value) > Number(hi.value)) hi.value = lo.value; paintDualRange(el); });
    hi.addEventListener("input", () => { if (Number(hi.value) < Number(lo.value)) lo.value = hi.value; paintDualRange(el); });
    lo.addEventListener("change", commit);
    hi.addEventListener("change", commit);
}

function paintDualRange(el) {
    const lo = el.querySelector(".dr-min"), hi = el.querySelector(".dr-max");
    const min = Number(el.dataset.min), max = Number(el.dataset.max);
    const a = Number(lo.value), b = Number(hi.value);
    const span = max - min || 1;
    const fill = el.querySelector(".dr-fill");
    fill.style.left = ((a - min) / span) * 100 + "%";
    fill.style.right = ((max - b) / span) * 100 + "%";

    const title = document.getElementById(el.dataset.title);
    if (title) title.textContent = el.dataset.label + " (" + rangeText(el, a, b, min, max) + ")";
}

function rangeText(el, a, b, min, max) {
    const unit = el._unit || "";
    const n = (v) => Math.round(el._convert ? el._convert(v) : v);
    if (a <= min && b >= max) return "any";
    if (b >= max) return n(a) + " " + unit + "+";
    if (a <= min) return "up to " + n(b) + " " + unit;
    return n(a) + " - " + n(b) + " " + unit;
}

function syncDualRange(id, minKey, maxKey, convert, unit) {
    const el = document.getElementById(id);
    if (!el) return;
    buildDualRange(el);
    el._convert = convert;
    el._unit = unit;
    el.querySelector(".dr-min").value = filter.get(minKey) ?? el.dataset.min;
    el.querySelector(".dr-max").value = filter.get(maxKey) ?? el.dataset.max;
    paintDualRange(el);
}

function setFilterValue(key, value) {
    filter.set(key, value);
    applyFilter();
}

function resetFilter() {
    filter.reset();
    applyFilter();
}

register({
    openFilterPanel: () => openFilterPanel(),
    resetFilter: () => resetFilter(),
    toggleFilterBucket: (e, d) => {
        filter.toggle("bucket", d.bucket);
        applyFilter();
    },
    toggleFilterItem: (e, d) => {
        filter.toggle(d.kind, d.kind === "bucket" ? d.id : Number(d.id));
        applyFilter();
    },
    setAllFilterItems: (e, d) => {
        filter.setAll(d.kind, d.on === "1");
        applyFilter();
    },
    setFilterNumber: (e, d, el) => setFilterValue(d.key, el.value === "" ? null : Number(el.value)),
    setFilterChoice: (e, d, el) => setFilterValue(d.key, el.value),
    setFilterFlag: (e, d, el) => setFilterValue(d.key, el.checked),
    updateFilterSeenDisplay: (e, d, el) => {
        const label = document.getElementById("filter_seen_label");
        const v = Number(el.value) || 0;
        if (label) label.textContent = "Seen within" + (v > 0 ? " (" + v + " min)" : " (any)");
    },
});
