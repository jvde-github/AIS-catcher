// script.js

(function(global) {
    'use strict';

    // ============================================================================
    // 1. Core Utilities & DOM Builder
    // ============================================================================

    const el = (tag, className = '', attrs = {}, ...children) => {
        const isSvg = ['svg', 'path', 'circle'].includes(tag);
        const element = isSvg 
            ? document.createElementNS('http://www.w3.org/2000/svg', tag)
            : document.createElement(tag);

        if (className) {
            if (isSvg) element.setAttribute('class', className);
            else element.className = className;
        }
        
        Object.entries(attrs).forEach(([key, value]) => {
            if (value === false || value === null || value === undefined) return;
            if (key === 'innerHTML') element.innerHTML = value;
            else if (key.startsWith('on') && typeof value === 'function') element.addEventListener(key.substring(2).toLowerCase(), value);
            else if (key === 'value') element.value = value;
            else if (key === 'checked') element.checked = !!value;
            else if (key === 'dataset') Object.entries(value).forEach(([k, v]) => 
                element.setAttribute('data-' + k.replace(/([A-Z])/g, "-$1").toLowerCase(), v));
            else element.setAttribute(key, value);
        });

        children.forEach(child => child && element.appendChild(
            typeof child === 'string' || typeof child === 'number' ? document.createTextNode(child) : child
        ));
        return element;
    };

    const Utils = {
        debounce: (fn, ms) => { let t; return (...a) => { clearTimeout(t); t = setTimeout(() => fn(...a), ms); }; },
        getNested: (obj, path) => path?.split('.').reduce((a, p) => a?.[p], obj),
        setNested: (obj, path, val) => { 
            const keys = path.split('.'), last = keys.pop(); 
            keys.reduce((a, k) => a[k] ||= {}, obj)[last] = val; 
        },
        parseInteger: value => {
            if (typeof value === 'number') return Math.floor(value);
            if (typeof value !== 'string') return 0;
            const str = value.trim(), mult = /k$/i.test(str) ? 1000 : 1;
            const num = parseInt(str, 10);
            return isNaN(num) ? 0 : num * mult;
        },
        toBoolean: v => typeof v === 'string' ? ['true', 'on', 'yes', '1'].includes(v.toLowerCase()) : !!v
    };

    const ActionRegistry = {
        openSerialDeviceModal: () => {
            if (typeof window.openSerialDeviceModal === 'function') window.openSerialDeviceModal();
        },
        openDeviceSelectionModal: () => {
            if (typeof window.openDeviceSelectionModal === 'function') window.openDeviceSelectionModal();
        },
        openRegistration: () => window.open('https://aiscatcher.org/register', '_blank'),
        clearSerial: () => {
            // Clear the serial field when device type changes
            const serialField = document.querySelector('[data-field="serial"] input');
            if (serialField) {
                serialField.value = '';
                serialField.dispatchEvent(new Event('input', { bubbles: true }));
            }
        }
    };

    // ============================================================================
    // 2. Centralized Styles
    // ============================================================================

    const Styles = {
        input: 'w-full px-2 sm:px-3 py-1.5 sm:py-2 bg-white border border-slate-300 rounded-lg text-slate-700 placeholder-slate-400 focus:outline-none focus:ring-2 focus:ring-slate-500 focus:border-transparent transition-all duration-200 shadow-sm text-xs sm:text-sm',
        select: 'appearance-none pr-8',
        toggle: "w-11 h-6 bg-slate-200 peer-focus:outline-none peer-focus:ring-2 peer-focus:ring-slate-500 rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-slate-800",
        slider: 'flex-1 h-2 bg-slate-200 rounded-lg appearance-none cursor-pointer accent-slate-800',
        sliderContainer: 'flex items-center gap-2 sm:gap-3 p-1.5 sm:p-2 bg-slate-50 rounded-lg border border-slate-100',
        sliderDisplay: 'text-xs sm:text-sm font-mono font-medium text-slate-600 min-w-[2.5rem] sm:min-w-[3rem] text-center px-1.5 sm:px-2 py-0.5 sm:py-1 bg-slate-100 rounded border border-slate-200',
        label: 'block text-slate-700 text-xs sm:text-sm font-semibold mb-1 sm:mb-2 ml-0.5',
        description: 'mt-0.5 text-[10px] sm:text-xs text-slate-500 ml-0.5',
        button: 'w-full sm:w-auto bg-white border border-slate-300 text-slate-700 px-3 sm:px-4 py-1.5 sm:py-2 rounded-lg hover:bg-slate-50 transition duration-200 shadow-sm inline-flex items-center justify-center gap-2 text-xs sm:text-sm font-medium',
        buttonPrimary: 'px-2.5 sm:px-3 py-1.5 sm:py-2 bg-slate-800 text-white rounded-lg hover:bg-slate-700 transition duration-200 shadow-sm flex items-center justify-center min-w-[40px] sm:min-w-[44px] text-xs sm:text-sm',
        card: 'bg-white p-3 sm:p-6 rounded-none sm:rounded-xl border-x-0 sm:border-x border-slate-200 shadow-sm sm:max-w-2xl sm:mx-auto mb-2 sm:mb-6 relative hover:shadow-md transition-shadow duration-300',
        cardHeader: 'flex justify-between items-center mb-3 sm:mb-6 pb-2 sm:pb-4 border-b border-slate-100',
        deleteBtn: 'text-rose-600 hover:text-rose-700 hover:bg-rose-50 p-1.5 sm:p-2 rounded-lg transition duration-200',
        chevron: 'pointer-events-none absolute inset-y-0 right-0 flex items-center px-2 text-slate-500',
        saveActive: 'bg-slate-800 text-white px-4 sm:px-6 py-1.5 sm:py-2 rounded-lg hover:bg-slate-700 shadow-md transition-all duration-200 text-xs sm:text-sm font-medium transform hover:-translate-y-0.5',
        saveInactive: 'bg-white border border-slate-300 text-slate-400 px-4 sm:px-6 py-1.5 sm:py-2 rounded-lg hover:bg-slate-50 shadow-sm transition-all duration-200 text-xs sm:text-sm font-medium cursor-default',
    };

    const Icons = {
        chevronDown: () => el('svg', 'h-4 w-4', { fill: 'none', stroke: 'currentColor', viewBox: '0 0 24 24' }, 
            el('path', '', { 'stroke-linecap': 'round', 'stroke-linejoin': 'round', 'stroke-width': '2', d: 'M19 9l-7 7-7-7' })
        ),
        delete: () => el('svg', 'h-5 w-5', { fill: 'none', viewBox: '0 0 24 24', stroke: 'currentColor' }, 
            el('path', '', { 'stroke-linecap': 'round', 'stroke-linejoin': 'round', 'stroke-width': '2', d: 'M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16' })
        ),
        plus: () => el('svg', 'w-4 h-4 text-slate-500', { fill: 'none', stroke: 'currentColor', viewBox: '0 0 24 24'}, 
            el('path', '', {'stroke-linecap': 'round', 'stroke-linejoin': 'round', 'stroke-width': '2', d: 'M12 4v16m8-8H4'})
        ),
    };

    // ============================================================================
    // 3. UI & Notifications
    // ============================================================================

    const App = {
        state: { data: {}, unsaved: false },
        
        notify(type, message) {
            const container = document.getElementById('toast-container') || this.createToastContainer();
            // Matching the Slate/Emerald/Rose theme
            const colorClass = type === 'error' 
                ? 'bg-rose-600 text-white' 
                : 'bg-slate-800 text-white border border-slate-700';
            
            const toast = el('div', `mb-3 px-4 py-3 rounded-lg shadow-xl transform transition-all duration-300 translate-y-2 opacity-0 flex items-center gap-3 ${colorClass}`, {}, 
                type === 'success' ? el('span', 'font-bold text-emerald-400', {}, '✓') : el('span', 'font-bold text-white', {}, '!'),
                el('span', 'text-sm font-medium', {}, message)
            );
            
            container.appendChild(toast);
            requestAnimationFrame(() => toast.classList.remove('translate-y-2', 'opacity-0'));
            setTimeout(() => {
                toast.classList.add('opacity-0', 'translate-y-2');
                setTimeout(() => toast.remove(), 300);
            }, 3000);
        },

        createToastContainer() {
            const div = el('div', 'fixed bottom-6 right-6 z-50 flex flex-col items-end', { id: 'toast-container' });
            document.body.appendChild(div);
            return div;
        },

        setUnsaved(bool) {
            this.state.unsaved = bool;
            const statusEl = document.getElementById('status-message');
            
            // Styled Unsaved Indicator
            if (statusEl) {
                if (bool) {
                    statusEl.className = 'mb-6 px-4 py-3 bg-amber-50 border border-amber-200 text-amber-800 rounded-lg text-sm flex items-center shadow-sm';
                    statusEl.innerHTML = `
                        <svg class="w-4 h-4 mr-2 text-amber-500" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z"></path></svg>
                        <span class="font-medium">You have unsaved changes.</span>
                    `;
                } else {
                    statusEl.className = 'hidden';
                }
            }

            // Update Save Button Visuals
            document.querySelectorAll('button').forEach(btn => {
                if (btn.textContent.trim() === 'Save') {
                    btn.className = bool ? Styles.saveActive : Styles.saveInactive;
                }
            });
            window.onbeforeunload = bool ? () => true : null;
        }
    };

    // ============================================================================
    // 3. Schema Renderer (Updated Visuals)
    // ============================================================================

    const Renderer = {
        createInput(field, currentValue, onUpdate) {
            const type = field.type;
            
            if (type === 'select') {
                const valStr = currentValue !== undefined && currentValue !== null ? String(currentValue) : '';
                return el('div', 'relative', {}, 
                    el('select', `${Styles.input} ${Styles.select}`, { onChange: e => {
                        onUpdate(e.target.value);
                        if (field.onChange) {
                            const action = typeof field.onChange === 'function' 
                                ? field.onChange 
                                : ActionRegistry[field.onChange];
                            if (action) action();
                        }
                    } }, 
                        ...field.options.map(opt => el('option', '', { 
                            value: opt.value, 
                            selected: String(opt.value).toLowerCase() === valStr.toLowerCase() 
                        }, opt.label))
                    ),
                    el('div', Styles.chevron, {}, Icons.chevronDown())
                );
            }

            if (type === 'toggle') {
                return el('label', 'relative inline-flex items-center cursor-pointer group', {},
                    el('input', 'sr-only peer', { 
                        type: 'checkbox', 
                        checked: Utils.toBoolean(currentValue),
                        onChange: e => onUpdate(e.target.checked)
                    }),
                    el('div', Styles.toggle)
                );
            }

            if (type === 'integer-select') {
                const parsedCurrent = Utils.parseInteger(currentValue);
                const isPreset = field.presets && field.presets.some(p => Utils.parseInteger(p.value) === parsedCurrent);
                
                const customInput = el('input', `${Styles.input} mt-2`, {
                    type: 'number',
                    value: isPreset ? '' : parsedCurrent,
                    style: isPreset ? 'display: none' : 'display: block',
                    placeholder: field.placeholder || 'Enter custom value...',
                    onInput: (e) => onUpdate(parseInt(e.target.value, 10))
                });

                const select = el('div', 'relative', {}, 
                    el('select', `${Styles.input} ${Styles.select}`, {
                        onChange: (e) => {
                            if (e.target.value === 'custom') {
                                customInput.style.display = 'block';
                                customInput.focus();
                            } else {
                                customInput.style.display = 'none';
                                onUpdate(parseInt(e.target.value, 10));
                            }
                        }
                    }, 
                    ...(field.presets || []).map(p => el('option', '', { value: p.value, selected: Utils.parseInteger(p.value) === parsedCurrent }, p.label)),
                    el('option', '', { value: 'custom', selected: !isPreset }, 'Custom Value...')
                    ),
                    el('div', Styles.chevron, {}, Icons.chevronDown())
                );
                return el('div', 'space-y-2', {}, select, customInput);
            }

            if (type === 'auto-float' || type === 'auto-integer') {
                const isAuto = String(currentValue).toLowerCase() === 'auto';
                const numVal = isAuto ? (field.min || 0) : parseFloat(currentValue);
                const display = el('span', Styles.sliderDisplay, {}, isNaN(numVal) ? 0 : numVal);

                const slider = el('input', Styles.slider, {
                    type: 'range', 
                    min: field.min, max: field.max, step: field.step || (field.type === 'auto-integer' ? 1 : 0.1), 
                    value: isNaN(numVal) ? (field.min || 0) : numVal,
                    onInput: (e) => {
                         display.textContent = e.target.value;
                         if (selector.querySelector('select').value === 'custom') onUpdate(parseFloat(e.target.value));
                    }
                });
                
                const sliderContainer = el('div', `${Styles.sliderContainer} mt-2`, { style: isAuto ? 'display: none' : 'display: flex' }, slider, display);
                
                const selector = el('div', 'relative', {}, 
                    el('select', `${Styles.input} ${Styles.select}`, {
                        onChange: (e) => {
                            const isNowAuto = e.target.value === 'auto';
                            sliderContainer.style.display = isNowAuto ? 'none' : 'flex';
                            onUpdate(isNowAuto ? 'auto' : parseFloat(slider.value));
                        }
                    },
                    el('option', '', { value: 'auto', selected: isAuto }, 'Auto'),
                    el('option', '', { value: 'custom', selected: !isAuto }, `Custom`)
                    ),
                    el('div', Styles.chevron, {}, Icons.chevronDown())
                );
                return el('div', '', {}, selector, sliderContainer);
            }

            if (type === 'range') {
                 const display = el('span', Styles.sliderDisplay, {}, currentValue || 0);
                 const slider = el('input', Styles.slider, {
                    type: 'range', min: field.min, max: field.max, step: field.step, value: currentValue || 0,
                    onInput: (e) => {
                        display.textContent = e.target.value;
                        onUpdate(e.target.value);
                    }
                 });
                 return el('div', Styles.sliderContainer, {}, slider, display);
            }

            return el('input', Styles.input, {
                type: type === 'integer' ? 'text' : (type || 'text'),
                value: currentValue !== undefined ? currentValue : '',
                placeholder: field.placeholder || '',
                onInput: Utils.debounce((e) => {
                    let val = e.target.value;
                    if (field.type === 'number') val = parseFloat(val);
                    if (field.type === 'integer') val = Utils.parseInteger(val);
                    onUpdate(val);
                }, 200)
            });
        },

        renderField(field, index, dataContext, onUpdateCallback, onDepencyCheck) {
            let val = field.jsonpath ? Utils.getNested(dataContext, field.jsonpath) : dataContext[field.name];
            if (val === undefined) val = field.defaultValue;

            let inputEl = field.type === 'button'
                ? el('button', Styles.button, {
                    type: 'button',
                    onClick: () => (typeof field.onClick === 'function' ? field.onClick : ActionRegistry[field.onClick])?.()
                }, field.buttonIcon && el('span', '', { innerHTML: field.buttonIcon }), field.buttonLabel || 'Action')
                : this.createInput(field, val, newValue => { onUpdateCallback(field, newValue); onDepencyCheck(); });

            if (field.withButton && field.type !== 'button') {
                const action = typeof field.withButton.onClick === 'function' 
                    ? field.withButton.onClick 
                    : ActionRegistry[field.withButton.onClick] || (() => {});
                inputEl = el('div', 'flex items-center gap-2', {}, 
                    el('div', 'flex-1', {}, inputEl),
                    el('button', Styles.buttonPrimary, { type: 'button', onClick: action }, 
                        el('span', '', { innerHTML: field.withButton.icon || 'Action' }))
                );
            }

            const container = el('div', 'mb-1.5 sm:mb-2 transition-all duration-200', {
                dataset: { field: field.name, dependsOn: field.dependsOn ? JSON.stringify(field.dependsOn) : null },
                style: field.width ? `flex: 0 1 calc(${field.width}% - 0.375rem); min-width: 0;` : 'flex: 1 1 100%;'
            });

            if (field.type !== 'button') container.appendChild(el('label', Styles.label, {}, field.label));
            container.appendChild(inputEl);
            if (field.description) container.appendChild(el('p', Styles.description, {}, field.description));
            
            return container;
        },

        updateVisibility(container, dataContext) {
            for (let pass = 0; pass < 2; pass++) {
                container.querySelectorAll('[data-depends-on]').forEach(div => {
                    const rawDep = div.getAttribute('data-depends-on');
                    if (!rawDep || rawDep === 'null') return;
                    const dep = JSON.parse(rawDep);
                    const ctrl = container.querySelector(`[data-field="${dep.field}"]`);
                    
                    if (ctrl && getComputedStyle(ctrl).display === 'none') {
                        div.style.display = 'none';
                        div.querySelectorAll('input, select, button').forEach(el => el.disabled = true);
                        return;
                    }
                    
                    const input = ctrl?.querySelector('input, select');
                    const val = input ? (input.type === 'checkbox' ? input.checked : input.value) 
                                     : Utils.getNested(dataContext, dep.field) ?? dataContext[dep.field];
                    
                    const valStr = String(val).toLowerCase();
                    const match = Array.isArray(dep.value) 
                        ? dep.value.some(v => String(v).toLowerCase() === valStr)
                        : String(dep.value).toLowerCase() === valStr;
                    
                    div.style.display = match ? 'block' : 'none';
                    div.querySelectorAll('input, select, button').forEach(el => el.disabled = !match);
                });
            }
        }
    };

    // ============================================================================
    // 4. Config Manager (Updated Container Visuals)
    // ============================================================================

    class ConfigManager {
        constructor(config) {
            this.config = config;
            this.container = document.getElementById(config.containerId);
            this.fields = Object.values(config.schema);
            this.data = config.isList ? [] : {};
            
            if (!this.container) return;

            this.loadData().then(() => {
                this.render();
                this.updateJsonDebug();
            });
        }

        async loadData() {
            try {
                const res = await fetch('/api/config');
                if (!res.ok) throw new Error('Fetch failed');
                const fullConfig = await res.json();
                this.data = this.config.channelType 
                    ? (fullConfig[this.config.channelType] || (this.config.isList ? [] : {}))
                    : fullConfig;
            } catch {
                const jsonEl = document.getElementById('json_content');
                if (jsonEl?.textContent.trim()) {
                    try {
                        const fullConfig = JSON.parse(jsonEl.textContent);
                        this.data = this.config.channelType 
                            ? (fullConfig[this.config.channelType] || (this.config.isList ? [] : {}))
                            : fullConfig;
                    } catch {}
                }
            }
            if (!this.config.isList) {
                this.fields.forEach(f => this.data[f.name] ??= f.defaultValue);
            }
        }

        render() {
            this.container.innerHTML = '';
            const items = this.config.isList ? this.data : [this.data];

            items.forEach((item, index) => {
                const wrapper = el('div', Styles.card, {}, 
                    this.config.isList ? el('div', Styles.cardHeader, {}, 
                        el('h4', 'text-base sm:text-lg font-semibold text-slate-800', {}, `${this.config.title} ${index + 1}`),
                        el('button', Styles.deleteBtn, { 
                            type: 'button', 
                            title: 'Remove Item',
                            onClick: () => this.removeItem(index) 
                        }, Icons.delete())
                    ) : null
                );
                
                const fieldsDiv = el('div', 'flex flex-wrap gap-x-3 gap-y-2');
                this.fields.forEach(field => {
                    const fieldEl = Renderer.renderField(field, index, item, 
                        (fld, val) => this.updateValue(index, fld, val),
                        () => Renderer.updateVisibility(fieldsDiv, item)
                    );
                    fieldsDiv.appendChild(fieldEl);
                });

                wrapper.appendChild(fieldsDiv);
                this.container.appendChild(wrapper);
                
                Renderer.updateVisibility(fieldsDiv, item);
            });

            this.renderControls();
        }

        renderControls() {
            const containerIdClass = `controls-${this.config.containerId}`;
            const existing = this.container.parentElement.querySelector('.' + containerIdClass);
            if (existing) existing.remove();

            // Ensure JSON section exists first so we can insert buttons before it
            this.ensureJsonUI();
            
            const btnGroup = el('div', `${containerIdClass} mt-6 sm:mt-8 px-4 sm:px-0 flex flex-col sm:flex-row justify-end items-stretch sm:items-center gap-2 sm:gap-3 max-w-2xl sm:mx-auto`);
            
            if (this.config.isList) {
                btnGroup.appendChild(el('button', 'w-full sm:w-32 bg-white border border-slate-300 text-slate-700 px-4 py-2 rounded-lg hover:bg-slate-50 transition duration-200 shadow-sm inline-flex items-center justify-center gap-2 text-sm font-medium', {
                    type: 'button', onClick: () => this.addItem()
                }, 
                Icons.plus(),
                'Add Item'));
            }
            
            btnGroup.appendChild(el('button', 'w-full sm:w-32 bg-white border border-slate-300 text-slate-400 px-6 py-2 rounded-lg hover:bg-slate-50 shadow-sm transition-all duration-200 text-sm font-medium cursor-default', {
                type: 'button', onClick: () => this.save()
            }, 'Save'));

            // Insert buttons before the JSON section
            const jsonSection = this.container.parentElement.querySelector('.border-t.border-slate-200.pt-6');
            if (jsonSection) {
                this.container.parentElement.insertBefore(btnGroup, jsonSection);
            } else {
                this.container.parentElement.appendChild(btnGroup);
            }
            
            if(App.state.unsaved) App.setUnsaved(true);
        }

        ensureJsonUI() {
            const parent = this.container.parentElement;
            const existingContainer = document.getElementById('json-content-container');
            
            // If JSON section already exists, move it to the end to ensure proper order
            if (existingContainer) {
                const toggleDiv = existingContainer.parentElement;
                if (toggleDiv) {
                    parent.appendChild(toggleDiv);
                }
                return;
            }
            
            // Collapsible JSON Debugger
            const toggleDiv = el('div', 'mt-6 sm:mt-8 px-4 sm:px-0 max-w-2xl sm:mx-auto border-t border-slate-200 pt-6');
            const btn = el('button', 'flex items-center text-slate-500 hover:text-slate-800 transition-colors focus:outline-none group text-sm font-medium', { 
                type: 'button', onClick: () => global.toggleJsonContent() 
            });
            
            const chevron = el('svg', 'h-4 w-4 mr-2 transform transition-transform duration-200 text-slate-400 group-hover:text-slate-600', {
                id: 'chevron-icon', fill: 'none', viewBox: '0 0 24 24', stroke: 'currentColor'
            }, el('path', '', { 'stroke-linecap': 'round', 'stroke-linejoin': 'round', 'stroke-width': '2', d: 'M19 9l-7 7-7-7' }));

            btn.appendChild(chevron);
            btn.appendChild(el('span', '', {}, 'JSON'));

            const contentDiv = el('div', 'mt-3 hidden transition-all', { id: 'json-content-container' });
            // Dark mode terminal look for JSON
            const pre = el('pre', 'w-full p-4 rounded-lg bg-slate-900 text-slate-200 text-xs font-mono overflow-auto custom-scrollbar border border-slate-700 shadow-inner', {
                id: 'json_content', readonly: '', style: 'max-height: 300px;'
            });

            contentDiv.appendChild(pre);
            toggleDiv.appendChild(btn);
            toggleDiv.appendChild(contentDiv);
            parent.appendChild(toggleDiv);
        }

        updateValue(index, field, value) {
            const target = this.config.isList ? this.data[index] : this.data;
            if (field.jsonpath) Utils.setNested(target, field.jsonpath, value);
            else target[field.name] = value;
            App.setUnsaved(true);
            this.updateJsonDebug();
        }

        addItem() {
            const newItem = {};
            this.fields.forEach(f => {
                if (f.defaultValue !== undefined) {
                    if (f.jsonpath) Utils.setNested(newItem, f.jsonpath, f.defaultValue);
                    else newItem[f.name] = f.defaultValue;
                }
            });
            this.data.push(newItem);
            this.render();
            App.setUnsaved(true);
            this.updateJsonDebug();
        }

        removeItem(index) {
            if (confirm(`Are you sure you want to remove this ${this.config.title}?`)) {
                this.data.splice(index, 1);
                this.render();
                App.setUnsaved(true);
                this.updateJsonDebug();
            }
        }

        updateJsonDebug() {
            const el = document.getElementById('json_content');
            if (!el) return;
            let cfg = {};
            try { cfg = JSON.parse(el.textContent) || {}; } catch {}
            if (this.config.channelType) cfg[this.config.channelType] = this.data;
            else Object.assign(cfg, this.data);
            el.textContent = JSON.stringify(cfg, null, 2);
            global.appState = { jsonData: cfg };
        }

        async save() {
            const btn = document.querySelector('button[type="button"]:not([data-action])');
            const saveButtons = Array.from(document.querySelectorAll('button[type="button"]')).filter(b => 
                b.textContent.trim() === 'Save' && !b.querySelector('svg')
            );
            const saveBtn = saveButtons[0];
            
            if(saveBtn) {
                const originalText = saveBtn.textContent;
                saveBtn.textContent = 'Saving...';
                saveBtn.disabled = true;
            }

            try {
                // Fetch full config first
                const fullConfigRes = await fetch('/api/config');
                if (!fullConfigRes.ok) throw new Error('Failed to fetch current config');
                const fullConfig = await fullConfigRes.json();
                
                // Merge current section data into full config
                if (this.config.channelType) {
                    fullConfig[this.config.channelType] = this.data;
                } else {
                    Object.assign(fullConfig, this.data);
                }
                
                // Ensure required top-level fields are present
                if (!fullConfig.config) fullConfig.config = 'aiscatcher';
                if (!fullConfig.version) fullConfig.version = 1;

                // Save merged config
                const saveRes = await fetch('/api/config', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(fullConfig)
                });
                
                if (!saveRes.ok) throw new Error('Save Failed');
                App.setUnsaved(false);
                App.notify('success', 'Configuration saved successfully');
            } catch (e) {
                console.error(e);
                App.notify('error', 'Failed to save configuration: ' + e.message);
            } finally {
                if(saveBtn) {
                    saveBtn.textContent = 'Save';
                    saveBtn.disabled = false;
                }
            }
        }
    }

    // ============================================================================
    // 5. EXPORTS
    // ============================================================================

    // Debug log to verify exports are being set
    console.log('Config-manager.js: Setting up exports');
    
    global.App = App;
    global.createConfigManager = (config) => new ConfigManager(config);
    global.createSimpleConfigManager = (config) => { config.isList = false; return new ConfigManager(config); };
    global.createChannelManager = (config) => { config.isList = true; return new ConfigManager(config); };
    
    console.log('Config-manager.js: Exports set', {
        App: typeof global.App,
        createChannelManager: typeof global.createChannelManager,
        createSimpleConfigManager: typeof global.createSimpleConfigManager
    });
    
    global.toggleJsonContent = () => {
        const c = document.getElementById('json-content-container');
        const i = document.getElementById('chevron-icon');
        if (c) {
            const hidden = c.classList.toggle('hidden');
            if(i) {
                const rotateClass = hidden ? '' : 'rotate-180';
                i.setAttribute('class', 'h-4 w-4 mr-2 transform transition-transform duration-200 text-slate-400 group-hover:text-slate-600 ' + rotateClass);
            }
            const label = i ? i.nextElementSibling : null;
            if (label) label.textContent = hidden ? 'Advanced: Show JSON Config' : 'Advanced: Hide JSON Config';
        }
    };

})(window);


