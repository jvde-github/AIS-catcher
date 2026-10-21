// A dropdown to choose one of many: a button that shows the chosen one and a
// list that opens under it, with a search field on top once the list is long.
// The viewer's station filter and the control panel's station of an input use
// it; the host gives the options and says what a pick means.

const SEARCH_FROM = 8;

const CHEVRON = '<svg class="pick__chev" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M6 9l6 6 6-6"/></svg>';

function node(tag, cls, text) {
    const n = document.createElement(tag);
    n.className = cls;
    if (text !== undefined && text !== null && text !== '') n.textContent = String(text);
    return n;
}

// an option's face, on the button and in the list: number, name over its line, tag
function face(opt) {
    const text = node('span', 'pick__text');
    text.append(node('strong', 'pick__name', opt.name));
    if (opt.sub) text.append(node('small', 'pick__sub', opt.sub));
    return [opt.badge !== undefined && opt.badge !== null ? node('b', 'pick__num', opt.badge) : null, text,
        opt.tag ? node('em', 'pick__tag', opt.tag) : null].filter(Boolean);
}

/* An option is { value, name, sub, badge, tag, group }: a new group draws a line
   above it. o: { options(): the options, read each time the list opens; value;
   onPick(value); shown(value): the button's face for a value no option has, warn
   set to mark it; footer(): an element at the end of the list; label: what the
   options are, for the search field; empty: the line when the search finds none } */
export function createPicker(o) {
    let value = o.value, open = false;

    const root = node('div', 'pick');
    const button = node('button', 'pick__btn');
    button.type = 'button';
    button.setAttribute('aria-haspopup', 'listbox');
    button.setAttribute('aria-expanded', 'false');
    const panel = node('div', 'pick__panel');
    panel.hidden = true;
    const search = node('input', 'input pick__search');
    search.type = 'search';
    search.placeholder = 'Search ' + (o.label || 'options').toLowerCase() + '…';
    search.setAttribute('aria-label', 'Search ' + (o.label || 'options').toLowerCase());
    const list = node('div', 'pick__list');
    list.setAttribute('role', 'listbox');
    if (o.label) list.setAttribute('aria-label', o.label);
    panel.append(search, list);
    root.append(button, panel);

    function paintButton() {
        const opt = o.options().find((x) => x.value === value) || (o.shown && o.shown(value)) || { name: String(value) };
        button.replaceChildren(...face(opt));
        button.insertAdjacentHTML('beforeend', CHEVRON);
        button.classList.toggle('pick__btn--warn', !!opt.warn);
    }

    function paintList() {
        const all = o.options();
        const q = search.value.trim().toLowerCase();
        const found = q ? all.filter((x) => [x.name, x.sub, x.badge].some((v) => v !== undefined && v !== null && String(v).toLowerCase().includes(q))) : all;
        const items = [];
        found.forEach((x, i) => {
            if (!q && i > 0 && x.group !== found[i - 1].group) items.push(node('div', 'pick__sep'));
            const opt = node('button', 'pick__opt');
            opt.type = 'button';
            opt.setAttribute('role', 'option');
            opt.setAttribute('aria-selected', String(x.value === value));
            opt.append(...face(x));
            opt.addEventListener('click', () => pick(x.value));
            items.push(opt);
        });
        if (!found.length) items.push(node('div', 'pick__empty', o.empty || 'Nothing matches'));
        if (o.footer && !q) items.push(node('div', 'pick__sep'), o.footer());
        list.replaceChildren(...items);
    }

    function pick(v) {
        const changed = v !== value;
        value = v;
        toggle(false);
        paintButton();
        if (changed && o.onPick) o.onPick(v);
    }

    // the arrows walk the options from the search field and between them; Enter in
    // the field takes the first one found
    function step(e) {
        const opts = [...list.querySelectorAll('.pick__opt')];
        const at = opts.indexOf(document.activeElement);
        if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
            e.preventDefault();
            const next = e.key === 'ArrowDown' ? (at < 0 ? 0 : Math.min(at + 1, opts.length - 1)) : at - 1;
            if (next < 0) search.hidden ? button.focus() : search.focus();
            else opts[next]?.focus();
        } else if (e.key === 'Enter' && e.target === search && opts.length) {
            e.preventDefault();
            opts[0].click();
        }
    }

    // while the list is open it takes a press elsewhere and Escape, before anything else hears the key
    const outside = (e) => { if (!root.isConnected || !root.contains(e.target)) toggle(false); };
    const escape = (e) => {
        if (!root.isConnected) return toggle(false);
        if (e.key !== 'Escape') return;
        e.stopPropagation();
        toggle(false);
        button.focus();
    };

    function toggle(on = !open) {
        if (on === open) return;
        open = on;
        button.setAttribute('aria-expanded', String(open));
        panel.hidden = !open;
        if (open) {
            search.value = '';
            search.hidden = o.options().length < (o.searchFrom || SEARCH_FROM);
            paintList();
            panel.scrollIntoView({ block: 'nearest' });
            (search.hidden ? list.querySelector('[aria-selected="true"]') || list.querySelector('.pick__opt') : search)?.focus();
            document.addEventListener('pointerdown', outside, true);
            window.addEventListener('keydown', escape, true);
        } else {
            document.removeEventListener('pointerdown', outside, true);
            window.removeEventListener('keydown', escape, true);
        }
    }

    button.addEventListener('click', () => toggle());
    search.addEventListener('input', paintList);
    panel.addEventListener('keydown', step);
    paintButton();

    return {
        el: root,
        button,
        pick,
        // the value from elsewhere, or the options changed: no onPick
        set(v) { value = v; paintButton(); if (open) paintList(); },
        refresh() { paintButton(); if (open) paintList(); },
        close: () => toggle(false),
    };
}
