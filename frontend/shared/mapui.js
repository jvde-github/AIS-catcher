/* ============================================================================
   mapui.js — the map's UI in one call

   Wires chrome, markers, the card shell, the toolbar, the context menu and
   the view helper together with defaults that work for a page laid out with
   map.css. A host passes the elements it has and overrides only what is
   particular to it; what it does not pass, it does not get.

       const ui = mapui.create({
           map: () => map,                          // the OpenLayers map, or a getter
           settings: () => settings,                // read by markers and the card
           card: { mount: cardEl },
           toolbar: { bar: barEl, rail: railEl },
           menu: { mount: menuEl, checks: { toggleLabels: () => settings.labels } },
           markers: { hover: () => ..., selected: () => ..., lookup: (type, id) => ... },
       });

       ui.reveal([lon, lat]);                      // after the card grew or the target moved
       ui.layout();                                // after a resize or a panel toggle
       new VectorLayer({ source, style: ui.markers.marker });

   Defaults: the map's free area comes from the --map-inset-* properties and
   the pills/toolbar as obstacles; a phone (max-width 500px, or max-height
   800px) shows the card full-bleed; the toolbar goes wide when settings().map_toolbar
   is "wide" and it fits; the card pins where settings() says.
   ========================================================================= */

import * as chromeLib from "./chrome.js";
import * as markersLib from "./markers.js";
import * as cardLib from "./card.js";
import * as toolbarLib from "./toolbar.js";
import * as menuLib from "./menu.js";
import { fromLonLat } from "ol/proj.js";

var PHONE = "(max-width: 500px), (max-height: 800px)";

/* --- keeping a target visible ---------------------------------------------
   reveal keeps one safety margin around everything: twice the selection
   ring (opts.margin in pixels, or opts.settings for the ring size), but never
   more than 10% of the free area, so a phone keeps its margin in proportion:
   a target clear of the card and of the borders by that margin stays put;
   one behind the card (or within the margin of it) is panned into the
   strip with most room, just far enough to clear the margin; one inside a
   border's margin is moved in that direction only, just far enough; one
   off screen lands in the middle of the biggest strip. A strip under 120px
   either way (a phone) is no place to pan to, so then nothing moves. */

var MIN_ROOM = 120;
var MARGIN_CAP = 0.1;

/* the breakpoints are tokens (tokens.css --bp-*); map.css repeats the numbers
   in its media and container queries, which cannot read custom properties */
function bp(name, dflt) {
    var v = parseFloat(getComputedStyle(document.documentElement).getPropertyValue(name));
    return isNaN(v) ? dflt : v;
}

/* a pane that cannot hold both a map and an expanded card keeps the card
   minimized; it is the pane's width that counts, so a side table counts */
export function paneCramped(pane) {
    pane = pane || document.getElementById("map");
    var w = pane && pane.clientWidth ? pane.clientWidth : window.innerWidth;
    return window.innerHeight <= bp("--bp-cramped-height", 1250) && w <= bp("--bp-narrow", 500);
}

/* pans so that map pixel `from` ends up at map pixel `to` (default: centre) */
function panToOn(map, from, to) {
    var view = map.getView();
    var size = map.getSize();
    var dest = to || [size[0] / 2, size[1] / 2];
    var res = view.getResolution();
    var c = view.getCenter();
    view.animate({
        center: [c[0] + (from[0] - dest[0]) * res, c[1] - (from[1] - dest[1]) * res],
        duration: 1000,
    });
}

/* put map coordinate `coord` at pixel `dest` while flying to `zoom` */
/* a map shown a moment ago has no frame to ask for a pixel; the view's
   centre and resolution give the same answer */
function pixelFromView(map, coord, size) {
    var view = map.getView();
    var c = view.getCenter(), res = view.getResolution();
    if (!c || !res) return null;
    return [size[0] / 2 + (coord[0] - c[0]) / res, size[1] / 2 - (coord[1] - c[1]) / res];
}

function flyToOn(map, coord, dest, zoom) {
    var view = map.getView();
    var size = map.getSize();
    var res = view.getResolutionForZoom(zoom);
    view.cancelAnimations();
    view.animate({
        center: [coord[0] + (size[0] / 2 - dest[0]) * res, coord[1] - (size[1] / 2 - dest[1]) * res],
        zoom: zoom,
        duration: 600,
    });
}

