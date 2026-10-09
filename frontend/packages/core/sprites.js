/* ============================================================================
   sprites.js — which cell of the sprite sheet a vessel class uses (pure)

   Kept free of DOM and OpenLayers so a worker can import it: the tile decoder
   derives the same cells the map draws. markers.js re-exports these for hosts
   that already import the registry from there.

   The sheet is icons-tint.png: one cell per shape, a white body with a grey
   outline, coloured where it is drawn (ol/style/Icon's `color`, or the
   .sprites-tint class in sprites.css). icons.png, the pre-coloured sheet, is
   served as it is for KML, GeoJSON and anything outside that loads it;
   SPRITES describes its cells.
   ========================================================================= */

import { ShippingClass } from "./constants.js";

/* Cell origins in icons-tint.png (1x; icons-tint-2x.png doubles them) */
export const CELLS = {
    // row 0, 20 px
    arrow: { cx: 0, cy: 0, imgSize: 20 },        // vessel under way, turned to its course
    dot: { cx: 20, cy: 0, imgSize: 20 },         // vessel stopped or without a course
    diamond: { cx: 40, cy: 0, imgSize: 20 },     // AtoN, base station, SART: open in the centre
    // row 1, 25 px, nose up
    airliner: { cx: 0, cy: 20, imgSize: 25 },    // A3 A4
    widebody: { cx: 25, cy: 20, imgSize: 25 },   // A5
    bizjet: { cx: 50, cy: 20, imgSize: 25 },     // A2
    light: { cx: 75, cy: 20, imgSize: 25 },      // A1 B4
    fighter: { cx: 100, cy: 20, imgSize: 25 },   // A6
    helicopter: { cx: 125, cy: 20, imgSize: 25 },// A7
    glider: { cx: 150, cy: 20, imgSize: 25 },    // B1
    balloon: { cx: 175, cy: 20, imgSize: 25, noRotate: true },   // B2
    uav: { cx: 200, cy: 20, imgSize: 25 },       // B6
    vehicle: { cx: 225, cy: 20, imgSize: 25 },   // C1 C2
    obstacle: { cx: 250, cy: 20, imgSize: 25, noRotate: true },  // C3-C7
    unknown: { cx: 275, cy: 20, imgSize: 25 },   // A0 B0 B3 B7, none
};

/* ADS-B emitter category, stored as TC*10+ST: A = 4x, B = 3x, C = 2x */
const PLANE_BY_CATEGORY = {
    41: "light", 42: "bizjet", 43: "airliner", 44: "airliner", 45: "widebody",
    46: "fighter", 47: "helicopter",
    31: "glider", 32: "balloon", 34: "light", 36: "uav",
    21: "vehicle", 22: "vehicle", 23: "obstacle", 24: "obstacle", 25: "obstacle", 26: "obstacle", 27: "obstacle",
};
export const planeCell = (category) => CELLS[PLANE_BY_CATEGORY[category] || "unknown"];

/* The colour of a class: its marker and, by default, its track. The familiar
   hues at mid lightness, so none is lost on light or on dark tiles. Tanker,
   fishing and special are apart in lightness (dark brown, mid red, light
   pink). A vessel that reported no type takes Other's colour; the card and
   the filter tell the two apart, the map does not. */
export const CLASS_COLORS = {
    [ShippingClass.CARGO]: "#1fbf5c",
    [ShippingClass.TANKER]: "#d93a2b",
    [ShippingClass.PASSENGER]: "#2c4fd0",
    [ShippingClass.HIGHSPEED]: "#f2c81a",
    [ShippingClass.SPECIAL]: "#6b3d10",
    [ShippingClass.FISHING]: "#ff86b4",
    [ShippingClass.B]: "#c04ef0",
    [ShippingClass.OTHER]: "#22bdf0",
    [ShippingClass.UNKNOWN]: "#22bdf0",
    [ShippingClass.ATON]: "#f2c81a",
    [ShippingClass.STATION]: "#2c4fd0",
    [ShippingClass.SARTEPIRB]: "#d93a2b",
    // aircraft heard on AIS carry no altitude band: the middle of the orange scale
    [ShippingClass.PLANE]: "#ff9340",
    [ShippingClass.HELICOPTER]: "#ff9340",
};

/* A track and a label plate take the class colour too */
export const TRACK_COLORS = { ...CLASS_COLORS };

/* Aircraft are coloured in fixed bands of altitude (feet), never a continuous
   scale: every colour is one more tinted copy of the sheet. In every mode an
   aircraft on the ground is grey. "classic", the default, is one orange for
   every flying aircraft; "orange" is that hue by altitude, bright in every band
   so a low aircraft still shows on dark water, warmer when low and paler when
   high; "pastel" is tar1090's order of hues at a lightness no vessel class
   has. */
export const PLANE_PALETTES = {
    classic: [[Infinity, "#ff5400"]],
    orange: [[2000, "#ff5a1f"], [5000, "#ff7a2e"], [10000, "#ff9340"], [15000, "#ffaa4d"],
             [20000, "#ffbf5c"], [30000, "#ffd36b"], [Infinity, "#ffe27a"]],
    pastel: [[2000, "#fec394"], [10000, "#b9e092"], [20000, "#6ee8e1"], [30000, "#c7cdff"], [Infinity, "#fab7f3"]],
};
export const DEFAULT_PLANE_PALETTE = "classic";
export const PLANE_GROUND = "#a3a8b0";

/* undefined leaves the cell as drawn, white: an aircraft in the air whose
   altitude is not known, in a palette that colours by altitude */
