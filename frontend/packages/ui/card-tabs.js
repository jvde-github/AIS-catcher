// Shared tab navigation, compact summary and disclosure for map cards.
import { attach as attachDisclosure } from './card-disclosure.js';
import { fieldRows } from './components.js';

// Both hosts and every entity use the same metric markup as ordinary fields.
export function metrics(head, fields, prefix) {
    head.style.setProperty('--sc-metric-columns', String(fields.length));
    const cells = fieldRows(head, [{ fields }], { rowClass: 'sc-metrics', idPrefix: prefix });
    Object.values(cells).forEach(cell => cell.classList.add('sc-metric-value'));
    return cells;
}

function el(tag, cls, attrs) {
    const e = document.createElement(tag);
    e.className = cls;
    for (const [k, v] of Object.entries(attrs || {})) e.setAttribute(k, v);
    return e;
}

export function build(mount, prefix, o) {
    const { tabs, label = 'Details' } = o;
    const defaultTab = tabs[0][0];
    mount.innerHTML = '';
    mount.classList.add('sc-host');
    const list = el('div', 'sc-tabs', { role: 'tablist', 'aria-label': label });
    const panels = el('div', 'sc-panels', { id: prefix + 'panels' });
    const tabEls = {}, panelEls = {};
    for (const [key, label] of tabs) {
        const b = el('button', 'sc-tab', { type: 'button', role: 'tab', id: prefix + 'tab_' + key, 'aria-controls': prefix + 'panel_' + key, 'aria-selected': 'false', tabindex: '-1' });
        b.textContent = label;
        b.dataset.tab = key;
        const p = el('div', 'sc-panel', { role: 'tabpanel', id: prefix + 'panel_' + key, 'aria-labelledby': prefix + 'tab_' + key });
        p.hidden = true;
        list.appendChild(b);
        panels.appendChild(p);
        tabEls[key] = b;
        panelEls[key] = p;
    }
    const head = el('div', 'sc-head');
    mount.appendChild(list);
    mount.appendChild(head);
    mount.appendChild(panels);

    let active;   // undefined until the first select, so a folded start still applies
    let disclosure;
    function select(key, notify) {
        // A direct choice supersedes any in-flight gesture or snap animation.
        if (notify !== false) disclosure?.cancel();
        key = key == null ? null : (tabs.some(([k]) => k === key) ? key : defaultTab);
        if (key === active) return;
        active = key;
        for (const [k] of tabs) {
            tabEls[k].setAttribute('aria-selected', String(k === key));
            tabEls[k].tabIndex = k === key || (!key && k === defaultTab) ? 0 : -1;
            panelEls[k].hidden = k !== key;
        }
        mount.classList.toggle('sc-folded', !key);
        head.hidden = key != null && key !== defaultTab;
        panels.scrollTop = 0;
        if (disclosure) disclosure.update();
        if (notify !== false && o.onTab) o.onTab(key);
    }
    list.addEventListener('click', (e) => {
        const b = e.target.closest('.sc-tab');
        if (!b) return;
        select(b.dataset.tab);
    });
    list.addEventListener('keydown', (e) => {
        const keys = tabs.map(([k]) => k);
        let i = keys.indexOf(active);
        if (e.key === 'ArrowRight') i = i < 0 ? 0 : (i + 1) % keys.length;
        else if (e.key === 'ArrowLeft') i = i < 0 ? keys.length - 1 : (i - 1 + keys.length) % keys.length;
        else if (e.key === 'Home') i = 0;
        else if (e.key === 'End') i = keys.length - 1;
        else return;
        e.preventDefault();
        select(keys[i]);
        tabEls[keys[i]].focus();
    });
    select(o.tab, false);
    disclosure = attachDisclosure({ mount, panels, enabled: () => !!(o.foldable && o.foldable()), active: () => active, select, defaultTab });

    return { head, panels: panelEls, select, active: () => active, scrollTop: () => { panels.scrollTop = 0; }, destroy() {
        disclosure.destroy();
        mount.classList.remove('sc-host', 'sc-folded');
    } };
}
