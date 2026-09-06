/* ============================================================================
   sprites.js — which cell of the sprite sheet a vessel class uses (pure)

   Kept free of DOM and OpenLayers so a worker can import it: the tile decoder
   derives the same cells the map draws. markers.js re-exports these for hosts
   that already import the registry from there.
   ========================================================================= */

import { ShippingClass } from "./constants.js";

/* Cell origins in icons.png. Row cy=20 is the "stationary" row; a moving
   vessel takes the same column on row 0 and is rotated to its course. */
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

const MOVING_KNOTS = 0.5;
const DEG = Math.PI / 180;

export function spriteFor(shipClass, speed, cog) {
    const sprite = SPRITES[shipClass] || { cx: 120, cy: 20, imgSize: 20, hint: "" };

    let cy = sprite.cy;
    let rot = 0;

    if (sprite.cy === 20) {
        if (speed != null && speed > MOVING_KNOTS && cog != null) {
            cy = 0;
            rot = cog * DEG;
        }
    } else if ((shipClass == ShippingClass.HELICOPTER || shipClass == ShippingClass.PLANE) && cog != null) {
        rot = cog * DEG;
    }

    return { cx: sprite.cx, cy: cy, imgSize: sprite.imgSize, hint: sprite.hint, rot: rot };
}

/* puts the sprite fields on the vessel the style functions read */
export function applySprite(ship) {
    const s = spriteFor(ship.shipclass, ship.speed, ship.cog);
    ship.rot = s.rot;
    ship.cx = s.cx;
    ship.cy = s.cy;
    ship.imgSize = s.imgSize;
    ship.hint = s.hint;
    return s;
}
