import { icon } from './icons.js';

// One element, its class, its attributes and its children. An `on…` key adds a
// listener, `text` and `html` set the content, a property of the element is set
// as one, anything else is an attribute (true for a bare one); a string for a
// switch (spellcheck: 'false', open: '') is an attribute too. A child that is
// null, undefined or false is left out.
export function el(tag, cls, attrs, ...kids) {
    const n = document.createElement(tag);
    if (cls) n.className = cls;
    for (const [k, v] of Object.entries(attrs || {})) {
        if (v === null || v === undefined || v === false) continue;
        if (k.startsWith('on')) n.addEventListener(k.slice(2).toLowerCase(), v);
        else if (k === 'dataset') Object.assign(n.dataset, v);
        else if (k === 'text') n.textContent = v;
        else if (k === 'html') n.innerHTML = v;
        else if (k in n && k !== 'list' && !(typeof n[k] === 'boolean' && typeof v === 'string')) n[k] = v;
        else n.setAttribute(k, v === true ? '' : v);
    }
    for (const k of kids.flat()) if (k !== null && k !== undefined && k !== false) n.append(k);
    return n;
}

// the element a piece of markup makes, an icon's <svg> as it is
export function fromHtml(markup) {
    const t = document.createElement('template');
    t.innerHTML = markup.trim();
    return t.content.firstElementChild;
}

// a switch as the receiver reads it: true, "true" or "on", in any case; `unset` when it is not there
export const on = (v, unset = false) => (v === undefined || v === null ? unset : v === true || ['true', 'on'].includes(String(v).trim().toLowerCase()));
// an input, an output or a server: on unless it says otherwise
export const isActive = item => on(item?.active, true);

// JSON text coloured: keys, strings, numbers and literals each in their own span
export function highlightJson(text) {
    const esc = text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
    return esc.replace(/("(?:\\.|[^"\\])*")(\s*:)?|\b(?:true|false|null)\b|-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?/g, (m, str, colon) => {
        if (str) return colon ? `<span class="j-key">${str}</span>${colon}` : `<span class="j-str">${str}</span>`;
        if (m === 'true' || m === 'false') return `<span class="j-bool">${m}</span>`;
        if (m === 'null') return `<span class="j-null">${m}</span>`;
        return `<span class="j-num">${m}</span>`;
    });
}

// The ⋯ menu of a page: its actions behind a button, each [label, onClick, danger]. While it is
// open, Escape closes it before anything else hears the key.
export function moreMenu(label, actions) {
    const summary = el('summary', 'st-icon-btn', { 'aria-label': label, title: 'More actions', html: icon('more') });
    const menu = el('details', 'st-menu', {}, summary,
        el('div', 'st-menu__items', {}, actions.map(([text, onClick, danger]) =>
            el('button', 'st-menu__item' + (danger ? ' st-menu__item--danger' : ''), { type: 'button', text, onClick }))));
    const escape = e => {
        if (!menu.isConnected) return window.removeEventListener('keydown', escape, true);
        if (e.key !== 'Escape') return;
        e.stopPropagation();
        menu.open = false;
        summary.focus();
    };
    menu.addEventListener('toggle', () => {
        if (menu.open) window.addEventListener('keydown', escape, true);
        else window.removeEventListener('keydown', escape, true);
    });
    return menu;
}
