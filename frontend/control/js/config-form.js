// A section of the configuration as a page's form: read, edited and saved through its editor. The
// page's `layout(form)` draws it from the pieces here: the essentials and the Advanced settings
// (renderSplit), the head of a page of one item, the routing line, the ⋯ menu. The options are the
// layout's: `containerId` where it draws and what the editor is called, `schema`, `section` the
// part of the file it holds (none: settings at the top of the file), `list` a list of rows.
import { el, fromHtml, highlightJson, isActive, moreMenu } from './dom.js';
import { icon } from './icons.js';
import { hub } from './hub.js';
import { App, ConfigStore, getNested, registerEditor, setNested } from './config-store.js';
import { Renderer } from './config-widgets.js';
import { ZoneColors } from './zone-modal.js';

const getZoneCss = ZoneColors.css;

export class ConfigForm {
    constructor(config) {
        this.config = config;
        this.container = document.getElementById(config.containerId);
        this.fields = Object.values(config.schema);
        this.data = config.list ? [] : {};
        this.loaded = false;

        if (!this.container) return;

        this.editor = registerEditor(config.containerId, { save: () => this.save(), data: () => this.data });
        // settles once the data is drawn, or the page says it could not be read
        this.ready = this.load();
    }

    load() {
        this.container.textContent = '';
        this.container.appendChild(el('div', 't-center sys-empty', {},
            el('div', 'spinner icon-lg center-block'),
            el('p', 't-muted', {}, 'Loading...')));

        return this.loadData().then(() => {
            this.loaded = true;
            this.render();
            this.updateJsonDebug();
        }).catch((e) => {
            console.error('Configuration page failed:', e);
            this.container.textContent = '';
            this.container.appendChild(el('div', 't-center sys-empty', {},
                el('p', 't-muted', {}, 'Failed to load configuration' + (e && e.message ? ': ' + e.message : '')),
                el('button', 'btn btn-primary', {
                    type: 'button', onClick: () => this.load()
                }, 'Retry')));
        });
    }

    async loadData() {
        const cfg = await ConfigStore.fetch();
        if (!cfg || typeof cfg !== 'object') throw new Error('Invalid configuration');
        const { section, list } = this.config;
        this.data = section ? (cfg[section] || (list ? [] : {})) : cfg;
        if (!list)
            this.fields.forEach(f => {
                if (f.jsonpath) {
                    if (getNested(this.data, f.jsonpath) === undefined)
                        setNested(this.data, f.jsonpath, f.defaultValue);
                } else {
                    this.data[f.name] ??= f.defaultValue;
                }
            });
    }

    // the page's layout draws it; the JSON view goes under what it returns, or under the whole form
    render() {
        // the JSON pane may sit inside the container: take it out before clearing, so it keeps its
        // open state and content
        const kept = this.container.querySelector('[data-json-section]');
        if (kept) this.container.parentElement.appendChild(kept);
        this.container.innerHTML = '';
        const jsonHost = this.config.layout(this);
        this.ensureJsonUI();
        const json = jsonHost && this.container.parentElement.querySelector('[data-json-section]');
        if (json) jsonHost.appendChild(json);
        this.config.onChange?.(this);
    }

