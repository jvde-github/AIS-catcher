// A field of a form: its label, its control by its type (switch, list, slider, the engines), and
// whether it shows, as the other fields say.
import { el, fromHtml, on } from './dom.js';
import { icon } from './icons.js';
import { debounce, getNested } from './config-store.js';
import { openZoneModal, zoneChip } from './zone-modal.js';

const parseNumberLike = (str) => {
    const s = String(str).trim();
    const m = /^([+-]?(?:\d+\.?\d*|\.\d+))([kK]?)$/.exec(s);
    if (!m) return null;
    let n = parseFloat(m[1]);
    if (!isFinite(n)) return null;
    if (m[2]) n *= 1000;
    return n;
};

const parseInteger = value => {
    if (typeof value === 'number') return Math.floor(value);
    const n = parseNumberLike(value);
    return n === null ? 0 : Math.floor(n);
};

const Styles = {
    input: 'st-input',
    select: 'select',
    toggle: 'toggle',
    slider: 'slider',
    sliderContainer: 'slider-row',
    sliderDisplay: 'slider-value',
    label: 'st-label',
    description: 'st-help',
    deleteBtn: 'btn btn-danger',
};

function sliderInput(field, currentValue, onUpdate, opts) {
    const isOff = opts.isOff(currentValue);
    const numVal = isOff ? opts.fallback : opts.parse(currentValue);
    const display = el('span', Styles.sliderDisplay, {}, isNaN(numVal) ? 0 : numVal);

    const commit = debounce(onUpdate, 200);
    const slider = el('input', Styles.slider, {
        type: 'range',
        min: field.min, max: field.max, step: field.step || opts.defaultStep,
        value: isNaN(numVal) ? opts.fallback : numVal,
        onInput: (e) => {
            display.textContent = e.target.value;
            if (isActive()) commit(opts.parse(e.target.value));
        }
    });

    const children = [slider, display];
    if (opts.unit) children.push(el('span', 't-small t-muted', {}, opts.unit));
    const sliderContainer = el('div', Styles.sliderContainer + (isOff ? ' hidden' : ''), {}, ...children);

    const setActive = (active) => {
        if (!active) commit.cancel();
        sliderContainer.classList.toggle('hidden', !active);
        onUpdate(active ? opts.parse(slider.value) : opts.offValue);
    };

    let isActive, control;
    if (opts.control === 'toggle') {
        const checkbox = el('input', Styles.toggle, {
            type: 'checkbox',
            checked: !isOff,
            onChange: (e) => setActive(e.target.checked)
        });
        isActive = () => checkbox.checked;
        control = el('label', 'toggle-slot', {}, checkbox);
    } else {
        const select = el('select', `${Styles.input} ${Styles.select}`, {
            onChange: (e) => setActive(e.target.value === 'custom')
        },
            el('option', '', { value: opts.offOption.value, selected: isOff }, opts.offOption.label),
            el('option', '', { value: 'custom', selected: !isOff }, 'Custom')
        );
        isActive = () => select.value === 'custom';
        control = select;
    }
    return el('div', '', {}, control, sliderContainer);
}

