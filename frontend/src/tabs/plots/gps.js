// The GPS sub-tab: the receiver now, whether it was jammed or spoofed, and what it did over the last
// day or week. The server keeps the history (api/gnss.json); the events are read from it here.

import { cssvar } from './polar.js';
import { decode, merge } from './gnss-data.js';
import { head, figure, summary } from './head.js';
import { setup, ART } from './setup.js';
import { SYSTEMS, SYSTEM, ID, SIGNAL, USED, L2, L5 } from './sky.js';
import { gpsTitle } from '../../features/status/health-data.js';
import { sanitizeString } from '@aiscatcher/core/text.js';

const ORDER = [1, 2, 3, 4];
const BAND_NAME = { 1: ["L1", "L2", "L5"], 2: ["G1", "G2", "G3"], 3: ["E1", "E5b", "E5a"], 4: ["B1", "B3", "B2a"] };
const FIX = { 0: "—", 1: "No fix", 2: "2D", 3: "3D" };
const SPAN = { "1h": ["day", 30], "3h": ["day", 90], "24h": ["day", 720], "7d": ["week", 1008] };

let root = null, range = "24h", band = 0, allRows = false, selected = null, hover = null, chosen = false, last = [null, -1];
let day = null, week = null, sky = null, verdict = null, hidden = false, fetchedAt = 0, askedAt = 0, fetchedFor = null, tailAt = 0, events = [];
let geometry = null;
// the page polls far more often than what is shown changes: the card and the signals are drawn again
// with a new sky or verdict, the charts of the history only with a new history, range or choice
let stale = true, shown = "";
const hiddenLines = new Set(), folded = new Set();

const $ = (id) => document.getElementById(id);
const clock = (t) => new Date(t * 1000).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit", hourCycle: "h23" });
const dayClock = (t) => {
    const d = new Date(t * 1000), today = new Date();
    return (d.toDateString() === today.toDateString() ? "" : d.toLocaleDateString([], { day: "numeric", month: "short" }) + " ") + clock(t);
};
const minutes = (n) => (n < 90 ? Math.round(n) + " min" : (n / 60).toFixed(1) + " h");

// ---------- data ----------

// The day's history is large and grows a slot at a time: it is asked for whole once, and after that only
// its newest slots, laid over what is held. The whole comes again when the station or the span changes,
// when the tail no longer reaches back, and once an hour so that nothing drifts. The week has no
// satellites and is asked for whole when one of its slots has closed.
async function fetchGnss(focus) {
    const now = Date.now(), want = SPAN[range][0];
    const get = async (query) => {
        const r = await fetch(`api/gnss.json?stations=${focus}&${query}`);
        return r.ok ? decode(await r.json()) : null;
    };
    try {
        const fresh = fetchedFor !== focus || !day || !fetchedAt || now - fetchedAt > 3600000;
        // a station without a GPS answers nothing: it is asked again every half minute, not at every refresh
        if (fresh && fetchedFor === focus && now - askedAt < 30000) return;
        if (fresh) {
            askedAt = now;
            fetchedFor = focus;
            day = await get("span=day");
            week = null;
            fetchedAt = day ? now : 0;
        } else if (now / 1000 >= day.end + day.interval + 3 && now - tailAt > 30000) {
            // the slots that passed, the one that was open when last asked, and one to spare
            const passed = Math.ceil((now / 1000 - day.end) / day.interval) + 2;
            tailAt = now;
            const merged = passed <= 60 ? merge(day, await get("span=day&last=" + passed)) : null;
            if (merged) day = merged;
            else fetchedAt = 0;
        } else if (want !== "week" || (week && now / 1000 < week.end + week.interval + 3)) return;
        if (want === "week" && (!week || now / 1000 >= week.end + week.interval + 3)) week = await get("span=week");
        events = day ? day.events : [];
        if (!chosen) selected = events.filter((e) => e.kind === "jam").pop() || null;
    } catch (error) {
        console.error("gps: history fetch failed:", error);
    }
}

const lasted = (d, e) => ((e.to - e.from + 1) * d.interval) / 60;
const KINDS = { jam: ["Jamming suspected", "--color-warning-ink"], gap: ["No receiver data", "--color-text-subtle"], jump: ["Position jump", "--color-danger"] };

// the hours of history the day ring holds, from its first slot with data
function held() {
    if (!day) return 0;
    const first = day.reports.findIndex((r) => r !== null);
    return first < 0 ? 0 : ((day.n - first) * day.interval) / 3600;
}
const heldText = () => {
    const h = held();
    return h >= 23.5 ? "24 h" : h >= 1 ? Math.round(h) + " h" : Math.round(h * 60) + " min";
};

// ---------- pieces ----------

