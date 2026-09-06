// Speed palettes for speed-coloured tracks. A palette is a list of stops
// sampled at even fractions of the scale; the ramp interpolates them in sRGB
// and quantises to STEPS, so a vessel holding its speed still draws as one
// line feature instead of one per reported point.

export const SPEED_PALETTES = {
    turbo: {
        name: "Turbo",
        stops: ["#30123b", "#4145ab", "#4675ed", "#39a2fc", "#1bcfd4", "#24eca6",
                "#61fc6c", "#a4fc3b", "#d1e834", "#f3c63a", "#fe9b2d", "#f36315",
                "#d93806", "#b11901", "#7a0403"]
    },
    ocean: {
        name: "Ocean",
        stops: ["#2b3a67", "#2f6fb2", "#39a9c9", "#63cfa3", "#b8e067", "#f2d63c",
                "#f08a26", "#d93b1f"]
    },
    viridis: {
        name: "Viridis",
        stops: ["#440154", "#414487", "#2a788e", "#22a884", "#7ad151", "#fde725"]
    },
    plasma: {
        name: "Plasma",
        stops: ["#0d0887", "#6a00a8", "#b12a90", "#e16462", "#fca636", "#f0f921"]
    },
    traffic: {
        name: "Traffic",
        stops: ["#1e9e57", "#7dc832", "#f1c40f", "#e67e22", "#e74c3c"]
    },
    ice: {
        name: "Ice",
        stops: ["#08306b", "#2171b5", "#4292c6", "#6baed6", "#9ecae1", "#deebf7"]
    }
};

export const DEFAULT_PALETTE = "turbo";

// colour resolution of the ramp: coarse enough that reporting jitter does not
// split a steady track into a feature per point, fine enough to read as a ramp
export const STEPS = 16;

const hex = (c) => [parseInt(c.slice(1, 3), 16), parseInt(c.slice(3, 5), 16), parseInt(c.slice(5, 7), 16)];

const clamp01 = (f) => (f < 0 ? 0 : f > 1 ? 1 : f);

function sample(stops, f) {
    const x = clamp01(f) * (stops.length - 1);
    const i = Math.min(Math.floor(x), stops.length - 2);
    const t = x - i;
    const a = hex(stops[i]), b = hex(stops[i + 1]);
    const c = a.map((v, k) => Math.round(v + (b[k] - v) * t));
    return "#" + c.map((v) => v.toString(16).padStart(2, "0")).join("");
}

const ramps = {};

export function palette(key) {
    return SPEED_PALETTES[key] ? key : DEFAULT_PALETTE;
}

// STEPS colours, bottom of the scale first
export function ramp(key) {
    key = palette(key);
    if (!ramps[key]) {
        const stops = SPEED_PALETTES[key].stops;
        ramps[key] = Array.from({ length: STEPS }, (_, i) => sample(stops, i / (STEPS - 1)));
    }
    return ramps[key];
}

// bucket a speed in knots against a scale that tops out at `top` knots
export function speedBucket(knots, top) {
    if (!(knots >= 0)) return -1;
    const f = clamp01(knots / (top > 0 ? top : 1));
    return Math.min(STEPS - 1, Math.round(f * (STEPS - 1)));
}

export function bucketColor(key, bucket) {
    return ramp(key)[bucket < 0 ? 0 : bucket];
}

export function paletteCSS(key) {
    return "linear-gradient(to right, " + ramp(key).join(", ") + ")";
}
