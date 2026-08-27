/* ============================================================================
   card.js — the shell of a map card (layer 2)

   A card is a header, a column of sections made of rows, and a footer of
   actions. This owns that structure and its behaviour: sections open and
   close, a footer with more items than fit pages through them, a popover
   anchors to an icon and stays inside the card. What the rows say is the
   host's: it builds them from a spec, or hands over markup it wrote itself,
   and writes into the cells. Nothing here knows what a ship or a plane is.

       const card = AISCard.create({ mount: document.getElementById("targetcard") });

       const cells = card.rows(container, [
           { section: { key: "voyage", label: "Voyage", open: true },
             fields: [{ key: "dest", label: "Destination" }, { key: "eta", label: "ETA" }] },
       ], { group: "ship" });
       cells.dest.textContent = "ROTTERDAM";

       card.footer.add({ icon: "route", label: "Track", title: "Show track",
                         action: "showTrack", group: "ship" });
       card.footer.show("ship");

   Sections are addressed by key; every element carrying data-section=key
   belongs to it, so rows written by hand in HTML take part the same way as
   rows built here. The card reports through DOM events on `mount`:
   "card:section" { key, open } when a section opens or closes,
   "card:pin" { x, y } / "card:unpin" when the user pins it.

   Where the card sits is the second half, `layout`: beside a point on the
   map, docked top-left, pinned where the user left it, or full-bleed on a
   phone. The host describes its furniture; the shell does the geometry.

       const card = AISCard.create({ mount, layout: {
           chrome,                                   // a chrome instance
           fullBleed: () => mq.matches,              // phone: card takes the screen
           topInset: () => chrome.inset("top"),      // where the map's free area starts
           dockOffset: () => 0,                      // extra top offset when docked top-left
           viewSize: () => map.getSize(),
           freeWidth: () => chrome.freeBox(mapEl).width,
           reserveRight: () => 30,                   // room to keep free beside the card
           preferDock: () => settings.card_top_left,
           pinned: () => settings.pin ? { x, y } : null,
       }});
       card.place(pixel);      // on open
       card.fit();             // after content or window changes
   ========================================================================= */

import { fieldRows, dismissOnce, draggable as componentDraggable } from "./components.js";
import { clamp } from "./chrome.js";

var DEFAULT_CLASSES = {
    section: "card-section",
    caretChip: "card-caret-chip",
    caret: "card-section-caret",
    row: "mapcard-content-row card-row",
    maxOnly: "card-max-only",
    collapsed: "card-collapsed",
    empty: "card-empty",
    iconUnavailable: "card-icon-unavailable",
    popover: "card-popover",
    max: "card-max",
    minOnly: "card-min-only",
    rowSelected: "card-row-selected",
};

var CARET_OPEN = "keyboard_arrow_up_icon";
var CARET_CLOSED = "keyboard_arrow_down_icon";

