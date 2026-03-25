import { registerActions } from '../events.js';
import { settings, persist, onSettingsChange } from '../settings.js';

let deps = {};

// ── Utility (depends only on settings.metric) ─────────────────────────────────

const getDistanceConversion = (c) => (settings.metric === "DEFAULT" ? c : settings.metric === "SI" ? c * 1.852 : c * 1.15078);
const getDistanceUnit      = ()  => (settings.metric === "DEFAULT" ? "nmi" : settings.metric === "SI" ? "km" : "mi");
const getDistanceVal       = (c) => Number(getDistanceConversion(c)).toFixed(1).toLocaleString();

const getDeltaTimeVal = (s) => {
    const days    = Math.floor(s / (24 * 3600));
    const hours   = Math.floor((s % (24 * 3600)) / 3600);
    const minutes = Math.floor((s % 3600) / 60);
    const seconds = s % 60;
    let result = '';
    if (days > 0)                                         result += `${days}d `;
    if (hours > 0 || days > 0)                            result += `${hours}h `;
    if (minutes > 0 || hours > 0 || days > 0)             result += `${minutes}m `;
    if (seconds > 0 || (days === 0 && hours === 0 && minutes === 0)) result += `${seconds}s`;
    return result.trim();
};

// ── Graph visibility ──────────────────────────────────────────────────────────

export function setGraphVisibility(type, show, save = true) {
    if (type === 'signal') {
        settings.show_signal_graphs = show;
        document.querySelectorAll('.graph-panel').forEach(panel => {
            const header = panel.querySelector('header');
            if (header && header.textContent.includes('Signal Level'))
                panel.style.display = show ? '' : 'none';
        });
    } else if (type === 'ppm') {
        settings.show_ppm_graphs = show;
        document.querySelectorAll('.graph-panel').forEach(panel => {
            const header = panel.querySelector('header');
            if (header && header.textContent.includes('Frequency Shift'))
                panel.style.display = show ? '' : 'none';
        });
    }
    if (save) deps.saveSettings();
}

function toggleGraphVisibility(type) {
    if (type === 'signal') setGraphVisibility('signal', !settings.show_signal_graphs);
    else if (type === 'ppm') setGraphVisibility('ppm', !settings.show_ppm_graphs);
}

// ── CSS variable helper ───────────────────────────────────────────────────────

function cssvar(name) {
    const value = getComputedStyle(document.documentElement).getPropertyValue(name).trim();
    return value || getComputedStyle(document.body).getPropertyValue(name).trim();
}

// ── Chart instances ───────────────────────────────────────────────────────────

let chart_seconds, chart_minutes, chart_hours, chart_days;
let chart_ppm, chart_ppm_minute;
let chart_minute_vessel, chart_hour_vessel, chart_day_vessel;
let chart_distance_hour, chart_distance_day;
let chart_radar_hour, chart_radar_day;
let chart_level, chart_level_hour;

// ── Graph configs ─────────────────────────────────────────────────────────────

function average(d) {
    const b = d.chart.data.datasets[0].data;
    if (b.length == 1) return b[0].y;
    let start = 0;
    if (d.chart.data.datasets.length > 1) start = 1;
    let c = 0;
    for (let a = 0; a < b.length; a++) {
        if (b[a].x != 0) {
            for (let i = start; i < d.chart.data.datasets.length; i++) {
                if (!d.chart.getDatasetMeta(i).hidden) c += d.chart.data.datasets[i].data[a].y;
            }
        }
    }
    return c / (b.length - 1);
}

const graph_annotation = {
    type: "line",
    borderColor: "rgba(12,118,170)",
    borderDash: [6, 6],
    borderDashOffset: 0,
    borderWidth: 3,
    scaleID: "y",
    value: (a) => average(a),
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
            stacked: true, beginAtZero: true,
            ticks: { display: true, align: 'center' },
            grid: { display: true },
            title: { display: true, text: 'Message Count', font: { size: 12 }, color: '#666' },
        },
        y_right: {
            stacked: true, beginAtZero: true,
            ticks: { display: true, align: 'center' },
            grid: { display: false },
            position: 'right',
            title: { display: true, text: 'Vessel Count', font: { size: 12 }, color: '#666' },
        },
        x: {
            ticks: { display: true },
            title: { display: true, text: 'Time', font: { size: 16 }, color: '#666' },
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
            { label: "Class A",      data: [], showLine: true, fill: "origin", borderWidth: 2, pointStyle: false },
            { label: "Class B",      data: [], showLine: true, fill: "-1",     borderWidth: 2, pointStyle: false },
            { label: "Base Station", data: [], showLine: true, fill: "-1",     borderWidth: 2, pointStyle: false },
            { label: "Other",        data: [], showLine: true, fill: "-1",     borderWidth: 2, pointStyle: false },
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
            { label: "Max", data: [], showLine: true, backgroundColor: "rgb(211,211,211,0.9)", fill: true, pointStyle: false, borderWidth: 0 },
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
    animation: false,
    responsive: true,
    plugins: { legend: { display: true } },
    data: {
        datasets: [
            { label: "Class B", borderWidth: 1 },
            { label: "Class A", borderWidth: 1 },
        ],
    },
    options: {
        legend: { display: false },
        scale: { ticks: { min: 0 } },
    },
};

