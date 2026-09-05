// Units and coordinate notation: the one place a host's preferences reach
// the numbers. A host creates an instance with callables (or values) for the
// unit system and coordinate style, so two instances can disagree on one page.
//
//     const u = units.create({ system: () => settings.metric, coordinates: () => settings.coords });
//     u.getDistanceVal(12.3) + " " + u.getDistanceUnit()
//
// system: "DEFAULT" (nautical), "SI", or anything else for imperial.
// coordinates: "decimal", "dms" or "ddm".

import { getEtaVal, getStatusVal } from './text.js';

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


export function create(opts) {
    opts = opts || {};
    const fn = (v, dflt) => (typeof v === "function" ? v : () => (v == null ? dflt : v));
    const units = fn(opts.system, "DEFAULT");
    const coords = fn(opts.coordinates, "decimal");

    const getDistanceConversion = (c) =>
        units() === "DEFAULT" ? c : units() === "SI" ? c * 1.852 : c * 1.15078;
    const getDistanceVal = (c) => Number(getDistanceConversion(c)).toFixed(1);
    const getDistanceUnit = () =>
        units() === "DEFAULT" ? "nmi" : units() === "SI" ? "km" : "mi";

    const getSpeedConversion = (c) =>
        units() === "DEFAULT" ? c : units() === "SI" ? c * 1.852 : c * 1.151;
    const getSpeedVal = (c) =>
        units() === "DEFAULT" ? Number(c).toFixed(1)
            : units() === "SI" ? Number(c * 1.852).toFixed(1)
            : Number(c * 1.151).toFixed(1);
    const getSpeedUnit = () =>
        units() === "DEFAULT" ? "kts" : units() === "SI" ? "km/h" : "mph";

    const getDimVal = (c) =>
        units() === "DEFAULT" || units() === "SI"
            ? Number(c).toFixed(0)
            : Number(c * 3.2808399).toFixed(0);
    const getDimUnit = () =>
        units() === "DEFAULT" || units() === "SI" ? "m" : "ft";

    const getDraughtVal = (c) =>
        units() === "DEFAULT" || units() === "SI"
            ? Number(c).toFixed(1)
            : Number(c * 3.2808399).toFixed(1);


    const getShipDimension = (ship) =>
        ship.to_bow != null && ship.to_stern != null && ship.to_port != null && ship.to_starboard != null
            ? getDimVal(ship.to_bow + ship.to_stern) + " \u00d7 " + getDimVal(ship.to_port + ship.to_starboard) + " " + getDimUnit()
            : null;

    function formatCoordinate(value, isLatitude) {
        switch (coords()) {
            case "dms":
                return decimalToDMS(value, isLatitude);
            case "ddm":
                return decimalToDDM(value, isLatitude);
            default:
                return Number(value).toFixed(5);
        }
    }

    const getLatValFormat = (ship) => {
        const prefix = ship.approx ? "<i>" : "";
        const suffix = ship.approx ? "</i>" : "";
        return prefix + formatCoordinate(ship.lat, true) + suffix;
    };

    const getLonValFormat = (ship) => {
        const prefix = ship.approx ? "<i>" : "";
        const suffix = ship.approx ? "</i>" : "";
        return prefix + formatCoordinate(ship.lon, false) + suffix;
    };

    // change.f is a CHANGE id (text.js); draught arrives in decimetres
    function getChangeVal(change) {
        if (change.f === 2) return getStatusVal({ status: change.to });
        if (change.f === 1) return getDraughtVal(change.to / 10) + " " + getDimUnit();
        if (change.f === 6 && Array.isArray(change.to))
            return getEtaVal({ eta_month: change.to[0], eta_day: change.to[1], eta_hour: change.to[2], eta_minute: change.to[3] });
        return String(change.to);
    }

    return {
        getDistanceConversion, getDistanceVal, getDistanceUnit,
        getSpeedConversion, getSpeedVal, getSpeedUnit,
        getDimVal, getDimUnit, getDraughtVal, getShipDimension,
        formatCoordinate, getLatValFormat, getLonValFormat,
        getChangeVal,
    };
}
