/* ============================================================================
   color.js — colour arithmetic that needs a canvas to normalise names
   ========================================================================= */

let ctx = typeof document !== "undefined" ? document.createElement("canvas").getContext("2d") : null;

/* "#12a5ed", "12a5ed" or any CSS colour -> [r, g, b]; unknown -> the accent blue */
let memo = {};

export function hexToRgb(color) {
    color = String(color || "");
    if (memo[color]) return memo[color];
    let key = color;
    if (/^[a-f\d]{6}$/i.test(color)) color = "#" + color;
    if (ctx) {
        ctx.fillStyle = "#12a5ed";
        ctx.fillStyle = color;
        color = ctx.fillStyle;
    }
    let m = /^#([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(color);
    return (memo[key] = m ? [parseInt(m[1], 16), parseInt(m[2], 16), parseInt(m[3], 16)] : [18, 165, 237]);
}

/* A readable label background from a (often bright) line colour: push the
   channels away from luma, then darken. */
export function deriveLabelBackground(hex, alpha) {
    if (alpha == null) alpha = 0.88;
    let rgb = hexToRgb(hex);
    let r = rgb[0], g = rgb[1], b = rgb[2];
    let luma = 0.299 * r + 0.587 * g + 0.114 * b;
    let saturate = 1.35, darken = 0.6;
    let adjust = function (c) { return Math.max(0, Math.min(255, Math.round((luma + (c - luma) * saturate) * darken))); };
    return "rgba(" + adjust(r) + ", " + adjust(g) + ", " + adjust(b) + ", " + alpha + ")";
}

export function token(name, fallback) {
    if (typeof document === "undefined") return fallback;
    let v = getComputedStyle(document.documentElement).getPropertyValue(name).trim();
    return v || fallback;
}
