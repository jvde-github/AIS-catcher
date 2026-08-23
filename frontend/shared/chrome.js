/* ============================================================================
   chrome.js — where a floating panel may sit (layer 2)

   An ES module, like components.js.

   The host declares what occupies each edge in CSS, as registered <length>
   properties on the document body:

   :root            { --map-inset-top: var(--size-map-gap); }
   body.table-open  { --map-inset-right: calc(var(--size-map-gap) + var(--size-panel)); }

   so a frontend with a ticker, a nav bar or nothing at all says so in its own
   stylesheet, and nothing here needs to know which furniture exists.

   Controls that float inside that box rather than reserving an edge are named
   through configure({ obstacles }), and freeBox() reports what is left. The
   placers below work off that one box, so a tooltip and a panel can never
   disagree about where the map ends.
   ========================================================================= */

/* --- placement: geometry only ----------------------------------------------
   Which side is preferred, whether a panel is pinned and what a host does
   when nothing fits are policy, and stay with the host. */

export function clamp(box, w, h, left, top) {
    var maxLeft = Math.max(box.left, box.left + box.width - w);
    var maxTop = Math.max(box.top, box.top + box.height - h);
    return {
        left: Math.min(Math.max(left, box.left), maxLeft),
        top: Math.min(Math.max(top, box.top), maxTop),
    };
}

/* Leans away from `at` along `angle` so the label clears the target's own
   track, tries the far side when that lands outside, then clamps. */
export function alongCourse(box, w, h, at, distance, angle) {
    var a = angle || 0;

    function candidate(deg) {
        var rad = (deg + 45) * Math.PI / 180;
        var s = Math.sin(rad);
        var c = -Math.cos(rad);
        return {
            left: at[0] + distance * s + (s < 0 ? -w : 0),
            top: at[1] + distance * c + (c < 0 ? -h : 0),
        };
    }

    var p = candidate(a);
    if (p.left > box.left + box.width - w || p.left < box.left ||
        p.top > box.top + box.height - h || p.top < box.top) p = candidate(a + 180);

    return clamp(box, w, h, p.left, p.top);
}

/* Beside `at` on whichever side has room, centred on it vertically.
   Returns null when neither side fits, which is the host's cue to fall
   back to wherever it docks panels that cannot float. */
export function beside(view, w, h, at, opts) {
    opts = opts || {};
    var margin = opts.margin || 0;
    var rightSpace = view.width - (at[0] + w + margin + (opts.reserveRight || 0));
    var leftSpace = at[0] - (w + margin);

    if (!(rightSpace > 0 || leftSpace > 0)) return null;
    if (!(view.height > h + 2 * margin)) return null;

    var minTop = Math.max(margin, opts.topInset || 0);
    var top = Math.max(minTop, Math.min(view.height - h - margin, at[1] - h / 2));
    var left = rightSpace >= 0 ? at[0] + margin
        : leftSpace >= 0 ? at[0] - w - margin
            : Math.max(0, (view.width - w) / 2);

    return { left: left, top: top };
}

/* --- one map, one chrome ----------------------------------------------------
   Everything that depends on which furniture a page has lives on an instance,
   so two maps on one page can each describe their own. */

export function create(opts) {
    var fallback = (opts && opts.fallback) || null;   // host hook for browsers without @property support
    var obstacles = (opts && opts.obstacles) || [];   // [{ selector, edge }] floating over the free box

    var EDGES = ["top", "right", "bottom", "left"];
    var freeCache = [];    // one entry per parent asked about, cleared together

    function readInset(style, edge) {
        var px = parseFloat(style.getPropertyValue("--map-inset-" + edge));
        if (!isNaN(px)) return px;
        return fallback ? fallback(edge) : 0;
    }

    // an unregistered property comes back as the unresolved calc(), so the host
    // is asked to rebuild the number from the same facts its CSS derives it from
    function inset(edge, el) {
        return readInset(getComputedStyle(el || document.body), edge);
    }

    function insets(el) {
        var style = getComputedStyle(el || document.body);
        var out = {};
        EDGES.forEach(function (edge) { out[edge] = readInset(style, edge); });
        return out;
    }

    var pxCache = {};

    function px(name, dflt) {
        if (!(name in pxCache)) {
            var v = parseFloat(getComputedStyle(document.documentElement).getPropertyValue(name));
            pxCache[name] = isNaN(v) ? null : v;
        }
        return pxCache[name] === null ? dflt : pxCache[name];
    }

    function gap() {
        return px("--size-map-gap", 10);
    }

    // the box a panel may occupy inside `parent`, in parent-relative coordinates
    function availableBox(parent, el) {
        var w = parent ? parent.clientWidth : window.innerWidth;
        var h = parent ? parent.clientHeight : window.innerHeight;
        var i = insets(el);
        return {
            left: i.left,
            top: i.top,
            width: Math.max(0, w - i.left - i.right),
            height: Math.max(0, h - i.top - i.bottom),
        };
    }

    /* availableBox() less the controls floating inside it. Recomputed only when
       the chrome moves: hover asks for this once per feature the pointer
       crosses, and the answer changes on resize and panel toggles only. */
    function freeBox(parent, el) {
        for (var i = 0; i < freeCache.length; i++) {
            if (freeCache[i].parent === parent && freeCache[i].el === el) return freeCache[i].box;
        }

        var box = availableBox(parent, el);
        var g = gap();
        var base = parent && parent.getBoundingClientRect
            ? parent.getBoundingClientRect() : { left: 0, top: 0 };
        var right = box.left + box.width;
        var bottom = box.top + box.height;

        obstacles.forEach(function (o) {
            var node = document.querySelector(o.selector);
            if (!node || !node.offsetParent) return;
            var r = node.getBoundingClientRect();
            if (!r.width || !r.height) return;

            if (o.edge === "right") right = Math.min(right, r.left - base.left - g);
            else if (o.edge === "bottom") bottom = Math.min(bottom, r.top - base.top - g);
            else if (o.edge === "left") box.left = Math.max(box.left, r.right - base.left + g);
            else if (o.edge === "top") box.top = Math.max(box.top, r.bottom - base.top + g);
        });

        box.width = Math.max(0, right - box.left);
        box.height = Math.max(0, bottom - box.top);
        freeCache.push({ parent: parent, el: el, box: box });
        return box;
    }

    /* --- placement ---------------------------------------------------------

       Geometry only. Which side is preferred, whether a panel is pinned and
       what a host does when nothing fits are policy, and stay with the host. */

    function invalidate() { freeCache = []; pxCache = {}; }

    return {
        inset: inset, px: px, gap: gap, freeBox: freeBox, invalidate: invalidate,
        clamp: clamp, alongCourse: alongCourse, beside: beside,
    };
}
