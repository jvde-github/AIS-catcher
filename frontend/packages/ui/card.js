/* ============================================================================
   card.js — the shell of a map card (layer 2)

   A card is a header, its content and a footer of actions. The content is the
   host's (card-tabs.js builds the tabbed layout); nothing here knows what a
   ship or a plane is.

       const card = AISCard.create({ mount: document.getElementById("targetcard") });

       card.footer.add({ icon: "route", label: "Track", title: "Show track",
                         action: "showTrack", group: "ship" });
       card.footer.show("ship");

   The card reports through DOM events on `mount`: "card:pin" { x, y } /
   "card:unpin" when the user pins it, "card:moved" { x, y } after a drag.

   The header is the host's markup: `.mapcard-title`, `.mapcard-flag` and a
   state class on `.mapcard-header`.

   Where the card sits is the second half, `layout`: beside a point on the
   map, docked top-left, pinned where the user left it, or full-bleed on a
   phone. The host describes its furniture; the shell does the geometry.

       const card = AISCard.create({ mount, layout: {
           chrome,                                   // a chrome instance
           fullBleed: () => mq.matches,              // phone: card takes the screen
           topInset: () => chrome.inset("top"),      // where the map's free area starts
           viewSize: () => map.getSize(),
           freeWidth: () => chrome.freeBox(mapEl).width,
           reserveRight: () => 30,                   // room to keep free beside the card
           preferDock: () => settings.card_top_left,
           pinned: () => settings.pin ? { x, y } : null,
       }});
       card.place(pixel);      // on open
       card.fit();             // after content or window changes
   ========================================================================= */

import { dismissOnce, draggable as componentDraggable, flagHTML } from "./components.js";
import { clamp } from "./chrome.js";

let ICON_UNAVAILABLE = "card-icon-unavailable";
let POPOVER = "card-popover";
let HEADER_STATES = ["card-validated", "card-not-validated", "card-warning", "card-dubious"];

