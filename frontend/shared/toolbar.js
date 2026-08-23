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
    var bar = opts.bar;
    var rail = opts.rail;
    var wantWide = opts.wantWide || function () { return false; };
    var freeWidth = opts.freeWidth || function () { return window.innerWidth; };
    var reserve = opts.reserve || function () { return 0; };   // px the bar must leave beside it
    var onChange = opts.onChange || function () {};

    var barWidth = 0;
    var applied = null;

    function buttons() {
        return [].concat(
            Array.prototype.slice.call(rail.querySelectorAll("button[data-label]")),
            Array.prototype.slice.call(bar.querySelectorAll("button[data-label]")));
    }

    function label(btns) {
        btns.forEach(function (btn) {
            if (btn.querySelector(".mb-label")) return;
            var span = document.createElement("span");
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

    function apply() {
        if (!bar || !rail) return false;
        var btns = buttons();
        label(btns);
        measure(btns);

        var wide = !!wantWide() && fits();
        if (wide === applied) return wide;
        applied = wide;

        var host = wide ? bar : rail;
        btns.forEach(function (btn) { host.appendChild(btn); });
        bar.classList.toggle("open", wide);
        rail.classList.toggle("folded", wide);
        onChange(wide);
        return wide;
    }

    return {
        apply: apply,
        isWide: function () { return !!applied; },
        width: function () { return barWidth; },
    };
}
