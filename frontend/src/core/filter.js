import { settings } from './state.js';
import { ShippingClass } from './constants.js';

export const MOVING_KNOTS = 0.5;

// icon and pos address the same sprite sheet the counters card draws from, so a
// bucket looks the same wherever it is listed; spin marks the under-way sprites,
// which are drawn at an angle
export const BUCKETS = [
    { id: "am", label: "Class A moving", icon: "shipicon", pos: "-120px 0px", spin: true },
    { id: "as", label: "Class A stationary", icon: "shipicon", pos: "-120px -20px" },
    { id: "bm", label: "Class B moving", icon: "shipicon", pos: "-20px 0px", spin: true },
    { id: "bs", label: "Class B stationary", icon: "shipicon", pos: "-20px -20px" },
    { id: "aton", label: "Aid-to-Navigation", icon: "staticon", pos: "0px -20px" },
    { id: "base", label: "Base station", icon: "staticon", pos: "-20px -20px" },
    { id: "sarte", label: "SART / EPIRB", icon: "staticon", pos: "-40px -20px" },
    { id: "air", label: "Plane / helicopter", icon: "helicoptericon", pos: "0px 0px", spin: true },
];

export const CLASSES = [
    { id: ShippingClass.CARGO, label: "Cargo", pos: "0px 0px" },
    { id: ShippingClass.TANKER, label: "Tanker", pos: "-80px 0px" },
    { id: ShippingClass.PASSENGER, label: "Passenger", pos: "-40px 0px" },
    { id: ShippingClass.HIGHSPEED, label: "High-speed craft", pos: "-100px 0px" },
    { id: ShippingClass.SPECIAL, label: "Tug / special craft", pos: "-60px 0px" },
    { id: ShippingClass.FISHING, label: "Fishing", pos: "-140px 0px" },
    { id: ShippingClass.OTHER, label: "Other", pos: "-120px 0px" },
    { id: ShippingClass.UNKNOWN, label: "Not reported", pos: "-120px 0px" },
];

export const STATUSES = [
    { id: 0, label: "Under way using engine" },
    { id: 1, label: "At anchor" },
    { id: 5, label: "Moored" },
    { id: 8, label: "Under way sailing" },
    { id: 7, label: "Engaged in fishing" },
    { id: 2, label: "Not under command" },
    { id: 3, label: "Restricted manoeuvrability" },
    { id: 4, label: "Constrained by draught" },
    { id: 6, label: "Aground" },
    { id: 11, label: "Towing astern" },
    { id: 12, label: "Pushing ahead" },
    { id: 14, label: "AIS-SART active" },
    { id: 9, label: "Reserved (HSC)" },
    { id: 10, label: "Reserved (WIG)" },
    { id: 13, label: "Reserved" },
    { id: 15, label: "Not reported" },
];

const DEFAULTS = {
    hidden_buckets: [],
    hidden_classes: [],
    hidden_statuses: [],
    speed_min: null,
    speed_max: null,
    seen: 0,            // minutes; 0 means any age
    distance_min: null,
    distance_max: null,
    validated: "any",   // any | yes | no | pending
    repeated: "any",    // any | only | never
};

let clock = 0;

export function setClock(serverTime) {
    if (serverTime > 0) clock = serverTime;
}

// shipPasses runs per vessel per frame during replay, so the summary of what is
// active is computed once per change rather than rebuilt on every call.
let revision = 0;
let summary = { f: null, revision: -1, terms: [], active: false };

function state() {
    if (!settings.ship_filter || typeof settings.ship_filter !== "object")
        settings.ship_filter = {};

    const f = settings.ship_filter;
    for (const k in DEFAULTS)
        if (!(k in f)) f[k] = Array.isArray(DEFAULTS[k]) ? [...DEFAULTS[k]] : DEFAULTS[k];

    if (typeof f.repeated === "boolean") f.repeated = f.repeated ? "any" : "never";
    delete f.relayed;
    return f;
}

export function get(key) {
    const f = state();
    return key in f ? f[key] : DEFAULTS[key];
}

export function set(key, value) {
    state()[key] = value;
    revision++;
}

// The three checkbox lists differ only in what they hold.
export const LISTS = {
    bucket: { key: "hidden_buckets", items: () => BUCKETS },
    class: { key: "hidden_classes", items: () => CLASSES },
    status: { key: "hidden_statuses", items: () => STATUSES },
};

