import { settings } from './settings.js';

// ── Ship classification ────────────────────────────────────────────────────────

export const ShippingClass = {
    OTHER: 0, UNKNOWN: 1, CARGO: 2, B: 3, PASSENGER: 4, SPECIAL: 5,
    TANKER: 6, HIGHSPEED: 7, FISHING: 8, PLANE: 9, HELICOPTER: 10,
    STATION: 11, ATON: 12, SARTEPIRB: 13,
};

export const shippingMappings = {
    [ShippingClass.OTHER]:      { cx: 120, cy: 20, hint: 'Other',        imgSize: 20 },
    [ShippingClass.UNKNOWN]:    { cx: 120, cy: 20, hint: 'Unknown',      imgSize: 20 },
    [ShippingClass.CARGO]:      { cx:   0, cy: 20, hint: 'Cargo',        imgSize: 20 },
    [ShippingClass.TANKER]:     { cx:  80, cy: 20, hint: 'Tanker',       imgSize: 20 },
    [ShippingClass.PASSENGER]:  { cx:  40, cy: 20, hint: 'Passenger',    imgSize: 20 },
    [ShippingClass.HIGHSPEED]:  { cx: 100, cy: 20, hint: 'High Speed',   imgSize: 20 },
    [ShippingClass.SPECIAL]:    { cx:  60, cy: 20, hint: 'Special',      imgSize: 20 },
    [ShippingClass.FISHING]:    { cx: 140, cy: 20, hint: 'Fishing',      imgSize: 20 },
    [ShippingClass.ATON]:       { cx:   0, cy: 40, hint: 'AtoN',         imgSize: 20 },
    [ShippingClass.PLANE]:      { cx:   0, cy: 60, hint: 'Aircraft',     imgSize: 25 },
    [ShippingClass.HELICOPTER]: { cx:   0, cy: 85, hint: 'Helicopter',   imgSize: 25 },
    [ShippingClass.B]:          { cx:  20, cy: 20, hint: 'Class B',      imgSize: 20 },
    [ShippingClass.STATION]:    { cx:  20, cy: 40, hint: 'Base Station', imgSize: 20 },
    [ShippingClass.SARTEPIRB]:  { cx:  40, cy: 40, hint: 'SART/EPIRB',  imgSize: 20 },
};

// ── Coordinate conversion ─────────────────────────────────────────────────────

export function decimalToDMS(l, isLatitude) {
    const degrees   = Math.floor(Math.abs(l));
    const minutes   = Math.floor((Math.abs(l) - degrees) * 60);
    const seconds   = Number(((Math.abs(l) - degrees) * 60 - minutes) * 60).toFixed(1);
    const direction = isLatitude ? (l > 0 ? "N" : "S") : l > 0 ? "E" : "W";
    return degrees + "&deg" + minutes + "'" + seconds + '"' + direction;
}

export function decimalToDDM(l, isLatitude) {
    const degrees   = Math.floor(Math.abs(l));
    const minutes   = ((Math.abs(l) - degrees) * 60).toFixed(2);
    const direction = isLatitude ? (l > 0 ? "N" : "S") : l > 0 ? "E" : "W";
    return degrees + "&deg;" + minutes + "'" + direction;
}

// ── Units ─────────────────────────────────────────────────────────────────────

export const getDimVal = (c) =>
    settings.metric === "DEFAULT" || settings.metric === "SI"
        ? Number(c).toFixed(0)
        : Number(c * 3.2808399).toFixed(0);

export const getDimUnit = () =>
    settings.metric === "DEFAULT" || settings.metric === "SI" ? "m" : "ft";

export const getDistanceConversion = (c) =>
    settings.metric === "DEFAULT" ? c
    : settings.metric === "SI"    ? c * 1.852
    : c * 1.15078;

export const getDistanceVal  = (c) => Number(getDistanceConversion(c)).toFixed(1).toLocaleString();
export const getDistanceUnit = ()  => settings.metric === "DEFAULT" ? "nmi" : settings.metric === "SI" ? "km" : "mi";