// the head: the verdict, the receiver now, the constellations, and what is checked
function status() {
    const sats = sky ? sky.satellites : [], used = sats.filter((x) => x[USED]).length;
    const per = ORDER.map((id) => [id, sats.filter((x) => x[SYSTEM] === id && x[USED]).length, sats.filter((x) => x[SYSTEM] === id).length]).filter(([, , n]) => n);
    const pts = day ? day.fixes.filter(Boolean) : [], r95 = radius(pts), span = heldText();
    // without the sky, as on a station that keeps its position home, the health's own figures
    const fix = sky ? sky.fix : verdict ? { "3d": 3, "2d": 2, none: 1 }[verdict.fix] || 0 : 0, hdop = sky ? sky.hdop : verdict && verdict.hdop !== null ? verdict.hdop : -1;
    const signals = day && day.rows.some((r) => r.band === 0), learning = held() < 1;
    const jams = events.filter((e) => e.kind === "jam"), lastJam = jams[jams.length - 1], jumps = events.filter((e) => e.kind === "jump");
    const off = verdict ? verdict.issues.find((i) => i.code === "spoofing_suspected") : null;
    const figures = [
        figure("Fix", FIX[fix] || "—", !fix ? "" : fix === 3 ? "Valid fix" : fix === 2 ? "No altitude" : "No position", fix === 3 ? "good" : fix ? "warn" : ""),
        sky ? figure("Satellites", `${used}<small>/${sats.length}</small>`, `${per.length} system${per.length === 1 ? "" : "s"}`) : figure("Satellites", verdict ? verdict.sats : "—", "Used"),
        figure("HDOP", hdop > 0 ? hdop.toFixed(1) : "—", hdop <= 0 ? "" : hdop < 1 ? "Good geometry" : hdop < 2 ? "Fair geometry" : "Poor geometry"),
        // without the history, what the health says of the position: never the position itself
        day ? figure("Stability", pts.length > 4 ? `${r95.toFixed(1)}<small> m</small>` : "—", `95% · ${span}`)
            : figure("Position", !verdict || verdict.pos_ok === null ? "—" : verdict.pos_ok ? "In place" : "Off", !verdict || verdict.pos_ok === null ? "No reference yet" : "Within 50 m of its reference", verdict && verdict.pos_ok === false ? "warn" : ""),
    ];
    const aside = !per.length ? "" : `<div class="hc-cons">${per.map(([id, u, n]) => `<div><div class="hc-label">${SYSTEMS[id][0]}</div><div class="hc-cvalue">${u}<small>/${n}</small></div><div class="hc-track"><i style="width:${(100 * u) / n}%;background:var(${SYSTEMS[id][1]})"></i></div></div>`).join("")}</div>`;
    const l1 = sats.map((x) => x[SIGNAL]).filter((v) => v > 0), centre = day ? centreOffset(pts) : null;
    const jamDetails = !signals ? [["Signal levels", "not reported by this GPS: jamming cannot be judged"]]
        : lastJam ? [["Last jam", `${dayClock(day.time(lastJam.from))}–${clock(day.time(lastJam.to) + day.interval)}, ${minutes(lasted(day, lastJam))}`], ["L1 signal", `−${lastJam.l1} dB on ${lastJam.satellites} satellites at once`],
            ["L5 signal", lastJam.l5 === null ? "not reported" : Math.abs(lastJam.l5) <= 2 ? "unchanged" : `−${lastJam.l5} dB`], ["Satellites used", lastJam.used_before === null ? "—" : `${lastJam.used_before} → ${lastJam.used_min}`],
            ["Fix", lastJam.fix === 3 ? "stayed 3D" : "3D → " + (lastJam.fix === 2 ? "2D" : "none")], ["Position deviation", `up to ${lastJam.off.toFixed(1)} m`]]
        : [["Signal levels", learning ? "learning each satellite's level" : "normal: no drop across the satellites"], ["Observed", span]];
    const ref = verdict ? verdict.pos_ref : "unknown";
    const spoofDetails = [["Reference", ref === "configured" ? "the station's configured position" : ref === "learned" ? "the day's median position" : "none: the station moves, or under an hour of fixes"],
        ["Latest fix", !verdict || verdict.pos_ok === null ? "—" : verdict.pos_ok ? "within 50 m of the reference" : "more than 50 m from the reference"],
        ["Position", centre === null ? "—" : `centre ${centre.toFixed(1)} m from the median`], ["Position jumps", jumps.length ? `${jumps.length} in ${span}` : `none in ${span}`],
        ["Signal levels", l1.length ? `${Math.min(...l1)}–${Math.max(...l1)} dB-Hz, varied as real satellites are` : "—"]];
    const dl = (rows) => `<dl>${rows.map(([k, v]) => `<dt>${k}</dt><dd>${sanitizeString(v)}</dd>`).join("")}</dl>`;
    const show = lastJam ? `<button type="button" class="hc-show" data-action="selectGpsEvent" data-event="${events.indexOf(lastJam)}">Show the last jam in the charts</button>` : "";
    // what the server judged, each with what was found
    const c = verdict ? verdict.checks : null;
    const checks = !verdict ? null : [
        { state: c.data, title: "Receiver data", note: c.data === "ok" ? "Data arriving" : "The GPS is silent", hint: "The GPS reported in the last two minutes." },
        { state: c.fix, title: "Fix", note: { "3d": "3D fix", "2d": "2D only: no altitude", none: "No fix" }[verdict.fix] || "Not reported", hint: "A 3D fix passes. A 2D fix, or none, counts after half a minute." },
        { state: c.jamming, title: "Jamming",
            note: c.jamming === "degraded" ? "Suspected now" : c.jamming === "ok" ? (jams.length ? `None now, ${jams.length} earlier in ${span}` : "No interference") : c.jamming === "learning" ? "Learning signal levels" : "Not available: no signal levels",
            hint: "Most satellites losing 6 dB at once against their own last half hour counts as jamming." },
        { state: c.position, title: "Position",
            note: c.position === "degraded" ? `${off ? off.value + " m off" : "Off"} its reference: spoofing suspected` : c.position === "ok" ? "Within 50 m of its reference" : c.position === "learning" ? "Learning its position" : "Not available: the station moves",
            hint: "The fix against where the station is: its configured position, or the day's median. More than 50 m off with a 3D fix and no jamming counts as spoofing." },
    ];
    const diag = (title, body) => `<div><div class="hc-diag-title">${title}</div>${body}</div>`;
    const details = `<div class="hc-diag">${day ? diag("Jamming", dl(jamDetails) + show) : ""}${diag("Position", dl(day ? spoofDetails : spoofDetails.slice(0, 2)))}</div>`;
    const age = verdict ? verdict.last_data_age_s : sky ? sky.age : null;
    head($("gps-head"), {
        state: verdict ? verdict.status : "learning", title: verdict ? gpsTitle(verdict) : "Insufficient receiver data",
        live: age === null ? "Unknown" : age < 30 ? "Live" : `${age < 120 ? age + " s" : Math.round(age / 60) + " min"} ago`,
        label: "GPS receiver health", figures, aside, checks, foot: checks && { ...summary(checks), more: "Details", details },
    });
}

function radius(pts) {
    if (pts.length < 5) return 0;
    const r = pts.map(([e, n]) => Math.hypot(e, n)).sort((a, b) => a - b);
    return r[Math.floor(r.length * 0.95)];
}

function centreOffset(pts) {
    if (!pts.length) return null;
    return Math.hypot(pts.reduce((s, p) => s + p[0], 0) / pts.length, pts.reduce((s, p) => s + p[1], 0) / pts.length);
}

