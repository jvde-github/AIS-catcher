// Inline-SVG charts for cards and panels: data in, markup out. Colour comes
// from classes (.dim-*, .sc-*, .tl-*) so the same chart follows any theme.
// Builders that print numbers take a units instance (core/units.js).

import { shipOutlineLocal } from './geo.js';
import { CHANGE, CHANGE_LABEL, getStatusVal, sanitizeString } from './text.js';

let chartSeq = 0;

/* The generic step chart behind the speed and draught builders; hosts plot
   their own series with it. fmt formats the axis maximum (default 1 decimal). */
export function stepChartSVG(series, unit, footLeft, footRight, aria, fmt) {
    if (series.length < 2) return "";
    const tick = fmt || ((v) => v.toFixed(1));

    const t0 = series[0].t, t1 = Math.max(series[series.length - 1].t, t0 + 1);
    const vmax = Math.max(...series.map((p) => p.v));
    const top = vmax > 0 ? vmax : 1;

    const W = 280, H = 96, L = 34, R = 10, T = 14, B = 22;
    const x = (t) => (L + ((t - t0) / (t1 - t0)) * (W - L - R)).toFixed(1);
    const y = (v) => (T + (1 - v / top) * (H - T - B)).toFixed(1);

    const pts = [];
    for (let i = 0; i < series.length; i++) {
        const p = series[i];
        pts.push(x(p.t) + "," + y(p.v));
        const nxt = series[i + 1];
        if (nxt) pts.push(x(nxt.t) + "," + y(p.v));   // hold until the next report
    }
    const last = series[series.length - 1];
    const grad = "chartGrad" + ++chartSeq;
    const area = pts.concat([x(last.t) + "," + y(0), x(series[0].t) + "," + y(0)]);

    return `<svg viewBox="0 0 ${W} ${H}" class="dim-svg" role="img" aria-label="${aria}">
  <defs>
    <linearGradient id="${grad}" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" class="chart-grad-top"/>
      <stop offset="1" class="chart-grad-bottom"/>
    </linearGradient>
  </defs>
  <line x1="${L}" y1="${y(0)}" x2="${W - R}" y2="${y(0)}" class="spd-axis"/>
  <line x1="${L}" y1="${T}" x2="${L}" y2="${y(0)}" class="spd-axis"/>
  <line x1="${L}" y1="${y(top)}" x2="${W - R}" y2="${y(top)}" class="spd-grid"/>
  <text x="${L - 4}" y="${(+y(top) + 3).toFixed(1)}" class="dim-t" text-anchor="end">${tick(top)}</text>
  <text x="${L - 4}" y="${(+y(0) + 3).toFixed(1)}" class="dim-t" text-anchor="end">0</text>
  <polygon points="${area.join(" ")}" fill="url(#${grad})" stroke="none"/>
  <polyline points="${pts.join(" ")}" class="spd-line"/>
  <circle cx="${x(last.t)}" cy="${y(last.v)}" r="3" class="dim-origin"/>
  <text x="${L}" y="${H - 6}" class="dim-t">${footLeft}</text>
  <text x="${W - R}" y="${H - 6}" class="dim-t" text-anchor="end">${footRight}</text>
</svg>`;
}

export function getDraughtChartSVG(changes, currentDraught, units) {
    const rows = (changes || []).filter((c) => c.f === CHANGE.DRAUGHT && c.from != null)
        .slice().sort((a, b) => a.t - b.t);
    if (!rows.length) return "";
    const nChanges = rows.filter((r) => !r.i).length;

    const series = [{ t: rows[0].t - 1, v: rows[0].from / 10 }];
    for (const r of rows) series.push({ t: r.t, v: r.to / 10 });
    if (currentDraught != null) series.push({ t: Math.floor(Date.now() / 1000), v: currentDraught });

    const u = units.getDimUnit();
    const disp = (m) => Number(units.getDraughtVal(m));
    const scaled = series.map((p) => ({ t: p.t, v: disp(p.v) }));
    const last = scaled[scaled.length - 1];

    const secs = scaled[scaled.length - 1].t - scaled[0].t;
    const span = secs < 120 ? secs + " s" : secs < 7200 ? Math.round(secs / 60) + " min" : Math.round(secs / 3600) + " h";
    const flat = scaled.every((p) => p.v === last.v);
    return stepChartSVG(scaled, u, span + " · " + nChanges + (nChanges === 1 ? " change" : " changes"),
        (flat ? "stayed at " : "now · ") + last.v.toFixed(1) + " " + u,
        "Draught history, " + nChanges + " changes, " + (flat ? "steady at " : "now ") + last.v.toFixed(1) + " " + u);
}