export const getSpeedVal  = (c) =>
    settings.metric === "DEFAULT" ? Number(c).toFixed(1)
    : settings.metric === "SI"    ? Number(c * 1.852).toFixed(1)
    : Number(c * 1.151).toFixed(1);

export const getSpeedUnit = () =>
    settings.metric === "DEFAULT" ? "kts" : settings.metric === "SI" ? "km/h" : "mph";

export const getShipDimension = (ship) =>
    ship.to_bow != null && ship.to_stern != null && ship.to_port != null && ship.to_starboard != null
        ? getDimVal(ship.to_bow + ship.to_stern) + " " + getDimUnit() + " x " +
          getDimVal(ship.to_port + ship.to_starboard) + " " + getDimUnit()
        : null;

// ── Coordinate formatting ─────────────────────────────────────────────────────

export const getLatValFormat = (ship) => {
    const prefix = ship.approx ? "<i>" : "";
    const suffix = ship.approx ? "</i>" : "";
    let content;
    switch (settings.coordinate_format) {
        case "dms": content = decimalToDMS(ship.lat, true);  break;
        case "ddm": content = decimalToDDM(ship.lat, true);  break;
        default:    content = Number(ship.lat).toFixed(5);    break;
    }
    return prefix + content + suffix;
};

export const getLonValFormat = (ship) => {
    const prefix = ship.approx ? "<i>" : "";
    const suffix = ship.approx ? "</i>" : "";
    let content;
    switch (settings.coordinate_format) {
        case "dms": content = decimalToDMS(ship.lon, false); break;
        case "ddm": content = decimalToDDM(ship.lon, false); break;
        default:    content = Number(ship.lon).toFixed(5);   break;
    }
    return prefix + content + suffix;
};

// ── Time ──────────────────────────────────────────────────────────────────────

export const getDeltaTimeVal = (s) => {
    const days    = Math.floor(s / (24 * 3600));
    const hours   = Math.floor((s % (24 * 3600)) / 3600);
    const minutes = Math.floor((s % 3600) / 60);
    const seconds = s % 60;
    let result = '';
    if (days    > 0)                              result += `${days}d `;
    if (hours   > 0 || days > 0)                  result += `${hours}h `;
    if (minutes > 0 || hours > 0 || days > 0)     result += `${minutes}m `;
    if (seconds > 0 || (days === 0 && hours === 0 && minutes === 0)) result += `${seconds}s`;
    return result.trim();
};

// ── Ship / plane fields ───────────────────────────────────────────────────────

export const getEtaVal   = (ship) =>
    ("0" + ship.eta_month).slice(-2) + "-" + ("0" + ship.eta_day).slice(-2) + " " +
    ("0" + ship.eta_hour).slice(-2)  + ":" + ("0" + ship.eta_minute).slice(-2);

export const getShipName = (ship)  => ship.shipname;
export const getCallSign = (ship)  => ship.callsign;

export const getICAOfromHexIdent = (h) => h.toString(16).toUpperCase().padStart(6, '0');
export const getICAO             = (plane) => getICAOfromHexIdent(plane.hexident);

export const includeShip = (_ship) => true;

