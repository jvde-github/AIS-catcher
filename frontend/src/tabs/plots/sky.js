
import { cssvar, angle, compassName, frame, disc, spokes, ring, rim, pill, compass, line, placeTip, legendItem } from './polar.js';

const SYSTEM = 0, ID = 1, ELEVATION = 2, AZIMUTH = 3, SIGNAL = 4, USED = 5;
const SYSTEMS = {
    1: ["GPS", "--color-chart-1"], 3: ["Galileo", "--color-chart-2"], 2: ["GLONASS", "--color-chart-6"], 4: ["BeiDou", "--color-chart-5"],
    5: ["QZSS", "--color-chart-3"], 6: ["NavIC", "--color-text-muted"], 7: ["SBAS", "--color-text-muted"], 0: ["Other", "--color-text-muted"],
};
const ORDER = [1, 3, 2, 4, 5, 6, 7, 0];
const FIX = { 2: "2D fix", 3: "3D fix" };

let canvas, tip, legend, sub, panel, signals;
let data = null, hover = null, placed = [];

const system = (s) => SYSTEMS[s[SYSTEM]] || SYSTEMS[0];
const size = (s) => 3.5 + (Math.max(0, Math.min(50, s[SIGNAL])) / 50) * 5;

function draw() {
    if (!canvas || !data) return;
    const f = frame(canvas);
    if (!f) return;
    const { g, cx, cy, R } = f, radius = (elevation) => (R * (90 - elevation)) / 90;
    disc(f);
    spokes(f);
    ring(f, radius(60));
    ring(f, radius(30));
    rim(f);
    for (const elevation of [60, 30, 0]) pill(f, elevation + "°", radius(elevation), 157.5);

    placed = data.satellites.filter((s) => s[ELEVATION] >= 0 && s[AZIMUTH] >= 0).sort((a, b) => b[SIGNAL] - a[SIGNAL]).map((s) => ({
        s, x: cx + radius(s[ELEVATION]) * Math.cos(angle(s[AZIMUTH])), y: cy + radius(s[ELEVATION]) * Math.sin(angle(s[AZIMUTH])), r: size(s),
    }));
    const ground = cssvar("--coverage-disc-in");
    for (const p of placed) {
        const color = cssvar(system(p.s)[1]);
        g.beginPath();
        g.arc(p.x, p.y, p.r, 0, 2 * Math.PI);
        g.fillStyle = p.s[USED] ? color : ground;
        g.fill();
        g.lineWidth = 2;
        g.strokeStyle = p.s[USED] ? ground : color;
        g.stroke();
        if (p.s === hover) {
            g.beginPath();
            g.arc(p.x, p.y, p.r + 3, 0, 2 * Math.PI);
            g.lineWidth = 1.5;
            g.strokeStyle = cssvar("--color-text-strong");
            g.stroke();
        }
    }

    compass(f);
}

function point(e) {
    const box = canvas.getBoundingClientRect(), x = e.clientX - box.left, y = e.clientY - box.top;
    let best = null, near = 14;
    for (const p of placed) {
        const d = Math.hypot(p.x - x, p.y - y) - p.r;
        if (d < near) { near = d; best = p; }
    }
    return best ? { p: best, x, y } : null;
}

