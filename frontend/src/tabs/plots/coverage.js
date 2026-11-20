
import { getDistanceConversion, getDistanceUnit } from '../../core/units.js';
import { cssvar, angle, compassName, frame, disc, spokes, ring, rim, pill, compass, line, placeTip, legendItem } from './polar.js';

const STEPS = 9;
const TOWARD = ["north", "north-east", "east", "south-east", "south", "south-west", "west", "north-west"];
const ROUND = [1, 2, 5, 10, 20, 50, 100, 200, 500, 1000];
const PERIODS = { hour: ["minute", 60], day: ["hour", 24], week: ["day", 7] };

let canvas, tip, legend, sub, panel;
let data = null, history = null, period = "day", measure = "n", hover = null, geometry = null;

const hex = (c) => [1, 3, 5].map((i) => parseInt(c.slice(i, i + 2), 16));
const ramp = () => ["--coverage-low", "--coverage-mid", "--coverage-high"].map((name) => hex(cssvar(name)));
const step = (stops, i) => {
    const t = (2 * i) / (STEPS - 1), from = stops[t < 1 ? 0 : 1], to = stops[t < 1 ? 1 : 2], f = t < 1 ? t : t - 1;
    return "rgb(" + from.map((v, k) => Math.round(v + (to[k] - v) * f)).join(",") + ")";
};

const edge = (k) => (k <= 0 ? 0 : data.ring0_nm * Math.pow(2, (k - 1) / data.rings_per_doubling));
const place = (nm) => (nm < data.ring0_nm ? nm / data.ring0_nm : 1 + data.rings_per_doubling * Math.log2(nm / data.ring0_nm));

const cell = (a, r) => a * data.rings + r;
const shown = () => (data && data[period]) || null;

function extent() {
    let far = 0;
    for (const p of Object.keys(PERIODS))
        if (data[p]) data[p].n.forEach((n, i) => { if (n) far = Math.max(far, i % data.rings); });
    return Math.min(data.rings, Math.max(9, far + 2));
}

function range(s) {
    const heard = [];
    s.n.forEach((n, i) => {
        const v = measure === "n" ? n : s.level[i];
        if (n && v !== null) heard.push(v);
    });
    if (!heard.length) return null;
    heard.sort((a, b) => a - b);
    return { lo: heard[0], hi: heard[heard.length - 1], top: heard[Math.floor(0.95 * (heard.length - 1))] };
}

function shade(s, i, scale) {
    if (!scale) return 0;
    const v = measure === "n" ? s.n[i] : s.level[i];
    if (v === null) return -1;
    const t = measure === "n" ? Math.sqrt(Math.min(1, v / scale.top)) : scale.hi > scale.lo ? (v - scale.lo) / (scale.hi - scale.lo) : 1;
    return Math.min(STEPS - 1, Math.floor(t * STEPS));
}

function draw() {
    if (!canvas || !data) return;
    const f = frame(canvas);
    if (!f) return;
    const { g, cx, cy, R } = f, rings = extent(), D = data.directions;
    const radius = (k) => (R * k) / rings, sector = (a) => angle((a * 360) / D);
    const wedge = (a, r) => {
        g.beginPath();
        g.arc(cx, cy, radius(r + 1), sector(a), sector(a + 1));
        g.arc(cx, cy, radius(r), sector(a + 1), sector(a), true);
        g.closePath();
    };
    geometry = { cx, cy, R, rings };
    disc(f);

    const s = shown(), scale = s && range(s), stops = ramp();
    if (s)
        for (let a = 0; a < D; a++)
            for (let r = 0; r < rings; r++) {
                const i = cell(a, r);
                if (!s.n[i]) continue;
                const k = shade(s, i, scale);
                wedge(a, r);
                g.fillStyle = k < 0 ? cssvar("--color-chart-grid") : step(stops, k);
                g.fill();
                g.lineWidth = 1;
                g.strokeStyle = cssvar("--coverage-disc-out");
                g.stroke();
            }

    spokes(f);
    const perMile = getDistanceConversion(1), labels = [];
    for (const v of ROUND) {
        const k = place(v / perMile);
        if (k >= rings - 0.3) break;
        if (k < 1) continue;
        ring(f, radius(k));
        if (String(v)[0] === "1") labels.push([v, k]);
    }
    rim(f);

    if (s) {
        g.beginPath();
        for (let a = 0; a < D; a++) {
            let far = 0;
            for (let r = 0; r < rings; r++) if (s.n[cell(a, r)]) far = r + 1;
            if (far) g.arc(cx, cy, radius(far), sector(a), sector(a + 1));
            else g.lineTo(cx, cy);
        }
        g.closePath();
        g.lineWidth = 1.5;
        g.lineJoin = "round";
        g.strokeStyle = cssvar("--coverage-line");
        g.stroke();
    }

    if (hover) {
        wedge(hover.a, hover.r);
        g.lineWidth = 2;
        g.strokeStyle = cssvar("--color-text-strong");
        g.stroke();
    }

    g.beginPath();
    g.arc(cx, cy, 4, 0, 2 * Math.PI);
    g.fillStyle = cssvar("--coverage-line");
    g.fill();
    g.lineWidth = 2;
    g.strokeStyle = cssvar("--coverage-disc-in");
    g.stroke();

    for (const [v, k] of labels) pill(f, v + " " + getDistanceUnit(), radius(k));
    compass(f);
}

const count = (n) => Number(n).toLocaleString();
const distance = (nm) => { const v = getDistanceConversion(nm); return v < 10 ? v.toFixed(1) : String(Math.round(v)); };

