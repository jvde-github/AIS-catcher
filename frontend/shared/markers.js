/* ============================================================================
   markers.js — how vessels and aircraft are drawn on an OpenLayers map

   The sprite sheet (icons.png) and its registry, the hull outline and range
   geometries, and the style functions for markers, hulls, tracks, labels and
   the hover/selection rings. Needs `ol` in the host's dependencies.

   The registry is pure: spriteFor() says which cell of the sheet a class
   uses and how it turns. The style functions come from create(), which is
   handed what they read: a settings getter, what is hovered and selected,
   and how faded a vessel is. A feature carries its vessel as `feature.ship`
   (or `feature.plane`) with the sprite fields applySprite() puts on it.

       const m = markers.create({
           sheet: "icons.png",
           settings: () => settings,
           hover: () => ({ type: hoverType, id: hoverMMSI }),
           selected: () => ({ type: card_type, id: card_mmsi }),
           opacity: (ship) => fadeOpacity(clock - ship.last_signal),
       });
       new VectorLayer({ source, style: m.marker });
   ========================================================================= */

import Style from "ol/style/Style.js";
import Stroke from "ol/style/Stroke.js";
import Fill from "ol/style/Fill.js";
import Icon from "ol/style/Icon.js";
import CircleStyle from "ol/style/Circle.js";
import Text from "ol/style/Text.js";
import Polygon from "ol/geom/Polygon.js";
import { fromLonLat } from "ol/proj.js";

import { ShippingClass } from "./core/constants.js";
import { calcOffset1M, calcMove, shipOutlineLocal } from "./core/geo.js";
import { hexToRgb, deriveLabelBackground } from "./color.js";

/* the registry is pure and lives in core/sprites.js so a worker can import
   it without OpenLayers; re-exported here for hosts that use it via markers */
export { SPRITES, spriteFor, applySprite } from "./core/sprites.js";
import { SPRITES, applySprite } from "./core/sprites.js";

const DEG = Math.PI / 180;

/* ADS-B: the icon follows the emitter category, the size its wake class */
export function applyPlaneSprite(plane) {
    let sprite = SPRITES[ShippingClass.PLANE];
    if (plane.category && plane.category == 47) sprite = SPRITES[ShippingClass.HELICOPTER];

    let scaling = 0.75;
    if (plane.category) {
        switch (plane.category % 10) {
            case 1: scaling = 0.5; break;
            case 2: case 3: scaling = 0.75; break;
            case 4: case 5: scaling = 1.0; break;
            case 6: scaling = 0.8; break;
            case 7: scaling = 0.6; break;
        }
    }

    plane.scaling = scaling * 1.2;
    plane.rot = plane.heading * DEG;
    plane.cx = plane.airborne == 0 ? 25 : 50;
    plane.cy = sprite.cy;
    plane.imgSize = sprite.imgSize;
    plane.hint = sprite.hint;
    return sprite;
}

/* icon scale bucket by vessel length */
/* radius of a hover/selection ring drawn with base `radiusBase`, in pixels */
function ringRadius(radiusBase, s) {
    const iconS = s.icon_scale || 1.0;
    const circleS = s.circle_scale || 6.0;
    return radiusBase * iconS * (1 + (circleS - 2.0) * 0.08);
}

/* outer radius of the selection ring, stroke included: what must stay clear */
export function selectionRadius(s) {
    s = s || {};
    return ringRadius(13, s) + ((s.circle_scale || 6.0) * (s.icon_scale || 1.0)) / 2;
}

export function iconScale(length) {
    return length >= 100 && length <= 200 ? 0.9 : length > 200 ? 1.1 : 0.75;
}

/* age-based fade: full for half an hour, floor at a fifth */
export function fadeCurve(age) {
    return Math.max(0.2, Math.min(1, 1 - (age / 1800) * 0.8));
}

/* --- geometry ------------------------------------------------------------- */

export function shipOutlineGeometry(ship) {
    if (!ship) return null;
    const coordinate = [ship.lon, ship.lat];

    let heading = ship.heading;
    const { to_bow, to_stern, to_port, to_starboard } = ship;
    if (to_bow == null || to_stern == null || to_port == null || to_starboard == null) return null;

    if (heading == null) {
        if (ship.cog == null || !(ship.speed > 1)) return null;
        heading = ship.cog;
    }

    const deltaBow = calcOffset1M(coordinate, heading % 360);
    const deltaStarboard = calcOffset1M(coordinate, (heading + 90) % 360);

    const ring = shipOutlineLocal(to_bow, to_stern, to_port, to_starboard)
        .map(([f, s]) => calcMove(calcMove(coordinate, deltaBow, f), deltaStarboard, s));
    ring.push(ring[0]);

    return new Polygon([ring.map((c) => fromLonLat(c))]);
}

