/* ============================================================================
   toolbar.js — the map's buttons: a rail of pills, or one labelled bar

   The same buttons live in either a vertical rail (.map-pill) or a horizontal
   bar (.map-toolbar); which one depends on whether the bar fits and whether
   the host wants it. This moves the buttons, labels them, and marks the
   containers (bar.open / rail.folded for map.css). What the buttons do is
   the host's: they carry data-action and the host dispatches.

       const tb = toolbar.create({
           bar: document.getElementById("maptoolbar"),
           rail: document.querySelector(".map-pill:not(.map-bottom)"),
           wantWide: () => settings.map_toolbar === "wide",
           freeWidth: () => chrome.freeBox(mapEl).width,
           onChange: () => chrome.invalidate(),
       });
       tb.apply();            // after layout changes
   ========================================================================= */

export function create(opts) {
    let bar = opts.bar;
    let rail = opts.rail;
    let wantWide = opts.wantWide || function () { return false; };
    let freeWidth = opts.freeWidth || function () { return window.innerWidth; };
    let reserve = opts.reserve || function () { return 0; };   // px the bar must leave beside it
    let gap = opts.gap || function () { return 0; };
    let onChange = opts.onChange || function () {};

    let barWidth = 0;
    let applied = null;

    function buttons() {
        return [].concat(
            Array.prototype.slice.call(rail.querySelectorAll("button[data-label]")),
            Array.prototype.slice.call(bar.querySelectorAll("button[data-label]")));
    }

    function label(btns) {
        btns.forEach(function (btn) {
            if (btn.querySelector(".mb-label")) return;
            let span = document.createElement("span");
            span.className = "mb-label";
            span.textContent = btn.dataset.label;
            btn.appendChild(span);
        });
    }

    /* the bar is display:none while folded, so its natural width is taken
       once from an off-screen layout rather than guessed */
    function measure(btns) {
        if (barWidth) return;
        bar.style.cssText = "display:flex;visibility:hidden;position:absolute;left:-9999px";
        btns.forEach(function (btn) { bar.appendChild(btn); });
        barWidth = bar.scrollWidth || 520;
        bar.removeAttribute("style");
    }

    function fits() {
        return freeWidth() >= barWidth + 2 * reserve();
    }

    /* a short pane: the rail sheds buttons from the bottom, sparing those marked
       data-keep, until it ends a gap above the bottom pill */
    function fitRail(btns) {
        let bottom = rail.parentElement && rail.parentElement.querySelector(".map-pill.map-bottom");
        let pane = rail.offsetParent;
        let g = gap();
        let end = bottom && bottom.offsetParent ? bottom.offsetTop : (pane ? pane.clientHeight : window.innerHeight) - g;
        let limit = end - rail.offsetTop - g;
        let spare = btns.filter(function (b) { return !b.hasAttribute("data-keep"); }).reverse();
        for (let i = 0; i < spare.length && rail.offsetHeight > limit; i++) spare[i].classList.add("fit-hidden");
    }

    function apply() {
        if (!bar || !rail) return false;
        let btns = buttons();
        label(btns);
        measure(btns);

        btns.forEach(function (btn) { btn.classList.remove("fit-hidden"); });
        let wide = !!wantWide() && fits();
        if (wide !== applied) {
            applied = wide;
            let host = wide ? bar : rail;
            btns.forEach(function (btn) { host.appendChild(btn); });
            bar.classList.toggle("open", wide);
            rail.classList.toggle("folded", wide);
            onChange(wide);
        }
        if (!wide) fitRail(btns);
        return wide;
    }

    return {
        apply: apply,
        isWide: function () { return !!applied; },
        width: function () { return barWidth; },
    };
}
