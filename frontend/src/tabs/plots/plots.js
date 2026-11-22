
import { chart } from './chart.js';
import * as busy from './busy.js';
import * as coverage from './coverage.js';
import * as sky from './sky.js';
import { settings } from '../../core/state.js';
import { getDistanceConversion, getDistanceUnit } from '../../core/units.js';
import { config, stationLabel } from '../../core/config.js';
import { focus } from '../../data.js';
import { createPicker } from '@aiscatcher/ui/picker.js';
import { focusOptions, mountFocusPicker } from '../../features/station/station.js';
import { register } from '../../actions.js';

const C1 = "--color-chart-1", C2 = "--color-chart-2", C4 = "--color-chart-4", C5 = "--color-chart-5", C6 = "--color-chart-6";

const seriesName = (id) => (id < 0 ? "All stations" : stationLabel(id));
const points = (ring, value) => ring.time.map((t, i) => ({ x: t, y: value(ring.stat[i]) }));
const line = (label, color, pts, more) => ({ label, color, points: pts, width: 2, ...more });
const sum = (msg, types) => types.reduce((n, t) => n + msg[t], 0);

function countSeries(r) {
    const a = (s) => sum(s.msg, [0, 1, 2, 4]), b = (s) => sum(s.msg, [17, 18, 23]), area = { fill: "stack", width: 1 };
    return [
        line("Class A", C1, points(r, a), area),
        line("Class B", C2, points(r, b), area),
        line("Base Station", C5, points(r, (s) => s.msg[3]), area),
        line("Other", C6, points(r, (s) => s.count - a(s) - b(s) - s.msg[3]), area),
    ];
}

const single = (key) => (r) => [line("Data", C4, points(r, (s) => s[key]))];

const unit = (s) => (s.unit ? " (" + s.unit + ")" : "");
const told = (r, name, label) => busy.reported(r, name).sort((a, b) => (a.channel < b.channel ? -1 : 1)).slice(0, 2)
    .map((s, i) => line(label + (s.channel ? " " + s.channel : "") + unit(s), [C4, C2][i], r.time.map((t, k) => ({ x: t, y: s.values[k] })), { width: 1.5 }));

const has = (r, name) => (r.reception || []).some((s) => s.name === name);

function levelSeries(r) {
    const peaks = busy.reported(r, "peak");
    return [
        line("Max", C1, points(r, (s) => s.level_max), { width: 1 }),
        line("Min", C1, points(r, (s) => s.level_min), { width: 1, fill: "band" }),
        ...told(r, "noise_floor", "Noise floor"),
        ...(peaks.length ? [line("Peak" + unit(peaks[0]), C5, r.time.map((t, k) => ({ x: t, y: Math.max(...peaks.map((s) => s.values[k] ?? -Infinity)) })), { width: 1 })] : []),
    ];
}

let measure = "load";

function loadSeries(r) {
    const clipping = has(r, "clipping"), m = clipping ? measure : "load";
    const panel = document.getElementById("chart-load").closest(".graph-panel");
    panel.querySelector("header").textContent = m === "clipping" ? "Clipping (%)" : "Channel Load (%)";
    panel.querySelectorAll("[data-measure]").forEach((b) => {
        b.hidden = b.dataset.measure === "clipping" && !clipping;
        b.classList.toggle("active", b.dataset.measure === m);
    });
    return m === "clipping" ? told(r, "clipping", "Clipping") : told(r, "channel_load", "Channel");
}

function distanceSeries(r) {
    const n = r.stat[0].radar_a.length;
    if (!n) return null;
    const quarter = (q) => points(r, (s) => getDistanceConversion(Math.max(0, ...s.radar_a.slice((q * n) / 4, ((q + 1) * n) / 4))));
    return [
        line("NE", C4, quarter(0)), line("SE", C1, quarter(1)), line("SW", C2, quarter(2)), line("NW", C5, quarter(3)),
        line("Max", "--color-chart-envelope", points(r, (s) => getDistanceConversion(s.dist)), { fill: "floor", width: 0 }),
    ];
}

const count = { legend: true, average: true, zero: true, yTitle: "Message Count", xTitle: "Time" };
const one = { average: true, zero: true };
const bars = { average: true, zero: true, bars: true, yTitle: "Vessel Count", xTitle: "Time" };
const far = { legend: true, log: true };
const share = { legend: true, zero: true };

const OVERVIEW = {
    "chart-messages": [count, "minute", countSeries],
    "chart-vessels": [bars, "minute", single("vessels")],
    "chart-ppm": [one, "minute", single("ppm")],
    "chart-level": [{}, "minute", levelSeries],
    "chart-load": [share, "minute", loadSeries, (r) => has(r, "channel_load") || has(r, "clipping")],
    "chart-distance": [far, "hour", distanceSeries],
};