/* a chrome free box as the rectangle reveal() works in */
export function freeRect(chrome, parent) {
    var box = chrome.freeBox(parent);
    return { left: box.left, top: box.top, right: box.left + box.width, bottom: box.top + box.height };
}

function center(s) { return [(s.left + s.right) / 2, (s.top + s.bottom) / 2]; }

/* rectangle `s` inset by `m` = { x, y } on every side (negative m grows it) */
function inset(s, m) {
    return { left: s.left + m.x, top: s.top + m.y, right: s.right - m.x, bottom: s.bottom - m.y };
}

/* the nearest point to `px` inside `s`: moves only along the axes that need it */
function clampInto(px, s) {
    return [Math.min(Math.max(px[0], s.left), s.right), Math.min(Math.max(px[1], s.top), s.bottom)];
}
function biggest(strips) { return strips.reduce(function (a, s) { return area(s) > area(a) ? s : a; }); }
function roomy(s) { return s.right - s.left >= MIN_ROOM && s.bottom - s.top >= MIN_ROOM; }

/* the card's rectangle in map pixels, or null without a card */
function cardRect(map, card) {
    if (!card || !card.offsetWidth) return null;
    var m = map.getTargetElement().getBoundingClientRect();
    var c = card.getBoundingClientRect();
    return { left: c.left - m.left, top: c.top - m.top, right: c.right - m.left, bottom: c.bottom - m.top };
}

/* the rectangles the card leaves free within `whole` (the map less the
   pills and strips), so a target behind the chrome counts as hidden too */
function freeStrips(cr, w) {
    if (!cr) return [w];
    return [
        { left: Math.max(cr.right, w.left), top: w.top, right: w.right, bottom: w.bottom },
        { left: w.left, top: w.top, right: Math.min(cr.left, w.right), bottom: w.bottom },
        { left: w.left, top: Math.max(cr.bottom, w.top), right: w.right, bottom: w.bottom },
        { left: w.left, top: w.top, right: w.right, bottom: Math.min(cr.top, w.bottom) },
    ];
}

function area(s) {
    return Math.max(0, s.right - s.left) * Math.max(0, s.bottom - s.top);
}

function inside(px, s) {
    return px[0] >= s.left && px[0] <= s.right && px[1] >= s.top && px[1] <= s.bottom;
}

export function reveal(map, lonLat, card, opts) {
    opts = opts || {};
    var size = map.getSize();
    if (!size) return null;
    var viewport = { left: 0, top: 0, right: size[0], bottom: size[1] };
    var whole = opts.free || viewport;
    var cr = cardRect(map, card);
    var strips = freeStrips(cr, whole);
    var best = biggest(strips);
    var coord = fromLonLat(lonLat);

    /* from further out, fly to the target at the minimum zoom */
    if (opts.minZoom != null && map.getView().getZoom() < opts.minZoom) {
        flyToOn(map, coord, roomy(best) ? center(best) : center(whole), opts.minZoom);
        return "panned";
    }

    var px = opts.pixel || map.getPixelFromCoordinate(coord) || pixelFromView(map, coord, size);
    if (!px) return null;
    var mg = opts.margin != null ? opts.margin : 2 * markersLib.selectionRadius(opts.settings);
    var m = { x: Math.min(mg, (whole.right - whole.left) * MARGIN_CAP), y: Math.min(mg, (whole.bottom - whole.top) * MARGIN_CAP) };
    var safe = inset(whole, m);                       // clear of the borders
    var covered = cr && inset(cr, { x: -m.x, y: -m.y }); // the card and its margin
    var hidden = !!covered && inside(px, covered);
    if (!hidden && inside(px, safe)) return null;
    if (!roomy(best)) return null;

    var intoBest = function () { return clampInto(px, inset(best, m)); };

    var dest;
    if (!inside(px, viewport)) dest = center(best);          // off screen
    else if (hidden) dest = intoBest();                      // behind the card
    else {
        dest = clampInto(px, safe);                          // inside a border margin
        if (covered && inside(dest, covered)) dest = intoBest();
    }
    panToOn(map, px, dest);
    return "panned";
}

