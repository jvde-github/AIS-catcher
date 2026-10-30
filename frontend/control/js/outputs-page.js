// The Outputs pages: the community map, and one output of a list on a page of its own.
import { el, isActive, on } from './dom.js';
import { docLink } from './docs.js';
import { icon } from './icons.js';
import { ConfigForm, plainLayout, rowsLayout } from './config-form.js';
import { outputOf, outputRouting } from './flow-editor.js';
import { LIST_OUTPUTS, OUTPUT_TYPES, channelTitle, outputType } from './schema.js';

const listOf = v => (Array.isArray(v) ? v : []);

// ---- Community: laid out like an input. The page header holds the switch (with its status)
// and the routing line; below, the sharing key and Advanced.
export function communityLayout(form) {
    form.container.className = 'st';
    const item = form.data;
    const f = name => form.fields.find(x => x.name === name);
    const sharing = on(item.sharing);
    const head = form.container.closest('.st-main')?.querySelector('.st-main__head');
    head?.querySelectorAll('.st-head-extra').forEach(e => e.remove());
    const routing = el('div', 'st-routing st-head-extra');
    const box = el('input', '', { type: 'checkbox', checked: sharing, 'aria-label': 'Share with the community map',
        onChange: e => { form.updateValue(0, f('sharing'), e.target.checked); paintNotes(); } });
    const sw = el('label', 'st-switch st-switch--lg', {}, box, el('span', 'st-switch__track'));
    if (head) {
        // title and routing as a page header, ruled off from the sections below
        head.classList.add('st-main__head--page');
        head.firstElementChild.appendChild(routing);
    }
    const keyBtnLabel = () => (item.sharing_key || '').trim() ? 'Edit station details' : 'Get a key';
    // only while sharing without a key: an amber warning at the top, where the red one sits when off
    const anon = el('div', 'st-warn-note', { role: 'note' },
        el('span', 'st-warn-note__icon', { html: icon('warning', '') }),
        el('span', '', {}, 'Without a key the station is anonymous.'));
    // switched off: no key warning, a red line asking to share instead
    const off = el('div', 'st-warn-note st-warn-note--quiet', { role: 'note' },
        el('span', 'st-warn-note__icon', { html: icon('info', '') }),
        el('span', '', {}, 'Not sharing with the community map.'));
    const paintNotes = () => {
        const sharing = box.checked;
        off.hidden = sharing;
        anon.hidden = !sharing || !!(key.value || '').trim();
    };
    // a station without a key registers; one with a key opens its page on aiscatcher.org
    const keyBtn = el('button', 'st-btn', { type: 'button', onClick: () => window.open(key.value.trim()
        ? `https://www.aiscatcher.org/editstation/${key.value.trim()}` : 'https://aiscatcher.org/register', '_blank') },
        el('span', '', {}, keyBtnLabel()),
        el('span', '', { html: icon('external', 'st-icon st-icon--xs') }));
    const key = el('input', 'st-input st-input--mono', { id: 'st-share-key', type: 'text', value: item.sharing_key || '',
        placeholder: 'Paste your key', spellcheck: 'false', autocomplete: 'off',
        onInput: e => { form.updateValue(0, f('sharing_key'), e.target.value.trim()); keyBtn.firstChild.textContent = keyBtnLabel(); paintNotes(); } });
    const card = el('div', 'st-field st-share-key', { dataset: { field: 'sharing_key' } },
        el('label', 'st-label', { for: 'st-share-key' }, 'Sharing Key ', el('span', 'st-help', {}, '(optional)')),
        el('div', 'st-inline', {}, key, keyBtn));
    paintNotes();
    const { node, refresh } = form.renderSplit(item, 0, { exclude: new Set(['sharing', 'sharing_key', ...(form.config.excludeFor ? form.config.excludeFor(item) : [])]),
        key: 'community', onZones: form.routingLine(routing, 'in', item) });
    // one section: the red note when off, the switch, the key; Advanced in a section of its own
    node.classList.add('st-community');
    const adv = node.querySelector(':scope > .st-adv, :scope > .st-adv__body');
    const share = el('section', 'st-page__sec', {}, off, anon,
        el('div', 'st-share-toggle', {}, el('span', 'st-share-toggle__label', {}, 'Share with the Community Map'), sw),
        card);
    node.prepend(share);
    if (adv) node.appendChild(el('section', 'st-page__sec st-page__sec--adv', {}, adv));
    form.container.appendChild(node);
    refresh();
}

