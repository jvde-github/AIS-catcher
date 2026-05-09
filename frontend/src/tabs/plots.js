// Manual Chart.js registration (vs `chart.js/auto`) to tree-shake out chart
// types we don't use. Only scatter (line) + polarArea are needed.
import {
    Chart,
    LineController, ScatterController, PolarAreaController,
    PointElement, LineElement, ArcElement,
    LinearScale, LogarithmicScale, RadialLinearScale,
    Tooltip, Legend, Filler,
} from 'chart.js';
import Annotation from 'chartjs-plugin-annotation';
import { getDistanceConversion, getDistanceUnit } from '../core/format.js';

Chart.register(
    LineController, ScatterController, PolarAreaController,
    PointElement, LineElement, ArcElement,
    LinearScale, LogarithmicScale, RadialLinearScale,
    Tooltip, Legend, Filler,
    Annotation,
);

const charts = {};
let initialized = false;

export function isInitialized() {
    return initialized;
}


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

function cloneChartConfig(config) {
    const clone = JSON.parse(JSON.stringify(config));
    if (clone.options?.plugins?.annotation?.annotations?.graph_annotation) {
        clone.options.plugins.annotation.annotations.graph_annotation.value = (a) => average(a);
    }
    return clone;
}

function updateChartColors(c, colorVariables) {
    c.data.datasets.forEach((dataset, index) => {
        const color = cssvar(colorVariables[index]);
        dataset.backgroundColor = color;
        dataset.borderColor = color;
    });
    c.options.scales.x.ticks.color = cssvar("--chart-color");
    c.options.scales.x.grid.color = cssvar("--chart-grid-color");
    c.options.scales.y.ticks.color = cssvar("--chart-color");
    c.options.scales.y.grid.color = cssvar("--chart-grid-color");
    c.options.plugins.legend.labels.color = cssvar("--chart-color");
}

function updateColorMulti(c) {
    const colorVariables = ["--chart4-color", "--chart1-color", "--chart2-color", "--chart5-color", "--chart6-color"];
    updateChartColors(c, colorVariables);
}

function updateColorSingle(c) {
    const colorVariables = ["--chart4-color", "--chart1-color", "--chart2-color", "--chart5-color", "--chart6-color"];
    updateChartColors(c, colorVariables);
}

function updateColorRadar(c) {
    const colorVariables = ["--chart2-color", "--chart4-color", "--chart2-color", "--chart5-color", "--chart4-color"];
    c.data.datasets.forEach((dataset, index) => {
        const color = cssvar(colorVariables[index]);
        dataset.backgroundColor = color;
        dataset.borderColor = color;
    });
    c.options.scales.r.grid.color = cssvar("--chart-grid-color");
    c.options.scales.r.ticks.color = cssvar("--chart-color");
    c.options.scales.r.ticks.backdropColor = cssvar("--panel-color");
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
            stacked: true,
            beginAtZero: true,
            ticks: { display: true, align: 'center' },
            grid: { display: true },
            title: { display: true, text: 'Message Count', font: { size: 12 }, color: '#666' },
        },
        y_right: {
            stacked: true,
            beginAtZero: true,
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


function init() {
    if (initialized) return;

    const chartConfigs = [
        { varName: "chart_radar_hour", id: "chart-radar-hour", ctx: "2d", config: plot_radar, clone: true },
        { varName: "chart_radar_day", id: "chart-radar-day", ctx: "2d", config: plot_radar, clone: true },
        { varName: "chart_seconds", id: "chart-seconds", config: plot_count, clone: true },
        { varName: "chart_minutes", id: "chart-minutes", config: plot_count, clone: true },
        { varName: "chart_hours", id: "chart-hours", config: plot_count, clone: true },
        { varName: "chart_days", id: "chart-days", config: plot_count, clone: true },
        { varName: "chart_ppm", id: "chart-ppm", config: plot_single, clone: true },
        { varName: "chart_ppm_minute", id: "chart-ppm-minute", config: plot_single, clone: true },
        { varName: "chart_distance_hour", id: "chart-distance-hour", config: plot_distance, clone: false },
        { varName: "chart_distance_day", id: "chart-distance-day", config: plot_distance, clone: false },
        { varName: "chart_minute_vessel", id: "chart-vessels-minute", config: plot_single, clone: false },
        { varName: "chart_hour_vessel", id: "chart-vessels-hour", config: plot_single, clone: false },
        { varName: "chart_day_vessel", id: "chart-vessels-day", config: plot_single, clone: false },
    ];

    for (const { varName, id, ctx, config, clone } of chartConfigs) {
        try {
            const canvas = document.getElementById(id);
            if (!canvas) {
                console.warn(`Canvas element not found: ${id}`);
                continue;
            }
            const context = ctx === "2d" ? canvas.getContext("2d") : canvas;
            const chartConfig = clone ? cloneChartConfig(config) : config;
            charts[varName] = new Chart(context, chartConfig);
        } catch (error) {
            console.error(`Failed to initialize chart ${id}:`, error);
        }
    }
    charts.chart_level = new Chart(document.getElementById("chart-level"), cloneChartConfig(plot_level));
    charts.chart_level_hour = new Chart(document.getElementById("chart-level-hour"), cloneChartConfig(plot_level));

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
    c.update();
}

function updateChartLevel(chartData, timeframe, chartName, chart) {
    if (!chartData?.[timeframe]) return;
    const timeSeriesData = chartData[timeframe];

    const minLevelData = [];
    for (let i = 0; i < timeSeriesData.time.length; i++) {
        minLevelData.push({
            x: timeSeriesData.time[i],
            y: timeSeriesData.stat[i].level_min == null ? null : timeSeriesData.stat[i].level_min,
        });
    }
    chart.data.datasets[1].data = minLevelData;

    const maxLevelData = [];
    for (let i = 0; i < timeSeriesData.time.length; i++) {
        maxLevelData.push({
            x: timeSeriesData.time[i],
            y: timeSeriesData.stat[i].level_max == null ? null : timeSeriesData.stat[i].level_max,
        });
    }
    chart.data.datasets[0].data = maxLevelData;
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

    const { activeReceiver } = window.__app__;
    const unit = getDistanceUnit().toUpperCase();
    document.querySelectorAll(".distunit").forEach((u) => {
        u.textContent = unit;
    });

    let response;
    try {
        response = await fetch("api/history_full.json?receiver=" + activeReceiver);
    } catch (error) {
        console.error('plots: history fetch failed:', error);
        return;
    }
    const b = await response.json();

    updateChartMulti(b, "second", charts.chart_seconds);
    updateChartMulti(b, "minute", charts.chart_minutes);
    updateChartMulti(b, "hour", charts.chart_hours);
    updateChartMulti(b, "day", charts.chart_days);

    updateChartSingle(b, "minute", "ppm", charts.chart_ppm_minute);
    updateChartSingle(b, "hour", "ppm", charts.chart_ppm);
    updateChartLevel(b, "minute", "level", charts.chart_level);
    updateChartLevel(b, "hour", "level", charts.chart_level_hour);
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
    const single = [charts.chart_distance_day, charts.chart_distance_hour, charts.chart_ppm,
                    charts.chart_minute_vessel, charts.chart_ppm_minute, charts.chart_hour_vessel,
                    charts.chart_day_vessel];
    const radar = [charts.chart_radar_day, charts.chart_radar_hour];

    multi.forEach((chart) => {
        if (chart) { updateColorMulti(chart); chart.update(); }
    });
    single.forEach((chart) => {
        if (chart) { updateColorSingle(chart); chart.update(); }
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
}