export function sanitizeString(input) {
    const map = { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#039;', '`': '&#96;' };
    return input.replace(/[&<>"'`]/g, m => map[m]);
}

// ── Country / flag ────────────────────────────────────────────────────────────

let displayNames = null;

export function getCountryName(isoCode) {
    if (!displayNames && typeof Intl !== "undefined" && typeof Intl.DisplayNames === "function") {
        displayNames = new Intl.DisplayNames(["en"], { type: "region" });
    }
    if (displayNames) {
        try { return displayNames.of(isoCode) || ""; } catch { return isoCode; }
    }
    return isoCode;
}

export const getFlag = (country) =>
    country ? `<span style="padding-right: 10px" title="${getCountryName(country)}" class="fi fi-${country.toLowerCase()}"></span> `
            : "<span></span>";

export const getFlagStyled = (country, style) =>
    country ? `<span style="${style}" title="${getCountryName(country)}" class="fi fi-${country.toLowerCase()}"></span> `
            : "<span></span>";

// ── MMSI types ────────────────────────────────────────────────────────────────

export const MMSI_OTHER = 0, CLASS_A = 1, CLASS_B = 2, BASESTATION = 3, SAR = 4, SARTEPIRB_TYPE = 5, ATON_TYPE = 6;

// ── Ship status / type labels ─────────────────────────────────────────────────

const STATUS_STRINGS = [
    "Under way using engine", "At anchor", "Not under command", "Restricted manoeuverability",
    "Constrained", "Moored", "Aground", "Engaged in Fishing", "Under way sailing",
    "Reserved for HSC", "Reserved for WIG", "Reserved", "Reserved", "Reserved",
    "AIS-SART is active", "Not available",
];
export const getStatusVal = (ship) => STATUS_STRINGS[Math.min(ship.status, 15)];

export function getTypeVal(ship) {
    switch (ship.mmsi_type) {
        case CLASS_A:    return "Class A";
        case CLASS_B: {
            let s = "Class B";
            if (ship.cs_unit === 1) s += " (SO)";
            else if (ship.cs_unit === 2) s += " (CS)";
            return s;
        }
        case BASESTATION:   return "Base Station";
        case SAR:           return "SAR aircraft";
        case SARTEPIRB_TYPE: return "AIS SART/EPIRB";
        case ATON_TYPE:     return "AtoN";
        default:            return "Unknown";
    }
}

export function getShipTypeVal(s) {
    if (s < 20)  return "Not available";
    if (s <= 29) return "WIG";
    if (s <= 30) return "Fishing";
    if (s <= 32) return "Towing";
    if (s <= 34) return "Dredging/Diving ops";
    if (s <= 35) return "Military";
    if (s <= 36) return "Sailing";
    if (s <= 37) return "Pleasure Craft";
    if (s <= 39) return "Reserved";
    if (s <= 49) return "High Speed Craft";
    if (s <= 50) return "Pilot";
    if (s <= 51) return "Search And Rescue";
    if (s <= 52) return "Tug";
    if (s <= 53) return "Port tender";
    if (s <= 54) return "Anti-pollution equipment";
    if (s <= 55) return "Law Enforcement";
    if (s <= 57) return "Local Vessel";
    if (s <= 58) return "Medical Transport";
    if (s <= 59) return "Noncombatant ship";
    if (s <= 69) return "Passenger";
    if (s <= 79) return "Cargo";
    if (s <= 89) return "Tanker";
    if (s <= 99) return "Other";
    const inlandMap = {
        8000: "Unknown (inland AIS)", 8010: "Motor Freighter", 8020: "Motor Tanker",
        8021: "Motor Tanker (liquid)", 8022: "Motor Tanker (liquid)", 8023: "Motor Tanker (dry)",
        8030: "Container", 8040: "Gas Tanker", 8050: "Motor Freighter (tug)",
        8060: "Motor Tanker (tug)", 8070: "Motor Freighter (alongside)",
        8080: "Motor Freighter (with tanker)", 8090: "Motor Freighter (pushing)",
        8100: "Motor Freighter (pushing)", 8110: "Tug, Freighter", 8120: "Tug, Tanker",
        8130: "Tug Freighter (coupled)", 8140: "Tug, freighter/tanker",
        8150: "Freightbarge", 8160: "Tankbarge", 8161: "Tankbarge (liquid)",
        8162: "Tankbarge (liquid)", 8163: "Tankbarge (dry)", 8170: "Freightbarge (with containers)",
        8180: "Tankbarge (gas)", 8210: "Pushtow (one cargo barge)", 8220: "Pushtow (two cargo barges)",
        8230: "Pushtow, (three cargo barges)", 8240: "Pushtow (four cargo barges)",
        8250: "Pushtow (five cargo barges)", 8260: "Pushtow (six cargo barges)",
        8270: "Pushtow (seven cargo barges)", 8280: "Pushtow (eigth cargo barges)",
        8290: "Pushtow (nine or more barges)", 8310: "Pushtow (one tank/gas barge)",
        8320: "Pushtow (two barges)", 8330: "Pushtow (three barges)", 8340: "Pushtow (four barges)",
        8350: "Pushtow (five barges)", 8360: "Pushtow (six barges)", 8370: "Pushtow (seven barges)",
        8380: "Pushtow (eight barges)", 8390: "Pushtow (nine or more barges)",
        8400: "Tug (single)", 8410: "Tug (one or more tows)", 8420: "Tug (assisting)",
        8430: "Pushboat (single)", 8440: "Passenger", 8441: "Ferry", 8442: "Red Cross",
        8443: "Cruise", 8444: "Passenger", 8450: "Service, Police or Port Service",
        8460: "Maintainance Craft", 8470: "Object (towed)", 8480: "Fishing",
        8490: "Bunkership", 8500: "Barge, Tanker, Chemical", 8510: "Object",
        1500: "General", 1510: "Unit Carrier Maritime", 1520: "bulk Carrier Maritime",
        1530: "Tanker", 1540: "Liquified Gas Tanker", 1850: "Pleasure",
        1900: "Fast Ship", 1910: "Hydrofoil", 1920: "Catamaran Fast",
    };
    if ((s >= 1500 && s <= 1920) || (s >= 8000 && s <= 8510)) {
        return inlandMap[s] ?? "Unknown (" + s + ")";
    }
    return "Unknown (" + s + ")";
}

export function getStringfromMsgType(m) {
    let s = "", delim = "";
    for (let i = 1; i <= 27; i++) {
        if ((m & (1 << i)) !== 0) { s += delim + Number(i).toFixed(0); delim = ", "; }
    }
    return s;
}

// ── Bitmask formatting ────────────────────────────────────────────────────────

export function getStringfromGroup(m) {
    let s = "", delim = "", count = 0;
    for (let i = 0; i < 32; i++) {
        if ((m & (1 << i)) !== 0) {
            if (count === 4) { s += delim + "..."; break; }
            s += delim + Number(i + 1).toFixed(0);
            delim = ", "; count++;
        }
    }
    return s;
}

export function getStringfromChannels(m) {
    let s = "", delim = "";
    for (let i = 0; i <= 3; i++) {
        if ((m & (1 << i)) !== 0) { s += delim + String.fromCharCode(65 + i); delim = ", "; }
    }
    return s;
}

// ── Sprite / icon ─────────────────────────────────────────────────────────────

export function getSprite(ship) {
    const shipClass = ship.shipclass;
    const sprite = shippingMappings[shipClass] || { cx: 120, cy: 20, imgSize: 20, hint: '' };

    ship.rot     = 0;
    ship.cx      = sprite.cx;
    ship.cy      = sprite.cy;
    ship.imgSize = sprite.imgSize;
    ship.hint    = sprite.hint;

    if (sprite.cy === 20) {
        if (ship.speed != null && ship.speed > 0.5 && ship.cog != null) {
            ship.cy  = 0;
            ship.rot = ship.cog * 3.1415926 / 180;
        }
    } else if ((shipClass === ShippingClass.HELICOPTER || shipClass === ShippingClass.PLANE) && ship.cog != null) {
        ship.rot = ship.cog * 3.1415926 / 180;
    }
}

export function getShipCSSClassAndStyle(ship, opacity = 1, scale = null) {
    const s = scale ?? settings.icon_scale;
    getSprite(ship);
    const style = `opacity: ${opacity}; background-position: -${ship.cx}px -${ship.cy}px; width: 20px; height: 20px; transform: rotate(${ship.rot}rad) scale(${s});`;
    return { class: "sprites", style, hint: ship.hint };
}

export const getIconCSS = (ship, opacity = 1) => {
    const { class: cls, style, hint } = getShipCSSClassAndStyle(ship, opacity);
    return `class="${cls}" style="${style}" title="${hint}"`;
};

export function getTableShiptype(ship, opacity = 1) {
    if (ship == null) return "";
    const { class: cls, style, hint } = getShipCSSClassAndStyle(ship, opacity);
    return settings.table_shiptype_use_icon
        ? `<span class="${cls}" style="${style}" title="${hint}"></span>`
        : hint;
}