export function rangeGeometry(lat, lon, radius) {
    const deltaNorth = calcOffset1M([lon, lat], 0)[0];
    const deltaEast = calcOffset1M([lon, lat], 90)[1];
    const N = 50;

    const outline = [];
    for (let i = 0; i < N; i++) {
        outline.push([
            lon + radius * deltaEast * Math.sin(((i * 2) / N) * Math.PI),
            lat + radius * deltaNorth * Math.cos(((i * 2) / N) * Math.PI),
        ]);
    }
    return new Polygon([outline.map((p) => fromLonLat(p))]);
}

/* --- styles --------------------------------------------------------------- */

const SETTING_DEFAULTS = {
    icon_scale: 1,
    circle_scale: 6,
    shipoutline_inner: "#12a5ed",
    shipoutline_opacity: 0.3,
    shipoutline_border: "#12a5ed",
    shiphover_color: "#ff0000",
    shipselection_color: "#ff0000",
    track_weight: 2,
    track_opacity: 1,
    track_class_colors: {},
    label_class_background: false,
    labels_active_only: false,
    labels_prioritize_active: true,
    tooltipLabelFontSize: 12,
    tooltipLabelColor: "#000000",
    tooltipLabelShadowColor: "#ffffff",
    tooltipLabelColorDark: "#ffffff",
    tooltipLabelShadowColorDark: "#000000",
    dark_mode: false,
};

