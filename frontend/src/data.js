// What the viewer fetches from the server, and when: the vessels, the ships
// table's extra columns, the aircraft and the tracks, merged into core/store.js
// with their cursors and the server clock; the receiver they are asked for;
// the polling loop that refreshes whichever tab is open.

import { settings } from './core/state.js';
import * as filter from './core/filter.js';
import {
    ships as shipsDB, planes as planesDB, paths, pathsFrom, shipsSince, planesSince, clock, markerTracks,
    setShips, setPlanes, setPaths, setPathsFrom, setStation, setShipsSince, setPlanesSince, setClock,
} from './core/store.js';
import { ShippingClass } from '@aiscatcher/core/constants.js';
import { currentPlaceIds } from '@aiscatcher/core/visits.js';
import { sanitizeString, getICAOFromHexIdent } from '@aiscatcher/core/text.js';
import { redraw } from './map.js';

// { configVersion(version), shipsLoaded(response) }: what the entry does with a response
let deps = { configVersion: async () => {}, shipsLoaded: () => {} };

export function init(d) {
    deps = d;
}

// ─── the receiver ────────────────────────────────────────────────────────────

export let receiver = 0;

export function setReceiver(idx) {
    receiver = idx;
}

// another receiver: everything fetched so far was the old one's
export function switchReceiver(idx) {
    receiver = parseInt(idx, 10) || 0;
    setShipsSince(0);
    tableSince = 0;
    lastPathFetch = 0;
    setPaths({});
    setPathsFrom(-1);
}

// ─── vessels ─────────────────────────────────────────────────────────────────

const includeShip = (ship) => true;
let shipFilterOverride = null;

// a plugin's own test of which vessels the viewer keeps
export function setShipFilter(fn) {
    shipFilterOverride = fn;
}

let isFetchingShips = false;
let shipsTimeout = 1800;
let shipsLastCleanup = 0;

export async function fetchShips(noDoubleFetch = true) {
    if (isFetchingShips && noDoubleFetch) {
        console.log("A fetch operation is already running.");
        return false;
    }

    isFetchingShips = true;
    try {
        return await fetchShipsBody();
    } finally {
        isFetchingShips = false;
    }
}

// The ships table's columns beyond the map's row, pulled only while that tab
// is open: one full pass on opening, then only ships heard since, merged by MMSI.
const tableKeys = ["mmsi", "bearing", "level", "ppm", "count", "msg_type", "last_group", "group_mask", "altitude", "received_stations", "mmsi_type"];
let tableSince = 0;

export async function fetchTableRows() {
    try {
        const response = await fetch("api/ships_table.json?receiver=" + receiver + (tableSince > 0 ? "&since=" + tableSince : ""));
        if (!response.ok) return;
        const data = await response.json();
        (data.rows || []).forEach((v) => {
            const s = Object.fromEntries(tableKeys.map((k, i) => [k, v[i]]));
            if (s.mmsi in shipsDB) Object.assign(shipsDB[s.mmsi].raw, s);
        });
        if (data.time) tableSince = data.time - 1;
    } catch (error) {
        console.log("failed loading table rows:", error);
    }
}

