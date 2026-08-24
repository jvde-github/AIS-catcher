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

       ui.showCard(pixel, [lon, lat]);             // place, fit, keep the target visible
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
   reveal: off screen or under the card, the map pans so the target sits in
   the middle of the biggest strip the card leaves free; a strip under 120px
   either way (a phone) is no place to pan to, so then nothing moves. */

var MIN_ROOM = 120;

/* pans so that map pixel `from` ends up at map pixel `to` (default: centre) */
function panToOn(map, from, to, duration) {
    var view = map.getView();
    var size = map.getSize();
    var dest = to || [size[0] / 2, size[1] / 2];
    var res = view.getResolution();
    var c = view.getCenter();
    view.animate({
        center: [c[0] + (from[0] - dest[0]) * res, c[1] - (from[1] - dest[1]) * res],
        duration: duration == null ? 1000 : duration,
    });
}

/* the rectangles the card leaves free: beside, above and below it */
function freeStrips(map, card) {
    var size = map.getSize();
    var whole = { left: 0, top: 0, right: size[0], bottom: size[1] };
    if (!card || !card.offsetWidth) return [whole];

    var m = map.getTargetElement().getBoundingClientRect();
    var c = card.getBoundingClientRect();
    var l = c.left - m.left, t = c.top - m.top, r = c.right - m.left, b = c.bottom - m.top;
    return [
        { left: r, top: 0, right: size[0], bottom: size[1] },
        { left: 0, top: 0, right: l, bottom: size[1] },
        { left: 0, top: b, right: size[0], bottom: size[1] },
        { left: 0, top: 0, right: size[0], bottom: t },
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
    var px = opts.pixel || map.getPixelFromCoordinate(fromLonLat(lonLat));
    if (!px || !size) return null;          // not rendered yet: nothing to measure against
    var strips = freeStrips(map, card);

    var onScreen = inside(px, { left: 0, top: 0, right: size[0], bottom: size[1] });
    if (onScreen && strips.some(function (s) { return inside(px, s); })) return null;

    var best = strips.reduce(function (a, s) { return area(s) > area(a) ? s : a; });
    if (best.right - best.left < MIN_ROOM || best.bottom - best.top < MIN_ROOM) return null;

    panToOn(map, px, [(best.left + best.right) / 2, (best.top + best.bottom) / 2], opts.duration);
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
                dockOffset: function () { return 0; },
                viewSize: function () { var m = getMap(); return (m && m.getSize()) || [window.innerWidth, window.innerHeight]; },
                freeWidth: freeWidth,
                reserveRight: function () { return 30; },
                preferDock: function () { return !!settings().targetcard_top_left; },
                pinned: function () {
                    var s = settings();
                    return s.targetcard_pinned ? { x: s.targetcard_pinned_x, y: s.targetcard_pinned_y } : null;
                },
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

    /* --- the moves a host makes ------------------------------------------------ */

    /* Opens the card for a target at map `pixel` (optional) and `lonLat`:
       places it, fits it, and moves the map if the target ended up hidden. */
    function showCard(pixel, lonLat) {
        if (!card) return null;
        var how = card.place(pixel);
        card.fit();
        if (lonLat && how !== "pinned") reveal(getMap(), lonLat, card.el, { pixel: pixel });
        return how;
    }

    function revealTarget(lonLat, pixel) {
        return reveal(getMap(), lonLat, card && card.el, { pixel: pixel });
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
        showCard: showCard,
        reveal: revealTarget,
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
