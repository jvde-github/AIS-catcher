// Manual Chart.js registration (vs `chart.js/auto`) to tree-shake out chart
// types we don't use. Only scatter (line), bar and polarArea are needed.
import {
    Chart,
    LineController, ScatterController, PolarAreaController, BarController,
    PointElement, LineElement, ArcElement, BarElement,
    LinearScale, LogarithmicScale, RadialLinearScale,
    Tooltip, Legend, Filler,
} from 'chart.js';
import Annotation from 'chartjs-plugin-annotation';
import { getDistanceConversion, getDistanceUnit } from '../core/format.js';

Chart.register(
    LineController, ScatterController, PolarAreaController, BarController,
    PointElement, LineElement, ArcElement, BarElement,
    LinearScale, LogarithmicScale, RadialLinearScale,
    Tooltip, Legend, Filler,
    Annotation,
);

Chart.defaults.font.family = 'ui-sans-serif, system-ui, sans-serif';

const charts = {};
let initialized = false;


function cssvar(name) {
    // documentElement first for iframe compatibility.
    const value = getComputedStyle(document.documentElement).getPropertyValue(name).trim();
    return value || getComputedStyle(document.body).getPropertyValue(name).trim();
}

function average(d) {
    const b = d.chart.data.datasets[0].data;
    if (b.length == 1) return b[0].y;

    let start = 0;
    if (d.chart.data.datasets.length > 1) start = 1;

    let c = 0;
    for (let a = 0; a < b.length; a++) {
        if (b[a].x != 0) {
            for (let i = start; i < d.chart.data.datasets.length; i++) {
                if (!d.chart.getDatasetMeta(i).hidden) {
                    c += d.chart.data.datasets[i].data[a].y;
                }
            }
        }
    }
    return c / (b.length - 1);
}

// Enough precision to be useful without crowding the chip.
function formatAverage(v) {
    if (!Number.isFinite(v)) return "";
    return Math.abs(v) >= 100 ? Math.round(v).toString() : v.toFixed(1);
}

function cloneChartConfig(config) {
    const clone = JSON.parse(JSON.stringify(config));
    // the JSON round trip drops scriptable options, so re-attach them
    const a = clone.options?.plugins?.annotation?.annotations?.graph_annotation;
    if (a) {
        a.value = (d) => average(d);
        a.label.display = (d) => Number.isFinite(average(d));
        a.label.content = (d) => formatAverage(average(d));
    }
    return clone;
}

const FILL_ALPHA = 0.55;

function withAlpha(color, alpha) {
    const hex = color.replace("#", "");
    if (!/^[0-9a-f]{3}$|^[0-9a-f]{6}$/i.test(hex)) return color;
    const full = hex.length === 3 ? hex.replace(/./g, (d) => d + d) : hex;
    const n = parseInt(full, 16);
    return `rgba(${(n >> 16) & 255}, ${(n >> 8) & 255}, ${n & 255}, ${alpha})`;
}

function updateChartColors(c, colorVariables) {
    c.data.datasets.forEach((dataset, index) => {
        const color = cssvar(colorVariables[index]);
        dataset.borderColor = color;
        dataset.backgroundColor = dataset.fill ? withAlpha(color, FILL_ALPHA) : color;
    });
    const ink = cssvar("--chart-color");
    const grid = cssvar("--chart-grid-color");

    // chart.options is a proxy whose setters recurse when enumerated
    const scales = c.config.options?.scales || {};
    Object.keys(scales).forEach((id) => {
        const scale = scales[id];
        if (!scale) return;
        if (scale.ticks) scale.ticks.color = ink;
        if (scale.grid) scale.grid.color = grid;
        if (scale.title) scale.title.color = ink;
    });
    c.options.plugins.legend.labels.color = ink;
    updateTooltipColors(c);
    updateAnnotationColors(c);
}

function updateTooltipColors(c) {
    const opts = c.config.options;
    if (!opts) return;
    opts.plugins = opts.plugins || {};
    opts.plugins.tooltip = opts.plugins.tooltip || {};

    const ink = cssvar("--chart-color");
    Object.assign(opts.plugins.tooltip, {
        backgroundColor: cssvar("--tooltip-background-color"),
        borderColor: cssvar("--tooltip-border-color"),
        borderWidth: 1,
        titleColor: ink,
        bodyColor: ink,
    });
}

