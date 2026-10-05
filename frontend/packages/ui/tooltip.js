/* tooltip.js — one floating label for icon-only map buttons. The rail pills
   clip ::after bubbles, so a fixed element follows the hovered button; a
   title is moved to aria-label so the browser does not add a second bubble.

       tooltip.create({ selector: ".map-pill .map-button" }).prepare();
   ========================================================================= */

export function create(opts) {
    opts = opts || {};
    var selector = opts.selector || ".map-pill .map-button";
    var margin = opts.margin || 8;
    var below = opts.placement === "below";

    var tip = document.createElement("div");
    tip.id = "toolbar-tooltip";
    document.body.appendChild(tip);

    function prepare(root) {
        (root || document).querySelectorAll(selector).forEach(function (btn) {
            if (!btn.title) return;
            if (!btn.getAttribute("aria-label")) btn.setAttribute("aria-label", btn.title);
            if (!btn.dataset.label) btn.dataset.label = btn.title;
            btn.removeAttribute("title");
        });
    }

    function show(btn) {
        var text = btn.dataset.label || btn.getAttribute("aria-label");
        if (!text) return;
        // A fullscreen element paints above the body, including fixed children.
        // Keep the tooltip in the same top-layer subtree as its button.
        var container = document.fullscreenElement || document.body;
        if (tip.parentElement !== container) container.appendChild(tip);
        tip.textContent = text;
        tip.classList.add("show");

        var r = btn.getBoundingClientRect();
        var w = tip.offsetWidth, h = tip.offsetHeight;
        var left, top;
        if (below) {
            left = Math.max(margin, Math.min(window.innerWidth - w - margin, r.left + r.width / 2 - w / 2));
            top = r.bottom + h + margin <= window.innerHeight ? r.bottom + margin : r.top - h - margin;
        } else {
            left = r.left - w - margin;
            if (left < margin) left = r.right + margin;
            top = Math.max(margin, Math.min(window.innerHeight - h - margin, r.top + r.height / 2 - h / 2));
        }
        tip.style.left = left + "px";
        tip.style.top = top + "px";
    }

    function hide() {
        tip.classList.remove("show");
    }

    document.addEventListener("pointerover", function (e) {
        var btn = e.target.closest(selector);
        if (btn) show(btn);
    });
    document.addEventListener("pointerout", function (e) {
        if (e.target.closest(selector)) hide();
    });
    document.addEventListener("pointerdown", hide, true);
    document.addEventListener("focusin", function (e) {
        var btn = e.target.closest(selector);
        if (btn && btn.matches(":focus-visible")) show(btn);
    });
    document.addEventListener("focusout", function (e) {
        if (e.target.closest(selector)) hide();
    });
    document.addEventListener("fullscreenchange", hide);

    return { prepare: prepare, hide: hide };
}