    // ---- essentials and Advanced: the few fields most people need in a card, the rest collapsed
    fieldValue(item, f) { return f.jsonpath ? getNested(item, f.jsonpath) : item[f.name]; }
    // a value counts as changed when it differs from the schema's default; empty and unset are alike,
    // and 10110 is "10110"
    isChanged(item, f) {
        const norm = v => (v === undefined || v === null || v === '' || (Array.isArray(v) && !v.length)) ? null : typeof v === 'object' ? JSON.stringify(v) : String(v);
        const v = this.fieldValue(item, f);
        return v !== undefined && norm(v) !== norm(f.defaultValue);
    }
    // The essentials up front (`flat`: a plain section under its title, no card around them),
    // then `lead` (the blocks the page puts before the rest), then the other settings in titled
    // sections: in an Advanced fold, or open when the page has seven settings or fewer, as a
    // fold would hide half of very little.
    renderSplit(item, index, { essentials = new Set(), exclude = new Set(), key = 'main', onZones = null, flat = null, lead = [] } = {}) {
        const panel = el('div', 'st-panel');
        const card = el('div', flat ? 'st-flat__fields st-fieldset' : 'st-card st-fieldset');
        const rows = el('div', 'st-adv__body st-secs');
        // a field's section is the schema's, else what the host says (the receiver's fields have
        // none), else Zones or Other
        const sections = new Map();
        const sectionFor = field => {
            const name = field.section || this.config.groupOf?.(field) || 'Other';
            if (!sections.has(name)) {
                const grid = el('div', 'st-sec__grid st-fieldset');
                const sec = el('section', 'st-sec', {}, el('h4', 'st-section__title', {}, name), grid);
                sections.set(name, { sec, grid });
                rows.appendChild(sec);
            }
            return sections.get(name).grid;
        };
        const fields = this.fields.filter(f => !exclude.has(f.name) && f.name !== 'active');
        const folded = fields.filter(f => !essentials.has(f.name));
        // how many settings apply to the item as it is; a choice that changes it draws the page again
        const applying = list => list.filter(f => this.applies(f, item)).length;
        const isFlat = () => applying(fields) + lead.length <= 7;
        const flatAll = isFlat();
        const sum = el('span', 'st-adv__sum');
        let adv = null;
        if (!flatAll && folded.length) {
            this.advOpen = this.advOpen || new Set();
            const openKey = `${key}|${index}`;
            adv = el('details', 'st-adv', this.advOpen.has(openKey) ? { open: '' } : {},
                el('summary', '', { html: icon('chevronRight', 'st-adv__chev') + 'Advanced settings' }, sum),
                rows);
            adv.addEventListener('toggle', () => {
                if (adv.open) this.advOpen.add(openKey); else this.advOpen.delete(openKey);
            });
        }
        const advanced = [];
        const refresh = () => {
            if (isFlat() !== flatAll) return this.render();
            Renderer.updateVisibility(panel, item);
            // the summary counts the settings that differ from their defaults, among those that apply
            const shown = advanced.filter(({ node }) => !node.classList.contains('hidden'));
            const changed = shown.filter(({ f }) => this.isChanged(item, f)).length;
            // the zones are edited in Flow: the row says them
            const zoneList = Array.isArray(item.zone) ? item.zone : Array.isArray(item.sharing_zone) ? item.sharing_zone : [];
            sum.innerHTML = '';
            // only how much differs from the defaults; nothing when all are at their defaults
            if (changed) sum.appendChild(el('b', '', {}, `${changed} changed`));
            // the zones as the pills they are everywhere else, in their own colours: under the
            // header when the caller has a place for them, else on the summary line
            if (onZones) onZones(zoneList);
            else zoneList.forEach(z => {
                const pill = el('span', 'st-chip st-chip--zone', {}, z);
                pill.style.setProperty('--z', getZoneCss(z));
                sum.appendChild(pill);
            });
            if (adv) adv.hidden = !shown.length;
            for (const { sec, grid } of sections.values())
                sec.hidden = ![...grid.children].some(c => !c.classList.contains('hidden'));
            for (const { node, f } of advanced) {
                const ch = this.isChanged(item, f);
                node.classList.toggle('st-opt--changed', ch);
                if (node._reset) node._reset.hidden = !ch;
            }
        };
        // a choice that empties another field (a new kind of device, its serial) empties its control too
        const update = (fld, val) => {
            this.updateValue(index, fld, val);
            (fld.clears || []).forEach(name => {
                const f = this.fields.find(x => x.name === name);
                const ctrl = panel.querySelector(`[data-field="${name}"] input`);
                if (f) this.updateValue(index, f, '');
                if (ctrl) ctrl.value = '';
            });
        };
        fields.forEach(field => {
            const node = Renderer.renderField(field, item, update, refresh, this.config);
            if (essentials.has(field.name)) return card.appendChild(node);
            // one setting per line: label and help on the left, the control on the right;
            // lists and the engines take the whole line under their label
            const simple = ['text', 'number', 'select', 'toggle', 'password', 'integer-select', 'off-number', 'switch-integer'].includes(field.type);
            node.classList.add('st-opt');
            node.dataset.stField = field.name;
            if (!simple) node.classList.add('st-opt--wide');
            // a switch needs little room: its help can run almost the whole line
            if (field.type === 'toggle') node.classList.add('st-opt--switch');
            // a field without a label of its own (the engines list) is titled by its group
            if (!field.label && typeof field.advanced === 'string')
                node.prepend(el('label', 'st-label', {}, field.advanced));
            // a way back to the default when changed; without a default the key is removed, as a cleared field does
            node._reset = el('button', 'st-reset', { type: 'button', hidden: '', onClick: () => {
                this.updateValue(index, field, field.defaultValue === undefined ? undefined : JSON.parse(JSON.stringify(field.defaultValue)));
                this.render();
            } }, 'Reset');
            (node.querySelector(':scope > .st-label') || node).appendChild(node._reset);
            node.removeAttribute('style');
            sectionFor(field).appendChild(node);
            advanced.push({ node, f: field });
        });
        if (card.children.length) {
            if (flat) panel.appendChild(el('div', 'st-flat' + (flat.title ? '' : ' st-flat--bare'), {},
                flat.title ? el('div', 'st-flat__head', {}, el('h3', 'st-section__title', {}, flat.title)) : null, card));
            else panel.appendChild(card);
        }
        panel.append(...lead);
        if (adv) panel.appendChild(adv);
        else if (folded.length) panel.appendChild(rows);
        return { node: panel, refresh };
    }