// The average chip borrows the hover tooltip's surface so the two read as one
// layer, and its text stays an ink colour rather than the series colour.
function updateAnnotationColors(c) {
    const annotations = c.options?.plugins?.annotation?.annotations;
    if (!annotations) return;

    Object.values(annotations).forEach((a) => {
        if (!a.label) return;
        a.label.backgroundColor = cssvar("--tooltip-background-color");
        a.label.borderColor = cssvar("--tooltip-border-color");
        a.label.color = cssvar("--chart-color");
    });
}

const COLORS_SERIES = ["--chart4-color", "--chart1-color", "--chart2-color", "--chart5-color", "--chart6-color"];
const COLORS_DISTANCE = ["--chart4-color", "--chart1-color", "--chart2-color", "--chart5-color", "--chart-envelope-color"];

function updateColorMulti(c) {
    updateChartColors(c, COLORS_SERIES);
}

function updateColorRadar(c) {
    const colorVariables = ["--chart2-color", "--chart1-color"];
    c.data.datasets.forEach((dataset, index) => {
        const color = cssvar(colorVariables[index]);
        dataset.backgroundColor = withAlpha(color, FILL_ALPHA);
        dataset.borderColor = color;
    });
    c.options.scales.r.grid.color = cssvar("--chart-grid-color");
    c.options.scales.r.ticks.color = cssvar("--chart-color");
    c.options.scales.r.ticks.backdropColor = cssvar("--panel-color");
    updateTooltipColors(c);
}


// One average line for every chart: same dash, width and colour, with the value
// in a chip. Colours are filled in by updateAnnotationColors() per theme.
const AVERAGE_LINE_COLOR = "rgb(12, 118, 170)";

const average_label = {
    display: true,
    position: "start",
    borderWidth: 1,
    borderRadius: 4,
    font: { size: 11 },
    padding: { top: 2, bottom: 2, left: 6, right: 6 },
    content: "",
};

const graph_annotation = {
    type: "line",
    borderColor: AVERAGE_LINE_COLOR,
    borderDash: [6, 6],
    borderDashOffset: 0,
    borderWidth: 3,
    scaleID: "y",
    value: (a) => average(a),
    label: {
        ...average_label,
        display: (d) => Number.isFinite(average(d)),
        content: (d) => formatAverage(average(d)),
    },
};

const graph_options_count = {
    responsive: true,
    plugins: {
        legend: { display: true },
        annotation: { annotations: { graph_annotation } },
    },
    elements: { point: { radius: 1 } },
    animation: false,
    tooltips: { mode: "index", intersect: false },
    hover: { mode: "index", intersect: false },
    scales: {
        y: {
            stacked: true,
            beginAtZero: true,
            ticks: { display: true, align: 'center' },
            grid: { display: true },
            title: { display: true, text: 'Message Count', font: { size: 12 } },
        },
        y_right: {
            stacked: true,
            beginAtZero: true,
            ticks: { display: true, align: 'center' },
            grid: { display: false },
            position: 'right',
            title: { display: true, text: 'Vessel Count', font: { size: 12 } },
        },
        x: {
            ticks: { display: true },
            title: { display: true, text: 'Time', font: { size: 16 } },
        },
    },
};

const graph_options_single = {
    responsive: true,
    plugins: {
        legend: { display: false },
        annotation: { annotations: { graph_annotation } },
    },
    elements: { point: { radius: 1 } },
    animation: false,
    tooltips: { mode: "index", intersect: false },
    hover: { mode: "index", intersect: false },
    scales: {
        y: { stacked: true, beginAtZero: true, ticks: { display: true } },
        x: { ticks: { display: true } },
    },
};

const graph_options_level = {
    responsive: true,
    plugins: { legend: { display: false } },
    elements: { point: { radius: 1 } },
    animation: false,
    tooltips: { mode: "index", intersect: false },
    hover: { mode: "index", intersect: false },
    scales: {
        y: { beginAtZero: false, ticks: { display: true } },
        x: { ticks: { display: true } },
    },
};

const graph_options_distance = {
    responsive: true,
    plugins: { legend: { display: true } },
    elements: { point: { radius: 1 } },
    animation: false,
    tooltips: { mode: "index", intersect: false },
    hover: { mode: "index", intersect: false },
    scales: {
        y: { type: 'logarithmic', beginAtZero: false, ticks: { display: true } },
        x: { ticks: { display: true } },
    },
};