// ── Color helpers ─────────────────────────────────────────────────────────────

function updateChartColors(c, colorVariables) {
    c.data.datasets.forEach((dataset, index) => {
        const color = cssvar(colorVariables[index]);
        dataset.backgroundColor = color;
        dataset.borderColor = color;
    });
    c.options.scales.x.ticks.color = cssvar("--chart-color");
    c.options.scales.x.grid.color  = cssvar("--chart-grid-color");
    c.options.scales.y.ticks.color = cssvar("--chart-color");
    c.options.scales.y.grid.color  = cssvar("--chart-grid-color");
    c.options.plugins.legend.labels.color = cssvar("--chart-color");
}

function updateColorMulti(c) {
    updateChartColors(c, ["--chart4-color", "--chart1-color", "--chart2-color", "--chart5-color", "--chart6-color"]);
}

function updateColorSingle(c) {
    updateChartColors(c, ["--chart4-color", "--chart1-color", "--chart2-color", "--chart5-color", "--chart6-color"]);
}

function updateColorRadar(c) {
    const colorVariables = ["--chart2-color", "--chart4-color", "--chart2-color", "--chart5-color", "--chart4-color"];
    c.data.datasets.forEach((dataset, index) => {
        const color = cssvar(colorVariables[index]);
        dataset.backgroundColor = color;
        dataset.borderColor = color;
    });
    c.options.scales.r.grid.color         = cssvar("--chart-grid-color");
    c.options.scales.r.ticks.color        = cssvar("--chart-color");
    c.options.scales.r.ticks.backdropColor = cssvar("--panel-color");
}

function cloneChartConfig(config) {
    const clone = JSON.parse(JSON.stringify(config));
    if (clone.options?.plugins?.annotation?.annotations?.graph_annotation) {
        clone.options.plugins.annotation.annotations.graph_annotation.value = (a) => average(a);
    }
    return clone;
}

// ── Initialise Chart.js instances ─────────────────────────────────────────────

function initPlots() {
    if (typeof Chart === "undefined") return;

    function makeChart(id, config, clone, ctx) {
        try {
            const canvas = document.getElementById(id);
            if (!canvas) { console.warn(`Canvas element not found: ${id}`); return null; }
            const context = ctx === "2d" ? canvas.getContext("2d") : canvas;
            return new Chart(context, clone ? cloneChartConfig(config) : config);
        } catch (error) {
            console.error(`Failed to initialize chart ${id}:`, error);
            return null;
        }
    }

    chart_radar_hour    = makeChart("chart-radar-hour",      plot_radar,    true,  "2d");
    chart_radar_day     = makeChart("chart-radar-day",       plot_radar,    true,  "2d");
    chart_seconds       = makeChart("chart-seconds",         plot_count,    true);
    chart_minutes       = makeChart("chart-minutes",         plot_count,    true);
    chart_hours         = makeChart("chart-hours",           plot_count,    true);
    chart_days          = makeChart("chart-days",            plot_count,    true);
    chart_ppm           = makeChart("chart-ppm",             plot_single,   true);
    chart_ppm_minute    = makeChart("chart-ppm-minute",      plot_single,   true);
    chart_distance_hour = makeChart("chart-distance-hour",   plot_distance, false);
    chart_distance_day  = makeChart("chart-distance-day",    plot_distance, false);
    chart_minute_vessel = makeChart("chart-vessels-minute",  plot_single,   false);
    chart_hour_vessel   = makeChart("chart-vessels-hour",    plot_single,   false);
    chart_day_vessel    = makeChart("chart-vessels-day",     plot_single,   false);
    chart_level         = makeChart("chart-level",           plot_level,    true);
    chart_level_hour    = makeChart("chart-level-hour",      plot_level,    true);
}

// ── Statistics ────────────────────────────────────────────────────────────────

async function fetchStatistics() {
    try {
        const response = await fetch("api/stat.json?receiver=" + deps.getActiveReceiver());
        return await response.json();
    } catch {
        return null;
    }
}

function formatBytes(b) {
    if (b >= 1e9) return (b / 1e9).toFixed(1) + " GB";
    if (b >= 1e6) return (b / 1e6).toFixed(1) + " MB";
    if (b >= 1e3) return (b / 1e3).toFixed(1) + " KB";
    return b + " B";
}