    // the name, a field under the title: the title shows it, or the device's own name until one is given
    nameField(item, field, index, fallback, title) {
        const input = el('input', 'st-input', { type: 'text', value: item.description || '', placeholder: fallback(), spellcheck: 'false', autocomplete: 'off',
            onInput: e => { this.updateValue(index, field, e.target.value); title.textContent = e.target.value.trim() || fallback(); } });
        return el('div', 'st-stations__row st-name', {}, el('label', 'st-field st-stations__field st-stations__grow', {}, el('span', 'st-label', {}, 'Name'), input));
    }

    // The head of a page of one item: its name as the title, the routing line (`dir`) where the
    // host counts routes, the docs, the Active switch and the ⋯ menu; under it the field that
    // names it. `fallback` is the title while no name is given.
    focusHead(item, index, { title: text, fallback, dir }) {
        const activeField = this.fields.find(f => f.name === 'active');
        const descField = this.fields.find(f => f.name === 'description');
        const title = el('h2', 'st-main__title', {}, text);
        const kind = this.config.kindOf ? this.config.kindOf(item) : '';
        const tag = kind ? el('span', 'st-main__kind', {}, kind) : null;
        const showKind = () => { if (tag) tag.hidden = title.textContent.startsWith(kind); };
        const routing = this.config.routing ? el('div', 'st-routing') : null;
        const head = el('div', 'st-main__head', {},
            el('div', 'st-main__headtext', {},
                el('div', 'st-main__titlerow', {}, title, tag), routing),
            el('div', 'st-main__actions', {},
                this.config.docs ? this.config.docs(item) : null,
                activeField ? el('label', 'st-switch', {},
                    el('input', '', { type: 'checkbox', checked: this.isActive(item), onChange: e => { this.updateValue(index, activeField, e.target.checked); this.render(); } }),
                    el('span', 'st-switch__track'), 'Active') : null,
                this.menu(text, index)));
        const name = descField ? this.nameField(item, descField, index, fallback, title) : null;
        showKind();
        name?.addEventListener('input', showKind);
        return { head, name, title, onZones: routing ? this.routingLine(routing, dir, item) : null };
    }