const plot_count = {
    type: "scatter",
    data: {
        datasets: [
            { label: "Vessel Count", data: [], type: "line", showLine: true, fill: false, pointStyle: false, borderWidth: 2, yAxisID: 'y_right' },
            { label: "Class A", data: [], showLine: true, fill: "origin", borderWidth: 2, pointStyle: false },
            { label: "Class B", data: [], showLine: true, fill: "-1", borderWidth: 2, pointStyle: false },
            { label: "Base Station", data: [], showLine: true, fill: "-1", borderWidth: 2, pointStyle: false },
            { label: "Other", data: [], showLine: true, fill: "-1", borderWidth: 2, pointStyle: false },
        ],
    },
    options: graph_options_count,
};

const plot_distance = {
    type: "scatter",
    data: {
        datasets: [
            { label: "NE", data: [], showLine: true, pointStyle: false, borderWidth: 2 },
            { label: "SE", data: [], showLine: true, pointStyle: false, borderWidth: 2 },
            { label: "SW", data: [], showLine: true, pointStyle: false, borderWidth: 2 },
            { label: "NW", data: [], showLine: true, pointStyle: false, borderWidth: 2 },
            { label: "Max", data: [], showLine: true, fill: true, pointStyle: false, borderWidth: 0 },
        ],
    },
    options: graph_options_distance,
};

const plot_single = {
    type: "scatter",
    data: {
        datasets: [
            { label: "Data", data: [], showLine: true, pointStyle: false, borderWidth: 2 },
        ],
    },
    options: graph_options_single,
};

const plot_level = {
    type: "scatter",
    data: {
        datasets: [
            { label: "Max", data: [], showLine: true, pointStyle: false, borderWidth: 1 },
            { label: "Min", data: [], showLine: true, pointStyle: false, borderWidth: 1, fill: "-1" },
        ],
    },
    options: graph_options_level,
};

const plot_radar = {
    type: "polarArea",
    data: {
        datasets: [
            { label: "Class B", borderWidth: 1 },
            { label: "Class A", borderWidth: 1 },
        ],
    },
    options: {
        animation: false,
        responsive: true,
        plugins: { legend: { display: true } },
    },
};


const plot_compare = (axis) => ({
    type: "scatter",
    data: {
        datasets: [
            { label: "A", data: [], showLine: true, fill: false, pointStyle: false, borderWidth: 2 },
            { label: "B", data: [], showLine: true, fill: false, pointStyle: false, borderWidth: 2 },
        ],
    },
    options: {
        responsive: true,
        plugins: { legend: { display: true } },
        elements: { point: { radius: 1 } },
        animation: false,
        scales: {
            y: {
                beginAtZero: true,
                ticks: { display: true, align: "center" },
                grid: { display: true },
                title: { display: true, text: axis, font: { size: 12 } },
            },
            x: {
                ticks: { display: true },
                title: { display: true, text: "Time", font: { size: 16 } },
            },
        },
    },
});

const gain_annotation = {
    type: "line",
    borderColor: AVERAGE_LINE_COLOR,
    borderDash: [6, 6],
    borderDashOffset: 0,
    borderWidth: 3,
    scaleID: "y",
    value: 0,
    label: { ...average_label },
};

const plot_gain = {
    type: "bar",
    data: {
        datasets: [
            { label: "Gain", data: [], borderWidth: 0 },
        ],
    },
    options: {
        responsive: true,
        plugins: {
            legend: { display: true },
            annotation: { annotations: { gain_annotation } },
        },
        animation: false,
        scales: {
            y: {
                beginAtZero: true,
                ticks: { display: true, align: "center" },
                grid: { display: true },
                title: { display: true, text: "More messages than A (%)", font: { size: 12 } },
            },
            x: {
                type: "linear",
                offset: true,
                ticks: { display: true },
                title: { display: true, text: "Time", font: { size: 16 } },
            },
        },
    },
};

// A is the viewer-wide scope (shared with the top bar); B is chart-only
let sourceB = null;
let updateToken = 0;

function receiverLabel(idx) {
    const r = (window.__app__.receivers || []).find((x) => x.idx === idx);
    return r ? r.label : `Receiver ${idx}`;
}

