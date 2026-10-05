// a browser-like globalThis for the shell tests
import { Window } from "happy-dom";

export function setupDom(html) {
    const window = new Window({ url: "http://localhost/" });
    const g = globalThis;
    for (const k of ["window", "document", "HTMLElement", "CustomEvent", "MouseEvent", "Event", "getComputedStyle", "requestAnimationFrame", "ResizeObserver", "matchMedia"]) {
        if (k in window) g[k] = window[k];
    }
    if (!g.ResizeObserver) g.ResizeObserver = class { observe() {} disconnect() {} };
    if (!g.requestAnimationFrame) g.requestAnimationFrame = (fn) => setTimeout(fn, 0);
    g.document.body.innerHTML = html || "";
    return window;
}
