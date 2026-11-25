// The empty state of an optional part of the station, as a GPS: what it would show,
// the one thing to do about it, and the command line for those who start the program by hand.
// Nothing is wrong when it shows, and it says so by staying small.

import { config } from '../../core/config.js';

const ICON = {
    satellite: '<path d="m7 9 4-4 3 3-4 4z"/><path d="m13 15 4-4 3 3-4 4z"/><path d="m10.5 11.5 2 2"/><path d="M4 15a5 5 0 0 0 5 5"/>',
    target: '<circle cx="12" cy="12" r="8"/><path d="M12 2v4"/><path d="M12 18v4"/><path d="M2 12h4"/><path d="M18 12h4"/>',
    shield: '<path d="M12 3 4 6v6c0 5 8 9 8 9s8-4 8-9V6l-8-3z"/><path d="m9 12 2 2 4-4"/>',
    copy: '<rect x="8" y="8" width="12" height="12" rx="2"/><path d="M16 8V6a2 2 0 0 0-2-2H6a2 2 0 0 0-2 2v8a2 2 0 0 0 2 2h2"/>',
};
const svg = (name) => `<svg viewBox="0 0 24 24" aria-hidden="true">${ICON[name]}</svg>`;

export const ART = {
    gps: '<circle cx="50" cy="50" r="46" class="sc-disc"/><g class="sc-rings"><circle cx="50" cy="50" r="14"/><circle cx="50" cy="50" r="27"/><circle cx="50" cy="50" r="40"/><path d="M10 50H90M50 10V90"/></g><g class="sc-dots"><circle cx="29" cy="28" r="4.5"/><circle cx="61" cy="21" r="4.5"/><circle cx="78" cy="48" r="4.5"/><circle cx="33" cy="70" r="4.5"/><circle cx="60" cy="78" r="4.5"/></g><circle cx="50" cy="50" r="7" class="sc-centre"/>',
};

// opts: art, title, text, features [[icon, words]], action (the button's words), docs (the guide's path on
// docs.aiscatcher.org), methods [{name, command, hint}]. With a control panel the button opens it and the
// guide is a link beside; without one the guide is the thing to do.
export function setup(host, opts) {
    if (host.dataset.ready) return;
    host.dataset.ready = "1";
    const panel = !!config.webcontrol_http;
    const guide = `href="https://docs.aiscatcher.org/${opts.docs}" target="_blank" rel="noopener"`;
    host.innerHTML = `<div class="sc-intro">
            <svg class="sc-art" viewBox="0 0 100 100" aria-hidden="true">${opts.art}</svg>
            <h2 class="sc-title">${opts.title}</h2>
            <p class="sc-text">${opts.text}</p>
            <div class="sc-features">${opts.features.map(([icon, words]) => `<span>${svg(icon)}${words}</span>`).join("")}</div>
            ${panel ? `<button type="button" class="sc-primary" data-action="openWebControl"><span aria-hidden="true">+</span>${opts.action}</button><p class="sc-hint">Configure through the control panel, or read the <a ${guide}>setup guide</a></p>`
                : `<a class="sc-primary" ${guide}>Read the setup guide</a><p class="sc-hint">docs.aiscatcher.org</p>`}
        </div>
        <div class="sc-setup">
            <h3 class="sc-sub">Command-line setup</h3>
            <div class="plot-seg" role="tablist">${opts.methods.map((m, k) => `<button type="button" role="tab" data-method="${k}"${k ? "" : ' class="active"'}>${m.name}</button>`).join("")}</div>
            <div class="sc-code"><code></code><button type="button" class="sc-copy" aria-label="Copy the command">${svg("copy")}</button></div>
            <p class="sc-hint" aria-live="polite"></p>
        </div>`;
    const code = host.querySelector("code"), hint = host.querySelector(".sc-setup .sc-hint");
    const show = (k) => {
        host.querySelectorAll("[data-method]").forEach((b) => b.classList.toggle("active", +b.dataset.method === k));
        code.textContent = opts.methods[k].command;
        hint.textContent = opts.methods[k].hint;
    };
    host.addEventListener("click", (e) => {
        const method = e.target.closest("[data-method]");
        if (method) show(+method.dataset.method);
        if (e.target.closest(".sc-copy"))
            navigator.clipboard.writeText(code.textContent).then(() => (hint.textContent = "Command copied."), () => (hint.textContent = "Select the command and copy it by hand."));
    });
    show(0);
}