function buildSourceSelectors() {
    const bar = document.getElementById("chart_sources");
    const selA = document.getElementById("chart_source_a");
    const selB = document.getElementById("chart_source_b");
    if (!bar || !selA || !selB) return;

    const receivers = window.__app__.receivers || [];
    if (receivers.length <= 1) {
        bar.style.display = "none";
        return;
    }
    bar.style.display = "flex";
    if (selA.options.length === receivers.length) return;

    selA.innerHTML = "";
    selB.innerHTML = "";
    for (const r of receivers) {
        selA.appendChild(new Option(r.label, r.idx));
        selB.appendChild(new Option(r.label, r.idx));
    }
    selB.insertBefore(new Option("none", ""), selB.firstChild);

    selA.value = String(window.__app__.activeReceiver);
    selB.value = sourceB === null ? "" : String(sourceB);

    selA.onchange = () => window.__app__.setReceiver(parseInt(selA.value, 10));
    selB.onchange = () => { sourceB = selB.value === "" ? null : parseInt(selB.value, 10); update(); };
}

async function fetchHistory(idx) {
    try {
        const response = await fetch("api/history_full.json?receiver=" + idx);
        if (!response.ok) return null;
        return await response.json();
    } catch (error) {
        console.error("plots: history fetch failed:", error);
        return null;
    }
}

const series = (h, f, key) => (h && h[f] ? h[f].time.map((t, i) => ({ x: t, y: h[f].stat[i][key] })) : []);

function setTimeAxis(c, source) {
    if (!c || !source || typeof source.now !== "number" || typeof source.interval !== "number") return;
    c.$timeAxis = { now: source.now, interval: source.interval };
}

let absoluteTime = true;

export function setAbsoluteTime(on) {
    absoluteTime = !!on;
    Object.values(charts).forEach((c) => c && c.update());
}

const bucketDate = (value, axis) => new Date((axis.now + Math.round(value) * axis.interval) * 1000);
const clock = (d) => d.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
const day = (d) => d.toLocaleDateString([], { day: "numeric", month: "short" });

function formatTimeTick(value, axis, prev) {
    // fractional steps fall between buckets
    if (Math.abs(value - Math.round(value)) > 1e-9) return "";

    const d = bucketDate(value, axis);
    const before = prev === undefined ? null : bucketDate(prev, axis);

    if (axis.interval >= 86400) return day(d);

    if (axis.interval >= 3600) {
        return !before || before.getDate() !== d.getDate() ? day(d) : clock(d);
    }

    if (axis.interval >= 60) {
        return !before || before.getHours() !== d.getHours() ? clock(d) : ":" + String(d.getMinutes()).padStart(2, "0");
    }

    return !before || before.getMinutes() !== d.getMinutes() ? clock(d) : ":" + String(d.getSeconds()).padStart(2, "0");
}

function formatTimeTooltip(value, axis) {
    const d = bucketDate(value, axis);
    if (axis.interval >= 86400) return day(d);
    if (axis.interval < 60) {
        return day(d) + " " + d.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit", second: "2-digit" });
    }
    return day(d) + " " + clock(d);
}

// cloneChartConfig's JSON round trip drops functions, so these attach to the live chart
function attachTimeAxis(c) {
    const opts = c.config.options;
    const x = opts?.scales?.x;
    if (!x) return;

    x.ticks = x.ticks || {};
    x.ticks.maxRotation = 0;
    x.ticks.autoSkipPadding = 12;
    x.ticks.callback = function (value, index, ticks) {
        const axis = this.chart.$timeAxis;
        if (!axis || !absoluteTime) return value;
        return formatTimeTick(value, axis, index > 0 ? ticks[index - 1].value : undefined);
    };

    opts.plugins = opts.plugins || {};
    opts.plugins.tooltip = opts.plugins.tooltip || {};
    opts.plugins.tooltip.callbacks = {
        title: (items) => {
            if (!items.length) return "";
            const axis = c.$timeAxis;
            return axis && absoluteTime ? formatTimeTooltip(items[0].parsed.x, axis) : String(items[0].parsed.x);
        },
        label: (item) => {
            const y = item.parsed.y;
            const value = Number.isInteger(y) ? y.toLocaleString() : y.toFixed(1);
            return `${item.dataset.label}: ${value}`;
        },
    };
}