// ---- one output as a page: its name as the title, where it takes from, then its settings
export function outputFocusLayout(form, index) {
    form.container.className = 'st';
    const item = form.data[index];
    if (!item) {
        form.container.appendChild(el('div', 'st-empty', {}, el('strong', '', {}, `No ${form.config.title} output here`),
            el('span', '', {}, 'It was deleted. Pick another one from the list.')));
        return;
    }
    const fallback = () => form.config.nameOf ? form.config.nameOf({ ...item, description: '' }) : `${form.config.title} output`;
    const { head, name, title, onZones } = form.focusHead(item, index, { title: form.outputName(item) || fallback(), fallback, dir: 'in' });
    const { node, refresh } = form.renderSplit(item, index, { essentials: form.config.essentials || new Set(), key: 'output',
        exclude: new Set(['description', ...(form.config.excludeFor ? form.config.excludeFor(item) : [])]),
        flat: { title: null }, onZones });
    node.dataset.receiverCard = index;
    node._sync = () => { if (!form.outputName(item)) title.textContent = fallback(); if (name) name.querySelector('input').placeholder = fallback(); };
    form.container.append(head, ...(name ? [name] : []), node);
    refresh();
}

// The outputs one by one for the rail: the community map first, then each configured one, the open
// kind's from its form with its edits. `mark(key, active, o)`: what is wrong with one;
// `screenOpen`: the screen's page shows, so it is listed while off.
export function outputRailItems(cfg, page, { mark, screenOpen }) {
    const form = page?.form?.loaded ? page.form : null;
    const openIs = value => (form && page.type === value ? form : null);
    const sharingRow = openIs('sharing') ? form.data : cfg;
    const sharing = on(sharingRow.sharing);
    const items = [{ key: 'sharing', label: 'Community map', dot: sharing, ...mark('sharing', sharing, outputOf('sharing', sharingRow)) }];
    LIST_OUTPUTS.forEach(t => {
        const rows = openIs(t.value) ? form.data : listOf(cfg[t.value]);
        rows.forEach((item, i) => items.push({ key: `${t.value}:${i}`, label: (item.description || '').trim() || channelTitle(t.label, item),
            dot: isActive(item), ...mark(`${t.value}:${i}`, isActive(item), outputOf(t.value, item)) }));
    });
    const screenOn = cfg.screen !== undefined && !['0', 'none'].includes(String(cfg.screen).toLowerCase());
    if (screenOn || screenOpen) items.push({ key: 'screen', label: 'Screen', dot: screenOn });
    return items;
}

// a rail key as the kind and the row: "sharing", "screen", or a kind and its place ("udp:0"); "udp:new" while one is being added
function keyParts(key) {
    const [type, idx] = String(key ?? 'sharing').split(':');
    return { type: type || 'sharing', focus: idx === undefined || idx === 'new' ? null : Number(idx) };
}

// Add an output: the kinds there are, each with what it is for
export function renderAddOutput(host, add) {
    const grid = el('div', 'st-addtypes', {}, OUTPUT_TYPES.filter(t => t.add).map(t =>
        el('button', 'st-addtype', { type: 'button', onClick: () => add(t.value) }, el('strong', '', { text: t.label }), el('span', '', { text: t.add }))));
    host.replaceChildren(el('div', 'st-main', {},
        el('div', 'st-main__head', {}, el('div', '', {},
            el('h2', 'st-main__title', { text: 'Add an output' }),
            el('p', 'st-main__desc', { text: 'Home Assistant and webhooks are under Services.' }))),
        grid));
}