const COMPARE = {
    "chart-compare-messages": ["count", "Message Count"],
    "chart-compare-gain": [null, "More messages than A (%)"],
    "chart-compare-vessels": ["vessels", "Vessel Count"],
};

const charts = {};
let mode = "overview";
let sourceB = null; // what the station in focus is compared with; -1 for all stations
let pickerA = null, pickerB = null;
let updateToken = 0;
let absoluteTime = !!settings.plot_absolute_time;

function stationsListed() {
    return config.stations || [];
}

function setMode(next) {
    if (next !== "compare") next = "overview";
    if (next === mode) return;
    mode = next;
    if (mode === "compare" && sourceB === null) {
        const other = stationsListed().find((s) => s.id !== focus);
        sourceB = focus < 0 || !other ? -1 : other.id;
    }
    update();
}

// the other station is never the one in focus, and one the server no longer lists goes
function settleSourceB() {
    if (sourceB === null) return;
    if (sourceB >= 0 && !stationsListed().some((s) => s.id === sourceB)) sourceB = -1;
    if (sourceB === focus) {
        const other = stationsListed().find((s) => s.id !== focus);
        sourceB = focus < 0 ? (other ? other.id : -1) : -1;
    }
    if (pickerB) pickerB.set(sourceB);
}

function applyMode() {
    const multi = stationsListed().length > 1;
    if (!multi) mode = "overview";

    const tabs = document.getElementById("plots_modes");
    if (tabs) {
        tabs.style.display = multi ? "flex" : "none";
        for (const tab of tabs.querySelectorAll(".sheet-tab"))
            tab.classList.toggle("active", tab.dataset.mode === mode);
    }

    const bar = document.getElementById("chart_sources");
    if (bar) bar.style.display = multi && mode === "compare" ? "flex" : "none";

    const plots = document.getElementById("plots");
    if (plots) plots.classList.toggle("compare-mode", mode === "compare");

    if (multi) buildSourceSelectors();
}

function buildSourceSelectors() {
    const hostA = document.getElementById("plots_scope");
    const hostB = document.getElementById("chart_source_b");
    if (!hostA || !hostB) return;
    if (!pickerA) pickerA = mountFocusPicker(hostA, { scope: true });
    if (!pickerB) {
        pickerB = createPicker({ label: "Stations", value: sourceB === null ? -1 : sourceB, options: focusOptions,
            empty: "No station by that name", onPick: (id) => { sourceB = id; update(); } });
        pickerB.el.classList.add("pick--scope");
        hostB.append(pickerB.el);
    }
    if (sourceB !== null) pickerB.set(sourceB);
}

const fetched = new Map();

async function fetchHistory(idx) {
    const used = new Set(Object.values(period)), slots = (f, n) => (used.has(f) ? n : 0), now = Date.now();
    const url = `api/history_full.json?second=${slots("second", 60)}&minute=${slots("minute", 60)}&hour=168&day=${slots("day", 365)}&stations=${idx}`;
    for (const [u, last] of fetched) if (now - last.at > 10000) fetched.delete(u);
    if (fetched.has(url) && !used.has("second")) return fetched.get(url).h;
    try {
        const response = await fetch(url);
        if (!response.ok) return null;
        const h = await response.json();
        fetched.set(url, { at: now, h });
        return h;
    } catch (error) {
        console.error("plots: history fetch failed:", error);
        return null;
    }
}

export function setAbsoluteTime(on) {
    if (!!on === absoluteTime) return;
    absoluteTime = !!on;
    Object.values(charts).forEach((c) => c.draw());
}

const clock = (d) => d.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
const day = (d) => d.toLocaleDateString([], { day: "numeric", month: "short" });
const two = (n) => ":" + String(n).padStart(2, "0");