function updateChartCompare(a, b, f, key, c, srcA, srcB) {
    if (!c) return;
    c.data.datasets[0].label = receiverLabel(srcA);
    c.data.datasets[0].data = series(a, f, key);
    c.data.datasets[1].label = receiverLabel(srcB);
    c.data.datasets[1].data = series(b, f, key);
    setTimeAxis(c, a && a[f]);
    c.update();
}

function updateChartGain(a, b, f, c, srcA, srcB) {
    if (!c) return;
    const other = new Map(series(b, f, "count").map((p) => [p.x, p.y]));
    const shared = series(a, f, "count").filter((p) => p.y > 0 && other.has(p.x));

    // the period average weighs each bucket by its message count, so quiet
    // minutes do not count as much as busy ones
    const totalA = shared.reduce((s, p) => s + p.y, 0);
    const totalB = shared.reduce((s, p) => s + other.get(p.x), 0);
    const average = totalA > 0 ? (100 * (totalB - totalA)) / totalA : 0;

    c.data.datasets[0].label = receiverLabel(srcB) + " vs " + receiverLabel(srcA);
    c.data.datasets[0].data = shared.map((p) => ({ x: p.x, y: (100 * (other.get(p.x) - p.y)) / p.y }));

    // with nothing to compare against, an average of 0 would read as parity
    const annotation = c.options.plugins.annotation.annotations.gain_annotation;
    annotation.display = shared.length > 0;
    annotation.value = average;
    annotation.label.content = (average >= 0 ? "+" : "") + average.toFixed(1) + "%";
    updateAnnotationColors(c);
    setTimeAxis(c, a && a[f]);
    c.update();
}

function init() {
    if (initialized) return;

    const chartConfigs = [
        { varName: "chart_radar_hour", id: "chart-radar-hour", config: plot_radar },
        { varName: "chart_radar_day", id: "chart-radar-day", config: plot_radar },
        { varName: "chart_compare_minutes", id: "chart-compare-minutes", config: plot_compare("Message Count") },
        { varName: "chart_compare_hours", id: "chart-compare-hours", config: plot_compare("Message Count") },
        { varName: "chart_compare_gain_minute", id: "chart-compare-gain-minute", config: plot_gain },
        { varName: "chart_compare_gain_hour", id: "chart-compare-gain-hour", config: plot_gain },
        { varName: "chart_compare_vessels_minute", id: "chart-compare-vessels-minute", config: plot_compare("Vessel Count") },
        { varName: "chart_compare_vessels_hour", id: "chart-compare-vessels-hour", config: plot_compare("Vessel Count") },
        { varName: "chart_seconds", id: "chart-seconds", config: plot_count },
        { varName: "chart_minutes", id: "chart-minutes", config: plot_count },
        { varName: "chart_hours", id: "chart-hours", config: plot_count },
        { varName: "chart_days", id: "chart-days", config: plot_count },
        { varName: "chart_ppm", id: "chart-ppm", config: plot_single },
        { varName: "chart_ppm_minute", id: "chart-ppm-minute", config: plot_single },
        { varName: "chart_distance_hour", id: "chart-distance-hour", config: plot_distance },
        { varName: "chart_distance_day", id: "chart-distance-day", config: plot_distance },
        { varName: "chart_minute_vessel", id: "chart-vessels-minute", config: plot_single },
        { varName: "chart_hour_vessel", id: "chart-vessels-hour", config: plot_single },
        { varName: "chart_day_vessel", id: "chart-vessels-day", config: plot_single },
        { varName: "chart_level", id: "chart-level", config: plot_level },
        { varName: "chart_level_hour", id: "chart-level-hour", config: plot_level },
    ];

    for (const { varName, id, config } of chartConfigs) {
        try {
            const canvas = document.getElementById(id);
            if (!canvas) {
                console.warn(`Canvas element not found: ${id}`);
                continue;
            }
            charts[varName] = new Chart(canvas, cloneChartConfig(config));
            attachTimeAxis(charts[varName]);
        } catch (error) {
            console.error(`Failed to initialize chart ${id}:`, error);
        }
    }

    initialized = true;
    updateColors();
}


