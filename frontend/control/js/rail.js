// The settings' left rail: every section always there, the current one open with its items and
// its add button, a count on a closed section of two or more. It holds which page shows and which
// item on it; the pages are drawn by their own code, which `open(tab, key)` reaches, false when
// unsaved edits keep the page. `sections(cfg)` says what the rail lists: per section its `tab`,
// `label`, `items` ({ key, label, dot, onClick }) and `add` ([label, onClick]); a section whose
// items are pages of their own (System's) says `tabs`.

import { el } from './dom.js';
import { icon } from './icons.js';
import { ConfigStore } from './config-store.js';

export function createRail(nav, { sections, open, shown }) {
    let sel = { tab: 'input', key: 0 };
    let last = { ...sel };

    function paint(list) {
        const groups = list.map(sec => {
            const g = el('div', 'st-rail__group');
            const items = sec.items || [];
            const here = sec.tabs ? items.some(it => it.key === sel.tab) : sel.tab === sec.tab || sel.tab === sec.tab + '-add';
            const isCurrent = it => (sec.tabs ? sel.tab === it.key : sel.tab === sec.tab && String(sel.key) === String(it.key));
            const go = key => (sec.tabs ? select(key) : select(sec.tab, key));
            const head = el('button', 'st-rail__sec', { type: 'button', html: icon(sec.tab, 'st-rail__icon'),
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