async function fetchShipsBody() {
    let ships = {};

    try {
        const response = await fetch("api/ships_array.json?receiver=" + receiver + (shipsSince > 0 ? "&since=" + shipsSince : ""));
        if (!response.ok) {
            console.log("failed loading ships: HTTP " + response.status);
            return false;
        }
        ships = await response.json();
    } catch (error) {
        console.log("failed loading ships: " + error);
        return false;
    }

    const dynamicKeys = [
        "mmsi", "lat", "lon", "distance",
        "heading", "cog", "speed", "status", "age", "flags",
        "shipclass", "country", "binary", "station", "place_ids"
    ];

    const staticKeys = [
        "mmsi", "shipname", "callsign", "destination",
        "shiptype", "imo",
        "to_bow", "to_stern", "to_port", "to_starboard",
        "draught", "eta_month", "eta_day", "eta_hour", "eta_minute",
        "eni", "vendorid", "model", "serial"
    ];

    await deps.configVersion(ships.config_version);
    const serverTime = ships.time || 0;
    const isIncremental = shipsSince > 0 && ships.full !== true;

    if (!isIncremental) {
        setShips({});
        setStation({});
    }

    // Process static data first (name/voyage)
    if (ships.static) {
        ships.static.forEach((v) => {
            const s = Object.fromEntries(staticKeys.map((k, i) => [k, v[i]]));
            s.shipname = sanitizeString(s.shipname || "");
            s.callsign = sanitizeString(s.callsign || "");
            s.destination = sanitizeString(s.destination || "");
            s.eni = sanitizeString(s.eni || "");
            s.vendorid = sanitizeString(s.vendorid || "");
            const mmsi = s.mmsi;
            if (mmsi in shipsDB) {
                Object.assign(shipsDB[mmsi].raw, s);
                shipsDB[mmsi].show = undefined;
            } else {
                shipsDB[mmsi] = { raw: s };
            }
        });
    }

    // Process dynamic data (position/signal)
    if (ships.dynamic) {
        ships.dynamic.forEach((v) => {
            const s = Object.fromEntries(dynamicKeys.map((k, i) => [k, v[i]]));
            s.place_visits = s.place_ids || [];
            s.place_ids = currentPlaceIds(s.place_visits);
            s.place_version = ships.place_version || "";
            s.last_signal = serverTime - (s.age || 0);
            delete s.age;

            const flags = s.flags;
            s.validated = (flags & 3) == 2 ? -1 : flags & 3;
            s.repeat = (flags >> 2) & 3;
            s.virtual_aid = (flags >> 4) & 1;
            s.approx = (flags >> 5) & 1;
            s.channels = (flags >> 6) & 0b1111;
            s.cs_unit = (flags >> 10) & 3;
            s.raim = (flags >> 12) & 3;
            s.dte = (flags >> 14) & 3;
            s.assigned = (flags >> 16) & 3;
            s.display = (flags >> 18) & 3;
            s.dsc = (flags >> 20) & 3;
            s.band = (flags >> 22) & 3;
            s.msg22 = (flags >> 24) & 3;
            s.off_position = (flags >> 26) & 3;
            s.maneuver = (flags >> 28) & 3;

            const mmsi = s.mmsi;
            if (mmsi in shipsDB) {
                Object.assign(shipsDB[mmsi].raw, s);
                shipsDB[mmsi].show = undefined;
            } else {
                shipsDB[mmsi] = { raw: s };
            }
        });
    }

    // Filter ships after merge
    for (const mmsi in shipsDB) {
        if (!(shipFilterOverride ?? includeShip)(shipsDB[mmsi].raw)) {
            delete shipsDB[mmsi];
        }
    }

    if (ships.timeout) shipsTimeout = ships.timeout;

    setShipsSince(serverTime - 1);
    setClock(serverTime);
    filter.setClock(serverTime);

    // periodically expire ships older than timeout
    if (isIncremental && serverTime - shipsLastCleanup > shipsTimeout / 2) {
        for (const mmsi in shipsDB) {
            if (serverTime - shipsDB[mmsi].raw.last_signal > shipsTimeout)
                delete shipsDB[mmsi];
        }
        shipsLastCleanup = serverTime;
    }

    capShipsDB();

    setStation(ships.station || {});
    deps.shipsLoaded(ships);

    return true;
}

const MAX_SHIPS = 50000;

function capShipsDB() {
    const keys = Object.keys(shipsDB);
    if (keys.length <= MAX_SHIPS) return;

    keys.sort((a, b) => shipsDB[b].raw.last_signal - shipsDB[a].raw.last_signal);
    for (let i = MAX_SHIPS; i < keys.length; i++) delete shipsDB[keys[i]];

    console.log("shipsDB capped at " + MAX_SHIPS + ", dropped " + (keys.length - MAX_SHIPS) + " quiet vessels");
}

// ─── aircraft ────────────────────────────────────────────────────────────────

const planesTimeout = 300;
let planesLastCleanup = 0;

export async function fetchPlanes() {

    let planes = {};

    try {
        const response = await fetch("api/planes_array.json" + (planesSince > 0 ? "?since=" + planesSince : ""));
        if (!response.ok) {
            console.log("failed loading planes: HTTP " + response.status);
            return false;
        }
        planes = await response.json();
    } catch (error) {
        console.log("failed loading planes: " + error);
        return false;
    }

    const keys = [
        "hexident",
        "lat",
        "lon",
        "altitude",
        "speed",
        "heading",
        "vertrate",
        "squawk",
        "callsign",
        "airborne",
        "nMessages",
        "last_signal",
        "category",
        "level",
        "country",
        "distance",
        "message_types",
        "message_subtypes",
        "group_mask",
        "last_group",
        "bearing"
    ];

    const serverTime = planes.time || 0;
    const isIncremental = planesSince > 0;

    if (!isIncremental) setPlanes({});

    if (planes.values) {
        planes.values.forEach((v) => {
            const p = Object.fromEntries(keys.map((k, i) => [k, v[i]]));

            p.shipclass = ShippingClass.PLANE;
            p.validated = 1;
            p.name = p.callsign || getICAOFromHexIdent(p.hexident);

            const hex = p.hexident;
            if (hex in planesDB) {
                Object.assign(planesDB[hex].raw, p);
            } else {
                planesDB[hex] = { raw: p };
            }
        });
    }

    setPlanesSince(serverTime - 1);

    // Periodically expire planes silently dropped by the server's activity filter.
    if (isIncremental && serverTime - planesLastCleanup > planesTimeout / 2) {
        for (const hex in planesDB) {
            if (serverTime - planesDB[hex].raw.last_signal > planesTimeout)
                delete planesDB[hex];
        }
        planesLastCleanup = serverTime;
    }

    return true;
}

