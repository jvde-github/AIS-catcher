
export const cssvar = (name) =>
    getComputedStyle(document.documentElement).getPropertyValue(name).trim() || getComputedStyle(document.body).getPropertyValue(name).trim();

export const angle = (deg) => ((deg - 90) * Math.PI) / 180;

const COMPASS = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"];
export const compassName = (deg) => COMPASS[Math.round(deg / 45) % 8];

export function frame(canvas) {
    const size = Math.floor(canvas.parentElement.getBoundingClientRect().width);
    if (!size) return null;
    const dpr = window.devicePixelRatio || 1;
    if (canvas.width !== Math.round(size * dpr) || canvas.height !== Math.round(size * dpr)) {
        canvas.width = canvas.height = Math.round(size * dpr);
        canvas.style.width = canvas.style.height = size + "px";
    }
    const g = canvas.getContext("2d");
    g.setTransform(dpr, 0, 0, dpr, 0, 0);
    g.clearRect(0, 0, size, size);
    return { g, cx: size / 2, cy: size / 2, R: size / 2 - 26 };
}

export function disc(f) {
    const { g, cx, cy, R } = f, fill = g.createRadialGradient(cx, cy, 0, cx, cy, R);
    fill.addColorStop(0, cssvar("--coverage-disc-in"));
    fill.addColorStop(1, cssvar("--coverage-disc-out"));
    g.beginPath();
    g.arc(cx, cy, R, 0, 2 * Math.PI);
    g.fillStyle = fill;
    g.fill();
}

export function spokes(f) {
    const { g, cx, cy, R } = f;
    g.lineWidth = 1;
    g.strokeStyle = cssvar("--color-chart-grid");
    g.globalAlpha = 0.5;
    for (let d = 0; d < 8; d++) {
        g.beginPath();
        g.moveTo(cx, cy);
        g.lineTo(cx + R * Math.cos(angle(d * 45)), cy + R * Math.sin(angle(d * 45)));
        g.stroke();
    }
    g.globalAlpha = 1;
}

export function ring(f, r) {
    const { g, cx, cy } = f;
    g.beginPath();
    g.setLineDash([3, 4]);
    g.arc(cx, cy, r, 0, 2 * Math.PI);
    g.lineWidth = 1;
    g.strokeStyle = cssvar("--color-chart-grid");
    g.stroke();
    g.setLineDash([]);
}

export function rim(f) {
    const { g, cx, cy, R } = f;
    g.beginPath();
    g.arc(cx, cy, R, 0, 2 * Math.PI);
    g.lineWidth = 1;
    g.strokeStyle = cssvar("--color-border-strong");
    g.stroke();
}

export function pill(f, text, r, bearing = 22.5) {
    const { g, cx, cy } = f;
    g.font = "500 11px " + cssvar("--font-sans");
    g.textAlign = "center";
    g.textBaseline = "middle";
    const w = g.measureText(text).width + 10, x = cx + r * Math.cos(angle(bearing)), y = cy + r * Math.sin(angle(bearing));
    g.fillStyle = cssvar("--color-surface");
    g.beginPath();
    g.roundRect(x - w / 2, y - 8, w, 16, 8);
    g.fill();
    g.fillStyle = cssvar("--color-text-muted");
    g.fillText(text, x, y + 0.5);
}

export function compass(f) {
    const { g, cx, cy, R } = f;
    g.textAlign = "center";
    g.textBaseline = "middle";
    COMPASS.forEach((name, d) => {
        const main = d % 2 === 0;
        g.font = (main ? "600 13px " : "500 11px ") + cssvar("--font-sans");
        g.fillStyle = cssvar(d === 0 ? "--color-chart-4" : main ? "--color-text-strong" : "--color-text-muted");
        g.fillText(name, cx + (R + 14) * Math.cos(angle(d * 45)), cy + (R + 14) * Math.sin(angle(d * 45)));
    });
}

export function line(parent, cls, text) {
    const el = document.createElement("div");
    el.className = cls;
    el.textContent = text;
    parent.append(el);
}

export function placeTip(tip, canvas, at) {
    const box = canvas.getBoundingClientRect();
    tip.style.left = Math.max(0, Math.min(box.width - tip.offsetWidth, at.x + 12)) + "px";
    tip.style.top = Math.max(0, Math.min(box.height - tip.offsetHeight, at.y + 12)) + "px";
}

export function legendItem(legend, cls, text, color) {
    if (cls) {
        const mark = document.createElement("span");
        mark.className = cls;
        if (color) mark.style.setProperty("--mark", color);
        legend.append(mark);
    }
    if (text) {
        const words = document.createElement("span");
        words.textContent = text;
        legend.append(words);
    }
}