function showTip(e) {
    const at = point(e), s = at ? at.p.s : null;
    if (s === hover) {
        if (at) placeTip(tip, canvas, at);
        return;
    }
    hover = s;
    draw();
    if (!s) {
        tip.hidden = true;
        return;
    }
    tip.replaceChildren();
    line(tip, "polar-tip-value", system(s)[0] + " " + s[ID]);
    line(tip, "polar-tip-row", s[SIGNAL] < 0 ? "no signal reported" : "signal " + s[SIGNAL] + " dB-Hz");
    line(tip, "polar-tip-row", s[ELEVATION] + "° up, toward " + compassName(s[AZIMUTH]) + " (" + s[AZIMUTH] + "°)");
    line(tip, "polar-tip-row", s[USED] ? "used for the position" : "seen only");
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
    const all = data.satellites, used = all.filter((s) => s[USED]).length, systems = new Set(all.map((s) => s[SYSTEM])).size;
    const loose = all.filter((s) => s[ELEVATION] < 0 || s[AZIMUTH] < 0).length;
    let text = all.length ? used + " of " + all.length + " satellites used, from " + systems + (systems === 1 ? " system" : " systems") : "No satellites in view";
    text += FIX[data.fix] ? ". " + FIX[data.fix] + (data.hdop > 0 ? ", HDOP " + data.hdop.toFixed(1) : "") + "." : data.fix === 1 ? ". No fix." : ".";
    if (loose) text += " " + loose + (loose === 1 ? " is" : " are") + " seen without a direction and not drawn.";
    if (data.age >= 120) text += " Last updated " + (data.age < 5400 ? Math.round(data.age / 60) + " minutes" : Math.round(data.age / 3600) + " hours") + " ago.";
    sub.textContent = text;
    canvas.setAttribute("aria-label", "Satellites in view by direction and elevation. " + text);
}

function key() {
    legend.replaceChildren();
    const present = new Set(data.satellites.map((s) => s[SYSTEM]));
    for (const id of ORDER) if (present.has(id)) legendItem(legend, "polar-key-sat", SYSTEMS[id][0], cssvar(SYSTEMS[id][1]));
    legendItem(legend, "polar-key-sat", "used for the position", cssvar("--color-text-muted"));
    legendItem(legend, "polar-key-sat seen", "seen only", cssvar("--color-text-muted"));
    legendItem(legend, "", "a bigger dot is a stronger signal");
}

function bars() {
    signals.replaceChildren();
    for (const id of ORDER) {
        const sats = data.satellites.filter((s) => s[SYSTEM] === id && s[SIGNAL] > 0).sort((a, b) => b[SIGNAL] - a[SIGNAL]);
        if (!sats.length) continue;
        const group = document.createElement("div"), row = document.createElement("div");
        group.className = "sat-group";
        group.style.flexGrow = sats.length;
        group.style.setProperty("--mark", cssvar(SYSTEMS[id][1]));
        row.className = "sat-row";
        for (const s of sats) {
            const bar = document.createElement("i");
            bar.className = s[USED] ? "" : "seen";
            bar.style.height = Math.min(100, (100 * s[SIGNAL]) / 55) + "%";
            bar.title = SYSTEMS[id][0] + " " + s[ID] + ": " + s[SIGNAL] + " dB-Hz" + (s[USED] ? "" : ", seen only");
            row.append(bar);
        }
        group.append(row);
        line(group, "sat-name", SYSTEMS[id][0]);
        signals.append(group);
    }
}

function refresh() {
    hover = null;
    tip.hidden = true;
    describe();
    key();
    bars();
    draw();
}

export function init() {
    canvas = document.getElementById("chart-sky");
    if (!canvas) return;
    panel = canvas.closest(".graph-panel");
    tip = panel.querySelector(".polar-tip");
    legend = panel.querySelector(".polar-legend");
    sub = panel.querySelector(".polar-sub");
    signals = document.getElementById("sky-signals");
    canvas.addEventListener("pointermove", showTip);
    canvas.addEventListener("pointerdown", showTip);
    canvas.addEventListener("pointerleave", hideTip);
    if (window.ResizeObserver) new ResizeObserver(draw).observe(canvas.parentElement);
}

export function update(h) {
    if (!panel) return;
    data = h && h.gnss && h.gnss.satellites ? h.gnss : null;
    panel.classList.toggle("polar-none", !data || !data.satellites.some((s) => s[ELEVATION] >= 0 && s[AZIMUTH] >= 0));
    signals.parentElement.classList.toggle("polar-none", !data || !data.satellites.some((s) => s[SIGNAL] > 0));
    if (data) refresh();
}

export function repaint() {
    if (data) refresh();
}