// ─── tracks ──────────────────────────────────────────────────────────────────

let lastPathFetch = 0;
let lastFullPathFetch = 0;

// server time from which tracks start, 0 for their whole history
export let trackCutoff = 0;

export function setTrackCutoff(t) {
    trackCutoff = t;
}

// the next fetch asks for the whole window again rather than what came since
export function resetTrackCursor() {
    lastPathFetch = 0;
}

// Oldest point to show, in SERVER epoch seconds, 0 for everything. Point times
// and the `since` filter are server-side, so a skewed browser clock must not leak in.
export function trackWindowStart() {
    if (!settings.track_history) return 0;
    const now = clock || Math.floor(Date.now() / 1000);
    return now - settings.track_history * 60;
}

function mergeTrackPoints(older, newer) {
    const merged = [];
    let i = 0, j = 0;

    while (i < newer.length && j < older.length) {
        const fresh = newer[i], prev = older[j];
        if (fresh[2] === prev[2]) {
            prev[0] = fresh[0]; prev[1] = fresh[1]; prev[3] = fresh[3];
            merged.push(prev);
            i++; j++;
        } else if (fresh[2] > prev[2]) {
            merged.push(fresh); i++;
        } else {
            merged.push(prev); j++;
        }
    }
    while (i < newer.length) merged.push(newer[i++]);
    while (j < older.length) merged.push(older[j++]);
    return merged;
}

export async function fetchTracks() {
    if (markerTracks.size == 0 && settings.show_all_tracks == false) return true;

    let a;
    let isDelta = false;
    try {
        if (settings.show_all_tracks) {
            // deltas accumulate points the server has pruned, so resync with a full fetch every hour
            isDelta = lastPathFetch > 0 && Date.now() - lastFullPathFetch < 3600 * 1000;
            let sinceParam = "&since=" + lastPathFetch;
            if (!isDelta) {
                setPathsFrom(trackWindowStart());
                sinceParam = pathsFrom ? "&since=" + pathsFrom : "";
                lastFullPathFetch = Date.now();
            }
            a = await fetch("api/allpath.json?receiver=" + receiver + sinceParam);
        } else {
            // a vessel gone from the store takes its track with it
            for (const mmsi of markerTracks) {
                if (!(mmsi in shipsDB)) {
                    markerTracks.delete(Number(mmsi));
                    redraw();
                }
            }
            const wanted = Array.from(markerTracks).filter((m) => !shipsDB[m] || filter.visible(shipsDB[m]));
            if (wanted.length === 0) return true;
            const mmsi_str = wanted.join(",");
            setPathsFrom(0);
            a = await fetch("api/path.json?" + mmsi_str + "&receiver=" + receiver);
        }

        const newPaths = await a.json();

        let maxTs = lastPathFetch;
        for (const mmsi in newPaths)
            for (const pt of newPaths[mmsi])
                if (pt[3] > maxTs) maxTs = pt[3];
        if (maxTs > lastPathFetch) lastPathFetch = maxTs;

        if (!isDelta) {
            setPaths(newPaths);
        } else {
            for (const mmsi in newPaths) {
                paths[mmsi] = paths[mmsi]
                    ? mergeTrackPoints(paths[mmsi], newPaths[mmsi])
                    : newPaths[mmsi];
            }
            for (const mmsi in paths) {
                if (!(mmsi in shipsDB)) delete paths[mmsi];
            }
        }
    } catch (error) {
        console.log("Error loading path: " + error);
        if (!isDelta) { setPaths({}); setPathsFrom(-1); }
        lastPathFetch = 0;
        return false;
    }

    const cutoff = trackCutoff;
    if (cutoff > 0) {
        for (const mmsi in paths) {
            const arr = paths[mmsi];
            let k = 0;
            while (k < arr.length && arr[k][3] >= cutoff) k++;
            paths[mmsi] = arr.slice(0, k + 1);
        }
    }

    return true;
}

// ─── the loop ────────────────────────────────────────────────────────────────

// tab id -> what refreshing it means
const refreshers = {};

export function onRefresh(tab, fn) {
    refreshers[tab] = fn;
}

export let refreshIntervalMs = 2500;
let interval;
let updateInProgress = false;

export function refresh() {
    if (!document.hidden && !updateInProgress) {
        updateInProgress = true;

        return (async () => {
            try {
                await refreshers[settings.tab]?.();
            } catch (error) {
                console.error("Error updating data:", error);
            } finally {
                updateInProgress = false;
            }
        })();
    }
    return Promise.resolve();
}

// refresh now, then on the interval from when that one is done
export function restart() {
    clearInterval(interval);

    refresh().then(() => {
        clearInterval(interval);
        interval = setInterval(refresh, refreshIntervalMs);
    });
}

export function setRefreshInterval(ms) {
    refreshIntervalMs = ms;
    if (interval) {
        clearInterval(interval);
        interval = setInterval(refresh, refreshIntervalMs);
    }
}