export function create(opts) {
    if (!opts || !opts.mount) throw new Error("card.create needs a mount");

    let mount = opts.mount;
    let iconMax = opts.iconMax || 3;

    function content() {
        return mount.querySelector(".mapcard-content") || mount;
    }

    function footerEl() {
        return mount.querySelector(".mapcard-footer");
    }

    /* --- header ------------------------------------------------------------- */

    let header = {
        title: function () { return mount.querySelector(".mapcard-title"); },
        flag: function (country, name) { mount.querySelector(".mapcard-flag").innerHTML = country ? flagHTML(country, "flag-card", name) : ""; },
        state: function (cls) {
            let el = mount.querySelector(".mapcard-header");
            HEADER_STATES.forEach(function (c) { el.classList.toggle(c, c === cls); });
        },
    };

    /* --- popovers ----------------------------------------------------------- */

    function closePopovers() {
        mount.querySelectorAll("." + POPOVER).forEach(function (el) { el.style.display = "none"; });
    }

    /* Shows `popover` hanging off `anchor`, flipped to stay on screen and
       clamped to the card. A second call on an open popover closes it. */
    function togglePopover(popover, anchor) {
        let wasOpen = popover.style.display === "block";
        closePopovers();
        if (wasOpen) return;

        let a = anchor.getBoundingClientRect();
        let c = mount.getBoundingClientRect();

        popover.style.display = "block";
        let w = popover.offsetWidth, h = popover.offsetHeight;

        let left = a.left - c.left + 20;
        let top = a.bottom - c.top + 5;
        if (a.left + w + 20 > window.innerWidth) left = a.right - c.left - w - 5;
        if (a.bottom + h + 5 > window.innerHeight) top = a.top - c.top - h - 5;

        let at = clamp({ left: 5, top: 5, width: c.width - 10, height: Infinity }, w, h, left, top);
        popover.style.left = at.left + "px";
        popover.style.top = at.top + "px";

        dismissOnce(popover, null, { keepInside: true, capture: true, swallow: true });
    }

    /* --- footer ------------------------------------------------------------- */

    let offset = {};        // group -> first visible index
    let count = {};         // group -> available items
    let available = opts.itemAvailable || function () { return true; };

    function isMore(el) { return el.dataset.more === "1"; }

    function itemShown(el, group) {
        if (el.dataset.contextType !== group) return false;
        if (el.classList.contains(ICON_UNAVAILABLE)) return false;
        return available(el, group);
    }

    /* Appends an action. `icon` is the icon class stem (route -> route_icon);
       `action` is a data-action name the host dispatches on; `more` marks
       the paging item, which is shown only when there is something to page. */
    function addItem(item) {
        let foot = footerEl();
        if (!foot) return null;

        let div = document.createElement("div");
        div.title = item.title || "";
        div.dataset.contextType = item.group || "default";
        if (item.id) div.id = item.id;
        if (item.more) div.dataset.more = "1";
        if (item.action) div.dataset.action = item.action;

        let i = document.createElement("i");
        i.className = (item.icon || "question_mark") + "_icon";
        let span = document.createElement("span");
        span.textContent = item.label || "";
        div.appendChild(i);
        div.appendChild(span);

        foot.appendChild(div);
        return div;
    }

    function showGroup(group) {
        let foot = footerEl();
        if (!foot) return;
        let items = Array.prototype.slice.call(foot.children);
        let more = items.filter(function (el) { return isMore(el) && el.dataset.contextType === group; });
        let shown = items.filter(function (el) { return !isMore(el) && itemShown(el, group); });

        count[group] = shown.length;
        if (!(group in offset) || offset[group] >= shown.length) offset[group] = 0;

        let paged = shown.length > iconMax;
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

    let lay = opts.layout || {};
    let chrome = lay.chrome || null;
    let fullBleed = lay.fullBleed || function () { return false; };
    let topInset = lay.topInset || function () { return chrome ? chrome.inset("top") : 10; };
    let viewSize = lay.viewSize || function () { return [window.innerWidth, window.innerHeight]; };
    let freeWidth = lay.freeWidth || function () { return viewSize()[0]; };
    let reserveRight = lay.reserveRight || function () { return 30; };
    let preferDock = lay.preferDock || function () { return false; };
    let pinned = lay.pinned || function () { return null; };
    let aside = lay.aside || "beside";                     // "beside" | "alongCourse"
    let angle = lay.angle || function () { return 0; };   // alongCourse: the target's course, degrees
    let besideMargin = lay.margin != null ? lay.margin : 35;
    let minWidth = lay.minWidth || 280;

    function gap() { return chrome ? chrome.gap() : 10; }

    function pinActive() {
        let p = pinned();
        return !!p && p.x != null && p.y != null && !fullBleed();
    }

    function parentSize() {
        let parent = mount.offsetParent;
        return [parent ? parent.clientWidth : window.innerWidth, parent ? parent.clientHeight : window.innerHeight];
    }

    function clampToFree(left, top) {
        if (!chrome) return { left: left, top: top };
        let box = chrome.freeBox(mount.offsetParent);
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
        let ps = parentSize();   // all reads first: each toggle would force a new layout
        let l = mount.offsetLeft, t = mount.offsetTop, r = l + mount.offsetWidth, b = t + mount.offsetHeight;
        mount.classList.toggle("flush-left", l <= 0);
        mount.classList.toggle("flush-top", t <= 0);
        mount.classList.toggle("flush-right", r >= ps[0]);
        mount.classList.toggle("flush-bottom", b >= ps[1]);
    }

    let placedHow = null;   // "pinned" | "beside" | "dock" — what fit() may move

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
        let vs = viewSize();
        let rect = mount.getBoundingClientRect();
        if (aside === "alongCourse") {
            let free = chrome.freeBox(mount.offsetParent);
            let m = besideMargin;
            let fitsBeside = (free.left + free.width) - (pixel[0] + rect.width + m + reserveRight()) > 0 ||
                             (pixel[0] - free.left) - (rect.width + m) > 0;
            if (!fitsBeside || free.height <= rect.height + 2 * m) return null;
            let box = { left: free.left + m, top: free.top + m, width: free.width - 2 * m, height: free.height - 2 * m };
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
            let p = pinned();
            let at = clampToFree(p.x, p.y);
            floatAt(at.left, at.top);
            placedHow = "pinned";
        } else {
            mount.style.left = "";
            mount.style.top = "";
            mount.classList.remove("floating");

            let b = beside(pixel);
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

        let bleed = fullBleed();
        let margin = bleed ? 0 : gap();
        let ps = parentSize();
        let top = bleed ? 0 : topInset();
        let bottom = ps[1] - margin;
        let available = bottom - top;

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
            let overflow = mount.offsetTop + mount.offsetHeight - bottom;
            if (overflow > 0) setPos(null, Math.max(top, mount.offsetTop - overflow));
            if (pinActive()) {
                let at = position();
                let c = clampToFree(at.x, at.y);
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
        let handle = mount.querySelector(".draggable");
        if (handle) handle.classList.toggle("opacity-50", !!on);
    }

    function pin() {
        let at = position();
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

    /* places the card for a target at `pixel` and fits it to the pane */
    function openCard(pixel) {
        let how = place(pixel);
        fit();
        return how;
    }

    /* runs `fn` (which may rewrite content) and puts the scroll position back:
       a refresh must not move a card the user has scrolled */
    function keepScroll(fn) {
        let c = content();
        let top = c.scrollTop;
        let r = fn();
        c.scrollTop = top;
        return r;
    }

    return {
        el: mount,
        header: header,
        keepScroll: keepScroll,
        place: place,
        open: openCard,
        fit: fit,
        pin: pin,
        unpin: unpin,
        markPinned: applyPinStyle,
        draggable: draggable,
        popover: { toggle: togglePopover, closeAll: closePopovers },
        footer: { add: addItem, show: showGroup, rotate: rotate, reset: resetFooter },
    };
}
