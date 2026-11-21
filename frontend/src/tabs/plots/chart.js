const NS = "http://www.w3.org/2000/svg";
const TEXT = 6.2;

function node(name, attrs, parent) {
    const e = document.createElementNS(NS, name);
    for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
    parent.append(e);
    return e;
}

function html(tag, cls, parent, text = "") {
    const e = document.createElement(tag);
    e.className = cls;
    e.textContent = text;
    parent.append(e);
    return e;
}

const paint = (e, prop, name, alpha) => {
    e.style[prop] = `var(${name})`;
    if (alpha !== undefined) e.style[prop + "Opacity"] = alpha;
};

const label = (svg, x, y, text, anchor = "middle") => (node("text", { x, y, "text-anchor": anchor }, svg).textContent = text);

const FORMAT = new Intl.NumberFormat("en", { notation: "compact", maximumSignificantDigits: 3 });
export const compact = (v) => FORMAT.format(v);

function stride(span, count, whole) {
    const raw = span / count, mag = 10 ** Math.floor(Math.log10(raw)), f = raw / mag;
    const s = (f < 1.5 ? 1 : f < 3.5 ? 2 : f < 7.5 ? 5 : 10) * mag;
    return whole ? Math.max(1, Math.round(s)) : s;
}

function ticksOf(lo, hi, count, whole) {
    const s = stride(hi - lo || 1, count, whole), out = [];
    for (let v = Math.ceil(lo / s - 1e-9) * s; v <= hi + s * 1e-9; v += s) out.push(Math.abs(v) < s * 1e-9 ? 0 : v);
    return out;
}

function linear(values, zero, count) {
    if (zero || !values.length) values = [...values, 0];
    let lo = Math.min(...values), hi = Math.max(...values);
    if (!(hi > lo)) [lo, hi] = [lo - 1, hi + 1];
    const s = stride(hi - lo, count);
    lo = Math.floor(lo / s + 1e-9) * s;
    hi = Math.ceil(hi / s - 1e-9) * s;
    return { ticks: ticksOf(lo, hi, (hi - lo) / s), at: (v) => (v - lo) / (hi - lo) };
}

function logarithmic(values) {
    const positive = values.filter((v) => v > 0);
    const a = Math.floor(Math.log10(Math.min(...positive, 1))), b = Math.max(a + 1, Math.ceil(Math.log10(Math.max(...positive, 1))));
    const ticks = [];
    for (let e = a; e <= b; e++) for (const m of b - a > 3 ? [1] : [1, 2, 5]) if (e < b || m === 1) ticks.push(m * 10 ** e);
    return { ticks, at: (v) => (Math.log10(Math.max(v, 10 ** a)) - a) / (b - a) };
}