export const Renderer = {
    // dataContext: the row the field is on; host: the options of the form drawing it, for what a
    // control needs from its page
    createInput(field, currentValue, onUpdate, dataContext = null, host = null) {
        const type = field.type;

        if (type === 'select') {
            const valStr = currentValue !== undefined && currentValue !== null ? String(currentValue) : '';
            return el('div', 'relative', {},
                el('select', `${Styles.input} ${Styles.select}`, {
                    onChange: e => onUpdate(e.target.value)
                },
                    ...field.options.map(opt => el('option', '', {
                        value: opt.value,
                        selected: String(opt.value).toLowerCase() === valStr.toLowerCase()
                    }, opt.label))
                )
            );
        }

        if (type === 'toggle') {
            // match Styles.input height so the pill centers on the same midline as inputs in the row
            return el('label', 'toggle-slot', {},
                el('input', Styles.toggle, {
                    type: 'checkbox',
                    checked: on(currentValue),
                    onChange: e => onUpdate(e.target.checked)
                })
            );
        }

        if (type === 'integer-select') {
            const parsedCurrent = parseInteger(currentValue);
            const isPreset = field.presets && field.presets.some(p => parseInteger(p.value) === parsedCurrent);

            // Free text so "1536K" can be typed; it is stored and shown as the plain number
            // (1536000), the unit being whatever the field is in (Hz for rates). A value
            // below the minimum whose ×1000 fits gets a hint rather than a silent save.
            const hint = el('div', 'field-hint hidden');
            const check = (input, raw) => {
                let cleaned = raw.replace(/[^0-9kK]/g, '').replace(/[kK](?=.)/g, '');
                if (/[kK]$/.test(cleaned)) cleaned = cleaned.slice(0, -1) + (cleaned.length > 1 ? '000' : '');
                if (cleaned !== input.value) input.value = cleaned;
                const n = parseNumberLike(cleaned);
                const inRange = n !== null && (field.min === undefined || n >= field.min) && (field.max === undefined || n <= field.max);
                input.classList.toggle('invalid', !inRange);
                hint.classList.add('hidden');
                if (inRange) return n;
                if (n !== null) {
                    const kFits = field.min !== undefined && n < field.min && n * 1000 >= field.min && (field.max === undefined || n * 1000 <= field.max);
                    hint.textContent = kFits
                        ? `${n} is below the minimum ${field.min}. Did you mean ${n}K (${n * 1000})?`
                        : `Allowed range: ${field.min ?? '…'} – ${field.max ?? '…'}`;
                    hint.classList.remove('hidden');
                }
                return null;
            };
            const customInput = el('input', Styles.input + (isPreset ? ' hidden' : ''), {
                type: 'text',
                inputMode: 'numeric',
                value: isPreset ? '' : parsedCurrent,
                placeholder: field.placeholder || 'Enter custom value...',
                onInput: (e) => {
                    const n = check(e.target, e.target.value);
                    if (n !== null) onUpdate(Math.round(n));
                }
            });
            if (!isPreset) check(customInput, String(parsedCurrent ?? ''));

            const select = el('div', 'relative', {},
                el('select', `${Styles.input} ${Styles.select}`, {
                    onChange: (e) => {
                        if (e.target.value === 'custom') {
                            customInput.classList.remove('hidden');
                            check(customInput, customInput.value);
                            customInput.focus();
                        } else {
                            customInput.classList.add('hidden');
                            onUpdate(parseInt(e.target.value, 10));
                        }
                    }
                },
                    ...(field.presets || []).map(p => el('option', '', { value: p.value, selected: parseInteger(p.value) === parsedCurrent }, p.label)),
                    el('option', '', { value: 'custom', selected: !isPreset }, 'Custom Value...')
                )
            );
            return el('div', 'col', {}, select, customInput, hint);
        }

        if (type === 'auto-float' || type === 'auto-integer') {
            return sliderInput(field, currentValue, onUpdate, {
                isOff: v => String(v).toLowerCase() === 'auto',
                offValue: 'auto',
                offOption: { value: 'auto', label: 'Auto' },
                parse: parseFloat,
                defaultStep: type === 'auto-integer' ? 1 : 0.1,
                fallback: field.min || 0
            });
        }

        if (type === 'off-number') {
            return sliderInput(field, currentValue, onUpdate, {
                isOff: v => v === false || v === 'false',
                offValue: false,
                offOption: { value: 'off', label: 'Off' },
                parse: parseFloat,
                defaultStep: 1,
                fallback: field.defaultNumber || field.min || 0,
                unit: field.unit
            });
        }

        if (type === 'switch-integer') {
            return sliderInput(field, currentValue, onUpdate, {
                isOff: v => v === false || v === 'false',
                offValue: false,
                control: 'toggle',
                parse: v => parseInt(v, 10),
                defaultStep: 1,
                fallback: field.defaultInteger || field.min || 0,
                unit: field.unit || 's'
            });
        }

        if (type === 'engines') {
            const list = Array.isArray(currentValue) && currentValue.length
                ? currentValue.map(e => (e && typeof e === 'object') ? { ...e } : {})
                : [{}];
            const stationPicker = host?.stationPicker;

            const wrapper = el('div', 'col');
            const rows = el('div', 'col col-loose');

            const push = () => onUpdate(list.map(e => ({ ...e })));

            const addBtn = el('button', 'chip sys-chip-btn self-start', {
                type: 'button',
                onClick: () => { list.push({}); push(); render(); }
            }, fromHtml(icon('add', 'icon-sm')), 'Add Engine');

            function render() {
                rows.innerHTML = '';
                list.forEach((entry, i) => {
                    const type = String(entry.type || 'auto').toLowerCase();
                    const select = el('select', `${Styles.input} ${Styles.select}`, {
                        onChange: e => {
                            if (e.target.value === 'auto') delete list[i].type;
                            else list[i].type = e.target.value;

                            // drop settings the new type does not accept
                            const ok = (field.settings || [])
                                .filter(st => !st.types || st.types.includes(e.target.value))
                                .map(st => st.name);
                            if (list[i].settings) {
                                (field.settings || []).forEach(st => {
                                    if (!ok.includes(st.name)) delete list[i].settings[st.name];
                                });
                                if (!Object.keys(list[i].settings).length) delete list[i].settings;
                            }
                            push();
                            render();
                        }
                    });
                    (field.options || []).forEach(o => select.appendChild(
                        el('option', '', { value: o.value, selected: type === o.value }, o.label)));

                    const shown = (field.settings || []).filter(st => !st.types || st.types.includes(type)).map(st => st.name);
                    const tuned = Object.keys(entry.settings || {}).some(k => k !== 'station' && !shown.includes(k));
                    const row = el('div', 'box col', {},
                        el('div', 'row', {},
                            el('div', 'grow relative', {}, select),
                            tuned ? el('span', 't-small t-subtle t-nowrap', {}, 'tuned') : null,
                            el('button', Styles.deleteBtn, {
                                type: 'button',
                                title: 'Remove engine',
                                disabled: list.length < 2,
                                style: list.length < 2 ? 'visibility:hidden' : '',
                                onClick: () => { list.splice(i, 1); push(); render(); }
                            }, fromHtml(icon('bin', 'icon-md')))));

                    // an engine may carry a station of its own, so two engines of one input are told apart
                    if (stationPicker && (list.length > 1 || entry.settings?.station !== undefined)) {
                        const picked = id => {
                            if (id === undefined) {
                                if (entry.settings) { delete entry.settings.station; if (!Object.keys(entry.settings).length) delete entry.settings; }
                            } else (entry.settings = entry.settings || {}).station = id;
                            push();
                        };
                        row.appendChild(el('div', 'col', {}, el('span', 't-small t-subtle', {}, 'Station'),
                            stationPicker(entry.settings?.station, picked, { none: 'Same as the input' })));
                    }

                    // the receiver's zones apply to every engine, shown greyed
                    if (list.length > 1 || (Array.isArray(entry.zone) && entry.zone.length)) {
                        const zones = Array.isArray(entry.zone) ? entry.zone : [];
                        const inherited = Array.isArray(dataContext?.zone) ? dataContext.zone : [];
                        const chips = el('div', 'row row-wrap row-tight');
                        const commit = updated => {
                            if (updated.length) entry.zone = updated;
                            else delete entry.zone;
                            push();
                            render();
                        };
                        zones.forEach((z, zi) => chips.appendChild(zoneChip(z, () => {
                            const next = zones.slice();
                            next.splice(zi, 1);
                            commit(next);
                        })));
                        inherited.forEach(z => chips.appendChild(
                            el('span', 'chip',
                                { title: 'from the receiver, applies to every engine' }, z)));
                        chips.appendChild(el('button', 'chip sys-chip-btn', {
                            type: 'button',
                            onClick: () => openZoneModal(zones, commit)
                        }, fromHtml(icon('add', 'icon-sm')), 'Add Zone'));
                        row.appendChild(chips);
                    }

                    (field.settings || [])
                        .filter(st => !st.types || st.types.includes(type))
                        .forEach(st => {
                            const own = entry.settings || {};
                            const toggle = Renderer.createInput({ type: 'toggle' }, own[st.name], on => {
                                if (on) (entry.settings = entry.settings || {})[st.name] = true;
                                else if (entry.settings) {
                                    delete entry.settings[st.name];
                                    if (!Object.keys(entry.settings).length) delete entry.settings;
                                }
                                push();
                                render();
                            });
                            toggle.appendChild(el('span', 't-small t-muted', { title: st.tooltip || '' }, st.label));
                            row.appendChild(toggle);
                        });

                    rows.appendChild(row);
                });
                addBtn.classList.toggle('hidden', list.length >= 4);
            }

            render();
            let zonesSnapshot = JSON.stringify(dataContext?.zone ?? null);
            wrapper.dataset.syncContext = '1';
            wrapper._syncContext = () => {
                const z = JSON.stringify(dataContext?.zone ?? null);
                if (z !== zonesSnapshot) { zonesSnapshot = z; render(); }
            };
            wrapper.appendChild(rows);
            wrapper.appendChild(addBtn);
            return wrapper;
        }

        if (type === 'list') {
            // one entry per line; a cleared field removes the key rather than saving [""]
            const lines = Array.isArray(currentValue) ? currentValue : (currentValue ? [String(currentValue)] : []);
            return el('textarea', Styles.input, {
                rows: Math.max(2, Math.min(lines.length + 1, 6)),
                value: lines.join('\n'),
                placeholder: field.placeholder || '',
                onInput: debounce((e) => {
                    const items = e.target.value.split('\n').map(s => s.trim()).filter(Boolean);
                    onUpdate(items.length ? items : undefined);
                }, 200)
            });
        }
        const isNumber = type === 'number';
        return el('input', Styles.input, {
            type: type || 'text',
            value: currentValue !== undefined ? currentValue : '',
            placeholder: field.placeholder || '',
            min: isNumber ? field.min : undefined,
            max: isNumber ? field.max : undefined,
            step: isNumber ? field.step : undefined,
            onInput: debounce((e) => {
                let val = e.target.value;
                if (isNumber) {
                    // a cleared field removes the key (never NaN -> null in
                    // the saved JSON); unparsable input keeps the last value
                    if (val.trim() === '') {
                        val = undefined;
                    } else {
                        let n = parseNumberLike(val);
                        if (n === null) return;
                        if (field.min !== undefined && n < field.min) n = field.min;
                        if (field.max !== undefined && n > field.max) n = field.max;
                        val = n;
                    }
                }
                onUpdate(val);
            }, 200)
        });
    },

    renderField(field, dataContext, onUpdateCallback, onDependencyCheck, host = null) {
        let val = field.jsonpath ? getNested(dataContext, field.jsonpath) : dataContext[field.name];
        if (val === undefined) val = field.defaultValue;

        const inputEl = Renderer.createInput(field, val, newValue => { onUpdateCallback(field, newValue); onDependencyCheck(); }, dataContext, host);

        const container = el('div', 'st-field', {
            dataset: { field: field.name, dependsOn: field.dependsOn ? JSON.stringify(field.dependsOn) : null },
            style: field.width ? `flex: 0 1 calc(${field.width}% - 0.375rem); min-width: 0;` : 'flex: 1 1 100%;'
        });

        if (field.label) container.appendChild(el('label', Styles.label, {}, field.label));
        container.appendChild(inputEl);
        const desc = field.description || field.tooltip;
        if (desc) container.appendChild(el('p', Styles.description, {}, desc));

        return container;
    },

    updateVisibility(container, dataContext) {
        const divs = container.querySelectorAll('[data-depends-on]');
        // two passes so chains of dependent fields settle; stop early once stable
        for (let pass = 0; pass < 2; pass++) {
            let changed = false;
            divs.forEach(div => {
                if (div._dep === undefined) {
                    const rawDep = div.getAttribute('data-depends-on');
                    div._dep = rawDep && rawDep !== 'null' ? JSON.parse(rawDep) : null;
                }
                const dep = div._dep;
                if (!dep) return;
                const ctrl = container.querySelector(`[data-field="${dep.field}"]`);

                let hide;
                if (ctrl && ctrl.classList.contains('hidden'))
                    hide = true;
                else {
                    const input = ctrl?.querySelector('input, select');
                    const val = input ? (input.type === 'checkbox' ? input.checked : input.value)
                        : getNested(dataContext, dep.field) ?? dataContext[dep.field];

                    const valStr = String(val).toLowerCase();
                    hide = !(Array.isArray(dep.value)
                        ? dep.value.some(v => String(v).toLowerCase() === valStr)
                        : String(dep.value).toLowerCase() === valStr);
                }

                if (div.classList.contains('hidden') !== hide) changed = true;
                div.classList.toggle('hidden', hide);
                div.querySelectorAll('input, select, button').forEach(el => el.disabled = hide);
            });
            if (!changed) break;
        }
        container.querySelectorAll('[data-sync-context]').forEach(div => div._syncContext && div._syncContext());
    }
};