function band(r) {
    const unit = getDistanceUnit();
    if (r === 0) return "within " + distance(edge(1)) + " " + unit;
    if (r === data.rings - 1) return "beyond " + distance(edge(r)) + " " + unit;
    return distance(edge(r)) + " to " + distance(edge(r + 1)) + " " + unit;
}

function point(e) {
    if (!geometry || !shown()) return null;
    const box = canvas.getBoundingClientRect(), x = e.clientX - box.left - geometry.cx, y = e.clientY - box.top - geometry.cy;
    const r = Math.floor((Math.hypot(x, y) / geometry.R) * geometry.rings);
    if (r >= geometry.rings) return null;
    const deg = ((Math.atan2(y, x) * 180) / Math.PI + 90 + 360) % 360;
    const a = Math.min(data.directions - 1, Math.floor((deg * data.directions) / 360));
    return shown().n[cell(a, r)] ? { a, r, x: e.clientX - box.left, y: e.clientY - box.top } : null;
}

function showTip(e) {
    const at = point(e);
    if ((at && hover && at.a === hover.a && at.r === hover.r) || (!at && !hover)) {
        if (at) placeTip(tip, canvas, at);
        return;
    }
    hover = at;
    draw();
    if (!at) {
        tip.hidden = true;
        return;
    }
    const s = shown(), i = cell(at.a, at.r), w = 360 / data.directions, level = s.level[i];
    tip.replaceChildren();
    line(tip, "polar-tip-value", measure === "n" ? count(s.n[i]) + " messages" : level === null ? "no signal level" : level.toFixed(1) + " dB");
    line(tip, "polar-tip-row", compassName((at.a + 0.5) * w) + ", " + at.a * w + "° to " + (at.a + 1) * w + "°");
    line(tip, "polar-tip-row", band(at.r));
    line(tip, "polar-tip-row", measure === "n" ? (level === null ? "no signal level" : "average " + level.toFixed(1) + " dB") : count(s.n[i]) + " messages");
    tip.hidden = false;
    placeTip(tip, canvas, at);
}

function hideTip() {
    if (!hover) return;
    hover = null;
    tip.hidden = true;
    draw();
}

function describe() {
    const s = shown();
    let far = -1, most = 0, toward = 0, total = 0;
    if (s)
        s.n.forEach((n, i) => {
            total += n;
            const r = i % data.rings;
            if (n && (r > far || (r === far && n > most))) { far = r; most = n; toward = Math.floor(i / data.rings); }
        });
    if (!total) {
        sub.textContent = "No positions heard in this period.";
        canvas.setAttribute("aria-label", sub.textContent);
        return;
    }
    const [bucket, last] = PERIODS[period];
    const nm = history && history[bucket] ? Math.max(0, ...history[bucket].stat.slice(0, last).map((x) => x.dist || 0)) : 0;
    const where = TOWARD[Math.round(((toward + 0.5) * 360) / data.directions / 45) % 8];
    sub.textContent = (nm ? "Ships heard up to " + distance(nm) + " " + getDistanceUnit() + " away, farthest" : "Farthest") + " toward the " + where + ".";
    canvas.setAttribute("aria-label", "Coverage by direction and distance. " + sub.textContent + " " + count(total) + " messages.");
}

function key() {
    const s = shown(), scale = s && range(s);
    legend.replaceChildren();
    if (!scale) return;
    const stops = ramp();
    legendItem(legend, "", measure === "n" ? "few" : "weak");
    const swatches = document.createElement("span");
    swatches.className = "polar-ramp";
    for (let i = 0; i < STEPS; i++) {
        const sw = document.createElement("i");
        sw.style.background = step(stops, i);
        swatches.append(sw);
    }
    legend.append(swatches);
    legendItem(legend, "", measure === "n" ? "many (busiest cell " + count(scale.hi) + ")" : "strong (" + scale.lo.toFixed(0) + " to " + scale.hi.toFixed(0) + " dB)");
    legendItem(legend, "polar-key-line", "farthest reception");
    legendItem(legend, "polar-key-dot", "station");
}

function refresh() {
    if (!panel) return;
    panel.querySelectorAll("[data-period]").forEach((b) => b.classList.toggle("active", b.dataset.period === period));
    panel.querySelectorAll("[data-measure]").forEach((b) => b.classList.toggle("active", b.dataset.measure === measure));
    hover = null;
    if (tip) tip.hidden = true;
    describe();
    key();
    draw();
}

export function init() {
    canvas = document.getElementById("chart-coverage");
    if (!canvas) return;
    panel = canvas.closest(".graph-panel");
    tip = panel.querySelector(".polar-tip");
    legend = panel.querySelector(".polar-legend");
    sub = panel.querySelector(".polar-sub");
    canvas.addEventListener("pointermove", showTip);
    canvas.addEventListener("pointerdown", showTip);
    canvas.addEventListener("pointerleave", hideTip);
    if (window.ResizeObserver) new ResizeObserver(draw).observe(canvas.parentElement);
}

export function update(h) {
    if (!panel) return;
    history = h;
    data = h && h.coverage && h.coverage.day ? h.coverage : null;
    panel.classList.toggle("polar-none", !data);
    if (data) refresh();
}

export function repaint() {
    if (data) refresh();
}

export function setPeriod(p) {
    period = p;
    if (data) refresh();
}

export function setMeasure(m) {
    measure = m;
    if (data) refresh();
}
