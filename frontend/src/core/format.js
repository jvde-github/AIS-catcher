// Presentation helpers (distance/speed/coordinate/time/name formatters).
// `getTableShiptype` and `getShipTypeVal` stay in script.js — they touch
// the sprite registry and a large IMO lookup respectively.

import { settings } from './state.js';
import { CLASS_A, CLASS_B, BASESTATION, SAR, SARTEPIRB, ATON } from './constants.js';

export const getDistanceConversion = (c) =>
    settings.metric === "DEFAULT" ? c : settings.metric === "SI" ? c * 1.852 : c * 1.15078;
export const getDistanceVal = (c) => Number(getDistanceConversion(c)).toFixed(1).toLocaleString();
export const getDistanceUnit = () =>
    settings.metric === "DEFAULT" ? "nmi" : settings.metric === "SI" ? "km" : "mi";

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

export const getShipDimension = (ship) =>
    ship.to_bow != null && ship.to_stern != null && ship.to_port != null && ship.to_starboard != null
        ? getDimVal(ship.to_bow + ship.to_stern) + " " + getDimUnit() + " x " + getDimVal(ship.to_port + ship.to_starboard) + " " + getDimUnit()
        : null;


export function decimalToDMS(l, isLatitude) {
    const degrees = Math.floor(Math.abs(l));
    const minutes = Math.floor((Math.abs(l) - degrees) * 60);
    const seconds = Number(((Math.abs(l) - degrees) * 60 - minutes) * 60).toFixed(1);
    const direction = isLatitude ? (l > 0 ? "N" : "S") : l > 0 ? "E" : "W";
    return degrees + "&deg" + minutes + "'" + seconds + '"' + direction;
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


export const getEtaVal = (ship) =>
    ("0" + ship.eta_month).slice(-2) + "-" + ("0" + ship.eta_day).slice(-2) + " " +
    ("0" + ship.eta_hour).slice(-2) + ":" + ("0" + ship.eta_minute).slice(-2);

export const getDeltaTimeVal = (s) => {
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
    "Reserved",
    "Reserved",
    "Reserved",
    "AIS-SART is active",
    "Not available",
];

export function getStatusVal(ship) {
    return STATUS_STRINGS[Math.min(ship.status, 15)];
}


export function getTypeVal(ship) {
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