    // The routing line: "Sends to" (an input) or "Receives from" (an output), the zones as pills, and
    // a link counting the other side that matches, which opens Flow. Returns a painter for the zones.
    routingLine(box, dir, item) {
        let seq = 0;
        return zones => {
            const mine = ++seq;
            const parts = [el('span', 'st-routing__label', { html: icon(dir === 'out' ? 'arrowRight' : 'arrowLeft', 'st-routing__icon') + (dir === 'out' ? 'Sends to' : 'Receives from') })];
            zones.forEach(z => { const p = el('span', 'st-zone', {}, z); p.style.setProperty('--z', getZoneCss(z)); parts.push(p); });
            const sep = () => el('span', 'st-routing__sep', { 'aria-hidden': 'true' }, '·');
            const link = el('button', 'st-routing__link', { type: 'button', onClick: () => this.config.openFlow?.() }, '…');
            if (zones.length) parts.push(sep());
            parts.push(link);
            box.replaceChildren(...parts);
            Promise.resolve(this.config.routing ? this.config.routing(item, dir) : null).then(n => {
                if (mine !== seq) return;
                const what = dir === 'out' ? 'output' : 'input';
                if (n === null || n === undefined) link.remove();
                else link.textContent = n === 0 ? `no ${what}s` : n === 1 ? `1 ${what}` : `${n} ${what}s`;
                const problem = this.config.problem?.(item);
                if (problem) {
                    if (n === 0) link.classList.add('st-routing__link--warn');
                    box.append(el('p', 'st-routing__problem st-routing__problem--' + problem.tone, {}, problem.text));
                }
            });
        };
    }

    // the ⋯ menu of an input or output: Duplicate, and Delete behind a confirmation
    menu(name, index) {
        return moreMenu(`More actions for ${name}`, [['Duplicate', () => this.duplicateItem(index)], ['Delete…', () => this.removeItem(index), true]]);
    }

    // whether a row is switched on, where its kind can be
    isActive(item) {
        return !this.fields.some(x => x.name === 'active') || isActive(item);
    }
    outputName(item) { return (item.description || '').trim(); }

    // Each form owns its own JSON disclosure (ids keyed by containerId)
    // so sibling sections in the same view don't fight over one shared box.
    jsonPreId() { return 'json-pre-' + this.config.containerId; }

    ensureJsonUI() {
        const parent = this.container.parentElement;
        if (!parent) return;

        const contentId = 'json-content-' + this.config.containerId;
        const chevronId = 'chevron-' + this.config.containerId;
        const toggle = () => {
            toggleJsonContent(contentId, chevronId);
            if (this.jsonStale) this.updateJsonDebug();
        };

        const existingPre = document.getElementById(this.jsonPreId());
        if (existingPre) {
            // shared pane across sub-tabs: re-point the toggle at this manager
            const section = existingPre.closest('[data-json-section]');
            const btn = section && section.querySelector('button');
            if (btn) btn.onclick = () => { toggle(); btn.setAttribute('aria-expanded', String(!document.getElementById(contentId).classList.contains('hidden'))); };
            return;
        }

        // the same disclosure row as "Advanced settings": a chevron that turns, a quiet label, a rule above
        const toggleDiv = el('div', 'st st-json sys-pane-inner', { dataset: { jsonSection: '' } });
        const btn = el('button', 'st-json__toggle', { type: 'button', 'aria-expanded': 'false' });
        btn.onclick = () => { toggle(); btn.setAttribute('aria-expanded', String(!document.getElementById(contentId).classList.contains('hidden'))); };

        const chevron = fromHtml(icon('chevronRight', 'st-adv__chev'));
        chevron.id = chevronId;

        btn.appendChild(chevron);
        btn.appendChild(el('span', '', {}, 'JSON'));

        const contentDiv = el('div', 'hidden', { id: contentId });
        const pre = el('pre', 'term-pane sys-json-pre', {
            id: this.jsonPreId(), readonly: '', style: 'max-height: 300px;'
        });

        contentDiv.appendChild(pre);
        toggleDiv.appendChild(btn);
        toggleDiv.appendChild(contentDiv);
        parent.appendChild(toggleDiv);
    }

    markDirty() {
        this.editor.touched();
        this.updateJsonDebug();
    }

    updateValue(index, field, value) {
        const target = this.config.list ? this.data[index] : this.data;
        if (field.jsonpath) setNested(target, field.jsonpath, value);
        else target[field.name] = value;
        // a kind of device chosen: its settings start at their defaults, as a new row's other settings do
        if (this.fields.some(f => f.dependsOn && f.dependsOn.field === field.name))
            this.fields.forEach(f => {
                if (f.defaultValue === undefined || this.fieldValue(target, f) !== undefined || !this.applies(f, target)) return;
                const v = JSON.parse(JSON.stringify(f.defaultValue));
                if (f.jsonpath) setNested(target, f.jsonpath, v); else target[f.name] = v;
            });
        this.markDirty();
        this.container.querySelector(`[data-receiver-card="${index}"]`)?._sync?.();
        this.config.onChange?.(this);
    }

