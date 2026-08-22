import { shipOutlineLocal } from './geo.js';
// Presentation helpers (distance/speed/coordinate/time/name/ship-type formatters).
// `getTableShiptype` stays in script.js — it touches the sprite registry.

import { settings } from './state.js';
import { CLASS_A, CLASS_B, BASESTATION, SAR, SARTEPIRB, ATON } from './constants.js';

export const getDistanceConversion = (c) =>
    settings.metric === "DEFAULT" ? c : settings.metric === "SI" ? c * 1.852 : c * 1.15078;
export const getDistanceVal = (c) => Number(getDistanceConversion(c)).toFixed(1);
export const getDistanceUnit = () =>
    settings.metric === "DEFAULT" ? "nmi" : settings.metric === "SI" ? "km" : "mi";

export const getSpeedConversion = (c) =>
    settings.metric === "DEFAULT" ? c : settings.metric === "SI" ? c * 1.852 : c * 1.151;
export const getSpeedVal = (c) =>
    settings.metric === "DEFAULT" ? Number(c).toFixed(1)
        : settings.metric === "SI" ? Number(c * 1.852).toFixed(1)
        : Number(c * 1.151).toFixed(1);
export const getSpeedUnit = () =>
    settings.metric === "DEFAULT" ? "kts" : settings.metric === "SI" ? "km/h" : "mph";

export const getDimVal = (c) =>
    settings.metric === "DEFAULT" || settings.metric === "SI"
        ? Number(c).toFixed(0)
        : Number(c * 3.2808399).toFixed(0);
export const getDimUnit = () =>
    settings.metric === "DEFAULT" || settings.metric === "SI" ? "m" : "ft";

export const getDraughtVal = (c) =>
    settings.metric === "DEFAULT" || settings.metric === "SI"
        ? Number(c).toFixed(1)
        : Number(c * 3.2808399).toFixed(1);

// one decimal while it says something (1.0K, 9.9K), none above (34K, 120K)
export function compactCount(n) {
    const v = Number(n) || 0;

    if (v < 1000) return String(v);
    if (v < 10000) return (v / 1000).toFixed(1) + "K";
    if (v < 1000000) return Math.round(v / 1000) + "K";
    return (v / 1000000).toFixed(1) + "M";
}

// Field ids shared with StaticHistory on the server.
export const CHANGE = { DRAUGHT: 1, STATUS: 2, SHIPNAME: 3, CALLSIGN: 4, DESTINATION: 5, ETA: 6 };
export const CHANGE_LABEL = { 1: "Draught", 2: "Status", 3: "Name", 4: "Callsign", 5: "Destination", 6: "ETA" };

export function getChangeVal(change) {
    if (change.f === CHANGE.STATUS) return getStatusVal({ status: change.to });
    if (change.f === CHANGE.DRAUGHT) return getDraughtVal(change.to / 10) + " " + getDimUnit();
    return String(change.to);
}

let chartSeq = 0;