export function create(opts) {
    opts = opts || {};
    const sheet = opts.sheet || "icons.png";
    const host = opts.settings || (() => ({}));
    const view = new Proxy({}, { get: (_, k) => { const v = host()[k]; return v === undefined ? SETTING_DEFAULTS[k] : v; } });
    const settings = () => view;
    const hover = opts.hover || (() => ({ type: null, id: null }));
    const selected = opts.selected || (() => ({ type: null, id: null }));
    const opacity = opts.opacity || (() => 1);
    const lookup = opts.lookup || (() => null);      // (type, id) -> vessel, for the selection ring

    const isHovered = (ship) => { const h = hover(); return h.type === "ship" && ship.mmsi == h.id; };
    const isSelected = (ship) => { const s = selected(); return s.type === "ship" && ship.mmsi == s.id; };

    /* Style parts are shared between features: a Style per sprite whose Icon
       is set to the feature's rotation, scale and opacity, a Stroke per
       colour/width/dash. The keys carry every setting they depend on, so a
       changed setting simply lands on a new entry. */
    const CACHE_MAX = 512;
    function memo(map, key, make) {
        let v = map.get(key);
        if (v === undefined) {
            if (map.size >= CACHE_MAX) map.clear();
            v = make(); map.set(key, v);
        }
        return v;
    }
    const iconStyles = new Map();   // sprite -> Style with a mutable Icon
    const strokes = new Map();
    const fills = new Map();
    const stroke = (color, width, dash) => memo(strokes, color + "|" + width + "|" + (dash ? dash.join(",") : ""), () => new Stroke({ color, width, lineDash: dash }));
    const fill = (color) => memo(fills, color, () => new Fill({ color }));

    function spriteStyle(cx, cy, imgSize) {
        return memo(iconStyles, cx + "," + cy + "," + imgSize, () => new Style({
            image: new Icon({ src: sheet, offset: [cx, cy], size: [imgSize, imgSize] }),
        }));
    }

    function marker(feature) {
        const s = settings();
        const ship = feature.ship;
        const mult = iconScale((ship.to_bow || 0) + (ship.to_stern || 0));
        const highlighted = isHovered(ship) || isSelected(ship);
        const style = spriteStyle(ship.cx, ship.cy, ship.imgSize), icon = style.getImage();
        icon.setRotation(ship.rot);
        icon.setScale(s.icon_scale * mult);
        icon.setOpacity(highlighted ? 1 : opacity(ship));
        return style;
    }

    function plane(feature) {
        const s = settings();
        const p = feature.plane;
        const style = spriteStyle(p.cx, p.cy, p.imgSize), icon = style.getImage();
        icon.setRotation(p.rot);
        icon.setScale(s.icon_scale * p.scaling);
        icon.setOpacity(1);
        return [style];
    }

    function hull(feature) {
        const s = settings();
        const [r, g, b] = hexToRgb(s.shipoutline_inner);
        return new Style({
            fill: fill(`rgba(${r}, ${g}, ${b}, ${s.shipoutline_opacity})`),
            stroke: stroke(feature.ship && isHovered(feature.ship) ? s.shiphover_color : s.shipoutline_border, 2),
        });
    }

    /* a track feature carries mmsi, shipclass, optional speedColor and isDashed */
    function track(feature) {
        const s = settings();
        let w = Number(s.track_weight);
        let c = "#12a5ed";
        let highlighted = false;

        if (feature.speedColor) c = feature.speedColor;
        else if (feature.shipclass && s.track_class_colors[feature.shipclass]) c = s.track_class_colors[feature.shipclass];

        const h = hover(), sel = selected();
        if (h.type === "ship" && feature.mmsi == h.id) { c = s.shiphover_color; w += 2; highlighted = true; }
        else if (sel.type === "ship" && feature.mmsi == sel.id) { c = s.shipselection_color; w += 2; highlighted = true; }

        const o = Number(s.track_opacity);
        if (!highlighted && o < 1) {
            const [r, g, b] = hexToRgb(c);
            c = `rgba(${r}, ${g}, ${b}, ${o})`;
        }

        return new Style({ stroke: stroke(c, w, feature.isDashed ? [6, 6] : undefined) });
    }

    /* the text part of a label; `cls` picks the class colour in background mode */
    function labelText(txt, op, cls) {
        const s = settings();
        const text = new Text({
            text: txt,
            overflow: true,
            offsetY: 25,
            offsetX: 25,
            font: s.tooltipLabelFontSize + "px Arial",
        });

        if (s.label_class_background) {
            const base = s.track_class_colors[cls] || "#12a5ed";
            text.setFill(new Fill({ color: `rgba(255, 255, 255, ${op})` }));
            text.setBackgroundFill(new Fill({ color: deriveLabelBackground(base, 0.88 * op) }));
            text.setPadding([2, 4, 2, 4]);
        } else {
            const [lr, lg, lb] = hexToRgb(s.dark_mode ? s.tooltipLabelColorDark : s.tooltipLabelColor);
            const [sr, sg, sb] = hexToRgb(s.dark_mode ? s.tooltipLabelShadowColorDark : s.tooltipLabelShadowColor);
            text.setFill(new Fill({ color: `rgba(${lr}, ${lg}, ${lb}, ${op})` }));
            text.setStroke(new Stroke({ color: `rgba(${sr}, ${sg}, ${sb}, ${op})`, width: 5 }));
        }
        return text;
    }

    /* `name(feature)` gives the label's text; the host decides what a vessel is called */
    function label(name) {
        return function (feature) {
            const s = settings();
            const sel = selected();
            const isShip = "ship" in feature;
            const obj = isShip ? feature.ship : feature.plane;
            const active = isShip
                ? sel.type === "ship" && feature.ship.mmsi == sel.id
                : sel.type === "plane" && feature.plane.hexident == sel.id;

            if (s.labels_active_only && !active) return new Style({});

            const op = !isShip || active || isHovered(feature.ship) ? 1 : opacity(feature.ship);
            const text = labelText(name(feature), op, obj.shipclass);
            const onTop = (s.labels_prioritize_active ?? true) && active;
            return new Style({ text: text, zIndex: onTop ? 1000 : 0 });
        };
    }

    function ring(radiusBase, colorKey) {
        const s = settings();
        const iconS = s.icon_scale || 1.0;
        const circleS = s.circle_scale || 6.0;
        return new Style({
            image: new CircleStyle({
                radius: ringRadius(radiusBase, s),
                stroke: new Stroke({ color: s[colorKey], width: circleS * iconS }),
            }),
        });
    }

    function hoverRing() {
        return ring(16, "shiphover_color");
    }

    /* the selection ring redraws the selected vessel's icon on top of it */
    function selectRing() {
        const styles = [ring(13, "shipselection_color")];
        const sel = selected();
        const v = lookup(sel.type, sel.id);
        if (v && v.imgSize) {
            if (sel.type === "ship") styles.push(marker({ ship: v }));
            else if (sel.type === "plane") styles.push(...plane({ plane: v }));
        }
        return styles;
    }

    return { sheet, marker, plane, hull, track, label, labelText, hoverRing, selectRing };
}