function updateStat(stat, tf) {
    [0, 1, 2, 3].forEach((e) => (document.getElementById("stat_" + tf + "_channel" + e).innerText = stat[tf].channel[e].toLocaleString()));

    document.getElementById("stat_" + tf + "_count").innerText        = stat[tf].count.toLocaleString();
    document.getElementById("stat_" + tf + "_dist").innerText         = getDistanceVal(stat[tf].dist) + " " + getDistanceUnit();
    document.getElementById("stat_" + tf + "_vessel_count").innerText = stat[tf].vessels.toLocaleString();
    document.getElementById("stat_" + tf + "_msg123").innerText       = (stat[tf].msg[0] + stat[tf].msg[1] + stat[tf].msg[2]).toLocaleString();
    document.getElementById("stat_" + tf + "_msg5").innerText         = stat[tf].msg[4].toLocaleString();
    document.getElementById("stat_" + tf + "_msg18").innerText        = stat[tf].msg[17].toLocaleString();
    document.getElementById("stat_" + tf + "_msg19").innerText        = stat[tf].msg[18].toLocaleString();
    document.getElementById("stat_" + tf + "_msg68").innerText        = (stat[tf].msg[5] + stat[tf].msg[7]).toLocaleString();
    document.getElementById("stat_" + tf + "_msg1214").innerText      = (stat[tf].msg[11] + stat[tf].msg[13]).toLocaleString();
    document.getElementById("stat_" + tf + "_msg24").innerText        = stat[tf].msg[23].toLocaleString();
    document.getElementById("stat_" + tf + "_msg4").innerText         = stat[tf].msg[3].toLocaleString();
    document.getElementById("stat_" + tf + "_msg9").innerText         = stat[tf].msg[8].toLocaleString();
    document.getElementById("stat_" + tf + "_msg21").innerText        = stat[tf].msg[20].toLocaleString();
    document.getElementById("stat_" + tf + "_msg27").innerText        = stat[tf].msg[26].toLocaleString();

    let count_other = 0;
    [7, 10, 11, 13, 15, 16, 17, 20, 22, 23, 25, 26].forEach((i) => (count_other += stat[tf].msg[i - 1]));
    document.getElementById("stat_" + tf + "_msgother").innerText = count_other.toLocaleString();
}

export async function updateStatistics() {
    const stat = await fetchStatistics();
    if (!stat) return;

    ["os", "tcp_clients", "hardware", "build_describe", "build_date", "station", "product", "vendor", "serial", "model", "sample_rate"].forEach(
        (e) => (document.getElementById("stat_" + e).innerHTML = stat[e]),
    );

    if (stat.station_link != "")
        document.getElementById("stat_station").innerHTML = "<a href='" + stat.station_link + "'>" + stat.station + "</a>";

    const statSharingElement = document.getElementById("stat_sharing");
    statSharingElement.innerHTML = `<a href="${stat.sharing_link}" target="_blank">${stat.sharing ? 'Yes' : 'No'}</a>`;
    statSharingElement.style.color = stat.sharing ? "green" : "red";

    document.getElementById("stat_update_time").textContent = Number(deps.getRefreshIntervalMs() / 1000).toFixed(1) + " s";
    const title = document.getElementById("stat_station").textContent;
    if (title != "" && title != null) deps.setStationTitle(title);

    document.getElementById("stat_memory").innerText    = stat.memory ? Number(stat.memory / 1000000).toFixed(1) + " MB" : "N/A";
    document.getElementById("stat_received").innerText  = formatBytes(stat.received);
    document.getElementById("stat_msg_rate").innerText  = Number(stat.msg_rate).toFixed(1) + " msg/s";
    document.getElementById("stat_msg_min_rate").innerText = Number(stat.last_minute.count).toFixed(0) + " msg/min";
    document.getElementById("stat_run_time").innerHTML  = getDeltaTimeVal(stat.run_time);

    updateStat(stat, "total");
    updateStat(stat, "session");
    updateStat(stat, "last_minute");
    updateStat(stat, "last_hour");
    updateStat(stat, "last_day");

    document.getElementById("stat_total_vessel_count").innerText   = "-";
    document.getElementById("stat_session_vessel_count").innerText = stat.vessel_count;

    const outputSection = document.getElementById("output_stats");
    if (!outputSection) return;

    if (stat.outputs && stat.outputs.length > 0) {
        let html = "";
        for (let i = 0; i < stat.outputs.length; i++) {
            const o = stat.outputs[i];
            const s = o.stats;
            const showStatus = o.type !== "UDP" && !o.type.startsWith("HTTP");
            html += "<section>";
            html += `<div><span>Output</span><span>${o.description || o.type}</span></div>`;
            if (showStatus) {
                const connectedColor = s.connected ? "green" : "red";
                html += `<div><span>Status</span><span style="color:${connectedColor}">${s.connected ? "Connected" : "Not connected"}</span></div>`;
            }
            html += `<div><span>Bytes out / in</span><span>${formatBytes(s.bytes_out)} / ${formatBytes(s.bytes_in)}</span></div>`;
            if (o.type !== "UDP")
                html += `<div><span>Connect ok / fail</span><span>${s.connect_ok} / ${s.connect_fail}</span></div>`;
            if (s.reconnects > 0)
                html += `<div><span>Reconnects</span><span>${s.reconnects}</span></div>`;
            html += "</section>";
        }
        outputSection.innerHTML = html;
    }
}

