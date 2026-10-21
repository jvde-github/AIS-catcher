import { ICONS } from './icons.js';

// One element, its class, its attributes and its children. An `on…` key adds a
// listener, `text` and `html` set the content, a property of the element is set
// as one, anything else is an attribute (true for a bare one). A child that is
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
        else if (k in n && k !== 'list') n[k] = v;
        else n.setAttribute(k, v === true ? '' : v);
    }
    for (const k of kids.flat()) if (k !== null && k !== undefined && k !== false) n.append(k);
    return n;
}

// a switch as the configuration writes it: true, "true" or "on"
export const on = v => v === true || String(v).toLowerCase() === 'true' || String(v).toLowerCase() === 'on';

// The ⋯ menu of a page: its actions behind a button, each [label, onClick, danger]. While it is
// open, Escape closes it before anything else hears the key.
export function moreMenu(label, actions) {
    const summary = el('summary', 'st-icon-btn', { 'aria-label': label, title: 'More actions', html: ICONS.more });
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