export function create(opts) {
    var getMap = typeof opts.map === "function" ? opts.map : function () { return opts.map; };
    var settings = opts.settings || function () { return {}; };
    var phone = typeof matchMedia === "function" ? matchMedia(PHONE) : { matches: false };
    var fullBleed = opts.fullBleed || function () { return phone.matches; };

    /* --- chrome: where the free map is --------------------------------------- */

    var chrome = chromeLib.create(Object.assign({
        obstacles: [
            { selector: ".map-button-box.map-pill:not(.map-bottom)", edge: "right" },
            { selector: ".map-button-box.map-bottom", edge: "right" },
            { selector: ".map-toolbar", edge: "bottom" },
        ],
    }, opts.chrome || {}));

    /* the map pane already ends where a side panel begins */
    function freeWidth() {
        var map = getMap();
        var size = map ? map.getSize() : null;
        return size && size[0] > 0 ? size[0] : window.innerWidth;
    }

    /* --- markers ---------------------------------------------------------------- */

    var markers = markersLib.create(Object.assign({ settings: settings }, opts.markers || {}));

    /* --- card ------------------------------------------------------------------- */

    var card = null;
    if (opts.card && opts.card.mount) {
        card = cardLib.create(Object.assign({}, opts.card, {
            layout: Object.assign({
                chrome: chrome,
                fullBleed: fullBleed,
                topInset: function () { return fullBleed() ? 0 : chrome.inset("top"); },
                viewSize: function () { var m = getMap(); return (m && m.getSize()) || [window.innerWidth, window.innerHeight]; },
                freeWidth: freeWidth,
            }, opts.card.layout || {}),
        }));
    }

    /* --- toolbar ---------------------------------------------------------------- */

    var toolbar = null;
    if (opts.toolbar && opts.toolbar.bar && opts.toolbar.rail) {
        toolbar = toolbarLib.create(Object.assign({
            wantWide: function () { return settings().map_toolbar === "wide"; },
            freeWidth: freeWidth,
            reserve: function () { return chrome.px("--size-map-controls", 44) + chrome.gap() * 2; },
            onChange: function () { chrome.invalidate(); },
        }, opts.toolbar));
    }

    /* --- menu ------------------------------------------------------------------- */

    var menu = opts.menu && opts.menu.mount ? menuLib.create(opts.menu) : null;

    /* --- the stack of things that can be open ------------------------------------
       Ordered top-down: [{ isOpen, close, stage }]. Escape closes the topmost
       open one; a card taking the stage closes every entry marked stage
       (menus, panels, drawers). An entry whose isOpen is true but has no
       close (a modal that closes itself) simply stops the ladder. */

    var stack = opts.stack || [];

    function dismiss() {
        stack.forEach(function (e) { if (e.stage && e.isOpen()) e.close(); });
    }

    if (stack.length && typeof document !== "undefined") {
        document.addEventListener("keydown", function (ev) {
            if (ev.key !== "Escape") return;
            for (var i = 0; i < stack.length; i++) {
                if (!stack[i].isOpen()) continue;
                if (stack[i].close) stack[i].close();
                return;
            }
        });
    }

    /* --- the moves a host makes ------------------------------------------------ */

    function revealTarget(lonLat, pixel, opts) {
        var free = freeRect(chrome, getMap().getTargetElement());
        return reveal(getMap(), lonLat, card && card.el, Object.assign({ pixel: pixel, free: free, settings: settings() }, opts || {}));
    }

    function panTo(pixel, to) {
        panToOn(getMap(), pixel, to);
    }

    /* After a resize or a panel toggle: the chrome moved. */
    function layout() {
        chrome.invalidate();
        if (toolbar) toolbar.apply();
        if (card) card.fit();
    }

    return {
        chrome: chrome,
        markers: markers,
        card: card,
        toolbar: toolbar,
        menu: menu,
        reveal: revealTarget,
        dismiss: dismiss,
        panTo: panTo,
        layout: layout,
    };
}

/* The map pane animates its width when a side panel opens; OpenLayers only
   learns about a new size when told, so keep telling it until the slide ends. */
export function trackResize(map, ms) {
    var until = Date.now() + (ms || 400);
    var step = function () {
        map.updateSize();
        if (Date.now() < until) requestAnimationFrame(step);
    };
    requestAnimationFrame(step);
}