function stepChartSVG(series, unit, footLeft, footRight, aria) {
    if (series.length < 2) return "";

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
  <text x="${L - 4}" y="${(+y(top) + 3).toFixed(1)}" class="dim-t" text-anchor="end">${vmax.toFixed(1)}</text>
  <text x="${L - 4}" y="${(+y(0) + 3).toFixed(1)}" class="dim-t" text-anchor="end">0</text>
  <polygon points="${area.join(" ")}" fill="url(#${grad})" stroke="none"/>
  <polyline points="${pts.join(" ")}" class="spd-line"/>
  <circle cx="${x(last.t)}" cy="${y(last.v)}" r="3" class="dim-origin"/>
  <text x="${L}" y="${H - 6}" class="dim-t">${footLeft}</text>
  <text x="${W - R}" y="${H - 6}" class="dim-t" text-anchor="end">${footRight}</text>
</svg>`;
}

export function getDraughtChartSVG(changes, currentDraught) {
    const rows = (changes || []).filter((c) => c.f === CHANGE.DRAUGHT && c.from != null)
        .slice().sort((a, b) => a.t - b.t);
    if (!rows.length) return "";
    const nChanges = rows.filter((r) => !r.i).length;

    const series = [{ t: rows[0].t - 1, v: rows[0].from / 10 }];
    for (const r of rows) series.push({ t: r.t, v: r.to / 10 });
    if (currentDraught != null) series.push({ t: Math.floor(Date.now() / 1000), v: currentDraught });

    const u = getDimUnit();
    const disp = (m) => Number(getDraughtVal(m));
    const scaled = series.map((p) => ({ t: p.t, v: disp(p.v) }));
    const last = scaled[scaled.length - 1];

    const secs = scaled[scaled.length - 1].t - scaled[0].t;
    const span = secs < 120 ? secs + " s" : secs < 7200 ? Math.round(secs / 60) + " min" : Math.round(secs / 3600) + " h";
    return stepChartSVG(scaled, u, span + " · " + nChanges + (nChanges === 1 ? " change" : " changes"),
        "now · " + last.v.toFixed(1) + " " + u,
        "Draught history, " + nChanges + " changes, now " + last.v.toFixed(1) + " " + u);
}

export function getChangeListHTML(changes, fields, labels) {
    const rows = (changes || []).filter((c) => fields.indexOf(c.f) !== -1)
        .slice().sort((a, b) => a.t - b.t);          // oldest first to trace values
    if (!rows.length) return "";

    // Numeric changes carry their own before/after; text changes carry only the
    // new value, so the previous one is whatever the same field last said.
    const text = (c, v) => (typeof v === "string" ? v : labels ? labels({ f: c.f, to: v }) : String(v));
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

export function getSpeedHistorySVG(pts, sogDiv = 10) {
    if (!Array.isArray(pts) || pts.length === 0) return "";

    // the path endpoint walks the track backwards, so normalise to oldest-first
    if (pts.length > 1 && pts[0][2] > pts[pts.length - 1][2]) pts = pts.slice().reverse();

    const unit = getSpeedUnit();
    const toDisplay = (raw) => Number(getSpeedVal(raw / sogDiv));

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

    return stepChartSVG(series, unit, span + " of history",
        "now · " + lastV.toFixed(1) + " " + unit,
        "Speed history over the last " + span + ", peaking at " + vmax.toFixed(1) + " " + unit);
}

export function getShipDimensionSVG(ship) {
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

    const u = getDimUnit();
    const lbl = (v) => getDimVal(v) + " " + u;

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
      <path d="M 0 0 L 10 5 L 0 10 z" fill="context-stroke"/>
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

export const getShipDimension = (ship) =>
    ship.to_bow != null && ship.to_stern != null && ship.to_port != null && ship.to_starboard != null
        ? getDimVal(ship.to_bow + ship.to_stern) + " \u00d7 " + getDimVal(ship.to_port + ship.to_starboard) + " " + getDimUnit()
        : null;

export function decimalToDMS(l, isLatitude) {
    const degrees = Math.floor(Math.abs(l));
    const minutes = Math.floor((Math.abs(l) - degrees) * 60);
    const seconds = Number(((Math.abs(l) - degrees) * 60 - minutes) * 60).toFixed(1);
    const direction = isLatitude ? (l > 0 ? "N" : "S") : l > 0 ? "E" : "W";
    return degrees + "&deg;" + minutes + "'" + seconds + '"' + direction;
}

export function decimalToDDM(l, isLatitude) {
    const degrees = Math.floor(Math.abs(l));
    const minutes = ((Math.abs(l) - degrees) * 60).toFixed(2);
    const direction = isLatitude ? (l > 0 ? "N" : "S") : l > 0 ? "E" : "W";
    return degrees + "&deg;" + minutes + "'" + direction;
}

function formatCoordinate(value, isLatitude) {
    switch (settings.coordinate_format) {
        case "dms":
            return decimalToDMS(value, isLatitude);
        case "ddm":
            return decimalToDDM(value, isLatitude);
        default:
            return Number(value).toFixed(5);
    }
}

export const getLatValFormat = (ship) => {
    const prefix = ship.approx ? "<i>" : "";
    const suffix = ship.approx ? "</i>" : "";
    return prefix + formatCoordinate(ship.lat, true) + suffix;
};

export const getLonValFormat = (ship) => {
    const prefix = ship.approx ? "<i>" : "";
    const suffix = ship.approx ? "</i>" : "";
    return prefix + formatCoordinate(ship.lon, false) + suffix;
};

const ETA_MONTHS = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];

// ETA in AIS type 5 is UTC; month 0, day 0, hour 24 and minute 60 mean "not available"
export const getEtaVal = (ship) => {
    const date = ship.eta_month >= 1 && ship.eta_month <= 12 && ship.eta_day >= 1 && ship.eta_day <= 31
        ? ship.eta_day + " " + ETA_MONTHS[ship.eta_month - 1]
        : "";
    const time = ship.eta_hour >= 0 && ship.eta_hour <= 23 && ship.eta_minute >= 0 && ship.eta_minute <= 59
        ? ("0" + ship.eta_hour).slice(-2) + ":" + ("0" + ship.eta_minute).slice(-2) + "Z"
        : "";
    return date && time ? date + " " + time : (date || time || "-");
};

// Unix seconds -> local wall-clock time, e.g. "14:03:27".
export const formatTime = (timestamp) =>
    new Date(timestamp * 1000).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });

export const getDeltaTimeVal = (s) => {
    // negative would fall through every branch below and return 
    if (s < 0) s = 0;

    const days = Math.floor(s / (24 * 3600));
    const hours = Math.floor((s % (24 * 3600)) / 3600);
    const minutes = Math.floor((s % 3600) / 60);
    const seconds = s % 60;

    let result = '';
    if (days > 0) result += `${days}d `;
    if (hours > 0 || days > 0) result += `${hours}h `;
    if (minutes > 0 || hours > 0 || days > 0) result += `${minutes}m `;
    if (seconds > 0 || (days === 0 && hours === 0 && minutes === 0)) result += `${seconds}s`;

    return result.trim();
};

// Override returning null/undefined falls through to the default field.
let shipNameOverride = null;
let callSignOverride = null;

export function setShipNameProvider(fn) { shipNameOverride = fn; }
export function setCallSignProvider(fn) { callSignOverride = fn; }

export const getShipName = (ship) => {
    if (shipNameOverride) {
        const r = shipNameOverride(ship);
        if (r != null) return r;
    }
    return ship.shipname;
};

export const getCallSign = (ship) => {
    if (callSignOverride) {
        const r = callSignOverride(ship);
        if (r != null) return r;
    }
    return ship.callsign;
};

let displayNames;

export function getCountryName(isoCode) {
    if (!displayNames && typeof Intl !== "undefined" && typeof Intl.DisplayNames === "function") {
        displayNames = new Intl.DisplayNames(["en"], { type: "region" });
    }
    if (displayNames) {
        try {
            const countryName = displayNames.of(isoCode);
            return countryName ? countryName : "";
        } catch (error) {
            return isoCode;
        }
    } else {
        return isoCode;
    }
}

export function getFlag(country) {
    return country
        ? `<span style="padding-right: 10px" title="` + getCountryName(country) + `" class="fi fi-${country.toLowerCase()}"></span> `
        : "<span></span>";
}

export const TOOLTIP_FLAG_STYLE =
    "padding: 0px; margin: 0px; margin-right: 6px; box-shadow: 1px 1px 2px rgba(0, 0, 0, 0.2); font-size: 25px; opacity: 70%";

export const CARD_FLAG_STYLE = "padding: 0px; margin: 0px; margin-right: 5px; box-shadow: 2px 2px 3px rgba(0, 0, 0, 0.5); font-size: 26px;";

export function getFlagStyled(country, style) {
    return country
        ? `<span style="` + style + `" title="` + getCountryName(country) + `" class="fi fi-${country.toLowerCase()}"></span> `
        : "<span></span>";
}

const STATUS_STRINGS = [
    "Under way using engine",
    "At anchor",
    "Not under command",
    "Restricted maneuverability",
    "Constrained",
    "Moored",
    "Aground",
    "Engaged in Fishing",
    "Under way sailing",
    "Reserved for HSC",
    "Reserved for WIG",
    "Towing astern",
    "Pushing ahead / towing alongside",
    "Reserved",
    "AIS-SART is active",
    "Undefined",
];

export function getStatusVal(ship) {
    // 15 is "undefined" in the standard, so a missing or out-of-range status
    // lands there instead of rendering as undefined.
    const s = ship.status;
    return STATUS_STRINGS[Number.isInteger(s) && s >= 0 && s <= 15 ? s : 15];
}

export function getMmsiTypeVal(ship) {
    switch (ship.mmsi_type) {
        case CLASS_A:
            return "Class A";
        case CLASS_B: {
            let classB = "Class B";
            if (ship.cs_unit === 1) classB += " (SO)"; // SOTDMA - 5 watts
            else if (ship.cs_unit === 2) classB += " (CS)"; // CSTDMA - 2 watts
            return classB;
        }
        case BASESTATION:
            return "Base Station";
        case SAR:
            return "SAR aircraft";
        case SARTEPIRB:
            return "AIS SART/EPIRB";
        case ATON:
            return "AtoN";
    }
    return "Unknown";
}

export function getStringfromMsgType(m) {
    let s = "";
    let delim = "";
    for (let i = 1; i <= 28; i++) {
        if ((m & (1 << i)) != 0) {
            s += delim + Number(i).toFixed(0);
            delim = ", ";
        }
    }
    return s;
}

export function getStringfromGroup(m) {
    let s = "";
    let delim = "";
    let count = 0;
    for (let i = 0; i < 32; i++) {
        const mask = 1 << i;
        if ((m & mask) !== 0) {
            if (count == 4) {
                s += delim + "...";
                break;
            }
            s += delim + Number(i + 1).toFixed(0);
            delim = ", ";
            count++;
        }
    }
    return s;
}

export function getStringfromChannels(m) {
    let s = "";
    let delim = "";
    for (let i = 0; i <= 3; i++) {
        if ((m & (1 << i)) != 0) {
            s += delim + String.fromCharCode(65 + i);
            delim = ", ";
        }
    }
    return s;
}

// Mirror of LookupTable_ship_types in Source/JSON/Keys.cpp — keep in sync.
const SHIP_TYPE_TEXT = [
    "Not available",
    "Science / Research vessel",
    "Training vessel",
    "Ship owned or operated by a government",
    "Ice breaker",
    "Buoy (Aids to Navigation) tender",
    "Cable layer",
    "Pipe layer",
    "Reserved",
    "Special purpose ship, no additional information",
    "Reserved",
    "FPSO (Floating, Production, Storage, Offloading) vessel",
    "Fish factory ship",
    "Fish farm support vessel",
    "Offshore support vessel",
    "Reserved",
    "Reserved",
    "Construction vessel",
    "Crew boat",
    "Support vessel, no additional information",
    "Wing in ground (WIG) - all ships of this type",
    "Wing in ground (WIG) - Hazardous category X",
    "Wing in ground (WIG) - Hazardous category Y",
    "Wing in ground (WIG) - Hazardous category Z",
    "Wing in ground (WIG) - Hazardous category OS",
    "Wing in ground (WIG) - Reserved",
    "Wing in ground (WIG) - Reserved",
    "Wing in ground (WIG) - Reserved",
    "Wing in ground (WIG) - Reserved",
    "Wing in ground (WIG) - No additional information",
    "Fishing",
    "Towing",
    "Towing: length exceeds 200m or breadth exceeds 25m",
    "Dredging or underwater ops",
    "Diving ops",
    "Military ops",
    "Sailing",
    "Pleasure Craft",
    "Trawler",
    "Patrol vessel",
    "High speed craft (HSC) - all ships of this type",
    "High speed craft (HSC) - Hazardous category X",
    "High speed craft (HSC) - Hazardous category Y",
    "High speed craft (HSC) - Hazardous category Z",
    "High speed craft (HSC) - Hazardous category OS",
    "High speed craft (HSC) - Reserved",
    "High speed craft (HSC) - Reserved",
    "High speed craft (HSC) - Reserved",
    "High speed craft (HSC) - Reserved",
    "High speed craft (HSC) - No additional information",
    "Pilot Vessel",
    "Search and Rescue vessel",
    "Tug",
    "Port Tender",
    "Anti-pollution equipment",
    "Law Enforcement",
    "Spare - Local Vessel",
    "Spare - Local Vessel",
    "Medical Transport",
    "Ships of States not parties to an armed conflict",
    "Passenger ships - all ships of this type",
    "Passenger ships - Hazardous category X",
    "Passenger ships - Hazardous category Y",
    "Passenger ships - Hazardous category Z",
    "Passenger ships - Hazardous category OS",
    "Passenger (cruise) ship",
    "Passenger (ferry) ship",
    "Passenger (excursion) ship",
    "Reserved",
    "Passenger ships - No additional information",
    "Cargo ships - all ships of this type",
    "Cargo ships - Hazardous category X",
    "Cargo ships - Hazardous category Y",
    "Cargo ships - Hazardous category Z",
    "Cargo ships - Hazardous category OS",
    "Cargo ship, bulk carrier",
    "Cargo ship, container ship",
    "Cargo ship, roll-on-roll-off carrier",
    "Cargo ship, landing craft",
    "Cargo ships - No additional information",
    "Tanker(s) - all ships of this type",
    "Tanker(s) - Hazardous category X",
    "Tanker(s) - Hazardous category Y",
    "Tanker(s) - Hazardous category Z",
    "Tanker(s) - Hazardous category OS",
    "Tanker(s) - non-hazardous or non-pollutant carrier",
    "Integrated / articulated tug and tank barge",
    "Tanker(s) - Reserved",
    "Tanker(s) - Reserved",
    "Tanker(s) - No additional information",
    "Other Type - all ships of this type",
    "Other Type - Hazardous category X",
    "Other Type - Hazardous category Y",
    "Other Type - Hazardous category Z",
    "Other Type - Hazardous category OS",
    "Other Type - Reserved",
    "Other Type - Reserved",
    "Other Type - Reserved",
    "Other Type - Reserved",
    "Other Type - no additional information",
];

const SHIP_TYPE_SHORT = {
    1: "Research", 2: "Training", 3: "Government", 4: "Ice breaker",
    5: "Buoy tender", 6: "Cable layer", 7: "Pipe layer", 9: "Special purpose",
    11: "FPSO", 12: "Fish factory", 13: "Fish farm support", 14: "Offshore support",
    17: "Construction", 18: "Crew boat", 19: "Support vessel",
    30: "Fishing", 31: "Towing", 32: "Towing (large)",
    33: "Dredging", 34: "Diving ops", 35: "Military", 36: "Sailing",
    37: "Pleasure Craft", 38: "Trawler", 39: "Patrol vessel",
    50: "Pilot", 51: "Search And Rescue", 52: "Tug", 53: "Port tender",
    54: "Anti-pollution equipment", 55: "Law Enforcement",
    56: "Local Vessel", 57: "Local Vessel", 58: "Medical Transport",
    59: "Noncombatant ship",
    65: "Passenger (cruise)", 66: "Passenger (ferry)", 67: "Passenger (excursion)",
    75: "Cargo (bulk)", 76: "Cargo (container)", 77: "Cargo (ro-ro)", 78: "Cargo (landing craft)",
    85: "Tanker", 86: "Tug & tank barge",
};

const SHIP_TYPE_INLAND_SHORT = {
    8000: "Unknown", 8010: "Motor Freighter", 8020: "Motor Tanker",
    8021: "Motor Tanker", 8022: "Motor Tanker", 8023: "Motor Tanker",
    8030: "Container", 8040: "Gas Tanker", 8050: "Motor Freighter",
    8060: "Motor Tanker", 8070: "Motor Freighter", 8080: "Motor Freighter",
    8090: "Motor Freighter", 8100: "Motor Freighter", 8110: "Tug, Freighter",
    8120: "Tug, Tanker", 8130: "Tug Freighter", 8140: "Tug, freighter/tanker",
    8150: "Freightbarge", 8160: "Tankbarge", 8161: "Tankbarge",
    8162: "Tankbarge", 8163: "Tankbarge", 8170: "Freightbarge", 8180: "Tankbarge",
    8210: "Pushtow", 8220: "Pushtow", 8230: "Pushtow", 8240: "Pushtow",
    8250: "Pushtow", 8260: "Pushtow", 8270: "Pushtow", 8280: "Pushtow", 8290: "Pushtow",
    8310: "Pushtow", 8320: "Pushtow", 8330: "Pushtow", 8340: "Pushtow",
    8350: "Pushtow", 8360: "Pushtow", 8370: "Pushtow", 8380: "Pushtow", 8390: "Pushtow",
    8400: "Tug", 8410: "Tug", 8420: "Tug", 8430: "Pushboat",
    8440: "Passenger", 8441: "Ferry", 8442: "Red Cross", 8443: "Cruise", 8444: "Passenger",
    8445: "Day-trip", 8446: "Day-trip", 8447: "Sailing cruise", 8448: "Sailing passenger",
    8450: "Service/Port", 8451: "Service", 8452: "Police", 8453: "Port service",
    8454: "Surveillance", 8460: "Maintenance Craft", 8470: "Object",
    8480: "Fishing", 8490: "Bunkervessel", 8500: "Barge, Tanker, Chemical", 8510: "Object",
    1500: "General", 1510: "Unit Carrier", 1520: "Bulk Carrier", 1530: "Tanker",
    1540: "Liquefied Gas Tanker", 1850: "Pleasure craft", 1900: "Fast Ship",
    1910: "Hydrofoil", 1920: "Catamaran Fast",
};

// Full ERI descriptions (ES-RIS25, Annex 10 ERI ship type table), keyed by code.
const SHIP_TYPE_INLAND_TEXT = {
    8000: "Vessel, type unknown",
    8010: "Motor freighter",
    8020: "Motor tanker",
    8021: "Motor tanker, liquid cargo, type N",
    8022: "Motor tanker, liquid cargo, type C",
    8023: "Motor tanker, dry cargo as if liquid (e.g. cement)",
    8030: "Container vessel",
    8040: "Gas tanker",
    8050: "Motor freighter, tug",
    8060: "Motor tanker, tug",
    8070: "Motor freighter with one or more vessels alongside",
    8080: "Motor freighter with tanker",
    8090: "Motor freighter pushing one or more freighters",
    8100: "Motor freighter pushing at least one tank-vessel",
    8110: "Tug, freighter",
    8120: "Tug, tanker",
    8130: "Tug, freighter, coupled",
    8140: "Tug, freighter/tanker, coupled",
    8150: "Freightbarge",
    8160: "Tankbarge",
    8161: "Tankbarge, liquid cargo, type N",
    8162: "Tankbarge, liquid cargo, type C",
    8163: "Tankbarge, dry cargo as if liquid (e.g. cement)",
    8170: "Freightbarge with containers",
    8180: "Tankbarge, gas",
    8210: "Pushtow, one cargo barge",
    8220: "Pushtow, two cargo barges",
    8230: "Pushtow, three cargo barges",
    8240: "Pushtow, four cargo barges",
    8250: "Pushtow, five cargo barges",
    8260: "Pushtow, six cargo barges",
    8270: "Pushtow, seven cargo barges",
    8280: "Pushtow, eight cargo barges",
    8290: "Pushtow, nine or more barges",
    8310: "Pushtow, one tank/gas barge",
    8320: "Pushtow, two barges at least one tanker or gas barge",
    8330: "Pushtow, three barges at least one tanker or gas barge",
    8340: "Pushtow, four barges at least one tanker or gas barge",
    8350: "Pushtow, five barges at least one tanker or gas barge",
    8360: "Pushtow, six barges at least one tanker or gas barge",
    8370: "Pushtow, seven barges at least one tanker or gas barge",
    8380: "Pushtow, eight barges at least one tanker or gas barge",
    8390: "Pushtow, nine or more barges at least one tanker or gas barge",
    8400: "Tug, single",
    8410: "Tug, one or more tows",
    8420: "Tug, assisting a vessel or linked combination",
    8430: "Pushboat, single",
    8440: "Passenger vessel, ferry, red cross vessel, cruise vessel",
    8441: "Ferry",
    8442: "Red cross vessel",
    8443: "Cruise vessel",
    8444: "Passenger vessel without accommodation",
    8445: "Day-trip high speed vessel",
    8446: "Day-trip hydrofoil vessel",
    8447: "Sailing cruise vessel",
    8448: "Sailing passenger vessel without accommodation",
    8450: "Service vessel, police patrol, port service",
    8451: "Service vessel",
    8452: "Police patrol vessel",
    8453: "Port service vessel",
    8454: "Navigation surveillance vessel",
    8460: "Vessel, work maintenance craft, floating derrick, cable-ship, buoy-ship, dredge",
    8470: "Object, towed, not otherwise specified",
    8480: "Fishing boat",
    8490: "Bunkervessel",
    8500: "Barge, tanker, chemical",
    8510: "Object, not otherwise specified",
    1500: "General cargo vessel maritime",
    1510: "Unit carrier maritime",
    1520: "Bulk carrier maritime",
    1530: "Tanker",
    1540: "Liquefied gas tanker",
    1850: "Pleasure craft, longer than 20 metres",
    1900: "Fast vessel",
    1910: "Hydrofoil",
    1920: "Catamaran fast",
};

function maritimeShort(s) {
    if (s < 20 && SHIP_TYPE_SHORT[s]) return SHIP_TYPE_SHORT[s];
    if (s < 20) return s == 8 || s == 10 || s == 15 || s == 16 ? "Reserved" : "Not available";
    if (s <= 29) return "WIG";
    if (SHIP_TYPE_SHORT[s]) return SHIP_TYPE_SHORT[s];
    if (s <= 39) return "Reserved";
    if (s <= 49) return "High Speed Craft";
    if (s <= 69) return "Passenger";
    if (s <= 79) return "Cargo";
    if (s <= 89) return "Tanker";
    if (s <= 99) return "Other";
    return "Unknown (" + s + ")";
}

export function getShipTypeShort(s) {
    return SHIP_TYPE_INLAND_SHORT[s] || maritimeShort(s);
}

export function getShipTypeFull(s) {
    if (s >= 0 && s < SHIP_TYPE_TEXT.length) return SHIP_TYPE_TEXT[s];
    return SHIP_TYPE_INLAND_TEXT[s] || "Unknown (" + s + ")";
}

// Only http(s) is safe to put in an href; javascript:/data: URLs are not.
export function isHttpUrl(u) {
    return typeof u === 'string' && /^https?:\/\//i.test(u);
}

export function sanitizeString(input) {
    const map = {
        '&': '&amp;',
        '<': '&lt;',
        '>': '&gt;',
        '"': '&quot;',
        "'": '&#039;',
        '`': '&#96;'
    };
    return input.replace(/[&<>"'`]/g, (match) => map[match]);
}

export function formatBytes(b) {
    if (b >= 1e9) return (b / 1e9).toFixed(1) + " GB";
    if (b >= 1e6) return (b / 1e6).toFixed(1) + " MB";
    if (b >= 1e3) return (b / 1e3).toFixed(1) + " KB";
    return b + " B";
}