function updateChartMulti(b, f, c) {
    if (!Object.prototype.hasOwnProperty.call(b, f)) return;
    const hA = [], hB = [], hT = [], hS = [], hV = [];
    const source = b[f];
    for (let i = 0; i < source.time.length; i++) {
        const cA = source.stat[i].msg[0] + source.stat[i].msg[1] + source.stat[i].msg[2] + source.stat[i].msg[4];
        hA.push({ x: source.time[i], y: cA });
        const cB = source.stat[i].msg[17] + source.stat[i].msg[18] + source.stat[i].msg[23];
        hB.push({ x: source.time[i], y: cB });
        const cS = source.stat[i].msg[3];
        hS.push({ x: source.time[i], y: cS });
        const cT = source.stat[i].count - cA - cB - cS;
        hT.push({ x: source.time[i], y: cT });
        const cV = source.stat[i].vessels;
        hV.push({ x: source.time[i], y: cV });
    }
    c.data.datasets[0].data = hV;
    c.data.datasets[1].data = hA;
    c.data.datasets[2].data = hB;
    c.data.datasets[3].data = hS;
    c.data.datasets[4].data = hT;
    setTimeAxis(c, source);
    c.update();
}

function updateChartDistance(b, f, c) {
    if (!Object.prototype.hasOwnProperty.call(b, f)) return;
    const hNE = [], hSE = [], hSW = [], hNW = [], hM = [];
    const source = b[f];
    if (source.stat[0].radar_a.length == 0) return;
    const N = source.stat[0].radar_a.length;
    const N4 = N / 4;
    for (let i = 0; i < source.time.length; i++) {
        const count = [0, 0, 0, 0];
        for (let j = 0; j < N; j++) count[Math.floor(j / N4)] = Math.max(count[Math.floor(j / N4)], source.stat[i].radar_a[j]);
        hNE.push({ x: source.time[i], y: getDistanceConversion(count[0]) });
        hSE.push({ x: source.time[i], y: getDistanceConversion(count[1]) });
        hSW.push({ x: source.time[i], y: getDistanceConversion(count[2]) });
        hNW.push({ x: source.time[i], y: getDistanceConversion(count[3]) });
        hM.push({ x: source.time[i], y: getDistanceConversion(source.stat[i].dist) });
    }
    c.data.datasets[0].data = hNE;
    c.data.datasets[1].data = hSE;
    c.data.datasets[2].data = hSW;
    c.data.datasets[3].data = hNW;
    c.data.datasets[4].data = hM;
    setTimeAxis(c, source);
    c.update();
}

function updateChartSingle(b, f1, f2, c) {
    if (!Object.prototype.hasOwnProperty.call(b, f1)) return;
    const h = [];
    const source = b[f1];
    for (let i = 0; i < source.time.length; i++) {
        h.push({ x: source.time[i], y: source.stat[i][f2] });
    }
    c.data.datasets[0].data = h;
    setTimeAxis(c, source);
    c.update();
}

function updateChartLevel(chartData, timeframe, chart) {
    if (!chartData?.[timeframe]) return;
    const timeSeriesData = chartData[timeframe];

    const minLevelData = [];
    const maxLevelData = [];
    for (let i = 0; i < timeSeriesData.time.length; i++) {
        minLevelData.push({ x: timeSeriesData.time[i], y: timeSeriesData.stat[i].level_min });
        maxLevelData.push({ x: timeSeriesData.time[i], y: timeSeriesData.stat[i].level_max });
    }
    chart.data.datasets[0].data = maxLevelData;
    chart.data.datasets[1].data = minLevelData;
    setTimeAxis(chart, timeSeriesData);
    chart.update();
}

function updateRadar(b, f, c) {
    if (!Object.prototype.hasOwnProperty.call(b, f)) return;
    const data_a = [], data_b = [];
    let idx = 0;
    if (b[f].stat.length > 1) idx = 1;
    const N = b[f].stat[idx].radar_a.length;
    for (let i = 0; i < N; i++) {
        data_a.push({ r: getDistanceConversion(b[f].stat[idx].radar_a[i]), theta: (i * 360) / N });
        data_b.push({ r: getDistanceConversion(b[f].stat[idx].radar_b[i]), theta: (i * 360) / N });
    }
    c.data.datasets[0].data = data_b;
    c.data.datasets[1].data = data_a;
    c.update();
}


