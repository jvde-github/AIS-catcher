// The health card of the AIS and GPS views, drawn the same for both: the verdict with its dot, four
// figures, a bar under them (the channels, the constellations), and what is checked: a grid of
// checks, beside the figures when the card is wide, and a foot that opens on the details. The card
// is filled in place at every poll: only what changed is written.

// a state is the server's word: ok, degraded, down, learning (no baseline yet) or unknown (not measured)
const COLOR = { ok: "--hc-ok", degraded: "--color-warning-ink", down: "--color-danger" };

const ICON = {
    ok: '<circle cx="12" cy="12" r="9"/><path d="m8 12 3 3 5-6"/>',
    clock: '<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/>',
    stop: '<circle cx="12" cy="12" r="9"/><path d="m9 9 6 6"/><path d="m15 9-6 6"/>',
    alert: '<circle cx="12" cy="12" r="9"/><path d="M12 7.5v5.5"/><path d="M12 16.5h.01"/>',
    none: '<circle cx="12" cy="12" r="9"/><path d="M9.6 9.6a2.5 2.5 0 1 1 3.4 2.3c-.6.3-1 .8-1 1.5"/><path d="M12 16.6h.01"/>',
};
// a check's icon by its state
const CHECK = { ok: "ok", degraded: "alert", down: "stop", learning: "clock", unknown: "none" };
const icon = (name) => `<svg class="hc-icon" viewBox="0 0 24 24" aria-hidden="true">${ICON[name]}</svg>`;
export const tone = (state) => `var(${COLOR[state] || "--color-text-subtle"})`;
export const wrong = (state) => state === "degraded" || state === "down";

// written only when it changed, so that what is focused, hovered or about to be clicked stays in place
const drawn = new WeakMap();
function put(el, html) {
    if (drawn.get(el) === html) return;
    drawn.set(el, html);
    el.innerHTML = html;
}

// opts: state, title, live, label, figures [html, four], aside (html under them),
// checks [{state, title, note, hint}], foot {more, details}
export function head(host, opts) {
    // the card's parts are made once and filled; the foot that opens is one element for as long as the card lives
    if (!host.dataset.ready) {
        host.dataset.ready = "1";
        host.innerHTML = `<header class="hc-top"></header><div class="hc-body"></div><details class="hc-foot" hidden><summary class="hc-foot-top"></summary><div></div></details>`;
    }
    host.classList.add("health-card");
    host.dataset.state = opts.state;
    host.setAttribute("aria-label", opts.label);
    host.style.setProperty("--state", tone(opts.state));
    const [top, body, more] = host.children;
    const n = opts.checks ? opts.checks.length : 0;
    const check = (c) => `<div class="hc-check${wrong(c.state) ? " hc-alert" : ""}" style="--tone:${tone(c.state)}" title="${c.hint}">${icon(CHECK[c.state])}<div><div class="hc-check-title">${c.title}</div><div class="hc-check-note">${c.note}</div></div></div>`;
    put(top, `<span class="hc-dot"></span><h2 class="hc-title">${opts.title}</h2><span class="hc-live">${opts.live}</span>`);
    put(body, `<div><div class="hc-figs">${opts.figures.join("")}</div>${opts.aside ? `<div class="hc-aside">${opts.aside}</div>` : ""}</div>
        ${opts.checks ? `<div class="hc-checks" style="--rows2:${Math.ceil(n / 2)};--row1:${n <= 4 ? n : Math.ceil(n / 2)}">${opts.checks.map(check).join("")}</div>` : ""}`);
    const f = opts.foot;
    more.hidden = !(f && f.details);
    if (more.hidden) return;
    put(more.firstElementChild, `<span class="hc-foot-more">${f.more}</span>`);
    put(more.lastElementChild, f.details);
}

export const figure = (label, value, note, cls = "") => `<div class="hc-fig"><div class="hc-label">${label}</div><div class="hc-value ${cls}">${value}</div><div class="hc-note">${note}</div></div>`;
export const cell = (label, value) => `<div><span>${label}</span><strong>${value}</strong></div>`;
