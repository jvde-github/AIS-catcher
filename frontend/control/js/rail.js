// The settings' left rail: every section always there, the current one open with its items and
// its add button, a count on a closed section of two or more. It holds which page shows and which
// item on it; the pages are drawn by their own code, which `open(tab, key)` reaches, false when
// unsaved edits keep the page. `sections(cfg)` says what the rail lists: per section its `tab`,
// `label`, `items` ({ key, label, dot, onClick }) and `add` ([label, onClick]); a section whose
// items are pages of their own (System's) says `tabs`.

import { el } from './dom.js';
import { ConfigStore } from './config-manager.js';

const ICONS = {
    stations: '<path d="M12 21s-6-5.4-6-10a6 6 0 1 1 12 0c0 4.6-6 10-6 10z"/><circle cx="12" cy="11" r="2.2"/>',
    input: '<path d="M4.9 4.9a10 10 0 0 0 0 14.2M19.1 4.9a10 10 0 0 1 0 14.2M7.8 7.8a6 6 0 0 0 0 8.4M16.2 7.8a6 6 0 0 1 0 8.4"/><circle cx="12" cy="12" r="1.6"/>',
    output: '<circle cx="6" cy="12" r="2.2"/><circle cx="18" cy="6" r="2.2"/><circle cx="18" cy="18" r="2.2"/><path d="M8 11l8-4M8 13l8 4"/>',
    flow: '<rect x="3" y="4" width="6" height="5" rx="1.2"/><rect x="3" y="15" width="6" height="5" rx="1.2"/><rect x="15" y="9.5" width="6" height="5" rx="1.2"/><path d="M9 6.5c3 0 3 5.5 6 5.5M9 17.5c3 0 3-5.5 6-5.5"/>',
    services: '<rect x="4" y="5" width="16" height="6" rx="1.5"/><rect x="4" y="13" width="16" height="6" rx="1.5"/><path d="M8 8h.01M8 16h.01"/>',
    places: '<path d="M4 7l5-2 6 2 5-2v12l-5 2-6-2-5 2z"/><path d="M9 5v12M15 7v12"/>',
    system: '<circle cx="12" cy="12" r="3"/><path d="M12 3v2M12 19v2M3 12h2M19 12h2M5.6 5.6l1.4 1.4M17 17l1.4 1.4M5.6 18.4L7 17M17 7l1.4-1.4"/>'
};

export function createRail(nav, { sections, open, shown }) {
    let sel = { tab: 'input', key: 0 };
    let last = { ...sel };

    function paint(list) {
        const icon = tab => `<svg class="st-rail__icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${ICONS[tab]}</svg>`;
        const groups = list.map(sec => {
            const g = el('div', 'st-rail__group');
            const items = sec.items || [];
            const here = sec.tabs ? items.some(it => it.key === sel.tab) : sel.tab === sec.tab || sel.tab === sec.tab + '-add';
            const isCurrent = it => (sec.tabs ? sel.tab === it.key : sel.tab === sec.tab && String(sel.key) === String(it.key));
            const go = key => (sec.tabs ? select(key) : select(sec.tab, key));
            const head = el('button', 'st-rail__sec', { type: 'button', html: icon(sec.tab),
                'aria-current': here && !items.some(isCurrent) ? 'true' : null,
                onClick: () => (sec.tabs ? select(items[0].key) : select(sec.tab, items.length ? items[0].key : undefined)) },
                el('span', 'st-rail__label', { text: sec.label }));
            g.append(head);
            if (!here) return g;
            items.forEach(it => {
                if (it.onClick) return g.append(el('button', 'st-rail__add', { type: 'button', text: it.label, onClick: it.onClick }));
                // on goes unmarked: only off, or on but losing data (amber: settings, red: now)
                const off = it.dot === false;
                const why = off ? null : it.warn || it.bad;
                g.append(el('button', 'st-rail__item' + (off ? ' st-rail__item--off' : ''), { type: 'button', 'aria-current': isCurrent(it) ? 'true' : null,
                    title: off ? 'Off' : why, 'aria-label': why ? `${it.label}: ${why}` : null, onClick: () => go(it.key) },
                    off ? el('span', 'st-rail__dot') : why ? el('span', 'st-rail__mark st-rail__mark--' + (it.warn ? 'warn' : 'bad')) : null,
                    el('span', 'st-rail__label', { text: it.label, title: it.label })));
            });
            if (sec.add)
                g.append(el('button', 'st-rail__add', { type: 'button', text: '+ ' + sec.add[0], onClick: sec.add[1],
                    'aria-current': sel.tab === sec.tab + '-add' ? 'true' : null }));
            return g;
        });
        nav.replaceChildren(...groups);
    }

    function render() {
        if (!shown()) return;
        ConfigStore.fetch().then(cfg => paint(sections(cfg))).catch(() => {});
    }

    // open a page of the rail with the item on it; false when unsaved edits keep the page shown
    function select(tab, key) {
        const prev = sel;
        sel = { tab, key };
        const ok = open(tab, key);
        if (!ok) sel = prev;
        render();
        return ok;
    }

    return {
        get tab() { return sel.tab; },
        get key() { return sel.key; },
        select,
        render,
        // a page that changed which item it shows says so; the rail follows
        follow(tab, key) { if (sel.tab === tab) sel = { tab, key }; },
        // where the panel was left, to open there the next time
        remember() { last = { ...sel }; },
        get last() { return last; }
    };
}
