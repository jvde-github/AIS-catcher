/* ============================================================================
   components.js — shared UI behaviour (layer 2)

   Companion to components.css. Copied by build-html.sh to both served trees:
     dist/components.js              (viewer, served at /)
     dist/control/js/components.js   (hub, served at /control/)
   The two frames are separate documents on separate ports, so this is loaded
   as a plain script in each and exposes one global rather than being imported.

   Load before the frame's own script.
   ========================================================================= */

window.AISComponents = (function () {
    "use strict";

    var ARROW = {
        left: '<svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 -960 960 960" fill="currentColor"><path d="M560-240 320-480l240-240 56 56-184 184 184 184-56 56Z"/></svg>',
        right: '<svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 -960 960 960" fill="currentColor"><path d="M504-480 320-664l56-56 240 240-240 240-56-56 184-184Z"/></svg>'
    };

    /* Wraps a horizontally scrolling strip (a tab bar) in a container with
       fade-out arrow buttons at each end, shown only when there is more to
       scroll to. Works whether `nav` is already in the DOM or still detached.

       Returns { wrap, update } — call update() after changing the strip's
       contents, since neither scroll nor resize fires for that. */
    function tabScroller(nav, extraClass) {
        var wrap = document.createElement("div");
        wrap.className = "tab-scroll-wrap" + (extraClass ? " " + extraClass : "");

        if (nav.parentNode) nav.parentNode.insertBefore(wrap, nav);
        wrap.appendChild(nav);

        ["left", "right"].forEach(function (dir) {
            var btn = document.createElement("button");
            btn.type = "button";
            btn.className = "tab-scroll-btn " + dir;
            btn.tabIndex = -1;
            btn.setAttribute("aria-hidden", "true");
            btn.innerHTML = ARROW[dir];
            btn.addEventListener("click", function () {
                nav.scrollBy({
                    left: (dir === "left" ? -0.6 : 0.6) * nav.clientWidth,
                    behavior: "smooth"
                });
            });
            wrap.appendChild(btn);
        });

        function update() {
            var max = nav.scrollWidth - nav.clientWidth;
            wrap.classList.toggle("can-left", nav.scrollLeft > 4);
            wrap.classList.toggle("can-right", nav.scrollLeft < max - 4);
        }

        nav.addEventListener("scroll", update, { passive: true });

        /* trackpads and mice without a horizontal axis still need to reach the
           overflow, so map vertical wheel onto scrollLeft */
        nav.addEventListener("wheel", function (e) {
            if (!e.deltaX && nav.scrollWidth > nav.clientWidth) {
                nav.scrollLeft += e.deltaY;
                e.preventDefault();
            }
        }, { passive: false });

        if (typeof ResizeObserver !== "undefined") {
            new ResizeObserver(update).observe(nav);
        } else {
            var wasConnected = false;
            var onResize = function () {
                if (nav.isConnected) {
                    wasConnected = true;
                    update();
                } else if (wasConnected) {
                    window.removeEventListener("resize", onResize);
                }
            };
            window.addEventListener("resize", onResize);
        }

        update();
        return { wrap: wrap, update: update };
    }

    function sectionHead(section, opts) {
        var head = document.createElement("div");
        head.className = opts.sectionClass || "";
        head.dataset.section = section.key;
        if (opts.headId) head.id = opts.headId(section.key);
        Object.keys(opts.attrs || {}).forEach(function (a) {
            if (a !== "data-action") head.setAttribute(a, opts.attrs[a]);
        });
        if (opts.sectionAction) head.dataset.action = opts.sectionAction;

        var label = document.createElement("span");
        label.textContent = section.label;

        var chip = document.createElement("span");
        chip.className = opts.caretChipClass || "";
        var caret = document.createElement("i");
        caret.className = (opts.caretClass || "") +
            (section.open ? " keyboard_arrow_up_icon" : " keyboard_arrow_down_icon");
        chip.appendChild(caret);

        head.appendChild(label);
        head.appendChild(chip);
        return head;
    }

    function fieldRows(mount, rows, opts) {
        opts = opts || {};
        var cells = {};

        rows.forEach(function (row) {
            if (row.section) mount.appendChild(sectionHead(row.section, opts));

            var el = document.createElement("div");
            el.className = [opts.rowClass, row.cls].filter(Boolean).join(" ");
            Object.keys(opts.attrs || {}).forEach(function (a) {
                el.setAttribute(a, opts.attrs[a]);
            });

            var sec = row.section ? row.section.key : row.sectionKey;
            if (sec) el.dataset.section = sec;

            row.fields.forEach(function (f) {
                var pair = document.createElement("div");
                if (f.spacer) { el.appendChild(pair); return; }

                var label = document.createElement("span");
                label.textContent = f.label;
                var value = document.createElement("span");
                if (opts.idPrefix) value.id = opts.idPrefix + f.key;
                pair.appendChild(label);
                pair.appendChild(value);
                el.appendChild(pair);
                cells[f.key] = value;
            });

            mount.appendChild(el);
        });

        return cells;
    }

    /* --- toast ------------------------------------------------------------ */

    var TOAST_TIME = { info: 5000, success: 5000, warning: 7000, error: 9000 };
    var TOAST_ICON = { info: "i", success: "✓", warning: "!", error: "\u2715" };
    var TOAST_MAX = 4;   /* a burst must not become a wall */

    /* Shows a transient message. `container` is the stack to append to; if the
       element is missing it is created, so callers need not own one.
       Returns a dismiss() so a caller can retract a toast it no longer means. */
    function toast(type, message, duration, containerId, onClose) {
        if (!(type in TOAST_TIME)) type = "info";
        var id = containerId || "toast-container";
        var container = document.getElementById(id);
        if (!container) {
            container = document.createElement("div");
            container.id = id;
            document.body.appendChild(container);
        }
        /* also when the page supplied the element: without the class it gets
           neither the stacking gap nor equal-width children */
        container.classList.add("toast-container");

        function alive() { return container.querySelectorAll(".toast:not(.toast-dying)"); }

        var twin = null;
        var live = alive();
        for (var t = 0; t < live.length; t++) {
            if (live[t].dataset.key === type + "|" + message) { twin = live[t]; break; }
        }
        if (twin) {
            var n = (parseInt(twin.dataset.count, 10) || 1) + 1;
            twin.dataset.count = n;
            var badge = twin.querySelector(".toast-count");
            if (!badge) {
                badge = document.createElement("span");
                badge.className = "toast-count";
                twin.insertBefore(badge, twin.querySelector(".toast-close"));
            }
            badge.textContent = "\u00d7" + n;
            if (twin._restart) twin._restart();
            return twin._dismiss;
        }

        /* keep the stack readable: retire the oldest once it is full. A culled
           toast leaves at once rather than over 300ms, or this would spin. */
        while (live.length >= TOAST_MAX) {
            var oldest = live[0];
            if (oldest._dismiss) oldest._dismiss();
            oldest.remove();
            live = alive();
        }
        var el = document.createElement("div");
        el.className = "toast toast-" + type;
        el.dataset.key = type + "|" + message;
        el.dataset.count = 1;

        if (TOAST_ICON[type]) {
            var icon = document.createElement("span");
            icon.className = "toast-icon";
            icon.textContent = TOAST_ICON[type];
            el.appendChild(icon);
        }
        var text = document.createElement("span");
        text.className = "toast-text";
        text.textContent = message;
        var close = document.createElement("span");
        close.className = "toast-close";
        close.title = "Dismiss";
        close.textContent = "×";
        el.appendChild(text);
        el.appendChild(close);

        container.appendChild(el);
        requestAnimationFrame(function () { el.classList.add("toast-visible"); });

        var life = duration || TOAST_TIME[type];
        var timer = setTimeout(dismiss, life);
        var closed = false;

        el._restart = function () { clearTimeout(timer); timer = setTimeout(dismiss, life); };
        function dismiss() {
            if (closed) return;
            closed = true;
            clearTimeout(timer);
            el.classList.add("toast-dying");
            el.classList.remove("toast-visible");
            setTimeout(function () { el.remove(); }, 300);
            if (onClose) onClose();
        }
        close.addEventListener("click", dismiss);
        el._dismiss = dismiss;
        return dismiss;
    }

    /* --- modal ------------------------------------------------------------ */

    /* Card-in-scrim dialog shared by viewer and hub; the same id returns the
       same instance. opts: { id, title, cardClass (extra card classes, e.g.
       "modal-sm"/"modal-fit"), bodyClass, footerLabel (falsy = no footer
       bar), onClose }.
       Returns { root, card, body, foot, setTitle, open, close, isOpen }. */
    function modal(opts) {
        var existing = document.getElementById(opts.id);
        if (existing && existing._modal) return existing._modal;

        var root = document.createElement("div");
        root.className = "modal-overlay hidden";
        root.id = opts.id;

        var card = document.createElement("div");
        card.className = "card col modal-card" + (opts.cardClass ? " " + opts.cardClass : "");

        var header = document.createElement("div");
        header.className = "dialog-header";

        var title = document.createElement("div");
        title.className = "dialog-title";
        title.textContent = opts.title || "";

        var closeBtn = document.createElement("div");
        closeBtn.className = "close_icon";

        header.appendChild(title);
        header.appendChild(closeBtn);
        card.appendChild(header);

        var body = document.createElement("div");
        body.className = opts.bodyClass || "modal-body";
        card.appendChild(body);

        var foot = null;
        if (opts.footerLabel) {
            foot = document.createElement("div");
            foot.className = "row row-end modal-foot";
            var okBtn = document.createElement("button");
            okBtn.type = "button";
            okBtn.className = "btn";
            okBtn.textContent = opts.footerLabel;
            okBtn.addEventListener("click", close);
            foot.appendChild(okBtn);
            card.appendChild(foot);
        }

        root.appendChild(card);
        document.body.appendChild(root);

        function onKey(e) {
            if (e.key === "Escape") close();
        }
        function open() {
            root.classList.remove("hidden");
            document.addEventListener("keydown", onKey);
        }
        function close() {
            root.classList.add("hidden");
            document.removeEventListener("keydown", onKey);
            if (opts.onClose) opts.onClose();
        }
        function isOpen() {
            return !root.classList.contains("hidden");
        }

        closeBtn.addEventListener("click", close);
        /* mousedown, not click: a text selection ending on the scrim must not dismiss */
        root.addEventListener("mousedown", function (e) {
            if (e.target === root) close();
        });

        var api = {
            root: root, card: card, body: body, foot: foot,
            setTitle: function (t) { title.textContent = t; },
            open: open, close: close, isOpen: isOpen
        };
        root._modal = api;
        return api;
    }

    /* ---------------------------------------------------------------------
       floatingMenu — place an already-populated menu and keep it on screen.
       What the menu contains, and which entries a context shows, stays with
       the host; only the geometry and the dismiss lifecycle are shared.
       --------------------------------------------------------------------- */
    function placeMenu(menu, opts) {
        opts = opts || {};
        menu.style.display = "block";
        menu.style.transform = "none";

        var rect = menu.getBoundingClientRect();
        var vw = Math.min(window.innerWidth || Infinity, window.outerWidth || Infinity) || document.documentElement.clientWidth;
        var vh = Math.min(window.innerHeight || Infinity, window.outerHeight || Infinity) || document.documentElement.clientHeight;

        if (opts.anchor) {
            var btn = opts.anchor.getBoundingClientRect();
            var top = btn.top - rect.height - 8;
            if (top < 8) top = Math.min(btn.bottom + 8, vh - rect.height - 8);
            menu.style.left = Math.max(8, btn.right - rect.width) + "px";
            menu.style.top = top + "px";
            return;
        }
        if (opts.center) {
            menu.style.left = "50%";
            menu.style.top = "50%";
            menu.style.transform = "translate(-50%, -50%)";
            return;
        }
        menu.style.left = Math.max(0, Math.min(opts.x + 5, vw - rect.width)) + "px";
        menu.style.top = Math.max(0, Math.min(opts.y + 5, vh - rect.height)) + "px";
    }

    function dismissOnce(menu, onClose) {
        function close(e) {
            menu.style.display = "none";
            document.removeEventListener("click", close);
            document.removeEventListener("keydown", key);
            if (onClose) onClose(e);
        }
        function key(e) { if (e.key === "Escape") close(e); }
        setTimeout(function () {
            document.addEventListener("click", close);
            document.addEventListener("keydown", key);
        }, 0);
        return close;
    }

    return { tabScroller: tabScroller, fieldRows: fieldRows, toast: toast,
             TOAST_TIME: TOAST_TIME, modal: modal,
             placeMenu: placeMenu, dismissOnce: dismissOnce };
})();