export function getChangeListHTML(changes, fields, units) {
    const rows = (changes || []).filter((c) => fields.indexOf(c.f) !== -1)
        .slice().sort((a, b) => a.t - b.t);          // oldest first to trace values
    if (!rows.length) return "";

    // Numeric changes carry their own before/after; text changes carry only the
    // new value, so the previous one is whatever the same field last said.
    const text = (c, v) => (typeof v === "string" ? v : units ? units.getChangeVal({ f: c.f, to: v }) : String(v));
    const seen = {};
    const events = rows.map((r) => {
        const to = text(r, r.to);
        const from = r.from != null && !r.i ? text(r, r.from) : seen[r.f];
        seen[r.f] = to;
        return { t: r.t, f: r.f, initial: !!r.i, from, to };
    });

    let html = '<ol class="tl">';
    for (const e of events.slice(-24).reverse()) {
        const when = new Date(e.t * 1000).toLocaleString([], {
            day: "numeric", month: "short", hour: "2-digit", minute: "2-digit" });
        html += '<li class="tl-item">'
             +  '<div class="tl-head"><span class="tl-dot"></span>'
             +  `<span class="tl-title">${CHANGE_LABEL[e.f] || ""}</span>`
             +  `<span class="tl-time">${when}</span></div><div class="tl-vals">`;

        if (e.from != null && e.from !== e.to)
            html += `<span class="tl-pill tl-was">${sanitizeString(e.from)}</span>`
                 +  '<span class="tl-arrow">&rarr;</span>';

        html += `<span class="tl-pill">${sanitizeString(e.to)}</span>`;
        if (e.initial) html += '<span class="tl-firstseen">first seen</span>';
        html += "</div></li>";
    }
    return html + "</ol>";
}

export function getSpeedHistorySVG(pts, sogDiv, units) {
    if (!Array.isArray(pts) || pts.length === 0) return "";

    // the path endpoint walks the track backwards, so normalise to oldest-first
    if (pts.length > 1 && pts[0][2] > pts[pts.length - 1][2]) pts = pts.slice().reverse();

    const unit = units.getSpeedUnit();
    const toDisplay = (raw) => Number(units.getSpeedVal(raw / sogDiv));

    const series = [];
    for (const p of pts) {
        if (p[4] == null) continue;
        const v = toDisplay(p[4]);
        series.push({ t: p[2], v });
        if (p[3] > p[2]) series.push({ t: p[3], v });
    }
    if (series.length < 2) return "";

    const secs = series[series.length - 1].t - series[0].t;
    const span = secs < 120 ? secs + " s" : Math.round(secs / 60) + " min";
    const lastV = series[series.length - 1].v;
    const vmax = Math.max(...series.map((p) => p.v));
    const flat = series.every((p) => p.v === lastV);

    return stepChartSVG(series, unit, span + " of history",
        (flat ? "stayed at " : "now · ") + lastV.toFixed(1) + " " + unit,
        "Speed history over the last " + span + (flat ? ", steady at " : ", peaking at ") + vmax.toFixed(1) + " " + unit);
}