export function chart(host, options = {}) {
    host.classList.add("chart");
    const legend = html("div", "chart-legend", host), svg = node("svg", { role: "img" }, host), tip = html("div", "chart-tip", host);
    let data = null, width = 0;
    tip.hidden = true;

    function draw(next) {
        if (next) data = next;
        const W = host.clientWidth;
        if (!data || !W) return;

        legend.hidden = !(data.legend ?? options.legend);
        legend.replaceChildren();
        for (const s of legend.hidden ? [] : data.series) {
            const key = html("span", "chart-key", legend);
            paint(html("i", "", key), "color", s.color);
            key.append(s.label);
        }

        const shown = data.series.filter((s) => s.points.length);
        let stack = null, before = null;
        for (const s of shown) {
            s.ys = s.points.map((p) => p.y);
            s.base = s.fill === "band" ? before : s.fill === "stack" ? stack : null;
            if (s.fill === "stack") stack = s.ys = s.ys.map((y, i) => y + (stack ? stack[i] : 0));
            before = s.ys;
        }

        const H = Math.round(W / 2), m = { l: 44 + (options.yTitle ? 16 : 0), r: 12, t: 10, b: 22 + (options.xTitle ? 18 : 0) };
        const pw = W - m.l - m.r, ph = H - m.t - m.b, values = shown.flatMap((s) => s.ys).filter(Number.isFinite);
        const yl = options.log ? logarithmic(values) : linear(values, options.zero, Math.max(2, Math.round(ph / 34)));

        const xs = [...new Set(shown.flatMap((s) => s.points.map((p) => p.x)))].sort((a, b) => a - b);
        const pad = options.bars || xs.length < 2 ? 0.5 : 0, x0 = (xs[0] ?? 0) - pad, x1 = (xs[xs.length - 1] ?? 0) + pad;
        const X = (v) => m.l + ((v - x0) / (x1 - x0)) * pw, Y = (v) => m.t + ph * (1 - yl.at(v));
        const bottom = m.t + ph;

        svg.replaceChildren();
        svg.setAttribute("viewBox", `0 0 ${W} ${H}`);

        for (const v of yl.ticks) {
            paint(node("line", { x1: m.l, x2: m.l + pw, y1: Y(v), y2: Y(v) }, svg), "stroke", "--color-chart-grid");
            label(svg, m.l - 6, Y(v) + 4, compact(v), "end");
        }

        let prev;
        for (const v of ticksOf(x0, x1, Math.max(2, Math.floor(pw / 70)), true)) {
            paint(node("line", { x1: X(v), x2: X(v), y1: m.t, y2: bottom }, svg), "stroke", "--color-chart-grid");
            label(svg, X(v), bottom + 15, data.tick ? data.tick(v, prev) : v, X(v) > W - 30 ? "end" : "middle");
            prev = v;
        }

        if (options.xTitle) node("text", { x: m.l + pw / 2, y: H - 4, "text-anchor": "middle", class: "chart-title" }, svg).textContent = options.xTitle;
        if (options.yTitle) node("text", { transform: `translate(12 ${m.t + ph / 2}) rotate(-90)`, "text-anchor": "middle" }, svg).textContent = options.yTitle;

        for (const s of shown) {
            const at = s.points.map((p, i) => (Number.isFinite(s.ys[i]) ? [X(p.x), Y(s.ys[i])] : null));
            if (options.bars) {
                const w = (pw / (x1 - x0)) * 0.8, zero = Y(0);
                for (const p of at.filter(Boolean)) {
                    const bar = node("rect", { x: p[0] - w / 2, y: Math.min(p[1], zero), width: w, height: Math.abs(p[1] - zero) }, svg);
                    paint(bar, "fill", s.color, 0.45);
                    paint(bar, "stroke", s.color);
                }
                continue;
            }
            const filled = (i) => s.fill && at[i] && (!s.base || Number.isFinite(s.base[i]));
            for (let i = 0, j; i < at.length; i = j + 1) {
                for (j = i; filled(j) && filled(j + 1); j++);
                if (!filled(i)) continue;
                const top = at.slice(i, j + 1);
                const floor = s.base ? top.map((p, k) => [p[0], Y(s.base[i + k])]).reverse() : [[top[top.length - 1][0], bottom], [top[0][0], bottom]];
                paint(node("path", { d: "M" + [...top, ...floor].map((p) => p.join(" ")).join("L") + "Z" }, svg), "fill", s.color, 0.3);
            }
            if (s.width) {
                const d = at.map((p, i) => (p ? (at[i - 1] ? "L" : "M") + p.join(" ") : "")).join("");
                paint(node("path", { d, fill: "none", "stroke-width": s.width, "stroke-linejoin": "round" }, svg), "stroke", s.color);
            }
        }

        const counted = xs.filter((x) => x !== 0);
        let mark = data.mark;
        if (options.average && shown.length && xs.length) {
            const top = shown[shown.length - 1], sum = shown.some((s) => s.fill === "stack") ? (i) => top.ys[i] : (i) => shown[0].ys[i];
            const total = top.points.reduce((t, p, i) => t + (p.x !== 0 || xs.length === 1 ? sum(i) : 0), 0);
            const value = total / (xs.length === 1 ? 1 : counted.length);
            if (Number.isFinite(value)) mark = { value, text: compact(value) };
        }
        if (mark) {
            const y = Y(mark.value), w = mark.text.length * TEXT + 12;
            node("line", { x1: m.l, x2: m.l + pw, y1: y, y2: y, class: "chart-mark" }, svg);
            node("rect", { x: m.l + 6, y: y - 9, width: w, height: 18, rx: 4, class: "chart-chip" }, svg);
            label(svg, m.l + 6 + w / 2, y + 4, mark.text);
        }

        const guide = node("line", { y1: m.t, y2: bottom, class: "chart-guide", visibility: "hidden" }, svg);
        const over = node("rect", { x: m.l, y: m.t, width: pw, height: ph, fill: "none", "pointer-events": "all" }, svg);
        over.onpointerleave = () => {
            tip.hidden = true;
            guide.setAttribute("visibility", "hidden");
        };
        over.onpointermove = (e) => {
            const box = svg.getBoundingClientRect(), px = e.clientX - box.left, v = x0 + ((px - m.l) / pw) * (x1 - x0);
            const x = xs.reduce((best, c) => (Math.abs(c - v) < Math.abs(best - v) ? c : best), xs[0]);
            if (x === undefined) return;
            guide.setAttribute("x1", X(x));
            guide.setAttribute("x2", X(x));
            guide.setAttribute("visibility", "visible");
            tip.replaceChildren();
            html("b", "", tip, data.title ? data.title(x) : String(x));
            for (const s of shown) {
                const i = s.points.findIndex((p) => p.x === x);
                if (i < 0 || !Number.isFinite(s.points[i].y)) continue;
                const row = html("div", "", tip);
                paint(html("i", "", row), "background", s.color);
                row.append(`${s.label}: ${compact(s.points[i].y)}`);
            }
            tip.hidden = false;
            const top = host.getBoundingClientRect().top, y = e.clientY - top, h = tip.offsetHeight;
            tip.style.top = (y + 12 + h <= Math.min(host.clientHeight, innerHeight - top) ? y + 12 : Math.max(0, y - 12 - h)) + "px";
            tip.style.left = px > W / 2 ? "" : px + 12 + "px";
            tip.style.right = px > W / 2 ? W - px + 12 + "px" : "";
        };
    }

    new ResizeObserver(() => {
        if (host.clientWidth !== width) draw();
        width = host.clientWidth;
    }).observe(host);
    return { draw };
}