// ── Chart data updaters ───────────────────────────────────────────────────────

function updateChartMulti(b, f, c) {
    if (!c || !b.hasOwnProperty(f)) return;
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
    c.update();
}

function updateChartDistance(b, f, c) {
    if (!c || !b.hasOwnProperty(f)) return;
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
    c.update();
}

function updateChartSingle(b, f1, f2, c) {
    if (!c || !b.hasOwnProperty(f1)) return;
    const h = [];
    const source = b[f1];
    for (let i = 0; i < source.time.length; i++) h.push({ x: source.time[i], y: source.stat[i][f2] });
    c.data.datasets[0].data = h;
    c.update();
}

function updateChartLevel(chartData, timeframe, chartName, chart) {
    if (!chartData?.hasOwnProperty(timeframe)) return;
    const ts = chartData[timeframe];
    chart.data.datasets[1].data = ts.time.map((t, i) => ({ x: t, y: ts.stat[i].level_min === 0 ? null : ts.stat[i].level_min }));
    chart.data.datasets[0].data = ts.time.map((t, i) => ({ x: t, y: ts.stat[i].level_max === 0 ? null : ts.stat[i].level_max }));
    chart.update();
}

function updateRadar(b, f, c) {
    if (!c || !b.hasOwnProperty(f)) return;
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

export async function updatePlots() {
    const unit = getDistanceUnit().toUpperCase();
    document.querySelectorAll(".distunit").forEach((u) => { u.textContent = unit; });

    let response;
    try {
        response = await fetch("api/history_full.json?receiver=" + deps.getActiveReceiver());
    } catch {
        return;
    }
    const b = await response.json();

    updateChartMulti(b, "second", chart_seconds);
    updateChartMulti(b, "minute", chart_minutes);
    updateChartMulti(b, "hour",   chart_hours);
    updateChartMulti(b, "day",    chart_days);

    updateChartSingle(b, "minute", "ppm",     chart_ppm_minute);
    updateChartSingle(b, "hour",   "ppm",     chart_ppm);
    updateChartLevel(b,  "minute", "level",   chart_level);
    updateChartLevel(b,  "hour",   "level",   chart_level_hour);
    updateChartSingle(b, "minute", "vessels", chart_minute_vessel);
    updateChartSingle(b, "hour",   "vessels", chart_hour_vessel);
    updateChartSingle(b, "day",    "vessels", chart_day_vessel);
    updateChartDistance(b, "hour", chart_distance_hour);
    updateChartDistance(b, "day",  chart_distance_day);
    updateRadar(b, "hour", chart_radar_hour);
    updateRadar(b, "day",  chart_radar_day);
}

// ── Update all chart colors (called on dark mode toggle) ──────────────────────

export function updateAllChartColors() {
    [chart_minutes, chart_hours, chart_days, chart_seconds].forEach(c => {
        if (c) { updateColorMulti(c); c.update(); }
    });
    [chart_distance_day, chart_distance_hour, chart_ppm, chart_minute_vessel, chart_ppm_minute, chart_hour_vessel, chart_day_vessel].forEach(c => {
        if (c) { updateColorSingle(c); c.update(); }
    });
    [chart_level, chart_level_hour].forEach(c => {
        if (c) { updateChartColors(c, ["--chart1-color", "--chart1-color", "--chart1-color"]); c.update(); }
    });
    [chart_radar_day, chart_radar_hour].forEach(c => {
        if (c) { updateColorRadar(c); c.update(); }
    });
}

// ── Public API ────────────────────────────────────────────────────────────────

export function init(dependencies) {
    deps = dependencies;

    registerActions({ toggleSignalGraph: () => toggleGraphVisibility('signal'), togglePPMGraph: () => toggleGraphVisibility('ppm') });

    initPlots();

    // Sync graph visibility on external settings change.
    onSettingsChange(() => {
        setGraphVisibility('signal', settings.show_signal_graphs, false);
        setGraphVisibility('ppm',    settings.show_ppm_graphs,    false);
    });
}