// ---------- history: timeline and waterfall on one time axis ----------

function view() {
    const [span, places] = SPAN[range], d = span === "week" ? week : day;
    if (!d) return null;
    let from = Math.max(0, d.n - places);
    const first = d.reports.findIndex((r, i) => i >= from && r !== null);
    if (first > from) from = Math.min(first, d.n - Math.min(places, 15));
    return { d, from, n: d.n - from };
}

function lines(d) {
    const out = [];
    for (const id of ORDER)
        for (let b = 0; b < 3; b++) {
            const s = d.signal[id + "." + b];
            if (s) out.push({ id, b, s, label: SYSTEMS[id][0] + " " + BAND_NAME[id][b], color: cssvar(SYSTEMS[id][1]) });
        }
    return out;
}

// four tracks on one time axis, each with its label above it: signal, satellites used, fix, deviation
function timeline() {
    const v = view(), host = $("gps-timeline");
    if (!v) return;
    const { d, from, n } = v, narrow = host.clientWidth < 640, W = host.clientWidth;
    const L = 36, R = 10, pw = W - L - R, cw = pw / n, X = (i) => L + (pw * (i - from)) / n;
    const ln = narrow ? { sig: [22, 140], used: [190, 36], fix: [250, 8], off: [282, 48] } : { sig: [22, 165], used: [215, 40], fix: [281, 8], off: [315, 60] };
    const bottom = ln.off[0] + ln.off[1], H = bottom + 22;
    const ink = cssvar("--color-text-muted"), grid = cssvar("--gnss-grid"), amber = cssvar("--gnss-amber");
    geometry = { L, R, pw, cw, X, from, n, d, lanes: ln };
    let g = "";
    const label = (lane, text) => `<text x="${L}" y="${ln[lane][0] - 8}" fill="${ink}">${text}</text>`;
    // the events through every track: jams amber, gaps grey, the one chosen outlined
    for (const e of d === day ? events : []) {
        if (e.to < from || e.kind === "jump") continue;
        const x = X(Math.max(from, e.from)), w = Math.max(2, X(e.to + 1) - x);
        g += `<rect x="${x}" y="${ln.sig[0]}" width="${w}" height="${bottom - ln.sig[0]}" fill="${e.kind === "gap" ? ink : amber}" fill-opacity="${e.kind === "gap" ? 0.15 : 0.13}"/>`;
        if (e === selected) g += `<rect x="${x}" y="${ln.sig[0]}" width="${Math.max(3, w)}" height="${bottom - ln.sig[0]}" fill="none" stroke="${amber}" stroke-dasharray="3 3"/>`;
    }
    // hour lines and their times: every 3 hours, 6 on a phone, a day apart over the week, 30 min over 3 hours
    const step = d.interval === 600 ? 86400 : range === "1h" ? 900 : range === "3h" ? 1800 : narrow ? 21600 : 10800;
    for (let i = from; i < d.n; i++) {
        const t = d.time(i), local = t - new Date(t * 1000).getTimezoneOffset() * 60;
        if (local % step >= d.interval) continue;
        g += `<line x1="${X(i)}" x2="${X(i)}" y1="${ln.sig[0]}" y2="${bottom}" stroke="${grid}"/>`;
        if (X(i) < W - R - 40) g += `<text x="${X(i)}" y="${H - 6}" text-anchor="middle" fill="${ink}">${step === 86400 ? new Date(t * 1000).toLocaleDateString([], { weekday: "short" }) : clock(t)}</text>`;
    }
    g += `<text x="${W - R}" y="${H - 6}" text-anchor="end" fill="${ink}">now</text>`;
    const path = (vals, y) => {
        let p = "", pen = false;
        for (let i = from; i < d.n; i++) {
            const val = vals[i];
            if (val === null || val === undefined) { pen = false; continue; }
            p += (pen ? "L" : "M") + (X(i) + cw / 2).toFixed(1) + " " + y(val).toFixed(1);
            pen = true;
        }
        return p;
    };
    const ticks = (lane, y, values, unit = "") => values.map((val) => `<line x1="${L}" x2="${W - R}" y1="${y(val)}" y2="${y(val)}" stroke="${grid}"/><text x="${L - 6}" y="${y(val) + 3.5}" text-anchor="end" fill="${ink}">${val}${unit}</text>`).join("");
    // signal
    const [st, sh] = ln.sig, ys = (val) => st + sh - (sh * (Math.max(10, Math.min(50, val)) - 10)) / 40;
    g += label("sig", "Signal · dB-Hz") + ticks("sig", ys, [20, 30, 40, 50]);
    for (const l of lines(d)) if (!hiddenLines.has(l.label)) g += `<path d="${path(l.s, ys)}" fill="none" stroke="${l.color}" stroke-width="1.35" stroke-dasharray="${l.b ? "4 3" : ""}" stroke-linejoin="round"/>`;
    // satellites used
    const [ut, uh] = ln.used, top = Math.max(10, ...d.used.slice(from).filter((x) => x !== null)), yu = (val) => ut + uh - (uh * val) / top;
    let a = "", run = [];
    const flush = () => {
        if (run.length) a += `M${X(run[0])} ${yu(0)}` + run.map((i) => `L${X(i)} ${yu(d.used[i])}L${X(i + 1)} ${yu(d.used[i])}`).join("") + `L${X(run[run.length - 1] + 1)} ${yu(0)}Z`;
        run = [];
    };
    for (let i = from; i < d.n; i++) d.used[i] === null ? flush() : run.push(i);
    flush();
    g += label("used", "Satellites used") + ticks("used", yu, [0, top]) + `<path d="${a}" fill="${cssvar("--gnss-gps")}" fill-opacity=".45"/>`;
    // fix
    const [ft, fh] = ln.fix, fixColor = { 3: cssvar("--hc-ok-strong"), 2: amber, 1: cssvar("--color-danger") };
    g += label("fix", "Fix");
    for (let i = from; i < d.n; i++) if (fixColor[d.fix[i]]) g += `<rect x="${X(i)}" y="${ft}" width="${cw + 0.4}" height="${fh}" fill="${fixColor[d.fix[i]]}"/>`;
    // position deviation from the median
    const [ot, oh] = ln.off, offs = d.position.map((p) => (p ? Math.hypot(p[0], p[1]) : null));
    const peak = Math.max(10, ...offs.slice(from).filter((x) => x !== null)), lim = peak <= 10 ? 10 : peak <= 20 ? 20 : peak <= 50 ? 50 : Math.ceil(peak / 100) * 100;
    const yo = (val) => ot + oh - (oh * Math.min(val, lim)) / lim;
    g += label("off", "Position deviation · m") + ticks("off", yo, [0, lim / 2, lim]) + `<path d="${path(offs, yo)}" fill="none" stroke="${ink}" stroke-width="1.35"/>`;
    host.innerHTML = `<svg width="${W}" height="${H}" viewBox="0 0 ${W} ${H}">${g}</svg>`;
    legend(d);
}