// One output to a page, the one the rail picks, in `host`. `key`: the rail's to start on; `mark(key,
// active, o)` what is wrong with one, as a `{ warn }` or `{ bad }`; `stationCount()`; `keepEdits()`
// whether unsaved edits may go; `open(tab, key)`, `follow(key)` the rail, `changed()` after an edit.
export function createOutputsPage(host, { key, mark, stationCount, keepEdits, open, follow, changed }) {
    const containerId = 'hub-output-container';
    const title = el('h2', 'st-main__title');
    const head = el('div', 'st-main__head', {}, el('div', '', {}, title));
    host.className = 'st st-single';
    host.replaceChildren(el('div', 'st-main', {}, head, el('div', '', { id: containerId })));
    let type = null, focus = null, form = null;

    const pageMark = (k, active, o) => {
        const m = mark(k, active, o);
        return m.warn ? { tone: 'warn', text: m.warn } : m.bad ? { tone: 'bad', text: m.bad } : null;
    };

    function draw() {
        const t = outputType(type);
        if (!t) return;
        // an output of a list carries its own heading
        head.hidden = !!t.list;
        title.textContent = t.value === 'sharing' ? 'Community map' : t.label;
        // what Community put in the header (its switch and routing line) leaves with it
        head.querySelectorAll('.st-head-extra').forEach(e => e.remove());
        head.classList.remove('st-main__head--page');
        head.querySelector('.st-doc')?.remove();
        head.append(docLink(t.docs));
        form?.destroy();
        form = t.list ? new ConfigForm({
            section: t.value,
            list: true,
            schema: t.schema,
            containerId,
            // one output to a page; the rows only while one is being added
            layout: f => (focus === null ? rowsLayout(f) : outputFocusLayout(f, focus)),
            title: t.label,
            kindOf: () => t.label,
            routing: outputRouting,
            problem: item => pageMark(`${t.value}:${form.data.indexOf(item)}`, isActive(item), outputOf(t.value, item)),
            // its zones are drawn in Flow, and a database's one station number for every row shows
            // only while set once there are stations
            excludeFor: item => ['zone', ...(t.value === 'db' && stationCount() > 1 && !item.station_id ? ['station_id'] : [])],
            nameOf: item => channelTitle(t.label, item),
            docs: () => docLink(t.docs),
            openFlow: () => open('flow'),
            // what an output needs to work, up front; the rest in Advanced
            essentials: new Set(['description', ...t.essentials]),
            onChange: changed
        }) : new ConfigForm({
            schema: t.schema,
            containerId,
            layout: t.value === 'sharing' ? communityLayout : plainLayout,
            excludeFor: () => ['sharing_zone'],
            routing: outputRouting,
            problem: t.value === 'sharing' ? item => pageMark('sharing', on(item.sharing), outputOf('sharing', item)) : null,
            openFlow: () => open('flow'),
            onChange: changed
        });
    }

    // show the output of this key; false when the switch was refused for unsaved edits
    function show(k) {
        const parts = keyParts(k);
        focus = parts.focus;
        if (type === parts.type) {
            if (form?.loaded) form.render();
            return true;
        }
        if (type !== null && !keepEdits()) return false;
        type = parts.type;
        draw();
        return true;
    }

    show(key);

    return {
        get form() { return form; },
        get type() { return type; },
        show,
        // a new row on the page of its kind
        addRow() {
            const m = form;
            m.ready.then(() => {
                if (!m.loaded || m !== form) return;
                focus = m.data.length;
                follow(`${type}:${focus}`);
                m.addItem();
            });
        },
        destroy() {
            form?.destroy();
            form = null;
        }
    };
}
