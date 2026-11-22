import { compact } from './chart.js';

const DAYS = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];

let panel, grid, sub, ring = null, measure = "messages";

const mean = (list) => (list.length ? list.reduce((a, b) => a + b, 0) / list.length : null);

// the reports of one measure, in the unit reported last: two units never share a line or a mean
export function reported(history, name) {
    const all = (history.reception || []).filter((s) => s.name === name);
    if (!all.length) return all;
    const last = (s) => s.values.reduce((at, v, i) => (v === null ? at : i), -1);
    const newest = all.reduce((a, b) => (last(b) > last(a) ? b : a), all[0]);
    return all.filter((s) => s.unit === newest.unit);
}
const channels = (name) => (i) => mean(reported(ring, name).map((s) => s.values[i]).filter(Number.isFinite));

const MEASURES = {
    messages: [(i) => ring.stat[i].count || null, (v) => compact(v) + " messages"],
    vessels: [(i) => ring.stat[i].vessels || null, (v) => compact(v) + " vessels"],
    signal: [(i) => (ring.stat[i].count ? ring.stat[i].level_max : null), (v) => v.toFixed(1) + " dB strongest"],
    noise: [channels("noise_floor"), (v) => v.toFixed(1) + " dB noise floor"],
    load: [channels("channel_load"), (v) => v.toFixed(0) + "% channel load"],
    clipping: [(i) => channels("clipping")(i) || null, (v) => compact(v) + "% of samples clipped", "No clipping"],
};

const day = (d) => d.getFullYear() * 400 + d.getMonth() * 32 + d.getDate();

function draw() {
    if (!grid || !ring) return;
    const [value, word, none = "Nothing heard"] = MEASURES[measure];
    const cells = new Map();
    ring.time.forEach((x, i) => {
        const d = new Date((ring.now + x * ring.interval) * 1000), v = value(i);
        if (v !== null && Number.isFinite(v)) cells.set(day(d) * 24 + d.getHours(), v);
    });
    const values = [...cells.values()], lo = Math.min(...values), hi = Math.max(...values);

    grid.replaceChildren();
    const add = (cls, text = "") => {
        const e = document.createElement("span");
        e.className = cls;
        e.textContent = text;
        grid.append(e);
        return e;
    };
    add("busy-day");
    for (let h = 0; h < 24; h++) add("busy-hour", h % 3 ? "" : String(h).padStart(2, "0"));
    for (let back = 6; back >= 0; back--) {
        const d = new Date();
        d.setDate(d.getDate() - back);
        add("busy-day", DAYS[d.getDay()] + " " + d.getDate());
        for (let h = 0; h < 24; h++) {
            const v = cells.get(day(d) * 24 + h), cell = add("busy-cell");
            if (v === undefined) continue;
            cell.style.opacity = hi > lo ? 0.15 + (0.85 * (v - lo)) / (hi - lo) : 1;
            cell.classList.add("busy-value");
            cell.title = `${DAYS[d.getDay()]} ${d.getDate()}, ${String(h).padStart(2, "0")}:00 · ${word(v)}`;
        }
    }
    sub.textContent = values.length ? `Lighter to darker: ${word(lo)} to ${word(hi)}, by local hour` : none + " in the last 7 days";
}

export function init() {
    grid = document.getElementById("chart-busy");
    if (!grid) return;
    panel = grid.closest(".graph-panel");
    sub = panel.querySelector(".polar-sub");
}

export function update(h) {
    if (!panel) return;
    ring = h && h.hour && h.hour.time.length ? h.hour : null;
    panel.classList.toggle("polar-none", !ring);
    const told = (name) => !!ring && (ring.reception || []).some((s) => s.name === name);
    panel.querySelector('[data-measure="noise"]').hidden = !told("noise_floor");
    panel.querySelector('[data-measure="load"]').hidden = !told("channel_load");
    panel.querySelector('[data-measure="clipping"]').hidden = !told("clipping");
    setMeasure(panel.querySelector(`[data-measure="${measure}"]`).hidden ? "messages" : measure);
}

export function setMeasure(m) {
    measure = m;
    panel.querySelectorAll("[data-measure]").forEach((b) => b.classList.toggle("active", b.dataset.measure === measure));
    draw();
}