export function planeColor(plane, mode) {
    if (plane.airborne == 0) return PLANE_GROUND;
    const bands = PLANE_PALETTES[mode] || PLANE_PALETTES[DEFAULT_PLANE_PALETTE];
    if (plane.altitude == null) return bands.length == 1 ? bands[0][1] : undefined;
    for (const [below, color] of bands) if (plane.altitude < below) return color;
    return undefined;
}

const VESSEL = "vessel";
const CLASSES = {
    [ShippingClass.OTHER]: { cell: VESSEL, hint: "Other" },
    [ShippingClass.UNKNOWN]: { cell: VESSEL, hint: "Unknown" },
    [ShippingClass.CARGO]: { cell: VESSEL, hint: "Cargo" },
    [ShippingClass.TANKER]: { cell: VESSEL, hint: "Tanker" },
    [ShippingClass.PASSENGER]: { cell: VESSEL, hint: "Passenger" },
    [ShippingClass.HIGHSPEED]: { cell: VESSEL, hint: "High Speed" },
    [ShippingClass.SPECIAL]: { cell: VESSEL, hint: "Special" },
    [ShippingClass.FISHING]: { cell: VESSEL, hint: "Fishing" },
    [ShippingClass.B]: { cell: VESSEL, hint: "Class B" },
    [ShippingClass.ATON]: { cell: "diamond", hint: "AtoN" },
    [ShippingClass.STATION]: { cell: "diamond", hint: "Base Station" },
    [ShippingClass.SARTEPIRB]: { cell: "diamond", hint: "SART/EPIRB" },
    [ShippingClass.PLANE]: { cell: "airliner", hint: "Aircraft", turns: true },
    [ShippingClass.HELICOPTER]: { cell: "helicopter", hint: "Helicopter", turns: true },
};

/* aircraft are drawn in outline and read smaller than a vessel's solid shape
   at the same size: an aircraft heard on AIS is drawn this much larger */
export const AIRCRAFT_SCALE = 1.15;
export const isAircraft = (shipClass) => shipClass === ShippingClass.PLANE || shipClass === ShippingClass.HELICOPTER;

const MOVING_KNOTS = 0.5;
const DEG = Math.PI / 180;

/* `colors` overrides the class colours, keyed like CLASS_COLORS */
export function spriteFor(shipClass, speed, cog, colors) {
    const cls = CLASSES[shipClass] || { cell: VESSEL, hint: "" };

    let cell = cls.cell;
    let rot = 0;

    if (cell === VESSEL) {
        cell = "dot";
        if (speed != null && speed > MOVING_KNOTS && cog != null) {
            cell = "arrow";
            rot = cog * DEG;
        }
    } else if (cls.turns && cog != null) {
        rot = cog * DEG;
    }

    const c = CELLS[cell];
    const color = (colors && colors[shipClass]) || CLASS_COLORS[shipClass] || CLASS_COLORS[ShippingClass.UNKNOWN];
    return { cx: c.cx, cy: c.cy, imgSize: c.imgSize, hint: cls.hint, rot: rot, color: color };
}

/* puts the sprite fields on the vessel the style functions read */
export function applySprite(ship, colors) {
    const s = spriteFor(ship.shipclass, ship.speed, ship.cog, colors);
    ship.rot = s.rot;
    ship.cx = s.cx;
    ship.cy = s.cy;
    ship.imgSize = s.imgSize;
    ship.hint = s.hint;
    ship.color = s.color;
    return s;
}

/* the inline style that shows a sprite in a .sprites-tint element (sprites.css) */
export function spriteCSS(sprite) {
    const pos = `-${sprite.cx}px -${sprite.cy}px`;
    return `background-position: ${pos}; -webkit-mask-position: ${pos}; mask-position: ${pos}; ` +
        `width: ${sprite.imgSize}px; height: ${sprite.imgSize}px;` +
        (sprite.color ? ` --sprite-color: ${sprite.color};` : "");
}

/* Cell origins in icons.png, the pre-coloured sheet KML and outside pages
   load. Row cy=20 is the "stationary" row; a moving vessel takes the same
   column on row 0. */
export const SPRITES = {
    [ShippingClass.OTHER]: { cx: 120, cy: 20, hint: "Other", imgSize: 20 },
    [ShippingClass.UNKNOWN]: { cx: 120, cy: 20, hint: "Unknown", imgSize: 20 },
    [ShippingClass.CARGO]: { cx: 0, cy: 20, hint: "Cargo", imgSize: 20 },
    [ShippingClass.TANKER]: { cx: 80, cy: 20, hint: "Tanker", imgSize: 20 },
    [ShippingClass.PASSENGER]: { cx: 40, cy: 20, hint: "Passenger", imgSize: 20 },
    [ShippingClass.HIGHSPEED]: { cx: 100, cy: 20, hint: "High Speed", imgSize: 20 },
    [ShippingClass.SPECIAL]: { cx: 60, cy: 20, hint: "Special", imgSize: 20 },
    [ShippingClass.FISHING]: { cx: 140, cy: 20, hint: "Fishing", imgSize: 20 },
    [ShippingClass.ATON]: { cx: 0, cy: 40, hint: "AtoN", imgSize: 20 },
    [ShippingClass.PLANE]: { cx: 0, cy: 60, hint: "Aircraft", imgSize: 25 },
    [ShippingClass.HELICOPTER]: { cx: 0, cy: 85, hint: "Helicopter", imgSize: 25 },
    [ShippingClass.B]: { cx: 20, cy: 20, hint: "Class B", imgSize: 20 },
    [ShippingClass.STATION]: { cx: 20, cy: 40, hint: "Base Station", imgSize: 20 },
    [ShippingClass.SARTEPIRB]: { cx: 40, cy: 40, hint: "SART/EPIRB", imgSize: 20 },
};