export function getShipDimensionSVG(ship, units) {
    const { to_bow: a, to_stern: b, to_port: c, to_starboard: d } = ship;
    if (a == null || b == null || c == null || d == null) return "";

    const loa = a + b, beam = c + d;
    if (loa <= 0 || beam <= 0) return "";

    const W = 280, LEFT = 46, HULL_W = 175;
    const BEAM_X = 30, BEAM_LABEL = 26;
    const MAX_HALF = 30;
    const k = Math.min(HULL_W / loa, MAX_HALF / Math.max(c, d));
    const MID = 34 + c * k;

    const ox = LEFT + b * k;
    const px = (f) => (ox + f * k).toFixed(1);
    const py = (s) => (MID + s * k).toFixed(1);

    const ring = shipOutlineLocal(a, b, c, d).map(([f, s]) => px(f) + "," + py(s)).join(" ");

    const u = units.getDimUnit();
    const lbl = (v) => units.getDimVal(v) + " " + u;

    // A hull too short or too thin to hold a label puts that pair outside it.
    const hullL = +px(-b), hullR = +px(a);
    const hullTopY = +py(-c), hullBotY = +py(d);
    const SIDE_X = hullR + 12, SIDE_LABEL = SIDE_X + 6;

    const thin = hullBotY - hullTopY < 26;
    const portY = thin ? hullTopY - 3 : hullTopY + 8;
    const stbdY = thin ? hullBotY + 10 : hullBotY - 2;

    const tight = hullR - hullL < 44;
    const sternX = tight ? hullL - 3 : hullL, sternAnchor = tight ? "end" : "start";
    const bowX = tight ? hullR + 3 : hullR, bowAnchor = tight ? "start" : "end";

    const GAP = 9;                               // the same clearance top and bottom
    const yArrow = 22, yLabel = 16;
    const hullTop = MID - c * k, hullBottom = MID + d * k;
    const yLoa = +(hullBottom + GAP).toFixed(1);
    const yLoaLabel = +(yLoa + 14).toFixed(1);
    const H = +(yLoaLabel + 6).toFixed(1);

    return `<svg viewBox="0 0 ${W} ${H}" class="dim-svg" role="img" aria-label="Hull outline, ${lbl(loa)} by ${lbl(beam)}, reported position ${lbl(a)} from the bow and ${lbl(b)} from the stern">
  <defs>
    <marker id="dimArrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="5" markerHeight="5" orient="auto-start-reverse">
      <path d="M 0 0 L 10 5 L 0 10 z" class="dim-arrowhead"/>
    </marker>
  </defs>

  <line x1="${px(-b)}" y1="${yArrow + 4}" x2="${px(-b)}" y2="${py(-c)}" class="dim-ext"/>
  <line x1="${px(0)}"  y1="${yArrow + 4}" x2="${px(0)}"  y2="${py(0)}"  class="dim-ext"/>
  <line x1="${px(a)}"  y1="${yArrow + 4}" x2="${px(a)}"  y2="${py(0)}"  class="dim-ext"/>
  <line x1="${px(-b)}" y1="${yArrow}" x2="${px(0)}" y2="${yArrow}" class="dim-arrow" marker-start="url(#dimArrow)" marker-end="url(#dimArrow)"/>
  <line x1="${px(0)}"  y1="${yArrow}" x2="${px(a)}" y2="${yArrow}" class="dim-arrow" marker-start="url(#dimArrow)" marker-end="url(#dimArrow)"/>
  <text x="${sternX.toFixed(1)}" y="${yLabel}" class="dim-t" text-anchor="${sternAnchor}">${lbl(b)}</text>
  <text x="${bowX.toFixed(1)}" y="${yLabel}" class="dim-t" text-anchor="${bowAnchor}">${lbl(a)}</text>

  <polygon points="${ring}" class="dim-hull"/>

  <line x1="${BEAM_X}" y1="${py(-c)}" x2="${px(-b)}" y2="${py(-c)}" class="dim-ext"/>
  <line x1="${BEAM_X}" y1="${py(d)}"  x2="${px(-b)}" y2="${py(d)}"  class="dim-ext"/>
  <line x1="${BEAM_X}" y1="${py(-c)}" x2="${BEAM_X}" y2="${py(d)}" class="dim-arrow" marker-start="url(#dimArrow)" marker-end="url(#dimArrow)"/>
  <text x="${BEAM_LABEL}" y="${(+py(0) + 3).toFixed(1)}" class="dim-t" text-anchor="end">${lbl(beam)}</text>

  <line x1="${px(0.8 * loa - b)}" y1="${py(-c)}" x2="${SIDE_X}" y2="${py(-c)}" class="dim-ext"/>
  <line x1="${px(0)}"             y1="${py(0)}"  x2="${SIDE_X}" y2="${py(0)}"  class="dim-ext"/>
  <line x1="${px(0.8 * loa - b)}" y1="${py(d)}"  x2="${SIDE_X}" y2="${py(d)}"  class="dim-ext"/>
  <line x1="${SIDE_X}" y1="${py(-c)}" x2="${SIDE_X}" y2="${py(0)}" class="dim-arrow" marker-start="url(#dimArrow)" marker-end="url(#dimArrow)"/>
  <line x1="${SIDE_X}" y1="${py(0)}"  x2="${SIDE_X}" y2="${py(d)}" class="dim-arrow" marker-start="url(#dimArrow)" marker-end="url(#dimArrow)"/>
  <text x="${SIDE_LABEL}" y="${portY.toFixed(1)}" class="dim-t">${lbl(c)}</text>
  <text x="${SIDE_LABEL}" y="${stbdY.toFixed(1)}" class="dim-t">${lbl(d)}</text>

  <circle cx="${px(0)}" cy="${py(0)}" r="6.5" class="dim-origin-ring"/>
  <circle cx="${px(0)}" cy="${py(0)}" r="3.2" class="dim-origin"/>

  <line x1="${px(-b)}" y1="${py(d)}" x2="${px(-b)}" y2="${yLoa + 4}" class="dim-ext"/>
  <line x1="${px(a)}"  y1="${py(0)}" x2="${px(a)}"  y2="${yLoa + 4}" class="dim-ext"/>
  <line x1="${px(-b)}" y1="${yLoa}" x2="${px(a)}" y2="${yLoa}" class="dim-arrow" marker-start="url(#dimArrow)" marker-end="url(#dimArrow)"/>
  <text x="${px((a - b) / 2)}" y="${yLoaLabel}" class="dim-t" text-anchor="middle">${lbl(loa)} overall</text>
</svg>`;
}
