import { sanitizeString, includeShip, ShippingClass } from '../../format.js';
import { settings } from '../../settings.js';

// ── Shared mutable data stores ────────────────────────────────────────────────
// Exported as const objects — cleared+refilled in-place so importers always
// see current data (same pattern as settings.js).

export const shipsDB  = {};
export const planesDB = {};
export const binaryDB = {};

// Station / center are also cleared+refilled in-place.
export const station = {};
export const center  = {};

export let standaloneBinaryMessages = [];

let _isFetchingShips = false;
let _shipCount = 0;

export function getShipCount() { return _shipCount; }

// ── Helpers ───────────────────────────────────────────────────────────────────

export function formatTime(timestamp) {
    const date = new Date(timestamp * 1000);
    return date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
}

export async function fetchJSON(l, m) {
    let response;
    try {
        response = await fetch(l + "?" + m);
    } catch (error) {
        return null;
    }
    return response.text();
}

// ── Ship keys (compact array from api/ships_array.json) ───────────────────────

const SHIP_KEYS = [
    "mmsi", "lat", "lon", "distance", "bearing", "level", "count", "ppm",
    "approx", "heading", "cog", "speed", "to_bow", "to_stern", "to_starboard", "to_port",
    "last_group", "group_mask", "shiptype", "mmsi_type", "shipclass", "msg_type",
    "country", "status", "draught", "eta_month", "eta_day", "eta_hour", "eta_minute",
    "imo", "callsign", "shipname", "destination", "last_signal",
    "flags", "validated", "channels", "altitude", "received_stations",
];

const PLANE_KEYS = [
    "hexident", "lat", "lon", "altitude", "speed", "heading", "vertrate",
    "squawk", "callsign", "airborne", "nMessages", "last_signal",
    "category", "level", "country", "distance", "message_types", "message_subtypes",
    "group_mask", "last_group", "bearing",
];

// ── Fetch functions ───────────────────────────────────────────────────────────

export async function fetchShips(activeReceiver, noDoubleFetch = true) {
    if (_isFetchingShips && noDoubleFetch) {
        console.log("A fetch operation is already running.");
        return false;
    }

    _isFetchingShips = true;
    let response;
    try {
        response = await fetch("api/ships_array.json?receiver=" + activeReceiver);
    } catch (error) {
        console.log("failed loading ships: " + error);
        return false;
    } finally {
        _isFetchingShips = false;
    }

    const ships = await response.json();

    // Clear in-place
    for (const k in shipsDB) delete shipsDB[k];
    for (const k in station) delete station[k];

    ships.values.forEach((v) => {
        const s = Object.fromEntries(SHIP_KEYS.map((k, i) => [k, v[i]]));

        const flags = s.flags;
        s.validated2   = (flags & 3) === 2 ? -1 : flags & 3;
        s.repeat       = (flags >> 2)  & 3;
        s.virtual_aid  = (flags >> 4)  & 1;
        s.approximate  = (flags >> 5)  & 1;
        s.channels2    = (flags >> 6)  & 0b1111;
        s.cs_unit      = (flags >> 10) & 3;
        s.raim         = (flags >> 12) & 3;
        s.dte          = (flags >> 14) & 3;
        s.assigned     = (flags >> 16) & 3;
        s.display      = (flags >> 18) & 3;
        s.dsc          = (flags >> 20) & 3;
        s.band         = (flags >> 22) & 3;
        s.msg22        = (flags >> 24) & 3;
        s.off_position = (flags >> 26) & 3;
        s.maneuver     = (flags >> 28) & 3;

        if (includeShip(s)) {
            s.shipname = sanitizeString(s.shipname);
            s.callsign = sanitizeString(s.callsign);
            shipsDB[s.mmsi] = { raw: s };
        }
    });

    if (Object.prototype.hasOwnProperty.call(ships, "station")) {
        Object.assign(station, ships.station);
    }

    // Resolve center
    for (const k in center) delete center[k];
    if (String(settings.center_point).toUpperCase() === "STATION") {
        Object.assign(center, station);
    } else if (settings.center_point in shipsDB) {
        const ship = shipsDB[settings.center_point].raw;
        Object.assign(center, { lat: ship.lat, lon: ship.lon });
    }

    _shipCount = ships.values.length;
    return true;
}

export async function fetchPlanes() {
    let response;
    try {
        response = await fetch("api/planes_array.json");
    } catch (error) {
        console.log("failed loading planes: " + error);
        return false;
    }

    const planes = await response.json();

    for (const k in planesDB) delete planesDB[k];

    planes.values.forEach((v) => {
        const p = Object.fromEntries(PLANE_KEYS.map((k, i) => [k, v[i]]));
        p.shipclass = ShippingClass.PLANE;
        p.validated = 1;
        p.name = p.callsign || p.hexident;
        planesDB[p.hexident] = { raw: p };
    });

    return true;
}

export async function fetchBinary(activeReceiver) {
    for (const k in binaryDB) delete binaryDB[k];
    standaloneBinaryMessages = [];

    let response;
    try {
        response = await fetch("api/binmsgs.json?receiver=" + activeReceiver);
        const messages = await response.json();

        messages.forEach((msg, index) => {
            if (msg.message && msg.message.mmsi && msg.message.lat && msg.message.lon) {
                const mmsi = msg.message.mmsi;
                if (!binaryDB[mmsi]) {
                    binaryDB[mmsi] = { ship_messages: [], standalone_messages: [] };
                }
                msg.formattedTime = formatTime(msg.timestamp);
                msg.index = index + 1;
                msg.message_lat = msg.message.lat;
                msg.message_lon = msg.message.lon;
                binaryDB[mmsi].ship_messages.push(msg);
            }
        });

        return true;
    } catch (error) {
        console.log("Failed loading binary:", error);
        return false;
    }
}
