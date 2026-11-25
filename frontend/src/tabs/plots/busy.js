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
// counted over the hour, so the hour under way is not whole yet
const COUNTED = ["messages", "vessels"];

// Today against the six days before it, hour by hour on the grid's own columns: the band runs from
// their lowest to their highest with the median in it, the line is today up to now. With fewer than
// three earlier days there is no band yet.
function trend(cells, word, W) {
    const at = (back, h) => {
        const d = new Date();
        d.setDate(d.getDate() - back);
        return cells.get(day(d) * 24 + h);
    };
    const hours = [...Array(24).keys()], earlier = [1, 2, 3, 4, 5, 6], now = new Date().getHours();
    const known = earlier.filter((b) => hours.some((h) => at(b, h) !== undefined)).length >= 3;
    const before = hours.map((h) => (known ? earlier.map((b) => at(b, h)).filter((v) => v !== undefined).sort((a, b) => a - b) : []));
    const band = before.map((v) => (v.length >= 2 ? [v[0], v[Math.floor(v.length / 2)], v[v.length - 1]] : null));
    const today = hours.map((h) => (h <= now ? at(0, h) : undefined)), whole = COUNTED.includes(measure) ? now - 1 : now;
    const all = [...band.filter(Boolean).flat(), ...today.filter((v) => v !== undefined)];
    if (!all.length || W < 100) return [""];
    const lo = COUNTED.includes(measure) ? 0 : Math.min(...all), hi = Math.max(...all), H = 84, cw = (W - 46) / 24;
    const x = (h) => h * (cw + 2) + cw / 2, y = (v) => 8 + (H - 16) * (1 - (hi > lo ? (v - lo) / (hi - lo) : 0.5));
    const p = (h, v) => `${x(h).toFixed(1)} ${y(v).toFixed(1)}`;
    // a run of hours that all have a value, drawn as one stroke
    const runs = (has) => {
        const out = [];
        for (const h of hours) if (has(h)) has(h - 1) ? out[out.length - 1].push(h) : out.push([h]);
        return out;
    };
    const usual = runs((h) => !!band[h]);
    let g = usual.map((r) => `<path class="busy-band" d="M${r.map((h) => p(h, band[h][2])).join("L")}L${r.slice().reverse().map((h) => p(h, band[h][0])).join("L")}Z"/>`).join("");
    // dotted along the lowest and the highest, dashed along the median
    for (const [k, cls] of [[0, "busy-edge"], [1, "busy-median"], [2, "busy-edge"]]) g += usual.map((r) => `<path class="${cls}" d="M${r.map((h) => p(h, band[h][k])).join("L")}"/>`).join("");
    g += runs((h) => h <= whole && today[h] !== undefined).map((r) => `<path class="busy-today" d="M${r.map((h) => p(h, today[h])).join("L")}"/>`).join("");
    if (today[now] !== undefined) g += `<circle class="${whole < now ? "busy-now" : "busy-now whole"}" cx="${x(now).toFixed(1)}" cy="${y(today[now]).toFixed(1)}" r="3.5"/>`;
    const short = (v) => word(v).split(" ")[0];
    g += hours.map((h) => (band[h] || today[h] !== undefined ? `<rect x="${(h * (cw + 2)).toFixed(1)}" y="0" width="${cw.toFixed(1)}" height="${H}" fill="transparent"><title>${String(h).padStart(2, "0")}:00${today[h] !== undefined ? ` · today ${word(today[h])}${h > whole ? ", so far" : ""}` : ""}${band[h] ? ` · the days before ${short(band[h][0])} to ${word(band[h][2])}` : ""}</title></rect>` : "")).join("");
    return [`<svg width="${W}" height="${H}" viewBox="0 0 ${W} ${H}">${g}</svg>`, short(hi), short(lo), usual.length > 0];
}

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
    // the strip's own scale stands beside it, where the grid has its days
    const label = add("busy-day busy-scale"), strip = add("busy-trend");
    const [svg, top = "", bottom = "", ranged] = values.length ? trend(cells, word, strip.clientWidth) : [""];
    strip.innerHTML = svg;
    label.innerHTML = `<small>${top}</small>Today<small>${bottom}</small>`;
    label.hidden = strip.hidden = !svg;
    sub.textContent = values.length ? `Lighter to darker: ${word(lo)} to ${word(hi)}, by local hour. Line: today${ranged ? ", over the range of the six days before" : svg ? "; the range of the days before shows after three of them" : ""}` : none + " in the last 7 days";
}

export function init() {
    grid = document.getElementById("chart-busy");
    if (!grid) return;
    panel = grid.closest(".graph-panel");
    sub = panel.querySelector(".polar-sub");
    // the strip under the grid is drawn to the grid's width
    let width = 0;
    if (window.ResizeObserver) new ResizeObserver(() => grid.clientWidth !== width && ((width = grid.clientWidth), draw())).observe(grid);
}

export function update(h) {
    if (!panel) return;
    // the same hours as are drawn: the page asks for them once a minute and polls far more often
    if (ring && h && h.hour === ring) return;
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