export function create(opts) {
    if (!opts || !opts.mount) throw new Error("card.create needs a mount");

    var mount = opts.mount;
    var cls = Object.assign({}, DEFAULT_CLASSES, opts.classes || {});
    var keepRows = opts.keepRows || null;
    var iconMax = opts.iconMax || 3;
    var sectionAction = opts.sectionAction || "toggleSection";
    var open = {};          // section key -> bool

    function content() {
        return mount.querySelector(".mapcard-content") || mount;
    }

    function footerEl() {
        return mount.querySelector(".mapcard-footer");
    }

    /* --- sections ------------------------------------------------------- */

    function applySection(key) {
        var on = !!open[key];
        mount.querySelectorAll('[data-section="' + key + '"]').forEach(function (el) {
            if (el.classList.contains(cls.section)) {
                var caret = el.querySelector("." + cls.caret);
                if (caret) {
                    caret.classList.toggle(CARET_OPEN, on);
                    caret.classList.toggle(CARET_CLOSED, !on);
                }
                return;
            }
            el.classList.toggle(cls.collapsed, !on);
        });
    }

    function setSection(key, on) {
        if (!key) return;
        var was = !!open[key];
        open[key] = !!on;
        applySection(key);
        if (was !== !!on) mount.dispatchEvent(new CustomEvent("card:section", { detail: { key: key, open: !!on } }));
    }

    function toggleSection(key) { setSection(key, !open[key]); }

    function setSectionAvailable(key, available) {
        mount.querySelectorAll('[data-section="' + key + '"]').forEach(function (el) {
            if (el.classList.contains(cls.section)) el.classList.toggle(cls.empty, !available);
            else el.classList.toggle(cls.collapsed, !available || !open[key]);
        });
    }

    /* Sets every section at once, from defaults or a saved state; keys not
       mentioned keep what they had. */
    function resetSections(state) {
        Object.keys(state || {}).forEach(function (k) { open[k] = !!state[k]; });
        Object.keys(open).forEach(applySection);
    }

    /* --- rows --------------------------------------------------------------- */

    /* Builds sections and rows from a spec into `container` (default: the
       content column) and returns the value cells by key. Rows without a
       section belong to the one before them. */
    function rows(container, spec, o) {
        o = o || {};
        var last = null;
        spec.forEach(function (row) {
            if (row.section) {
                last = row.section.key;
                if (!(last in open)) open[last] = !!row.section.open;
            } else if (last && !row.sectionKey) row.sectionKey = last;
        });

        var attrs = Object.assign({}, o.attrs || {});
        if (o.group) attrs["data-context-type"] = o.group;

        var cells = fieldRows(container || content(), spec, {
            rowClass: [cls.row, o.rowClass].filter(Boolean).join(" "),
            attrs: attrs,
            idPrefix: o.idPrefix,
            sectionClass: cls.section + " " + cls.maxOnly,
            sectionAction: sectionAction,
            caretChipClass: cls.caretChip,
            caretClass: cls.caret,
            headId: o.headId,
        });

        spec.forEach(function (row) { if (row.section) applySection(row.section.key); });
        return cells;
    }

    /* --- popovers ----------------------------------------------------------- */

    function closePopovers() {
        mount.querySelectorAll("." + cls.popover).forEach(function (el) { el.style.display = "none"; });
    }

    /* Shows `popover` hanging off `anchor`, flipped to stay on screen and
       clamped to the card. A second call on an open popover closes it. */
    function togglePopover(popover, anchor) {
        var wasOpen = popover.style.display === "block";
        closePopovers();
        if (wasOpen) return;

        var a = anchor.getBoundingClientRect();
        var c = mount.getBoundingClientRect();

        popover.style.display = "block";
        var w = popover.offsetWidth, h = popover.offsetHeight;

        var left = a.left - c.left + 20;
        var top = a.bottom - c.top + 5;
        if (a.left + w + 20 > window.innerWidth) left = a.right - c.left - w - 5;
        if (a.bottom + h + 5 > window.innerHeight) top = a.top - c.top - h - 5;

        var at = clamp({ left: 5, top: 5, width: c.width - 10, height: Infinity }, w, h, left, top);
        popover.style.left = at.left + "px";
        popover.style.top = at.top + "px";

        dismissOnce(popover, null, { keepInside: true, capture: true, swallow: true });
    }

    /* --- footer ------------------------------------------------------------- */

    var offset = {};        // group -> first visible index
    var count = {};         // group -> available items
    var available = opts.itemAvailable || function () { return true; };

    function isMore(el) { return el.dataset.more === "1"; }

    function itemShown(el, group) {
        if (el.dataset.contextType !== group) return false;
        if (el.classList.contains(cls.iconUnavailable)) return false;
        return available(el, group);
    }

    /* Appends an action. `icon` is the icon class stem (route -> route_icon);
       `action` is a data-action name the host dispatches on; `more` marks
       the paging item, which is shown only when there is something to page. */
    function addItem(item) {
        var foot = footerEl();
        if (!foot) return null;

        var div = document.createElement("div");
        div.title = item.title || "";
        div.dataset.contextType = item.group || "default";
        if (item.id) div.id = item.id;
        if (item.more) div.dataset.more = "1";
        if (item.action) div.dataset.action = item.action;

        var i = document.createElement("i");
        i.className = (item.icon || "question_mark") + "_icon";
        var span = document.createElement("span");
        span.textContent = item.label || "";
        div.appendChild(i);
        div.appendChild(span);

        foot.appendChild(div);
        return div;
    }

    function showGroup(group) {
        var foot = footerEl();
        if (!foot) return;
        var items = Array.prototype.slice.call(foot.children);
        var more = items.filter(function (el) { return isMore(el) && el.dataset.contextType === group; });
        var shown = items.filter(function (el) { return !isMore(el) && itemShown(el, group); });

        count[group] = shown.length;
        if (!(group in offset) || offset[group] >= shown.length) offset[group] = 0;

        var paged = shown.length > iconMax;
        items.forEach(function (el) { el.style.display = paged && more.indexOf(el) >= 0 ? "flex" : "none"; });
        shown.slice(offset[group], offset[group] + iconMax).forEach(function (el) { el.style.display = "flex"; });
    }

    function rotate(group) {
        offset[group] = (offset[group] || 0) + iconMax;
        if (offset[group] >= (count[group] || 0)) offset[group] = 0;
        showGroup(group);
    }

    function resetFooter() {
        offset = {};
        count = {};
    }

    /* --- layout --------------------------------------------------------------- */

    var lay = opts.layout || {};
    var chrome = lay.chrome || null;
    var fullBleed = lay.fullBleed || function () { return false; };
    var topInset = lay.topInset || function () { return chrome ? chrome.inset("top") : 10; };
    var dockOffset = lay.dockOffset || function () { return 0; };
    var viewSize = lay.viewSize || function () { return [window.innerWidth, window.innerHeight]; };
    var freeWidth = lay.freeWidth || function () { return viewSize()[0]; };
    var reserveRight = lay.reserveRight || function () { return 30; };
    var preferDock = lay.preferDock || function () { return false; };
    var pinned = lay.pinned || function () { return null; };
    var aside = lay.aside || "beside";                     // "beside" | "alongCourse"
    var angle = lay.angle || function () { return 0; };   // alongCourse: the target's course, degrees
    var besideMargin = lay.margin != null ? lay.margin : 35;
    var minWidth = lay.minWidth || 280;

    function gap() { return chrome ? chrome.gap() : 10; }

    function pinActive() {
        var p = pinned();
        return !!p && p.x != null && p.y != null && !fullBleed();
    }

    function parentSize() {
        var parent = mount.offsetParent;
        return [parent ? parent.clientWidth : window.innerWidth, parent ? parent.clientHeight : window.innerHeight];
    }

    function clampToFree(left, top) {
        if (!chrome) return { left: left, top: top };
        var box = chrome.freeBox(mount.offsetParent);
        return chrome.clamp(box, mount.offsetWidth, mount.offsetHeight, left, top);
    }

    /* moves the card; a null coordinate is left where it is */
    function setPos(left, top) {
        if (left != null) mount.style.left = left + "px";
        if (top != null) mount.style.top = top + "px";
    }

    function floatAt(left, top) {
        setPos(left, top);
        mount.classList.add("floating");
    }

    /* a card against an edge loses the rounded corners on that side */
    function flushCorners() {
        var ps = parentSize();   // all reads first: each toggle would force a new layout
        var l = mount.offsetLeft, t = mount.offsetTop, r = l + mount.offsetWidth, b = t + mount.offsetHeight;
        mount.classList.toggle("flush-left", l <= 0);
        mount.classList.toggle("flush-top", t <= 0);
        mount.classList.toggle("flush-right", r >= ps[0]);
        mount.classList.toggle("flush-bottom", b >= ps[1]);
    }

    var placedHow = null;   // "pinned" | "beside" | "dock" — what fit() may move

    /* the stylesheet docks the card (flush in the corner, or with the map gap
       on a roomy pane): no inline position */
    function dock() {
        mount.style.left = "";
        mount.style.top = "";
        mount.classList.remove("floating");
    }

    /* where the card sits beside a target at `pixel`, or null if it cannot:
       centred on it ("beside") or leaning away along its course ("alongCourse") */
    function beside(pixel) {
        if (preferDock() || !pixel || !chrome) return null;
        var vs = viewSize();
        var rect = mount.getBoundingClientRect();
        if (aside === "alongCourse") {
            var free = chrome.freeBox(mount.offsetParent);
            var m = besideMargin;
            var fitsBeside = (free.left + free.width) - (pixel[0] + rect.width + m + reserveRight()) > 0 ||
                             (pixel[0] - free.left) - (rect.width + m) > 0;
            if (!fitsBeside || free.height <= rect.height + 2 * m) return null;
            var box = { left: free.left + m, top: free.top + m, width: free.width - 2 * m, height: free.height - 2 * m };
            return chrome.alongCourse(box, rect.width, rect.height, pixel, m, angle(pixel));
        }
        return chrome.beside({ width: vs[0], height: vs[1] }, rect.width, rect.height, pixel,
            { margin: besideMargin, reserveRight: reserveRight(), topInset: topInset(),
              bottomInset: chrome.inset("bottom") });
    }

    /* Puts the card where it belongs for a target at `pixel` (parent-relative;
       may be undefined). Returns "pinned" | "dock" | "beside". */
    function place(pixel) {
        if (!mount.offsetParent) return null;

        if (pinActive()) {
            var p = pinned();
            var at = clampToFree(p.x, p.y);
            floatAt(at.left, at.top);
            placedHow = "pinned";
        } else {
            mount.style.left = "";
            mount.style.top = "";
            mount.classList.remove("floating");

            var b = beside(pixel);
            if (b) {
                floatAt(b.left, b.top);
                placedHow = "beside";
            } else {
                dock();
                placedHow = "dock";
            }
        }

        flushCorners();
        return placedHow;
    }

    /* Keeps the card inside the free area after its content or the window
       changed: caps its size, pulls it up when it overflows, takes the whole
       height when it cannot fit. */
    function fit() {
        if (!mount.offsetParent) return;

        var bleed = fullBleed();
        var margin = bleed ? 0 : gap();
        var ps = parentSize();
        var top = bleed ? 0 : topInset();
        var bottom = ps[1] - margin;
        var available = bottom - top;

        mount.style.maxHeight = available + "px";
        if (placedHow === "dock") {          // the stylesheet positions and sizes a docked card
            mount.style.maxWidth = "";
            return;
        }
        mount.style.maxWidth = Math.max(minWidth, freeWidth() - margin * 2) + "px";

        if (mount.offsetHeight >= available) {
            /* taller than the free area: take all of it */
            floatAt(pinActive() ? null : Math.max(0, Math.min(margin, ps[0] - mount.offsetWidth)), top);
        } else {
            var overflow = mount.offsetTop + mount.offsetHeight - bottom;
            if (overflow > 0) setPos(null, Math.max(top, mount.offsetTop - overflow));
            if (bleed && placedHow === "dock") setPos(0, top);
            if (pinActive()) {
                var at = position();
                var c = clampToFree(at.x, at.y);
                setPos(c.left, c.top);
            }
        }

        flushCorners();
    }

    function position() {
        return { x: parseInt(mount.style.left) || 0, y: parseInt(mount.style.top) || 0 };
    }

    function applyPinStyle(on) {
        mount.classList.toggle("pinned", !!on);
        var handle = mount.querySelector(".draggable");
        if (handle) handle.classList.toggle("opacity-50", !!on);
    }

    function pin() {
        var at = position();
        applyPinStyle(true);
        mount.dispatchEvent(new CustomEvent("card:pin", { detail: at }));
        return at;
    }

    function unpin() {
        applyPinStyle(false);
        mount.dispatchEvent(new CustomEvent("card:unpin"));
    }

    /* `handle` drags the card; "card:moved" { x, y } follows a drag */
    function draggable(handle) {
        componentDraggable(handle, mount);
        mount.addEventListener("moved", function (e) {
            mount.dispatchEvent(new CustomEvent("card:moved", { detail: { x: e.detail.left, y: e.detail.top } }));
        });
    }

    /* --- min / max ---------------------------------------------------------------
       A maximised card shows every row; a minimised one only the rows the
       host marked (.card-row-selected) and the .card-min-only elements. */

    function isMax() { return mount.classList.contains(cls.max || "card-max"); }

    function setMax(on) {
        on = !!on;
        if (on === isMax()) return;
        mount.classList.toggle(cls.max || "card-max", on);
        mount.querySelectorAll("." + cls.minOnly).forEach(function (el) { el.classList.toggle("visible", !on); });
        mount.querySelectorAll("." + cls.maxOnly).forEach(function (el) { el.classList.toggle("hidden", !on); });
        markRows();
        var btn = mount.querySelector(".card-minmax");
        if (btn) {
            btn.classList.toggle(CARET_CLOSED, !on);
            btn.classList.toggle(CARET_OPEN, on);
        }
        if (placedHow && mount.offsetParent) fit();   // the card changed height: keep it on the pane
    }

    /* --- kept rows -------------------------------------------------------------
       A minimised card keeps the rows without cls.maxOnly; while maximised those
       carry cls.rowSelected as the mark. The rows are the host's `keepRows`
       selector, each identified by the first id inside it, so a saved list
       survives rows being added or moved, and unknown keys are ignored. */

    function keptRows() {
        return keepRows ? Array.prototype.slice.call(mount.querySelectorAll(keepRows)) : [];
    }

    function rowKey(row) {
        var el = row.querySelector("[id]");
        return el ? el.id : "";
    }

    function markRows() {
        var max = isMax();
        keptRows().forEach(function (row) {
            row.classList.toggle(cls.rowSelected, max && !row.classList.contains(cls.maxOnly));
        });
    }

    /* a click on a row: the maximised card toggles whether the row is kept and
       answers true; the minimised one answers false so the host can maximise */
    function keepToggle(row) {
        if (!isMax()) return false;
        row.classList.toggle(cls.maxOnly);
        markRows();
        return true;
    }

    function keepState() {
        return keptRows()
            .filter(function (row) { return !row.classList.contains(cls.maxOnly); })
            .map(rowKey).filter(Boolean);
    }

    /* applies a saved list; one that names none of the current rows is
       ignored, so a stale save cannot flip the rows the wrong way round */
    function keepRestore(keys) {
        if (!Array.isArray(keys)) return false;
        var set = {};
        keys.forEach(function (k) { if (typeof k === "string") set[k] = true; });
        var rows = keptRows();
        if (!rows.some(function (row) { return set[rowKey(row)]; })) return false;
        rows.forEach(function (row) {
            var k = rowKey(row);
            if (k) row.classList.toggle(cls.maxOnly, !set[k]);
        });
        markRows();
        return true;
    }

    /* places the card for a target at `pixel` and fits it to the pane;
       returns how it was placed (`open` itself is the sections' state) */
    function openCard(pixel) {
        var how = place(pixel);
        fit();
        return how;
    }

    /* runs `fn` (which may rewrite content) and puts the scroll position back:
       a refresh must not move a card the user has scrolled */
    function keepScroll(fn) {
        var c = content();
        var top = c.scrollTop;
        var r = fn();
        c.scrollTop = top;
        return r;
    }

    return {
        el: mount,
        rows: rows,
        keepScroll: keepScroll,
        place: place,
        open: openCard,
        fit: fit,
        pin: pin,
        unpin: unpin,
        markPinned: applyPinStyle,
        draggable: draggable,
        isMax: isMax,
        setMax: setMax,
        keep: { toggle: keepToggle, state: keepState, restore: keepRestore, mark: markRows },
        section: {
            toggle: toggleSection,
            open: function (key) { if (!open[key]) setSection(key, true); },
            available: setSectionAvailable,
            reset: resetSections,
        },
        popover: { toggle: togglePopover, closeAll: closePopovers },
        footer: { add: addItem, show: showGroup, rotate: rotate, reset: resetFooter },
    };
}