export async function update() {
    init();

    buildSourceSelectors();
    const token = ++updateToken;
    const sourceA = window.__app__.activeReceiver;
    const sourceBAtStart = sourceB;
    const unit = getDistanceUnit().toUpperCase();
    document.querySelectorAll(".distunit").forEach((u) => {
        u.textContent = unit;
    });

    const [b, cmp] = await Promise.all([
        fetchHistory(sourceA),
        sourceBAtStart !== null ? fetchHistory(sourceBAtStart) : Promise.resolve(null),
    ]);
    if (!b || token !== updateToken) return;

    document.querySelectorAll(".compare-panel").forEach((p) => {
        p.style.display = sourceBAtStart === null ? "none" : "";
    });
    if (sourceBAtStart !== null) {
        updateChartCompare(b, cmp, "minute", "count", charts.chart_compare_minutes, sourceA, sourceBAtStart);
        updateChartCompare(b, cmp, "hour", "count", charts.chart_compare_hours, sourceA, sourceBAtStart);
        updateChartGain(b, cmp, "minute", charts.chart_compare_gain_minute, sourceA, sourceBAtStart);
        updateChartGain(b, cmp, "hour", charts.chart_compare_gain_hour, sourceA, sourceBAtStart);
        updateChartCompare(b, cmp, "minute", "vessels", charts.chart_compare_vessels_minute, sourceA, sourceBAtStart);
        updateChartCompare(b, cmp, "hour", "vessels", charts.chart_compare_vessels_hour, sourceA, sourceBAtStart);
    }

    updateChartMulti(b, "second", charts.chart_seconds);
    updateChartMulti(b, "minute", charts.chart_minutes);
    updateChartMulti(b, "hour", charts.chart_hours);
    updateChartMulti(b, "day", charts.chart_days);

    updateChartSingle(b, "minute", "ppm", charts.chart_ppm_minute);
    updateChartSingle(b, "hour", "ppm", charts.chart_ppm);
    updateChartLevel(b, "minute", charts.chart_level);
    updateChartLevel(b, "hour", charts.chart_level_hour);
    updateChartSingle(b, "minute", "vessels", charts.chart_minute_vessel);
    updateChartSingle(b, "hour", "vessels", charts.chart_hour_vessel);
    updateChartSingle(b, "day", "vessels", charts.chart_day_vessel);

    updateChartDistance(b, "hour", charts.chart_distance_hour);
    updateChartDistance(b, "day", charts.chart_distance_day);

    updateRadar(b, "hour", charts.chart_radar_hour);
    updateRadar(b, "day", charts.chart_radar_day);
}

export function updateColors() {
    if (!initialized) return;

    const multi = [charts.chart_minutes, charts.chart_hours, charts.chart_days, charts.chart_seconds];
    const level = [charts.chart_level, charts.chart_level_hour];
    const single = [charts.chart_ppm, charts.chart_minute_vessel, charts.chart_ppm_minute,
                    charts.chart_hour_vessel, charts.chart_day_vessel];
    const distance = [charts.chart_distance_day, charts.chart_distance_hour];
    const radar = [charts.chart_radar_day, charts.chart_radar_hour];
    const compare = [charts.chart_compare_minutes, charts.chart_compare_hours,
                     charts.chart_compare_vessels_minute, charts.chart_compare_vessels_hour];
    const gain = [charts.chart_compare_gain_minute, charts.chart_compare_gain_hour];

    multi.forEach((chart) => {
        if (chart) { updateColorMulti(chart); chart.update(); }
    });
    single.forEach((chart) => {
        if (chart) { updateColorMulti(chart); chart.update(); }
    });
    distance.forEach((chart) => {
        if (chart) { updateChartColors(chart, COLORS_DISTANCE); chart.update(); }
    });
    level.forEach((chart) => {
        if (chart) {
            const colorVariables = ["--chart1-color", "--chart1-color", "--chart1-color"];
            updateChartColors(chart, colorVariables);
            chart.update();
        }
    });
    radar.forEach((chart) => {
        if (chart) { updateColorRadar(chart); chart.update(); }
    });
    compare.forEach((chart) => {
        if (chart) { updateChartColors(chart, ["--chart4-color", "--chart1-color"]); chart.update(); }
    });
    gain.forEach((chart) => {
        if (chart) { updateChartColors(chart, ["--chart1-color"]); chart.update(); }
    });
}
