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

    /* --- toast ------------------------------------------------------------ */

    var TOAST_TIME = { info: 5000, success: 5000, warning: 7000, error: 9000 };
    var TOAST_ICON = { info: "i", success: "✓", warning: "!", error: "!" };

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

        var el = document.createElement("div");
        el.className = "toast toast-" + type;

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

        var timer = setTimeout(dismiss, duration || TOAST_TIME[type]);
        var closed = false;
        function dismiss() {
            if (closed) return;
            closed = true;
            clearTimeout(timer);
            el.classList.remove("toast-visible");
            setTimeout(function () { el.remove(); }, 300);
            if (onClose) onClose();
        }
        close.addEventListener("click", dismiss);
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

    return { tabScroller: tabScroller, toast: toast, TOAST_TIME: TOAST_TIME, modal: modal };
})();