    addItem() {
        const newItem = {};
        this.fields.forEach(f => {
            if (f.defaultValue !== undefined && this.applies(f, newItem)) {
                if (f.jsonpath) setNested(newItem, f.jsonpath, f.defaultValue);
                else newItem[f.name] = f.defaultValue;
            }
        });
        this.data.push(newItem);
        this.selected = this.data.length - 1;
        this.openItems?.add(newItem);
        this.freshItems?.add(newItem);
        this.render();
        this.markDirty();
    }

    duplicateItem(index) {
        const copy = JSON.parse(JSON.stringify(this.data[index]));
        if (copy.description) copy.description += ' (copy)';
        this.data.splice(index + 1, 0, copy);
        this.selected = index + 1;
        this.openItems?.add(copy);
        this.freshItems?.add(copy);
        this.render();
        this.markDirty();
    }

    removeItem(index) {
        const name = this.config.titleOf ? this.config.titleOf(this.data[index], index) : this.outputName(this.data[index]);
        const what = this.config.what || `${this.config.title} output`;
        if (confirm(name ? `Delete the ${what} “${name}”?` : `Are you sure you want to remove this ${this.config.title}?`)) {
            this.data.splice(index, 1);
            this.render();
            this.markDirty();
        }
    }

    // whether a field applies to an item, as its chain of dependsOn says; with `onlyKind`, only
    // the links on the kind of device count (an Airspy keeps its VGA while the gain mode is not free)
    applies(field, item, onlyKind = false, depth = 0) {
        const dep = field.dependsOn;
        if (!dep || depth > 6) return true;
        const parent = this.fields.find(f => f.name === dep.field);
        if (parent && !this.applies(parent, item, onlyKind, depth + 1)) return false;
        if (onlyKind && dep.field !== 'input') return true;
        let val = parent ? this.fieldValue(item, parent) : item[dep.field];
        if (val === undefined && parent) val = parent.defaultValue;
        const v = String(val).toLowerCase();
        return Array.isArray(dep.value) ? dep.value.some(x => String(x).toLowerCase() === v) : String(dep.value).toLowerCase() === v;
    }

    // a row as it is saved: the settings of another kind of device leave with it, and a block they leave empty
    pruneFor(item) {
        const out = JSON.parse(JSON.stringify(item));
        this.fields.forEach(f => {
            if (!f.jsonpath || !f.dependsOn || this.applies(f, item, true)) return;
            const path = f.jsonpath.split('.');
            let o = out;
            for (let i = 0; i < path.length - 1 && o; i++) o = o[path[i]];
            if (o && typeof o === 'object') delete o[path[path.length - 1]];
            if (path.length > 1) {
                const block = out[path[0]];
                if (block && typeof block === 'object' && !Array.isArray(block) && !Object.keys(block).length) delete out[path[0]];
            }
        });
        return out;
    }

    applyDataTo(target, { forSave = false } = {}) {
        if (this.config.section) {
            target[this.config.section] = forSave && this.config.list ? this.data.map(i => this.pruneFor(i)) : this.data;
        } else {
            this.fields.forEach(f => {
                const val = f.jsonpath ? getNested(this.data, f.jsonpath) : this.data[f.name];
                if (val === undefined) return;
                if (f.jsonpath) setNested(target, f.jsonpath, val);
                else target[f.name] = val;
            });
        }
    }

    updateJsonDebug() {
        const pre = document.getElementById(this.jsonPreId());
        if (!pre) return;
        const content = document.getElementById('json-content-' + this.config.containerId);
        if (content && content.classList.contains('hidden')) {
            this.jsonStale = true;
            return;
        }
        this.jsonStale = false;
        const cfg = {};
        this.applyDataTo(cfg);
        const text = JSON.stringify(cfg, null, 2);
        pre.innerHTML = highlightJson(text);
    }

    async save() {
        const problem = this.config.check?.(this.data);
        if (problem) {
            App.notify('error', problem);
            return;
        }
        // the rows saved are rows like any other
        this.freshItems = new WeakSet();
        if (!await this.editor.write(cfg => this.applyDataTo(cfg, { forSave: true }), 'the configuration')) return;
        App.notify('success', 'Configuration saved');
        hub.configSaved();
    }

