
import { cssvar, angle, compassName, frame, disc, spokes, ring, rim, pill, compass, line, placeTip, legendItem } from './polar.js';

// a satellite as the server sends it; SIGNAL is of L1, the other bands after it
export const SYSTEM = 0, ID = 1, ELEVATION = 2, AZIMUTH = 3, SIGNAL = 4, USED = 5, L2 = 6, L5 = 7;
export const SYSTEMS = {
    1: ["GPS", "--gnss-gps"], 3: ["Galileo", "--gnss-galileo"], 2: ["GLONASS", "--gnss-glonass"], 4: ["BeiDou", "--gnss-beidou"],
    5: ["QZSS", "--color-chart-3"], 6: ["NavIC", "--color-text-muted"], 7: ["SBAS", "--color-text-muted"], 0: ["Other", "--color-text-muted"],
};
const ORDER = [1, 3, 2, 4, 5, 6, 7, 0];
const FIX = { 2: "2D fix", 3: "3D fix" };

let canvas, tip, legend, sub, panel;
let data = null, hover = null, placed = [];

const system = (s) => SYSTEMS[s[SYSTEM]] || SYSTEMS[0];
// the strongest band: an L5-only satellite has no L1 signal
const best = (s) => Math.max(s[SIGNAL], s[L2] ?? -1, s[L5] ?? -1);
const size = (s) => 3.5 + (Math.max(0, Math.min(50, best(s))) / 50) * 5;

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

    placed = data.satellites.filter((s) => s[ELEVATION] >= 0 && s[AZIMUTH] >= 0).sort((a, b) => best(b) - best(a)).map((s) => ({
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
    const bands = [["L1", s[SIGNAL]], ["L2", s[L2] ?? -1], ["L5", s[L5] ?? -1]].filter(([, v]) => v > 0);
    line(tip, "polar-tip-row", bands.length ? bands.map(([b, v]) => b + " " + v + " dB-Hz").join(", ") : "no signal reported");
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
    legendItem(legend, "polar-key-sat", "used", cssvar("--color-text-muted"));
    legendItem(legend, "polar-key-sat seen", "not used", cssvar("--color-text-muted"));
    legendItem(legend, "", "marker size indicates C/N₀");
}

function refresh() {
    hover = null;
    tip.hidden = true;
    describe();
    key();
    draw();
}

export function init() {
    canvas = document.getElementById("chart-sky");
    if (!canvas) return;
    panel = canvas.closest(".polar-panel");
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
    data = h && h.gnss && h.gnss.satellites ? h.gnss : null;
    panel.classList.toggle("polar-none", !data || !data.satellites.some((s) => s[ELEVATION] >= 0 && s[AZIMUTH] >= 0));
    if (data) refresh();
}

export function repaint() {
    if (data) refresh();
}
