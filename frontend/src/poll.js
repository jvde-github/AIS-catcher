// The tab in view, refreshed every refreshIntervalMs from when its last refresh ended, at once when
// the page is shown again, and not while it is hidden. A tab with nothing to poll stops it.

export let refreshIntervalMs = 2500;
let poll = null, timer = null, gen = 0, running = null;

async function tick(g) {
    // never two at once: a refresh asked for while one runs follows it
    await running;
    if (g !== gen || !poll || document.hidden) return;
    running = Promise.resolve().then(poll).catch((error) => console.error("Error updating data:", error));
    await running;
    running = null;
    if (g === gen && !document.hidden) timer = setTimeout(() => tick(g), refreshIntervalMs);
}

// null stops it
export function startPoll(fn) {
    poll = fn || null;
    gen++;
    clearTimeout(timer);
    tick(gen);
}

export function refresh() {
    startPoll(poll);
}

export function setRefreshInterval(ms) {
    refreshIntervalMs = ms;
    refresh();
}

document.addEventListener("visibilitychange", () => {
    gen++;
    clearTimeout(timer);
    if (!document.hidden) tick(gen);
});