function wording(ring) {
    const date = (x) => new Date((ring.now + x * ring.interval) * 1000), step = ring.interval;
    const timed = () => absoluteTime && typeof ring.now === "number" && typeof step === "number";
    return {
        tick: (x, before) => {
            if (!timed()) return x;
            const d = date(x), b = before === undefined ? null : date(before);
            if (step >= 86400) return day(d);
            if (step >= 3600) return !b || b.getDate() !== d.getDate() ? day(d) : clock(d);
            if (step >= 60) return !b || b.getHours() !== d.getHours() ? clock(d) : two(d.getMinutes());
            return !b || b.getMinutes() !== d.getMinutes() ? clock(d) : two(d.getSeconds());
        },
        title: (x) => {
            if (!timed()) return String(x);
            const d = date(x);
            if (step >= 86400) return day(d);
            return day(d) + " " + (step < 60 ? d.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit", second: "2-digit" }) : clock(d));
        },
    };
}

const EMPTY = { time: [], stat: [] };

const period = {};
for (const [id, [, ring]] of Object.entries(OVERVIEW)) period[id] = ring;
for (const id of Object.keys(COMPARE)) period[id] = "minute";
let shown = null, compared = null;

function drawComparePanel(id, [a, b, srcA, srcB]) {
    const [key] = COMPARE[id], f = period[id];
    const A = a[f] || EMPTY, B = b[f] || EMPTY, shift = a[f] && b[f] && A.interval ? Math.round((B.now - A.now) / A.interval) : 0;
    const other = points(B, (s) => s[key || "count"]).map((p) => ({ x: p.x + shift, y: p.y }));
    if (key) {
        charts[id].draw({ series: [line(seriesName(srcA), C4, points(A, (s) => s[key])), line(seriesName(srcB), C1, other)], ...wording(A) });
        return;
    }
    const theirs = new Map(other.map((p) => [p.x, p.y])), shared = points(A, (s) => s.count).filter((p) => p.y > 0 && theirs.has(p.x));
    const totalA = shared.reduce((n, p) => n + p.y, 0), totalB = shared.reduce((n, p) => n + theirs.get(p.x), 0);
    const gain = totalA > 0 ? (100 * (totalB - totalA)) / totalA : 0;
    charts[id].draw({
        series: [line(seriesName(srcB) + " vs " + seriesName(srcA), C1, shared.map((p) => ({ x: p.x, y: (100 * (theirs.get(p.x) - p.y)) / p.y })))],
        mark: shared.length ? { value: gain, text: (gain >= 0 ? "+" : "") + gain.toFixed(1) + "%" } : null,
        ...wording(A),
    });
}

function drawCompare(...args) {
    compared = args;
    for (const id of Object.keys(COMPARE)) drawComparePanel(id, args);
}

function drawPanel(id, h) {
    const [, , series, shows] = OVERVIEW[id], f = period[id];
    if (!h[f] || (h.range_hidden && series === distanceSeries)) return;
    const list = series(h[f]);
    if (shows) document.getElementById(id).closest(".graph-panel").classList.toggle("polar-none", !shows(h[f]));
    if (list) charts[id].draw({ series: list, legend: series === levelSeries ? list.length > 2 : undefined, ...wording(h[f]) });
}

function setPeriod(id, f) {
    period[id] = f;
    document.getElementById(id).closest(".graph-panel").querySelectorAll("[data-period]").forEach((b) => b.classList.toggle("active", b.dataset.period === f));
    const have = COMPARE[id] ? compared && compared[0] : shown;
    if (have && !have[f]) update();
    else if (COMPARE[id] && compared) drawComparePanel(id, compared);
    else if (OVERVIEW[id] && shown) drawPanel(id, shown);
}

function drawOverview(h) {
    shown = h;
    for (const id of Object.keys(OVERVIEW)) drawPanel(id, h);
    busy.update(h);
    if (h.range_hidden) return;
    coverage.update(h);
    sky.update(h);
}

function init() {
    if (Object.keys(charts).length) return;
    const host = (id) => document.getElementById(id);
    for (const [id, [options]] of Object.entries(OVERVIEW)) charts[id] = chart(host(id), options);
    for (const [id, [key, yTitle]] of Object.entries(COMPARE)) charts[id] = chart(host(id), { legend: true, zero: true, bars: !key, yTitle, xTitle: "Time" });
    for (const [id, f] of Object.entries(period)) setPeriod(id, f);
    busy.init();
    coverage.init();
    sky.init();
}

export async function update() {
    init();

    applyMode();
    const token = ++updateToken;
    const unit = getDistanceUnit().toUpperCase();
    document.querySelectorAll(".distunit").forEach((u) => {
        u.textContent = unit;
    });

    if (mode === "compare") {
        settleSourceB();
        const a = focus, b = sourceB === null ? -1 : sourceB;
        const [ha, hb] = await Promise.all([fetchHistory(a), fetchHistory(b)]);
        if (ha && hb && token === updateToken) drawCompare(ha, hb, a, b);
        return;
    }

    const h = await fetchHistory(focus);
    if (!h || token !== updateToken) return;

    document.getElementById("plots").classList.toggle("range-hidden", !!h.range_hidden);
    drawOverview(h);
}

export function updateColors() {
    coverage.repaint();
    sky.repaint();
}

register({
    setPlotsMode: (e, d) => setMode(d.mode),
    setPlotPeriod: (e, d) => setPeriod(d.chart, d.period),
    setLoadMeasure: (e, d) => {
        measure = d.measure;
        if (shown) drawPanel("chart-load", shown);
    },
    setBusyMeasure: (e, d) => busy.setMeasure(d.measure),
    setCoveragePeriod: (e, d) => coverage.setPeriod(d.period),
    setCoverageMeasure: (e, d) => coverage.setMeasure(d.measure),
});