export function isHidden(kind, id) {
    return get(LISTS[kind].key).includes(id);
}

export function toggle(kind, id) {
    const key = LISTS[kind].key;
    const hidden = get(key);
    set(key, isHidden(kind, id) ? hidden.filter((x) => x !== id) : [...hidden, id]);
}

export function setAll(kind, on) {
    set(LISTS[kind].key, on ? [] : LISTS[kind].items().map((x) => x.id));
}

export function statusOf(ship) {
    const s = ship.status;
    return Number.isInteger(s) && s >= 0 && s <= 15 ? s : 15;
}

export function reset() {
    settings.ship_filter = {};
    revision++;
    state();
}

export function moving(ship) {
    return ship.speed != null && ship.speed > MOVING_KNOTS;
}

export function bucketFor(shipclass, speed) {
    const underway = speed != null && speed > MOVING_KNOTS;
    switch (shipclass) {
        case ShippingClass.ATON: return "aton";
        case ShippingClass.STATION: return "base";
        case ShippingClass.SARTEPIRB: return "sarte";
        case ShippingClass.PLANE:
        case ShippingClass.HELICOPTER: return "air";
        case ShippingClass.B: return underway ? "bm" : "bs";
        default: return underway ? "am" : "as";
    }
}

export function bucketOf(ship) {
    return bucketFor(ship.shipclass, ship.speed);
}

function current() {
    const f = state();
    if (summary.f !== f || summary.revision !== revision) {
        const list = describeTerms(f);
        summary = { f, revision, terms: list, active: list.length > 0 };
    }
    return summary;
}

function describeTerms(f) {
    const list = [];
    if (f.hidden_buckets?.length) list.push(hiddenLabel(f.hidden_buckets.length, "group"));
    if (f.hidden_classes?.length) list.push(hiddenLabel(f.hidden_classes.length, "type"));
    if (f.hidden_statuses?.length) list.push(hiddenLabel(f.hidden_statuses.length, "status", "es"));
    if (f.speed_min != null || f.speed_max != null)
        list.push("speed " + (f.speed_min ?? 0) + "-" + (f.speed_max ?? "∞") + " kn");
    if (f.seen > 0) list.push("seen < " + f.seen + " min");
    if (f.distance_min != null || f.distance_max != null)
        list.push("distance " + (f.distance_min ?? 0) + "-" + (f.distance_max ?? "∞"));
    if (f.validated && f.validated !== "any") list.push("validated: " + f.validated);
    if (f.repeated === "only") list.push("repeated only");
    else if (f.repeated === "never") list.push("no repeated");
    return list;
}

function hiddenLabel(n, what, plural = "s") {
    return n + " " + what + (n === 1 ? "" : plural) + " hidden";
}

export function isActive() {
    return current().active;
}

export function describe() {
    return current().terms;
}

// The terms a replayed vessel can be judged on: it carries a class and a speed,
// but no reception data, status or distance.
export function passesAppearance(ship) {
    const { active, f } = current();
    if (!active) return true;

    if (f.hidden_buckets.length && f.hidden_buckets.includes(bucketOf(ship))) return false;
    if (f.hidden_classes.length && f.hidden_classes.includes(ship.shipclass)) return false;
    if (f.speed_min != null && !(ship.speed != null && ship.speed >= f.speed_min)) return false;
    if (f.speed_max != null && !(ship.speed != null && ship.speed <= f.speed_max)) return false;

    return true;
}

export function shipPasses(ship) {
    const { active, f } = current();
    if (!active) return true;
    if (!passesAppearance(ship)) return false;

    if (f.hidden_statuses.length && f.hidden_statuses.includes(statusOf(ship))) return false;

    if (f.seen > 0 && !(ship.last_signal != null && clock - ship.last_signal <= f.seen * 60)) return false;
    if (f.distance_min != null && !(ship.distance != null && ship.distance >= f.distance_min)) return false;
    if (f.distance_max != null && !(ship.distance != null && ship.distance <= f.distance_max)) return false;

    if (f.validated !== "any") {
        const want = f.validated === "yes" ? 1 : f.validated === "no" ? -1 : 0;
        if (ship.validated !== want) return false;
    }

    if (f.repeated !== "any" && (ship.repeat > 0) !== (f.repeated === "only")) return false;

    return true;
}
