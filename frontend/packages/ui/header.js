/* ============================================================================
   header.js — the header's tab pill: names or icons, and what fits

   Each pill item is an icon with the tab's name inside (<span class="mb-name">);
   mode() picks which shows: "names", "icons", or "adaptive", which starts with
   names and falls back to icons once they no longer fit. When the row is still
   too narrow a chip beside the title, when there is one, takes its short form
   (class fit-short) and then leaves the row (fit-hidden), a "fit" event on it
   telling the host, which may show it elsewhere; then the pill sheds tabs from
   the end (never the active one, never below three), then the pill goes, then
   the title; the icons on the right of the header always stay. The gaps that
   survive are --header-gap and --header-title-gap (header.css).

       var hdr = header.create({
           box: document.querySelector(".header-title"),
           title: document.querySelector(".header-title > span"),
           pill: document.getElementById("menubar_mini"),
           chip: document.getElementById("header-focus"),         // optional
           mode: function () { return settings.menu_labels; },   // names | icons | adaptive
       });
       hdr.apply();     // after the setting or the set of tabs changed
   ========================================================================= */

export function create(opts) {
    var box = opts.box, title = opts.title, pill = opts.pill, chip = opts.chip;
    var mode = opts.mode || function () { return "icons"; };
    var minTabs = opts.minTabs || 3;

    function apply() {
        if (!box || !title || !pill) return;
        var want = mode();
        pill.classList.toggle("labels", want !== "icons");
        var all = Array.prototype.slice.call(pill.children);
        [title, pill].concat(all).forEach(function (el) { el.classList.remove("fit-hidden"); });
        box.classList.remove("no-pill");
        if (chip) chip.classList.remove("fit-short", "fit-hidden");
        /* tabs the host hides do not count */
        var tabs = all.filter(function (t) { return getComputedStyle(t).display !== "none"; });
        var room = function () { return box.clientWidth - parseFloat(getComputedStyle(box).paddingRight); };
        var fits = function () { return title.scrollWidth + pill.offsetWidth <= room(); };
        /* adaptive: icons buy room before any tab is dropped */
        if (want === "adaptive" && !fits()) pill.classList.remove("labels");
        /* the chip gives way before a tab does: short, then out of the row */
        if (chip && !chip.hidden && !fits()) {
            chip.classList.add("fit-short");
            if (!fits()) chip.classList.replace("fit-short", "fit-hidden");
        }
        if (chip) chip.dispatchEvent(new CustomEvent("fit"));
        var spare = tabs.filter(function (t) {
            return !t.classList.contains("active") && !t.querySelector(".active");
        }).reverse();
        for (var i = 0, shown = tabs.length; i < spare.length && !fits() && shown > minTabs; i++, shown--)
            spare[i].classList.add("fit-hidden");
        if (!fits()) {
            pill.classList.add("fit-hidden");
            box.classList.add("no-pill");
        }
        /* the title is never truncated: it fits whole or it goes */
        if (title.scrollWidth > room()) title.classList.add("fit-hidden");
    }

    window.addEventListener("resize", apply);
    if (document.fonts && document.fonts.ready) document.fonts.ready.then(apply);
    apply();
    return { apply: apply };
}