    // out of the registry, as its page is gone
    destroy() {
        this.editor?.destroy();
    }
}

const toggleJsonContent = (contentId, chevronId) => {
    const c = document.getElementById(contentId);
    const i = document.getElementById(chevronId);
    if (c) {
        const hidden = c.classList.toggle('hidden');
        if (i) i.classList.toggle('rotate-90', !hidden);
    }
};

// a page whose few settings are all essential: no Advanced group at all
export function plainLayout(form) {
    form.container.className = 'st';
    const { node, refresh } = form.renderSplit(form.data, 0, { essentials: new Set(form.fields.map(f => f.name)), key: 'plain', flat: { title: null } });
    form.container.appendChild(node);
    refresh();
}

// one collapsible row per item, its form inside
export function rowsLayout(form) {
    form.openItems = form.openItems || new WeakSet();
    form.freshItems = form.freshItems || new WeakSet();
    const list = el('ul', 'st-outputs');
    if (!form.data.length) {
        const [what, how] = form.config.empty || [`No ${form.config.title} outputs yet`, `Add one to forward messages over ${form.config.title}.`];
        list.appendChild(el('li', 'st-empty', {}, el('strong', '', {}, what), el('span', '', {}, how)));
    }
    form.data.forEach((item, index) => {
        const open = form.openItems.has(item);
        const li = el('li', 'st-out', { dataset: { open: String(open), off: String(!form.isActive(item)), receiverCard: index } });
        const name = form.outputName(item);
        const unnamed = () => (form.config.nameOf ? form.config.nameOf(item) : `Unnamed ${form.config.title} output`);
        const nameEl = el('span', 'st-out__name' + (name ? '' : ' st-out__name--empty'), {}, name || unnamed());
        const expand = el('button', 'st-out__expand', { type: 'button', 'aria-expanded': String(open),
            onClick: () => {
                if (form.openItems.has(item)) form.openItems.delete(item); else form.openItems.add(item);
                form.render();
            } },
            el('span', 'st-out__chev', { html: icon('chevronRight', '') }),
            nameEl,
            // a zoned output says so in its header: a small tinted box with a dot per zone, the zone
            // pill in miniature (a bare coloured dot would read as status, as it does elsewhere)
            ...(Array.isArray(item.zone) ? item.zone : []).map(z => {
                const d = el('span', 'st-zone-dot', { title: `Zone ${z}`, 'aria-label': `zone ${z}` });
                d.style.setProperty('--z', getZoneCss(z));
                return d;
            }),
            form.freshItems.has(item) ? el('span', 'st-chip st-chip--new', {}, 'New') : null);
        const meta = el('span', 'st-out__meta');
        const activeField = form.fields.find(f => f.name === 'active');
        const sw = activeField ? el('label', 'st-switch', { title: 'Active' },
            el('input', '', { type: 'checkbox', checked: form.isActive(item),
                onChange: e => { form.updateValue(index, activeField, e.target.checked); li.dataset.off = String(!e.target.checked); } }),
            el('span', 'st-switch__track'),
            el('span', 'st-sr', {}, 'Active')) : null;
        const menu = form.menu(name || 'this output', index);
        li.appendChild(el('div', 'st-out__row', {}, expand, meta, sw, menu));
        if (open) {
            const routing = el('div', 'st-routing st-routing--body');
            const { node, refresh } = form.renderSplit(item, index, { essentials: form.config.essentials || new Set(), key: 'output', flat: { title: null },
                onZones: form.config.routing ? form.routingLine(routing, 'in', item) : null });
            if (form.config.routing) node.prepend(routing);
            form.config.extra?.(node, item, index);
            li.appendChild(el('div', 'st-out__body', {}, node));
            // the name and the summary follow the form as it is typed in
            li._sync = () => {
                const n = form.outputName(item);
                nameEl.textContent = n || unnamed();
                nameEl.classList.toggle('st-out__name--empty', !n);
            };
            list.appendChild(li);
            refresh();
        } else list.appendChild(li);
    });
    form.container.appendChild(list);
}