// the legend: a signal line shown or hidden by a click; under it what the colours of the other tracks mean
function legend(d) {
    $("gps-tl-legend").innerHTML = lines(d).map((l) => `<button type="button" data-action="toggleGpsLine" data-line="${sanitizeString(l.label)}" aria-pressed="${!hiddenLines.has(l.label)}"><i class="gh-swatch${l.b ? " dash" : ""}" style="--c:${l.color}"></i>${l.label}</button>`).join("");
    $("gps-tl-key").innerHTML = `<span><i style="--c:var(--gnss-gps);opacity:.45"></i>Satellites used</span><span><i style="--c:var(--hc-ok-strong)"></i>3D fix</span><span><i style="--c:var(--gnss-amber)"></i>2D fix</span><span><i style="--c:var(--color-danger)"></i>No fix</span><span><i style="--c:var(--color-text-muted);opacity:.4"></i>No receiver data</span>`;
}

function rgb(color) {
    const m = color.match(/^#([0-9a-f]{6})$/i);
    if (m) return [0, 2, 4].map((k) => parseInt(m[1].slice(k, k + 2), 16));
    const r = color.match(/\d+(\.\d+)?/g);
    return r ? r.slice(0, 3).map(Number) : [128, 128, 128];
}

// one row a satellite under a heading per constellation, each folding away; at first only the satellites in
// view now. The time range and the chosen event are the history's.
function waterfall() {
    const v = view(), host = $("gps-wf-host");
    if (!v) return;
    const weekly = v.d !== day;
    host.hidden = weekly;
    $("gps-wf-week").hidden = !weekly;
    $("gps-wf-legend").hidden = weekly;
    $("gps-wf-count").hidden = weekly;
    $("gps-wf-sub").textContent = "Signal strength per satellite · " + range.replace("h", " h").replace("d", " d");
    if (weekly) return;
    const { from, n } = v, W = host.clientWidth, L = 40, R = 4, pw = W - L - R, X = (i) => L + (pw * (i - from)) / n, cw = pw / n;
    const heard = day.rows.filter((r) => r.band === band && r.v.slice(from).some((x) => x));
    const now = heard.filter((r) => r.v[day.n - 1] || r.v[day.n - 2]);
    const rows = allRows ? heard : now;
    const strong = rgb(cssvar("--wf-hi")), weak = rgb(cssvar("--wf-lo"));
    const shades = {};
    const shade = (val) => {
        const t = Math.max(0, Math.min(1, (val - 15) / 35));
        return shades[val] || (shades[val] = `rgb(${weak.map((c, k) => Math.round(c + (strong[k] - c) * t)).join(",")})`);
    };
    const mono = "ui-monospace, SFMono-Regular, Menlo, monospace";
    const step = n <= 30 ? 900 : n <= 90 ? 1800 : n <= 240 ? 7200 : 21600, letter = { 1: "G", 2: "R", 3: "E", 4: "C" };
    const hours = [];
    for (let i = from; i < day.n; i++) {
        const t = day.time(i), local = t - new Date(t * 1000).getTimezoneOffset() * 60;
        if (local % step < day.interval) hours.push(i);
    }
    const RH = 16, SH = 12, track = cssvar("--wf-track"), grid = cssvar("--gnss-grid"), amber = cssvar("--gnss-amber"), gap = cssvar("--wf-gap"), ink = cssvar("--color-text-muted");
    host.replaceChildren();
    for (const id of ORDER) {
        const group = rows.filter((r) => r.system === id).sort((a, b) => a.id - b.id), all = heard.filter((r) => r.system === id).length;
        if (!all) continue;
        const open = !folded.has(id);
        const head = document.createElement("button");
        head.type = "button";
        head.className = "wf-head";
        head.dataset.action = "toggleGpsGroup";
        head.dataset.system = id;
        head.setAttribute("aria-expanded", open);
        head.innerHTML = `<i style="background:var(${SYSTEMS[id][1]})"></i>${SYSTEMS[id][0]}<span>${group.length === all ? all : group.length + " of " + all} satellite${all === 1 ? "" : "s"}</span>`;
        host.append(head);
        if (!open || !group.length) continue;
        const canvas = document.createElement("canvas"), H = group.length * RH, dpr = window.devicePixelRatio || 1;
        canvas.width = Math.round(W * dpr);
        canvas.height = Math.round(H * dpr);
        canvas.style.width = W + "px";
        canvas.style.height = H + "px";
        const x = canvas.getContext("2d");
        x.setTransform(dpr, 0, 0, dpr, 0, 0);
        group.forEach((r, k) => {
            const y = k * RH + (RH - SH) / 2;
            x.fillStyle = track;
            x.fillRect(L, y, pw, SH);
            x.fillStyle = grid;
            for (const i of hours) x.fillRect(Math.round(X(i)), y, 1, SH);
            for (let i = from; i < day.n; i++) if (r.v[i]) {
                x.fillStyle = shade(r.v[i]);
                x.fillRect(X(i), y, cw + 0.5, SH);
            }
            x.fillStyle = ink;
            x.font = `11px ${mono}`;
            x.textBaseline = "middle";
            x.textAlign = "left";
            x.fillText(letter[id] + String(r.id).padStart(2, "0"), 0, y + SH / 2 + 0.5);
        });
        for (const e of events) {
            if (e.to < from || e.kind === "jump") continue;
            const ex = X(Math.max(from, e.from)), ew = Math.max(2, X(e.to + 1) - ex);
            x.fillStyle = e.kind === "gap" ? gap : amber;
            x.globalAlpha = e.kind === "gap" ? 0.6 : e === selected ? 0.15 : 0.07;
            x.fillRect(ex, 0, ew, H);
            x.globalAlpha = 1;
            if (e === selected) {
                x.fillStyle = amber;
                x.fillRect(Math.round(ex), 0, 1, H);
            }
        }
        host.append(canvas);
    }
    const axis = document.createElement("div");
    axis.className = "wf-axis";
    axis.style.paddingLeft = L + "px";
    axis.innerHTML = hours.filter((i) => X(i) > L + 20 && X(i) < W - R - 50).map((i) => `<span style="left:${X(i)}px">${clock(day.time(i))}</span>`).join("") + `<span class="now">now</span>`;
    host.append(axis);
    $("gps-wf-count").textContent = `Showing ${rows.length} of ${heard.length} satellites · ${["L1, E1, B1", "L2, E5b, B3", "L5, E5a, B2a"][band]}`;
    $("gps-wf-all").textContent = allRows ? "In view only" : "Show all";
    $("gps-wf-all").hidden = heard.length === now.length;
    const steps = Array.from({ length: 12 }, (_, k) => `<i style="background:${shade(15 + (35 * k) / 11)}"></i>`).join("");
    $("gps-wf-legend").innerHTML = `<span class="wf-scale">15 ${steps} 50 dB-Hz</span><span><i style="background:var(--wf-track)"></i>Not tracked</span><span><i style="background:var(--wf-gap)"></i>No receiver data</span>`;
    document.querySelectorAll("#gps [data-band]").forEach((b) => {
        b.classList.toggle("active", +b.dataset.band === band);
        b.hidden = !day.rows.some((r) => r.band === +b.dataset.band);
    });
}

function cross() {
    const tip = $("gps-tip"), line = $("gps-crossline");
    if (hover === null || !geometry) {
        tip.hidden = true;
        line.hidden = true;
        return;
    }
    const { d, X, cw } = geometry, i = hover, xpos = X(i) + cw / 2;
    line.hidden = false;
    line.style.left = xpos + "px";
    const rows = lines(d).map((l) => (l.s[i] ? `<div class="gps-tr"><i class="gps-line" style="border-color:${l.color};border-top-style:${l.b ? "dashed" : "solid"}"></i>${l.label}<em>${l.s[i]}</em></div>` : "")).join("");
    const p = d.position[i], ev = d === day ? events.find((e) => i >= e.from && i <= e.to) : null;
    tip.innerHTML = `<b>${dayClock(d.time(i))}</b>${ev ? ` · <span style="color:var(${KINDS[ev.kind][1]})">${KINDS[ev.kind][0].toLowerCase()}</span>` : ""}` +
        (d.reports[i] === null ? `<div class="gps-tr">No receiver data</div>` : rows +
            `<div class="gps-tr gps-tsep">Satellites used<em>${d.used[i] ?? "—"} of ${d.seen[i] ?? "—"}</em></div><div class="gps-tr">Fix<em>${FIX[d.fix[i]] || "—"}</em></div>` +
            `<div class="gps-tr">HDOP<em>${d.hdop[i] ? (d.hdop[i] / 10).toFixed(1) : "—"}</em></div><div class="gps-tr">Position deviation<em>${p ? Math.hypot(...p).toFixed(1) + " m" : "—"}</em></div>`);
    tip.hidden = false;
    const host = $("gps-hist-plot").getBoundingClientRect();
    tip.style.left = Math.max(0, Math.min(host.width - tip.offsetWidth - 4, xpos < host.width / 2 ? xpos + 14 : xpos - tip.offsetWidth - 14)) + "px";
    tip.style.top = (geometry.lanes.sig[0] + 8) + "px";
}

function onPoint(e) {
    if (!geometry) return;
    const box = $("gps-hist-plot").getBoundingClientRect(), px = e.clientX - box.left, { L, pw, from, n } = geometry;
    const i = Math.floor(((px - L) / pw) * n) + from;
    const next = px < L || px > L + pw || i < from || i >= from + n ? null : i;
    if (next === hover) return;
    hover = next;
    cross();
}

// ---------- now: signals ----------

function signals() {
    const host = $("gps-signals-plot");
    if (!sky || !sky.satellites.length) {
        host.innerHTML = `<p class="gps-none">No signal levels from this GPS.</p>`;
        return;
    }
    const W = host.clientWidth, x0 = 30, y0 = 8, ph = 200, H = ph + y0 + 44, y = (v) => y0 + ph - (ph * Math.min(v, 55)) / 55;
    const sats = ORDER.flatMap((id) => sky.satellites.filter((s) => s[SYSTEM] === id && (s[SIGNAL] > 0 || s[L2] > 0 || s[L5] > 0)).sort((a, b) => a[ID] - b[ID]));
    // too narrow for every band: L1 alone, the second bands in the tooltips of the wider screens
    const room = (list, gapSat, gapSys) => Math.floor((W - x0 - 12 - (sats.length - 1) * gapSat - (groups - 1) * gapSys - (list - sats.length)) / Math.max(1, list));
    const groups = new Set(sats.map((s) => s[SYSTEM])).size, all = sats.reduce((n, s) => n + [s[SIGNAL], s[L2], s[L5]].filter((v) => v > 0).length, 0);
    const wide = room(all, 5, 14) >= 4, gapSat = wide ? 5 : 2, gapSys = wide ? 14 : 8;
    const bands = (s) => [[s[SIGNAL], 0], [s[L2], 1], [s[L5], 2]].filter(([v], k) => v > 0 && (wide || k === 0 || s[SIGNAL] <= 0));
    const nb = sats.reduce((n, s) => n + bands(s).length, 0);
    const bw = Math.max(2, Math.min(22, room(nb, gapSat, gapSys)));
    const grid = cssvar("--color-border"), ink = cssvar("--color-text-muted");
    let g = `<rect x="${x0}" y="${y(25)}" width="${W - x0 - 4}" height="${y(0) - y(25)}" fill="${cssvar("--color-surface-alt")}"/>`;
    for (let v = 10; v <= 50; v += 10) g += `<line x1="${x0}" x2="${W - 4}" y1="${y(v)}" y2="${y(v)}" stroke="${grid}" stroke-opacity=".7"/><text x="${x0 - 5}" y="${y(v) + 3.5}" text-anchor="end" fill="${ink}">${v}</text>`;
    g += `<line x1="${x0}" x2="${W - 4}" y1="${y(0)}" y2="${y(0)}" stroke="${cssvar("--color-border-strong")}"/>`;
    let x = x0 + 6, prev = null, gx = x;
    const label = (id, from, to) => `<text x="${(from + to) / 2}" y="${y(0) + 36}" text-anchor="middle" fill="${cssvar("--color-text")}" font-weight="600">${wide ? SYSTEMS[id][0] : SYSTEMS[id][0].slice(0, 3)}</text>`;
    for (const s of sats) {
        if (prev !== null && prev !== s[SYSTEM]) {
            g += label(prev, gx, x - gapSat);
            x += gapSys;
            gx = x;
        }
        prev = s[SYSTEM];
        const c = cssvar(SYSTEMS[s[SYSTEM]][1]), sx = x;
        for (const [v, b] of bands(s)) {
            const tip = `${SYSTEMS[s[SYSTEM]][0]} ${s[ID]} ${BAND_NAME[s[SYSTEM]][b]}: ${v} dB-Hz${s[USED] ? "" : ", not used"}`;
            g += s[USED] ? `<rect x="${x}" y="${y(v)}" width="${bw}" height="${y(0) - y(v)}" rx="2" fill="${c}" fill-opacity="${b ? 0.55 : 1}"><title>${tip}</title></rect>`
                : `<rect x="${x + 0.75}" y="${y(v)}" width="${bw - 1.5}" height="${y(0) - y(v) - 0.75}" rx="2" fill="none" stroke="${c}" stroke-width="1.5" stroke-dasharray="${b ? "3 2" : ""}"><title>${tip}</title></rect>`;
            x += bw + 1;
        }
        x -= 1;
        if (bw >= 7) g += `<text x="${(sx + x) / 2}" y="${y(0) + 13}" text-anchor="middle" fill="${ink}" font-size="${bw >= 12 ? 10 : 9}">${s[ID]}</text>`;
        x += gapSat;
    }
    if (prev !== null) g += label(prev, gx, x - gapSat);
    host.innerHTML = `<svg width="${W}" height="${H}" viewBox="0 0 ${W} ${H}">${g}</svg>`;
}

// ---------- position ----------

// the fixes around their median: three figures, the plot in metres, the largest deviation called out
function position() {
    const v = view(), host = $("gps-position-plot");
    if (!v) return;
    const { d, from } = v, pts = [];
    $("gps-pos-sub").textContent = `Horizontal deviation · last ${range.replace("h", " h").replace("d", " days")}`;
    for (let i = from; i < d.n; i++) if (d.fixes[i]) pts.push({ e: d.fixes[i][0], n: d.fixes[i][1], i, ev: d === day && selected && i >= selected.from && i <= selected.to });
    if (pts.length < 2) {
        $("gps-pos-metrics").innerHTML = "";
        host.innerHTML = `<p class="gps-none">No positions in this period.</p>`;
        $("gps-pos-legend").innerHTML = $("gps-pos-event").innerHTML = "";
        return;
    }
    const r95 = radius(pts.map((p) => [p.e, p.n])), far = pts.reduce((m, p) => (Math.hypot(p.e, p.n) > Math.hypot(m.e, m.n) ? p : m));
    const reach = Math.hypot(far.e, far.n), jam = d === day ? events.find((e) => e.kind === "jam" && far.i >= e.from && far.i <= e.to) : null;
    $("gps-pos-metrics").innerHTML = figure("95% radius", `${r95.toFixed(1)}<small> m</small>`, "") + figure("Maximum", `${reach.toFixed(1)}<small> m</small>`, clock(d.time(far.i)), jam ? "event" : "") + figure("Samples", pts.length, `one per ${d.interval / 60} min`);

    const lim = reach <= 5 ? 5 : reach <= 10 ? 10 : reach <= 20 ? 20 : reach <= 50 ? 50 : Math.ceil(reach / 100) * 100;
    // room on the right for the last tick's number and the unit after it
    const W = Math.min(host.clientWidth, 480), PL = 30, PR = 26, PB = 24, S = W - PL - PR, c = PL + S / 2, k = S / 2 / lim, H = S + PB + 4;
    const px = (m) => c + m * k, py = (m) => 4 + S / 2 - m * k;
    const grid = cssvar("--gnss-grid"), axis = cssvar("--color-border-strong"), ink = cssvar("--color-text-muted"), amber = cssvar("--gnss-amber"), blue = cssvar("--gnss-gps");
    let g = `<rect x="${PL}" y="4" width="${S}" height="${S}" fill="${cssvar("--color-surface-alt")}" stroke="${grid}"/>`;
    for (const f of [-1, -0.5, 0, 0.5, 1]) {
        const m = f * lim;
        g += `<line x1="${px(m)}" x2="${px(m)}" y1="4" y2="${4 + S}" stroke="${f ? grid : axis}"/><line x1="${PL}" x2="${PL + S}" y1="${py(m)}" y2="${py(m)}" stroke="${f ? grid : axis}"/>`;
        g += `<text x="${px(m)}" y="${4 + S + 16}" text-anchor="middle" fill="${ink}">${m}</text><text x="${PL - 6}" y="${py(m) + 4}" text-anchor="end" fill="${ink}">${m}</text>`;
    }
    g += `<text x="${W - 2}" y="${4 + S + 16}" text-anchor="end" fill="${ink}">m</text>`;
    g += `<text x="${c + 8}" y="${py(lim) + 16}" fill="${ink}">N</text><text x="${PL + S - 8}" y="${py(0) - 8}" text-anchor="end" fill="${ink}">E</text>`;
    const strong = rgb(blue), surface = rgb(cssvar("--color-surface"));
    for (const p of pts) if (!p.ev) {
        const t = 0.3 + 0.7 * ((p.i - from) / Math.max(1, d.n - from));
        g += `<circle cx="${px(p.e)}" cy="${py(p.n)}" r="2.3" fill="rgb(${surface.map((x, q) => Math.round(x + (strong[q] - x) * t)).join(",")})"/>`;
    }
    const during = pts.filter((p) => p.ev);
    if (during.length) {
        g += `<path d="M${c} ${py(0)}${during.map((p) => `L${px(p.e).toFixed(1)} ${py(p.n).toFixed(1)}`).join("")}" fill="none" stroke="${amber}" stroke-width="1.5"/>`;
        for (const p of during) g += `<circle cx="${px(p.e)}" cy="${py(p.n)}" r="3.5" fill="${amber}" stroke="${cssvar("--color-surface")}"/>`;
    }
    const a = Math.PI / 6, rx = px(r95 * Math.cos(a)), ry = py(-r95 * Math.sin(a));
    g += `<circle cx="${c}" cy="${py(0)}" r="${r95 * k}" fill="none" stroke="${blue}" stroke-width="1.3" stroke-dasharray="4 3"/>`;
    g += `<line x1="${rx}" y1="${ry}" x2="${rx + 34}" y2="${ry + 18}" stroke="${blue}"/><text x="${rx + 37}" y="${ry + 22}" fill="${blue}">95%</text>`;
    g += `<path d="M${c - 8} ${py(0)}H${c + 8}M${c} ${py(0) - 8}V${py(0) + 8}" stroke="${cssvar("--color-text-strong")}" stroke-width="2"/>`;
    host.innerHTML = `<svg width="${W}" height="${H}" viewBox="0 0 ${W} ${H}">${g}</svg>`;
    $("gps-pos-legend").innerHTML = `<span><i style="--c:rgb(${surface.map((x, q) => Math.round(x + (strong[q] - x) * 0.35)).join(",")});border-radius:50%"></i><i style="--c:var(--gnss-gps);border-radius:50%"></i>Older → newer</span>${during.length ? `<span><i style="--c:var(--gnss-amber);border-radius:50%"></i>Event</span>` : ""}<span><b>+</b> Median</span>`;
    $("gps-pos-event").innerHTML = jam ? `<button type="button" class="pos-event" data-action="selectGpsEvent" data-event="${events.indexOf(jam)}"><svg class="hc-icon" viewBox="0 0 24 24" aria-hidden="true"><path d="M3 12a9 9 0 1 1 9 9"/><path d="M3 8v4h4"/><path d="M12 7v5l3 2"/></svg><span><b>${reach.toFixed(1)} m deviation during suspected jamming</b><small>${clock(d.time(far.i))} · ${jam === selected ? "selected event" : "show in the charts"}</small></span></button>` : "";
}

// ---------- events ----------

// the events, newest first: the chosen one open on its figures and evidence, the others a line each
const EVENT_ICON = {
    jam: '<path d="M12 3 2 20h20L12 3z"/><path d="M12 10v4"/><path d="M12 17h.01"/>',
    gap: '<path d="M2 8.5a15 15 0 0 1 20 0M5 12a10.5 10.5 0 0 1 14 0M8.5 15.5a5.5 5.5 0 0 1 7 0"/><path d="M12 19h.01"/><path d="M3 3l18 18"/>',
    jump: '<path d="M12 3 4 6v6c0 4.5 3.4 8.3 8 9 4.6-.7 8-4.5 8-9V6l-8-3z"/><path d="M12 8v5"/><path d="M12 16h.01"/>',
};
const CHART_ICON = '<path d="M3 20h18"/><path d="M6 16v-4M10 16V8M14 16v-6M18 16V5"/>';

function eventList() {
    const host = $("gps-events-body"), chips = $("gps-chips");
    if (!day) return;
    const list = [...events].reverse(), span = heldText();
    host.classList.toggle("gps-events-none", !list.length);
    // nothing to list: one line, and "no anomalies" only once an hour of history stands behind it
    if (!list.length)
        host.innerHTML = `<header>Events</header><div class="ev-line"><div class="ev-summary">Jamming, receiver gaps and position jumps</div><span class="ev-count">Last ${span}</span></div>
            <p class="ev-end">${held() >= 1 ? `<svg class="ev-icon" viewBox="0 0 24 24" aria-hidden="true"><circle cx="12" cy="12" r="9"/><path d="m8 12 3 3 5-6"/></svg>No anomalies detected` : `<svg class="ev-icon ev-wait" viewBox="0 0 24 24" aria-hidden="true"><circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/></svg>Not enough history yet to tell`}</p>`;
    else {
        const jams = list.filter((e) => e.kind === "jam").length, gaps = list.filter((e) => e.kind === "gap").length, jumps = list.filter((e) => e.kind === "jump").length;
        const ongoing = list.some((e) => e.to >= day.n - 2);
        const svg = (paths) => `<svg class="ev-icon" viewBox="0 0 24 24" aria-hidden="true">${paths}</svg>`;
        const item = (e) => {
            const k = events.indexOf(e), open = e === selected, t0 = day.time(e.from), t1 = day.time(e.to) + day.interval;
            const title = e.kind === "jam" ? "Suspected jamming" : e.kind === "gap" ? "No receiver data" : "Suspected spoofing";
            let body = "";
            if (open && e.kind === "jam")
                body = `<div class="ev-metrics"><div><span>L1 signal</span><b class="ev-warn">−${e.l1} dB</b></div><div><span>Satellites used</span><b>${e.used_before ?? "—"} → ${e.used_min}</b></div><div><span>Max. deviation</span><b>${e.off.toFixed(1)} m</b></div></div>
                    <p class="ev-evidence">${e.satellites} satellites affected at once · ${e.l5 === null ? "L5 not reported" : Math.abs(e.l5) <= 2 ? "L5 unchanged" : `L5 −${e.l5} dB`}${e.fix && e.fix < 3 ? " · fix fell to " + (e.fix === 2 ? "2D" : "none") : ""}</p>`;
            else if (open && e.kind === "gap") body = `<p class="ev-evidence">The GPS sent nothing for ${minutes(lasted(day, e))}: the receiver or its connection, not a loss of satellites.</p>`;
            else if (open) body = `<div class="ev-metrics"><div><span>Deviation</span><b class="ev-warn">${e.off.toFixed(0)} m</b></div></div><p class="ev-evidence">Far from the median position outside any jam: a sign of spoofing, or of a moved antenna.</p>`;
            if (open) body += `<p class="ev-charts">${svg(CHART_ICON)}Selected in charts</p>`;
            return `<div class="ev-item ev-${e.kind}${open ? " selected" : ""}"><button type="button" class="ev-toggle" data-action="selectGpsEvent" data-event="${k}" aria-expanded="${open}">
                    ${svg(EVENT_ICON[e.kind])}<span><span class="ev-title">${title}</span><span class="ev-time">${dayClock(t0)}–${clock(t1)} · ${minutes(lasted(day, e))}</span></span><i class="ev-chev"></i></button>${open ? `<div class="ev-body">${body}</div>` : ""}</div>`;
        };
        const summary = [jams ? `<span><i style="background:var(--gnss-amber)"></i>${jams} interference</span>` : "", gaps ? `<span><i style="background:var(--gnss-grey)"></i>${gaps} data gap${gaps === 1 ? "" : "s"}</span>` : "", jumps ? `<span><i style="background:var(--gnss-red)"></i>${jumps} suspected spoofing</span>` : ""].join("");
        host.innerHTML = `<header>Events</header><div class="ev-line"><div class="ev-summary">${summary}</div><span class="ev-count">Last ${span}</span></div>${list.map(item).join("")}
            <p class="ev-end">${ongoing ? `${svg(EVENT_ICON.jam)}An event is still going on` : `${svg('<circle cx="12" cy="12" r="9"/><path d="m8 12 3 3 5-6"/>')}All listed events have ended`}</p>`;
    }
    const shown = SPAN[range][0] === "day" ? day.n - SPAN[range][1] : 0;
    chips.innerHTML = events.filter((e) => e.kind !== "jump" && e.to >= shown).map((e) => {
        const k = events.indexOf(e);
        return `<button type="button" class="gh-event" data-action="selectGpsEvent" data-event="${k}" aria-pressed="${e === selected}"><i class="${e.kind === "gap" ? "gap" : ""}"></i>${clock(day.time(e.from))} ${e.kind === "jam" ? "Jamming" : "No data"}</button>`;
    }).join("");
}

// ---------- the tab ----------

function draw(history = true) {
    if (!root || root.hidden) return;
    // the health is told of any station; its sky and history only when its position may be shown
    const empty = !hidden && !sky && !day && !verdict, bare = hidden || (!sky && !day);
    $("gps-empty").hidden = !empty;
    if (empty)
        setup($("gps-empty"), {
            art: ART.gps, title: "Connect a GPS receiver", text: "GPS monitoring is not configured for this station.", action: "Add GPS input", docs: "advanced/GPS/",
            features: [["satellite", "Satellite signals"], ["target", "Position stability"], ["shield", "Interference detection"]],
            methods: [
                { name: "USB / Serial", command: "AIS-catcher -e 9600 /dev/ttyACM0", hint: "Adjust the serial device and baud rate to match your receiver." },
                { name: "gpsd", command: "AIS-catcher -t gpsd localhost 2947", hint: "Connects to a gpsd service that already reads the GPS." },
            ],
        });
    $("gps-private").hidden = !hidden;
    $("gps-body").hidden = empty || (hidden && !verdict);
    root.querySelector(".gps-grid").hidden = bare;
    if (empty || (hidden && !verdict)) return;
    status();
    if (bare) return;
    signals();
    if (!history) return;
    stale = false;
    document.querySelectorAll("#gps [data-gps-range]").forEach((b) => b.classList.toggle("active", b.dataset.gpsRange === range));
    eventList();
    timeline();
    waterfall();
    position();
    cross();
}

export function init() {
    root = $("gps");
    if (!root || root.dataset.ready) return;
    root.dataset.ready = "1";
    const plot = $("gps-hist-plot");
    plot.addEventListener("pointermove", onPoint);
    plot.addEventListener("pointerdown", onPoint);
    plot.addEventListener("pointerleave", () => {
        hover = null;
        cross();
    });
    if (window.ResizeObserver) {
        let width = 0;
        new ResizeObserver(() => {
            if (root.clientWidth !== width) {
                width = root.clientWidth;
                draw();
            }
        }).observe(root);
    }
}

export async function update(h, focus, gps) {
    init();
    // a tail is laid over the day's history in place: its end tells that it grew
    const told = JSON.stringify(gps || null), news = h !== last[0] || told !== shown, had = day, hadEnd = day && day.end, hadWeek = week;
    stale = stale || focus !== last[1];
    shown = told;
    last = [h, focus, gps];
    verdict = gps || null;
    hidden = !!(h && h.range_hidden);
    sky = h && h.gnss && h.gnss.satellites ? h.gnss : null;
    if (!hidden) await fetchGnss(focus);
    if (stale || day !== had || (day && day.end) !== hadEnd || week !== hadWeek) draw();
    else if (news) draw(false);
}

export function repaint() {
    draw();
}

export const actions = {
    setGpsRange: (e, d) => {
        range = d.gpsRange;
        hover = null;
        stale = true;
        update(...last);
    },
    toggleGpsLine: (e, d) => {
        hiddenLines.has(d.line) ? hiddenLines.delete(d.line) : hiddenLines.add(d.line);
        timeline();
        cross();
    },
    toggleGpsGroup: (e, d) => {
        const id = +d.system;
        folded.has(id) ? folded.delete(id) : folded.add(id);
        waterfall();
    },
    toggleGpsRows: () => {
        allRows = !allRows;
        waterfall();
    },
    setGpsBand: (e, d) => {
        band = +d.band;
        waterfall();
    },
    selectGpsEvent: (e, d) => {
        const ev = events[+d.event];
        chosen = true;
        selected = ev === selected ? null : ev;
        if (selected && SPAN[range][0] === "day" && selected.from < day.n - SPAN[range][1]) range = "24h";
        draw();
    },
};
