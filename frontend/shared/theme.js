/* ============================================================================
   theme.js — which palette is on (layer 2)

   tokens.css has one switch: the `dark` class on <html>. This sets it from a
   mode, and keeps it following the system preference when asked to.

       theme.apply("dark");        // explicit
       theme.apply("system");      // follows prefers-color-scheme, live
       theme.current();            // "dark" | "light"
   ========================================================================= */

var media = typeof matchMedia === "function" ? matchMedia("(prefers-color-scheme: dark)") : null;
var following = null;

function set(dark) {
    document.documentElement.classList.toggle("dark", !!dark);
}

export function current() {
    return document.documentElement.classList.contains("dark") ? "dark" : "light";
}

export function apply(mode) {
    if (following && media) media.removeEventListener("change", following);
    following = null;

    if (mode === "system" && media) {
        following = function (e) { set(e.matches); };
        media.addEventListener("change", following);
        set(media.matches);
        return;
    }
    set(mode === "dark");
}
