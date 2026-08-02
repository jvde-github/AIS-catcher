
(function (global) {
    'use strict';

    const ManagerRegistry = new Map();

    // Session may expire mid-edit: route 401s through the login modal so the
    // user can sign in again without losing unsaved edits.
    function authFailed(r) {
        if (r.status === 401 && typeof global.hubAuthRequired === 'function')
            global.hubAuthRequired();
        return r.status === 401;
    }

    // Shared /api/config fetch: readers reuse one request, each getting its
    // own copy to mutate. Invalidated on save and on every panel open.
    const ConfigStore = {
        promise: null,
        fetch(fresh) {
            if (fresh || !this.promise) {
                const p = fetch('/api/config').then(r => {
                    if (!r.ok) throw new Error(authFailed(r) ? 'Authentication required' : 'Failed to fetch configuration');
                    return r.json();
                });
                p.catch(() => { if (this.promise === p) this.promise = null; });
                this.promise = p;
            }
            return this.promise.then(cfg => JSON.parse(JSON.stringify(cfg)));
        },
        invalidate() { this.promise = null; }
    };

    function ensurePickerModal(id, title, opts = {}) {
        let modal = document.getElementById(id);
        if (modal) return modal;
        modal = el('div', 'sys-overlay hidden',
            { id, style: 'background-color: var(--color-scrim)' },
            el('div', `card col sys-modal-card ${opts.maxWidth || ''}`, {},
                el('div', 'row row-between card-header', {},
                    el('h3', 't-strong t-lg', {}, title),
                    el('button', 'btn btn-quiet', { type: 'button', onClick: () => modal.classList.add('hidden') },
                        el('svg', 'icon-lg', { fill: 'none', viewBox: '0 0 24 24', stroke: 'currentColor' },
                            el('path', '', { 'stroke-linecap': 'round', 'stroke-linejoin': 'round', 'stroke-width': '2', d: 'M6 18L18 6M6 6l12 12' })
                        )
                    )
                ),
                el('div', opts.bodyClass || 'sys-modal-body', { id: opts.bodyId || id + '-list' }),
                el('div', 'row row-end sys-modal-foot', {},
                    el('button', 'btn', {
                        type: 'button', onClick: () => modal.classList.add('hidden')
                    }, opts.footerLabel || 'Close')
                )
            )
        );
        document.body.appendChild(modal);
        return modal;
    }

    function applyToManager(containerId, index, fn) {
        const mgr = ManagerRegistry.get(containerId);
        if (!mgr) return;
        const target = mgr.config.isList ? mgr.data[index] : mgr.data;
        if (!target) return;
        fn(target);
        if (mgr.hasViewerFields) mgr.reloadWebviewer = true;
        mgr.dirty = true;
        App.setUnsaved(true);
        mgr.updateJsonDebug();
        mgr.render();
    }

    const ZONE_COLORS = ['zone-1', 'zone-2', 'zone-3', 'zone-4',
        'zone-5', 'zone-6', 'zone-7', 'zone-8'];
    function zoneHash(zone) {
        let hash = 0;
        for (let i = 0; i < zone.length; i++) hash = (hash * 31 + zone.charCodeAt(i)) & 0xFFFF;
        return hash;
    }
    const zoneColorIndex = new Map();
    function zoneIndex(zone) {
        if (zoneColorIndex.has(zone)) return zoneColorIndex.get(zone);
        let i = zoneHash(zone) % ZONE_COLORS.length;
        if (zoneColorIndex.size < ZONE_COLORS.length) {
            const used = new Set(zoneColorIndex.values());
            while (used.has(i)) i = (i + 1) % ZONE_COLORS.length;
        }
        zoneColorIndex.set(zone, i);
        return i;
    }
    const getZoneColor = zone => ZONE_COLORS[zoneIndex(zone)];
    const getZoneCss = zone => `var(--${getZoneColor(zone)})`;

    function zoneChip(zone, onRemove) {
        return el('span', `chip ${getZoneColor(zone)}`, {},
            zone,
            el('button', 'sys-dismiss', { type: 'button', onClick: onRemove }, '×')
        );
    }

    async function fetchAllZones() {
        try {
            const cfg = await ConfigStore.fetch();
            const zones = new Set();
            const walk = o => {
                if (Array.isArray(o)) { o.forEach(walk); return; }
                if (!o || typeof o !== 'object') return;
                if (Array.isArray(o.zone)) o.zone.forEach(z => zones.add(z));
                Object.values(o).forEach(walk);
            };
            walk(cfg);
            ManagerRegistry.forEach(mgr => walk(mgr.data));
            if (Array.isArray(cfg.sharing_zone)) cfg.sharing_zone.forEach(z => zones.add(z));
            return [...zones].sort();
        } catch { return []; }
    }

    let activeZoneEdit = { zones: [], onUpdate: null, allZones: [] };

    function ensureZoneModal() {
        ensurePickerModal('cm-zone-modal', 'Manage Zones', {
            maxWidth: 'sys-modal-sm',
            bodyId: 'cm-zone-body',
            bodyClass: 'col col-loose sys-modal-body',
            footerLabel: 'Done'
        });
    }

    function openZoneModal(currentZones, onUpdate) {
        ensureZoneModal();
        activeZoneEdit.zones = [...currentZones];
        activeZoneEdit.onUpdate = onUpdate;
        activeZoneEdit.allZones = [];
        renderZoneModalBody();
        fetchAllZones().then(all => {
            activeZoneEdit.allZones = all;
            renderZoneModalBody();
        });
        document.getElementById('cm-zone-modal').classList.remove('hidden');
    }

    function renderZoneModalBody() {
        const body = document.getElementById('cm-zone-body');
        if (!body) return;
        body.innerHTML = '';

        const chipsWrap = el('div', 'row row-wrap row-tight sys-minrow');
        if (activeZoneEdit.zones.length === 0) {
            chipsWrap.appendChild(el('span', 't-small t-subtle t-italic', {}, 'No zones assigned'));
        } else {
            activeZoneEdit.zones.forEach((zone, i) => {
                chipsWrap.appendChild(zoneChip(zone, () => {
                    activeZoneEdit.zones.splice(i, 1);
                    activeZoneEdit.onUpdate([...activeZoneEdit.zones]);
                    renderZoneModalBody();
                }));
            });
        }
        body.appendChild(chipsWrap);

        const addSection = el('div', 'col col-tight');
        const inputRow = el('div', 'row');
        const input = el('input', 'input grow', {
            type: 'text',
            placeholder: 'Zone name (e.g. internal)',
            onKeydown: (e) => {
                if (e.key === 'Enter') { e.preventDefault(); addZoneFromInput(); }
            },
            onInput: () => {
                input.value = input.value.replace(/[^a-zA-Z0-9_-]/g, '');
                renderSuggestions(input.value.toLowerCase());
            }
        });
        inputRow.appendChild(input);
        inputRow.appendChild(el('button', 'btn', {
            type: 'button', onClick: addZoneFromInput
        }, 'Add'));
        addSection.appendChild(inputRow);

        const suggestionsDiv = el('div', 'row row-wrap row-tight', { id: 'cm-zone-suggestions' });
        addSection.appendChild(suggestionsDiv);
        body.appendChild(addSection);

        function addZoneFromInput() {
            const val = input.value.trim().replace(/[^a-zA-Z0-9_-]/g, '');
            if (!val) return;
            input.value = val;
            if (!activeZoneEdit.zones.includes(val)) {
                activeZoneEdit.zones.push(val);
                activeZoneEdit.onUpdate([...activeZoneEdit.zones]);
                if (!activeZoneEdit.allZones.includes(val)) activeZoneEdit.allZones.push(val);
            }
            input.value = '';
            renderZoneModalBody();
        }

        function renderSuggestions(query) {
            suggestionsDiv.innerHTML = '';
            const suggestions = activeZoneEdit.allZones.filter(z =>
                !activeZoneEdit.zones.includes(z) && (query === '' || z.toLowerCase().includes(query))
            );
            suggestions.slice(0, 8).forEach(z => {
                suggestionsDiv.appendChild(el('button', `chip sys-chip-btn ${getZoneColor(z)}`, {
                    type: 'button',
                    onClick: () => {
                        if (!activeZoneEdit.zones.includes(z)) {
                            activeZoneEdit.zones.push(z);
                            activeZoneEdit.onUpdate([...activeZoneEdit.zones]);
                        }
                        input.value = '';
                        renderZoneModalBody();
                    }
                }, z));
            });
        }

        renderSuggestions('');
        setTimeout(() => input.focus(), 50);
    }

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

        children.forEach(child => (child || child === 0) && element.appendChild(
            typeof child === 'string' || typeof child === 'number' ? document.createTextNode(child) : child
        ));
        return element;
    };

    const parseNumberLike = (str) => {
        const s = String(str).trim();
        const m = /^([+-]?(?:\d+\.?\d*|\.\d+))([kK]?)$/.exec(s);
        if (!m) return null;
        let n = parseFloat(m[1]);
        if (!isFinite(n)) return null;
        if (m[2]) n *= 1000;
        return n;
    };

    const Utils = {
        el,
        debounce: (fn, ms) => { let t; return (...a) => { clearTimeout(t); t = setTimeout(() => fn(...a), ms); }; },
        getNested: (obj, path) => path?.split('.').reduce((a, p) => a?.[p], obj),
        setNested: (obj, path, val) => {
            const keys = path.split('.'), last = keys.pop();
            keys.reduce((a, k) => (a[k] && typeof a[k] === 'object') ? a[k] : (a[k] = {}), obj)[last] = val;
        },
        parseInteger: value => {
            if (typeof value === 'number') return Math.floor(value);
            const n = parseNumberLike(value);
            return n === null ? 0 : Math.floor(n);
        },
        toBoolean: v => typeof v === 'string' ? ['true', 'on', 'yes', '1'].includes(v.toLowerCase()) : !!v,

        coerceValue(value, type) {
            if (value === undefined || value === null) return { value, status: 'same' };
            switch (type) {
                case 'toggle': {
                    if (typeof value === 'boolean') return { value, status: 'same' };
                    if (typeof value === 'number') {
                        if (value === 1) return { value: true, status: 'converted' };
                        if (value === 0) return { value: false, status: 'converted' };
                        return { value, status: 'failed' };
                    }
                    if (typeof value === 'string') {
                        const s = value.trim().toLowerCase();
                        if (['true', 'on', 'yes', '1'].includes(s)) return { value: true, status: 'converted' };
                        if (['false', 'off', 'no', '0'].includes(s)) return { value: false, status: 'converted' };
                    }
                    return { value, status: 'failed' };
                }
                case 'number':
                case 'integer-select': {
                    if (typeof value === 'number') return { value, status: 'same' };
                    if (typeof value === 'string') {
                        const n = parseNumberLike(value);
                        if (n !== null) return { value: n, status: 'converted' };
                    }
                    return { value, status: 'failed' };
                }
                case 'off-number':
                case 'switch-integer': {
                    if (typeof value === 'number') return { value, status: 'same' };
                    if (typeof value === 'boolean') return value === false ? { value: false, status: 'same' } : { value, status: 'failed' };
                    if (typeof value === 'string') {
                        const s = value.trim().toLowerCase();
                        if (['false', 'off', 'no'].includes(s)) return { value: false, status: 'converted' };
                        const n = parseNumberLike(value);
                        if (n !== null) return { value: n, status: 'converted' };
                    }
                    return { value, status: 'failed' };
                }
                case 'text':
                case 'select': {
                    if (typeof value === 'string') return { value, status: 'same' };
                    if (typeof value === 'number' || typeof value === 'boolean') return { value: String(value), status: 'converted' };
                    return { value, status: 'failed' };
                }
                case 'zones': {
                    if (Array.isArray(value)) {
                        return value.every(v => typeof v === 'string')
                            ? { value, status: 'same' }
                            : { value: value.map(String), status: 'converted' };
                    }
                    return { value, status: 'failed' };
                }
                default:
                    return { value, status: 'same' };
            }
        }
    };

    const ConfigNormalizer = {
        arraySchemas: CHANNEL_REGISTRY.reduce((m, c) => { m[c.configKey] = c.schema; return m; }, {}),
        topLevelSchemas: () => [sharingSchema, generalSettingsSchema],

        receiverKeys: ['serial', 'input', 'verbose', 'model', 'engines', 'meta', 'own_mmsi',
            'rtlsdr', 'rtltcp', 'airspy', 'airspyhf', 'hydrasdr', 'sdrplay', 'serialport',
            'hackrf', 'udpserver', 'soapysdr', 'nmea2000', 'file', 'zmq',
            'spyserver', 'wavfile'],

        migrate(cfg) {
            let changed = false;

            if (cfg.server && !Array.isArray(cfg.server) && typeof cfg.server === 'object') {
                cfg.server = [cfg.server];
                changed = true;
            }

            // canonical viewer key is 'msg': fold the 'message' alias into it
            // and drop 'msgs', which was briefly seeded by a defaults bug
            const canonMsg = (v) => {
                if (!v || typeof v !== 'object') return;
                if ('msgs' in v) { delete v.msgs; changed = true; }
                if ('message' in v) {
                    if (!('msg' in v)) v.msg = v.message;
                    delete v.message;
                    changed = true;
                }
            };
            canonMsg(cfg.control && cfg.control.viewer);
            if (Array.isArray(cfg.server)) cfg.server.forEach(canonMsg);

            const receiverCfg = {};
            let hasReceiverSettings = false;
            for (const key of this.receiverKeys) {
                if (Object.prototype.hasOwnProperty.call(cfg, key)) {
                    receiverCfg[key] = cfg[key];
                    delete cfg[key];
                    hasReceiverSettings = true;
                    changed = true;
                }
            }
            if (!Array.isArray(cfg.receiver)) cfg.receiver = [];
            if (hasReceiverSettings) {
                if (cfg.receiver.length && cfg.receiver[0] && typeof cfg.receiver[0] === 'object') {
                    const first = cfg.receiver[0];
                    const noSelection = !('input' in first) && !('serial' in first);
                    for (const key of Object.keys(receiverCfg)) {
                        if ((key === 'input' || key === 'serial') && !noSelection) continue;
                        if (!(key in first)) first[key] = receiverCfg[key];
                    }
                } else {
                    if (!('input' in receiverCfg) && !('serial' in receiverCfg)) receiverCfg.input = 'RTLSDR';
                    cfg.receiver.push(receiverCfg);
                }
            }

            for (const r of cfg.receiver) {
                if (!r || typeof r !== 'object') continue;
                if ('sensitivity_high' in r) {
                    if (Utils.toBoolean(r.sensitivity_high) && !('engines' in r)) r.engines = [{ type: 'v1_high' }];
                    delete r.sensitivity_high;
                    changed = true;
                }
                if (typeof r.model === 'string') {
                    const model = r.model.toLowerCase();
                    if (!('engines' in r)) r.engines = [model === 'auto' ? {} : { type: model }];
                    delete r.model;
                    changed = true;
                }
                if (r.model && typeof r.model === 'object' && !Array.isArray(r.model)) {
                    if (!('active' in r.model) || Utils.toBoolean(r.model.active)) {
                        const settings = {};
                        for (const k of Object.keys(r.model)) if (k !== 'active') settings[k] = r.model[k];
                        const entry = Object.keys(settings).length ? { settings } : {};
                        r.engines = Array.isArray(r.engines) ? r.engines : [entry];
                    }
                    delete r.model;
                    changed = true;
                }
            }

            for (const key of ['udp', 'tcp_listener', 'tcp']) {
                if (!Array.isArray(cfg[key])) continue;
                for (const ch of cfg[key]) {
                    if (ch && typeof ch === 'object' && this.migrateMsgFormat(ch)) changed = true;
                }
            }
            return changed;
        },

        migrateMsgFormat(ch) {
            let changed = false;
            if ('msgformat' in ch) {
                if ('json' in ch) { delete ch.json; changed = true; }
                if ('json_full' in ch) { delete ch.json_full; changed = true; }
                return changed;
            }
            const hasJSON = 'json' in ch, hasJSONFull = 'json_full' in ch;
            ch.msgformat = (hasJSONFull && Utils.toBoolean(ch.json_full)) ? 'JSON_FULL'
                : (hasJSON && Utils.toBoolean(ch.json)) ? 'JSON_NMEA'
                    : 'NMEA';
            if (hasJSON) delete ch.json;
            if (hasJSONFull) delete ch.json_full;
            return true;
        },

        coerceFields(obj, fields, prefix) {
            const warnings = [];
            let changed = false;
            if (!obj || typeof obj !== 'object') return { warnings, changed };
            for (const field of fields) {
                if (!field.type || field.type === 'button') continue;
                const cur = field.jsonpath ? Utils.getNested(obj, field.jsonpath) : obj[field.name];
                if (cur === undefined || cur === null) continue;
                const { value, status } = Utils.coerceValue(cur, field.type);
                if (status === 'converted') {
                    if (field.jsonpath) Utils.setNested(obj, field.jsonpath, value);
                    else obj[field.name] = value;
                    changed = true;
                } else if (status === 'failed') {
                    warnings.push({ path: prefix + (field.jsonpath || field.name), value: cur, type: field.type });
                }
            }
            return { warnings, changed };
        },

        normalize(cfg) {
            const warnings = [];
            if (!cfg || typeof cfg !== 'object') return { config: cfg, warnings, changed: false };
            let changed = this.migrate(cfg);

            for (const [key, schema] of Object.entries(this.arraySchemas)) {
                if (!Array.isArray(cfg[key])) continue;
                const fields = Object.values(schema);
                cfg[key].forEach((item, i) => {
                    const r = this.coerceFields(item, fields, `${key}[${i}].`);
                    warnings.push(...r.warnings);
                    if (r.changed) changed = true;
                });
            }
            for (const schema of this.topLevelSchemas()) {
                const r = this.coerceFields(cfg, Object.values(schema), '');
                warnings.push(...r.warnings);
                if (r.changed) changed = true;
            }
            return { config: cfg, warnings, changed };
        }
    };

    function pickerAction({ id, title, endpoint, extract, renderRow, apply }) {
        return (index, containerId) => {
            const modal = ensurePickerModal(id, title);
            const list = document.getElementById(id + '-list');
            list.innerHTML = '<div class="t-center t-muted sys-empty">Loading...</div>';
            modal.classList.remove('hidden');
            fetch(endpoint)
                .then(r => { if (authFailed(r)) throw new Error(); return r.json(); })
                .then(data => {
                    list.innerHTML = '';
                    const devices = extract(data);
                    if (devices.length === 0) {
                        list.innerHTML = '<li class="t-center t-muted sys-empty">No devices found</li>';
                        return;
                    }
                    devices.forEach(device => {
                        list.appendChild(renderRow(device, () => {
                            applyToManager(containerId, index, target => apply(target, device));
                            modal.classList.add('hidden');
                        }));
                    });
                })
                .catch(() => { list.innerHTML = '<li class="t-center t-danger sys-empty">Error loading devices</li>'; });
        };
    }

    const ActionRegistry = {
        openSerialDeviceModal: pickerAction({
            id: 'cm-serial-modal',
            title: 'Select Serial Device',
            endpoint: '/api/serial',
            extract: data => Array.isArray(data) ? data : [],
            renderRow: (device, select) => el('li', 'sys-pick', { onClick: select }, device),
            apply: (target, device) => {
                if (!target.serialport) target.serialport = {};
                target.serialport.port = device;
            }
        }),

        openDeviceSelectionModal: pickerAction({
            id: 'cm-device-modal',
            title: 'Select Device',
            endpoint: '/api/devices',
            extract: data => Array.isArray(data.devices) ? data.devices : [],
            renderRow: (device, select) => el('li', 'row row-between box sys-pick', {},
                el('span', '', {}, device.name),
                el('button', 'btn', {
                    type: 'button', onClick: select
                }, 'Select')
            ),
            apply: (target, device) => {
                target.input = device.input ? device.input.toUpperCase() : '';
                target.serial = device.serial || '';
            }
        }),

        openSharingManagement: (index, containerId) => {
            const scope = containerId ? document.getElementById(containerId) : document;
            const card = scope?.querySelector(`[data-receiver-card="${index}"]`);
            const sharingKeyInput = (card || scope || document).querySelector('[data-field="sharing_key"] input');
            const sharingKey = sharingKeyInput ? sharingKeyInput.value.trim() : '';
            if (sharingKey === '') {
                window.open('https://aiscatcher.org/register', '_blank');
            } else {
                window.open(`https://www.aiscatcher.org/editstation/${sharingKey}`, '_blank');
            }
        },

        clearSerial: (index, containerId) => {
            const scope = containerId ? document.getElementById(containerId) : document;
            const card = scope?.querySelector(`[data-receiver-card="${index}"]`);
            const serialField = (card || scope || document).querySelector('[data-field="serial"] input');
            if (serialField) {
                serialField.value = '';
                serialField.dispatchEvent(new Event('input', { bubbles: true }));
            }
        }
    };

    const SECTION_ORDER = ['Label', 'Connection', 'Station', 'Location', 'Format',
        'Options', 'Retention', 'Service', 'Backup', 'Storage', 'Features', 'Filter', 'Zones'];

    const Styles = {
        input: 'input',
        select: 'select',
        toggle: 'toggle',
        slider: 'slider',
        sliderContainer: 'slider-row',
        sliderDisplay: 'slider-value',
        label: 'field-label',
        description: 'field-desc',
        section: 'field-section',
        button: 'btn',
        buttonPrimary: 'btn btn-icon',
        card: 'card',
        cardHeader: 'card-header',
        cardBody: 'card-body',
        deleteBtn: 'btn btn-danger',
        chevron: '',
        saveActive: 'btn btn-primary sys-save',
        saveInactive: 'btn is-disabled sys-save',
    };

    const Icons = {
        chevronDown: () => el('svg', 'icon-sm', { fill: 'none', stroke: 'currentColor', viewBox: '0 0 24 24' },
            el('path', '', { 'stroke-linecap': 'round', 'stroke-linejoin': 'round', 'stroke-width': '2', d: 'M19 9l-7 7-7-7' })
        ),
        delete: () => el('svg', 'icon-md', { fill: 'none', viewBox: '0 0 24 24', stroke: 'currentColor' },
            el('path', '', { 'stroke-linecap': 'round', 'stroke-linejoin': 'round', 'stroke-width': '2', d: 'M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16' })
        ),
        plus: () => el('svg', 'icon-sm t-muted', { fill: 'none', stroke: 'currentColor', viewBox: '0 0 24 24' },
            el('path', '', { 'stroke-linecap': 'round', 'stroke-linejoin': 'round', 'stroke-width': '2', d: 'M12 4v16m8-8H4' })
        ),
    };

    const App = {
        state: { unsaved: false },

        notify(type, message, duration, onClose) {
            return window.AISComponents.toast(type, message, duration, undefined, onClose);
        },

        notifyWarnings(warnings) {
            if (!warnings || !warnings.length) return;
            const msgs = warnings.map(w => `${w.path} = ${JSON.stringify(w.value)} is not a valid ${w.type}`);
            console.warn('Config normalization warnings:', msgs);
            let banner = document.getElementById('normalize-warning-banner');
            if (!banner) {
                banner = el('div', 'alert alert-warning sys-floating-alert', { id: 'normalize-warning-banner' });
                document.body.appendChild(banner);
            }
            banner.innerHTML = '';
            banner.appendChild(el('div', 'row row-between row-top row-loose', {},
                el('div', '', {},
                    el('p', 't-strong', {}, `${msgs.length} config value(s) could not be normalized and were left unchanged:`),
                    el('div', 'stack stack-tight', {}, ...msgs.slice(0, 12).map(m => el('div', '', {}, m))),
                    msgs.length > 12 ? el('p', 't-warn', {}, `…and ${msgs.length - 12} more (see console).`) : null
                ),
                el('button', 't-warn t-strong', { type: 'button', title: 'Dismiss', onClick: () => banner.remove() }, '✕')
            ));
        },

        setUnsaved(bool) {
            if (!bool) ManagerRegistry.forEach(m => { m.dirty = false; });
            this.state.unsaved = bool;
            const statusEl = document.getElementById('status-message');

            if (statusEl) {
                if (bool) {
                    statusEl.className = 'alert alert-warning sys-status-alert';
                    statusEl.innerHTML = `
                        <svg class="icon-sm t-warn" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z"></path></svg>
                        <span class="t-small">You have unsaved changes.</span>
                    `;
                } else {
                    statusEl.className = 'hidden';
                }
            }

            document.querySelectorAll('[data-save-btn]').forEach(btn => {
                btn.className = bool ? Styles.saveActive : Styles.saveInactive;
            });
            window.onbeforeunload = bool ? () => true : null;
        }
    };

    function sliderInput(field, currentValue, onUpdate, opts) {
        const isOff = opts.isOff(currentValue);
        const numVal = isOff ? opts.fallback : opts.parse(currentValue);
        const display = el('span', Styles.sliderDisplay, {}, isNaN(numVal) ? 0 : numVal);

        const slider = el('input', Styles.slider, {
            type: 'range',
            min: field.min, max: field.max, step: field.step || opts.defaultStep,
            value: isNaN(numVal) ? opts.fallback : numVal,
            onInput: (e) => {
                display.textContent = e.target.value;
                if (isActive()) onUpdate(opts.parse(e.target.value));
            }
        });

        const children = [slider, display];
        if (opts.unit) children.push(el('span', 't-small t-muted', {}, opts.unit));
        const sliderContainer = el('div', Styles.sliderContainer + (isOff ? ' hidden' : ''), {}, ...children);

        const setActive = (active) => {
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

    const Renderer = {
        createInput(field, currentValue, onUpdate, index = 0, containerId = '', dataContext = null) {
            const type = field.type;

            if (type === 'select') {
                const valStr = currentValue !== undefined && currentValue !== null ? String(currentValue) : '';
                return el('div', 'relative', {},
                    el('select', `${Styles.input} ${Styles.select}`, {
                        onChange: e => {
                            onUpdate(e.target.value);
                            if (field.onChange) {
                                const action = typeof field.onChange === 'function'
                                    ? field.onChange
                                    : ActionRegistry[field.onChange];
                                if (action) action(index, containerId);
                            }
                        }
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
                        checked: Utils.toBoolean(currentValue),
                        onChange: e => onUpdate(e.target.checked)
                    })
                );
            }

            if (type === 'integer-select') {
                const parsedCurrent = Utils.parseInteger(currentValue);
                const isPreset = field.presets && field.presets.some(p => Utils.parseInteger(p.value) === parsedCurrent);

                const customInput = el('input', Styles.input + (isPreset ? ' hidden' : ''), {
                    type: 'number',
                    value: isPreset ? '' : parsedCurrent,
                    placeholder: field.placeholder || 'Enter custom value...',
                    onInput: (e) => {
                        const n = parseInt(e.target.value, 10);
                        if (!isNaN(n)) onUpdate(n);
                    }
                });

                const select = el('div', 'relative', {},
                    el('select', `${Styles.input} ${Styles.select}`, {
                        onChange: (e) => {
                            if (e.target.value === 'custom') {
                                customInput.classList.remove('hidden');
                                customInput.focus();
                            } else {
                                customInput.classList.add('hidden');
                                onUpdate(parseInt(e.target.value, 10));
                            }
                        }
                    },
                        ...(field.presets || []).map(p => el('option', '', { value: p.value, selected: Utils.parseInteger(p.value) === parsedCurrent }, p.label)),
                        el('option', '', { value: 'custom', selected: !isPreset }, 'Custom Value...')
                    )
                );
                return el('div', 'col', {}, select, customInput);
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

                const wrapper = el('div', 'col');
                const rows = el('div', 'col col-loose');

                const push = () => onUpdate(list.map(e => ({ ...e })));

                const addBtn = el('button', 'chip sys-chip-btn self-start', {
                    type: 'button',
                    onClick: () => { list.push({}); push(); render(); }
                },
                    el('svg', 'icon-sm', { fill: 'none', stroke: 'currentColor', viewBox: '0 0 24 24' },
                        el('path', '', { 'stroke-linecap': 'round', 'stroke-linejoin': 'round', 'stroke-width': '2', d: 'M12 4v16m8-8H4' })),
                    'Add Engine');

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
                        const tuned = Object.keys(entry.settings || {}).some(k => !shown.includes(k));
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
                                }, Icons.delete())));

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
                            },
                                el('svg', 'icon-sm', { fill: 'none', stroke: 'currentColor', viewBox: '0 0 24 24' },
                                    el('path', '', { 'stroke-linecap': 'round', 'stroke-linejoin': 'round', 'stroke-width': '2', d: 'M12 4v16m8-8H4' })),
                                'Add Zone'));
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

            if (type === 'zones') {
                const zones = Array.isArray(currentValue) ? [...currentValue] : [];

                const wrapper = el('div', 'col col-tight');
                const badgesDiv = el('div', 'row row-wrap row-tight');
                const manageBtn = el('button', 'chip sys-chip-btn', {
                    type: 'button',
                    onClick: () => openZoneModal(zones, (updated) => {
                        zones.length = 0;
                        updated.forEach(z => zones.push(z));
                        onUpdate([...zones]);
                        renderZoneBadges();
                    })
                },
                    el('svg', 'icon-sm', { fill: 'none', stroke: 'currentColor', viewBox: '0 0 24 24' },
                        el('path', '', { 'stroke-linecap': 'round', 'stroke-linejoin': 'round', 'stroke-width': '2', d: 'M12 4v16m8-8H4' })
                    ),
                    'Add Zone'
                );

                function renderZoneBadges() {
                    badgesDiv.innerHTML = '';
                    zones.forEach((zone, i) => {
                        badgesDiv.appendChild(zoneChip(zone, () => {
                            zones.splice(i, 1);
                            onUpdate([...zones]);
                            renderZoneBadges();
                        }));
                    });
                    badgesDiv.appendChild(manageBtn);
                    if (field.hint) {
                        const hintText = zones.length === 0 ? field.hint : '';
                        hintEl.textContent = hintText;
                    }
                }

                const hintEl = el('span', 't-small t-subtle t-italic');
                renderZoneBadges();
                wrapper.appendChild(badgesDiv);
                if (field.hint) wrapper.appendChild(hintEl);
                return wrapper;
            }

            const isNumber = type === 'number';
            return el('input', Styles.input, {
                type: type || 'text',
                value: currentValue !== undefined ? currentValue : '',
                placeholder: field.placeholder || '',
                min: isNumber ? field.min : undefined,
                max: isNumber ? field.max : undefined,
                step: isNumber ? field.step : undefined,
                onInput: Utils.debounce((e) => {
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

        renderField(field, index, dataContext, onUpdateCallback, onDependencyCheck, containerId = '') {
            let val = field.jsonpath ? Utils.getNested(dataContext, field.jsonpath) : dataContext[field.name];
            if (val === undefined) val = field.defaultValue;

            let inputEl = field.type === 'button'
                ? el('button', Styles.button, {
                    type: 'button',
                    onClick: () => (typeof field.onClick === 'function' ? field.onClick : ActionRegistry[field.onClick])?.(index, containerId)
                }, field.buttonIcon && el('span', '', { innerHTML: field.buttonIcon }),
                typeof field.buttonLabel === 'function' ? field.buttonLabel(dataContext) : (field.buttonLabel || 'Action'))
                : this.createInput(field, val, newValue => { onUpdateCallback(field, newValue); onDependencyCheck(); }, index, containerId, dataContext);

            if (field.withButton && field.type !== 'button') {
                const action = typeof field.withButton.onClick === 'function'
                    ? field.withButton.onClick
                    : ActionRegistry[field.withButton.onClick] || (() => { });
                inputEl = el('div', 'row', {},
                    el('div', 'grow', {}, inputEl),
                    el('button', Styles.buttonPrimary, { type: 'button', onClick: () => action(index, containerId) },
                        el('span', '', { innerHTML: field.withButton.icon || 'Action' }))
                );
            }

            const container = el('div', 'field', {
                dataset: { field: field.name, dependsOn: field.dependsOn ? JSON.stringify(field.dependsOn) : null },
                style: field.width ? `flex: 0 1 calc(${field.width}% - 0.375rem); min-width: 0;` : 'flex: 1 1 100%;'
            });

            if (field.type !== 'button' && field.label) container.appendChild(el('label', Styles.label, {}, field.label));
            container.appendChild(inputEl);
            
            if (field.type === 'button' && typeof field.buttonLabel === 'function') {
                container._buttonEl = inputEl;
                container._field = field;
                container._dataContext = dataContext;
            }
            const desc = field.description || field.tooltip;
            if (desc) container.appendChild(el('p', Styles.description, {}, desc));

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
                        div.classList.add('hidden');
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

                    div.classList.toggle('hidden', !match);
                    div.querySelectorAll('input, select, button').forEach(el => el.disabled = !match);
                });
            }
            container.querySelectorAll('[data-sync-context]').forEach(div => div._syncContext && div._syncContext());
        }
    };

    class ConfigManager {
        constructor(config) {
            this.config = config;
            this.container = document.getElementById(config.containerId);
            this.fields = Object.values(config.schema);
            this.data = config.isList ? [] : {};
            this.hasViewerFields = this.fields.some(f => f.reloadWebviewer || f.restartWebviewer);
            this.reloadWebviewer = false;
            this.restartWebviewer = false;
            this.dirty = false;

            if (!this.container) return;

            ManagerRegistry.set(config.containerId, this);
            this.load();
        }

        load() {
            this.container.textContent = '';
            this.container.appendChild(el('div', 't-center sys-empty', {},
                el('div', 'spinner icon-lg center-block'),
                el('p', 't-muted', {}, 'Loading...')));

            this.loadData().then(() => {
                this.render();
                this.updateJsonDebug();
            }).catch(() => {
                this.container.textContent = '';
                this.container.appendChild(el('div', 't-center sys-empty', {},
                    el('p', 't-muted', {}, 'Failed to load configuration'),
                    el('button', 'btn btn-primary', {
                        type: 'button', onClick: () => this.load()
                    }, 'Retry')));
            });
        }

        async loadData() {
            const fullConfig = await ConfigStore.fetch();
            if (!fullConfig || typeof fullConfig !== 'object') throw new Error('Invalid configuration');

            const { warnings } = ConfigNormalizer.normalize(fullConfig);
            if (warnings.length) App.notifyWarnings(warnings);
            if (this.config.nestedPath)
                this.data = this.getNested(fullConfig) || (this.config.isList ? [] : {});
            else
                this.data = this.config.channelType
                    ? (fullConfig[this.config.channelType] || (this.config.isList ? [] : {}))
                    : fullConfig;
            if (!this.config.isList) {
                this.fields.forEach(f => {
                    if (f.jsonpath) {
                        if (Utils.getNested(this.data, f.jsonpath) === undefined)
                            Utils.setNested(this.data, f.jsonpath, f.defaultValue);
                    } else {
                        this.data[f.name] ??= f.defaultValue;
                    }
                });
            }
        }

        render() {
            this.container.innerHTML = '';
            const items = this.config.isList ? this.data : [this.data];

            items.forEach((item, index) => {
                const activeField = this.config.isList ? this.fields.find(f => f.name === 'active') : null;
                const isActive = activeField
                    ? Utils.toBoolean(item.active !== undefined ? item.active : activeField.defaultValue)
                    : true;

                const wrapper = el('div', Styles.card, { dataset: { receiverCard: index } });

                if (this.config.isList) {
                    const toggleInput = activeField ? el('input', 'toggle toggle-sm toggle-on', {
                        type: 'checkbox',
                        checked: isActive,
                        onChange: e => this.updateValue(index, activeField, e.target.checked)
                    }) : null;

                    const header = el('div', Styles.cardHeader, {},
                        el('div', 'row', {},
                            el('span', 'badge', {}, `${index + 1}`),
                            el('h4', 'card-title', {}, this.config.title)
                        ),
                        el('div', 'row', {},
                            activeField ? el('label', 'row row-tight clickable', {},
                                el('span', 't-small t-subtle', {}, 'Active'),
                                toggleInput
                            ) : null,
                            el('button', Styles.deleteBtn, {
                                type: 'button',
                                title: 'Remove Item',
                                onClick: () => this.removeItem(index)
                            }, Icons.delete())
                        )
                    );
                    wrapper.appendChild(header);
                } else if (this.config.title) {
                    const header = el('div', Styles.cardHeader, {},
                        el('h4', 'card-title', {}, this.config.title)
                    );
                    wrapper.appendChild(header);
                }

                const fieldsDiv = el('div', Styles.cardBody);
                const innerFields = el('div', 'fieldset');
                // `advanced` puts a field in a collapsible section; a string names it
                const sections = new Map();
                const syncs = [];
                let syncAdvanced = () => syncs.forEach(f => f());
                const refresh = () => { Renderer.updateVisibility(innerFields, item); syncAdvanced(); };

                // Section all fields of a schema or none: an unsectioned one trailing a block reads as part of it.
                const ordered = [];
                const bySection = new Map();
                this.fields.forEach(f => {
                    const key = (!f.advanced && f.section) || null;
                    if (key === null) return ordered.push(f);
                    if (!bySection.has(key)) bySection.set(key, []);
                    bySection.get(key).push(f);
                });
                const order = this.config.sectionOrder || SECTION_ORDER;
                const rank = k => { const i = order.indexOf(k); return i === -1 ? order.length : i; };
                [...bySection.keys()].sort((a, b) => rank(a) - rank(b))
                    .forEach(k => ordered.push(bySection.get(k)));

                let currentSection = null;
                ordered.flat().forEach(field => {
                    if (activeField && field.name === 'active') return;
                    if (field.section && field.section !== currentSection && !field.advanced) {
                        currentSection = field.section;
                        innerFields.appendChild(el('h5', Styles.section, {}, field.section));
                    }
                    const fieldEl = Renderer.renderField(field, index, item,
                        (fld, val) => this.updateValue(index, fld, val),
                        refresh,
                        this.config.containerId
                    );
                    if (!field.advanced) return innerFields.appendChild(fieldEl);
                    const title = typeof field.advanced === 'string' ? field.advanced : 'Advanced';
                    if (!sections.has(title)) sections.set(title, el('div', 'fieldset full'));
                    sections.get(title).appendChild(fieldEl);
                });

                this.advancedOpen = this.advancedOpen || new Set();
                sections.forEach((body, title) => {
                    const key = `${index}|${title}`;
                    const open = this.advancedOpen.has(key);
                    const chevron = el('span', 'sys-rotates' + (open ? ' rotate-90' : ''), { innerHTML: '&#9656;' });
                    body.classList.toggle('hidden', !open);
                    const btn = el('button', 'row row-tight t-small t-subtle clickable', {
                        type: 'button',
                        onClick: () => {
                            const nowOpen = !this.advancedOpen.has(key);
                            if (nowOpen) this.advancedOpen.add(key); else this.advancedOpen.delete(key);
                            body.classList.toggle('hidden', !nowOpen);
                            chevron.classList.toggle('rotate-90', nowOpen);
                        }
                    }, chevron, title);
                    const row = el('div', 'full', {}, btn);
                    innerFields.appendChild(row);
                    innerFields.appendChild(body);
                    syncs.push(() => {
                        const any = Array.from(body.children).some(c => !c.classList.contains('hidden'));
                        row.classList.toggle('hidden', !any);
                    });
                });
                fieldsDiv.appendChild(innerFields);

                wrapper.appendChild(fieldsDiv);
                this.container.appendChild(wrapper);

                refresh();
            });

            this.renderControls();
        }

        renderControls() {
            const containerIdClass = `controls-${this.config.containerId}`;
            const existing = this.container.parentElement.querySelector('.' + containerIdClass);
            if (existing) existing.remove();

            this.ensureJsonUI();

            const btnGroup = el('div', `${containerIdClass} row row-wrap row-end sys-pane-inner sys-actions`);

            if (this.config.isList) {
                btnGroup.appendChild(el('button', 'btn sys-save', {
                    type: 'button', onClick: () => this.addItem()
                },
                    Icons.plus(),
                    'Add Item'));
            }

            btnGroup.appendChild(el('button', 'btn is-disabled sys-save', {
                type: 'button', dataset: { saveBtn: 'true' }, onClick: () => this.save()
            }, 'Save'));

            const jsonSection = this.container.parentElement.querySelector('[data-json-section]');
            if (jsonSection) {
                this.container.parentElement.insertBefore(btnGroup, jsonSection);
            } else {
                this.container.parentElement.appendChild(btnGroup);
            }

            if (App.state.unsaved) App.setUnsaved(true);
        }

        // Each manager owns its own JSON disclosure (ids keyed by containerId)
        // so sibling sections in the same view don't fight over one shared box.
        jsonPreId() { return 'json-pre-' + this.config.containerId; }

        ensureJsonUI() {
            const parent = this.container.parentElement;
            if (!parent || document.getElementById(this.jsonPreId())) return;

            const contentId = 'json-content-' + this.config.containerId;
            const chevronId = 'chevron-' + this.config.containerId;

            const toggleDiv = el('div', 'sys-pane-inner sys-json-sep', { dataset: { jsonSection: '' } });
            const btn = el('button', 'row t-muted clickable', {
                type: 'button', onClick: () => global.toggleJsonContent(contentId, chevronId)
            });

            const chevron = el('svg', 'icon-sm t-subtle sys-rotates', {
                id: chevronId, fill: 'none', viewBox: '0 0 24 24', stroke: 'currentColor'
            }, el('path', '', { 'stroke-linecap': 'round', 'stroke-linejoin': 'round', 'stroke-width': '2', d: 'M19 9l-7 7-7-7' }));

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

        updateValue(index, field, value) {
            const target = this.config.isList ? this.data[index] : this.data;
            if (field.jsonpath) Utils.setNested(target, field.jsonpath, value);
            else target[field.name] = value;
            if (field.reloadWebviewer) this.reloadWebviewer = true;
            if (field.restartWebviewer) this.restartWebviewer = true;
            this.dirty = true;
            App.setUnsaved(true);
            this.updateJsonDebug();
            
            this.updateDynamicButtons(index);
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
            if (this.hasViewerFields) this.reloadWebviewer = true;
            this.dirty = true;
            App.setUnsaved(true);
            this.updateJsonDebug();
        }

        removeItem(index) {
            if (confirm(`Are you sure you want to remove this ${this.config.title}?`)) {
                this.data.splice(index, 1);
                this.render();
                if (this.hasViewerFields) this.reloadWebviewer = true;
                this.dirty = true;
                App.setUnsaved(true);
                this.updateJsonDebug();
            }
        }

        updateDynamicButtons(index) {
            const targetContainer = this.config.isList
                ? this.container.querySelector(`[data-receiver-card="${index}"]`)
                : this.container;
            if (!targetContainer) return;
            
            const dataContext = this.config.isList ? this.data[index] : this.data;
            
            targetContainer.querySelectorAll('[data-field]').forEach(fieldContainer => {
                if (fieldContainer._buttonEl && fieldContainer._field && typeof fieldContainer._field.buttonLabel === 'function') {
                    const newLabel = fieldContainer._field.buttonLabel(dataContext);
                    const textNode = Array.from(fieldContainer._buttonEl.childNodes).find(node => node.nodeType === Node.TEXT_NODE);
                    if (textNode) {
                        textNode.textContent = newLabel;
                    }
                }
            });
        }

        getNested(obj) {
            return Utils.getNested(obj, this.config.nestedPath.join('.'));
        }

        setNested(obj) {
            Utils.setNested(obj, this.config.nestedPath.join('.'), this.data);
        }

        applyDataTo(target, { forSave = false } = {}) {
            if (this.config.nestedPath) {
                this.setNested(target);
            } else if (this.config.channelType) {
                target[this.config.channelType] = this.data;
            } else {
                this.fields.forEach(f => {
                    if (forSave && f.skipSave) return;
                    const val = f.jsonpath ? Utils.getNested(this.data, f.jsonpath) : this.data[f.name];
                    if (val === undefined) return;
                    if (f.jsonpath) Utils.setNested(target, f.jsonpath, val);
                    else target[f.name] = val;
                });
            }
        }

        updateJsonDebug() {
            const el = document.getElementById(this.jsonPreId());
            if (!el) return;
            const cfg = {};
            this.applyDataTo(cfg);
            const text = JSON.stringify(cfg, null, 2);
            if (typeof global.highlightJson === 'function') el.innerHTML = global.highlightJson(text);
            else el.textContent = text;
        }

        async save() {
            const saveBtn = this.container.parentElement.querySelector(`.controls-${this.config.containerId} [data-save-btn]`);

            if (saveBtn) {
                saveBtn.textContent = 'Saving...';
                saveBtn.disabled = true;
            }

            try {
                // read-modify-write against a fresh copy so other sections keep
                // any changes made outside this manager
                const fullConfig = await ConfigStore.fetch(true);
                this.applyDataTo(fullConfig, { forSave: true });

                if (!fullConfig.config) fullConfig.config = 'aiscatcher';
                if (!fullConfig.version) fullConfig.version = 1;

                const { warnings } = ConfigNormalizer.normalize(fullConfig);
                if (warnings.length) App.notifyWarnings(warnings);

                const saveRes = await fetch('/api/config', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(fullConfig)
                });

                if (!saveRes.ok) {
                    authFailed(saveRes);
                    const body = await saveRes.json().catch(() => null);
                    throw new Error(body && body.error ? body.error : 'Save failed');
                }
                ConfigStore.invalidate();
                this.dirty = false;
                let anyDirty = false;
                ManagerRegistry.forEach(m => { if (m.dirty) anyDirty = true; });
                App.setUnsaved(anyDirty);
                App.notify('success', 'Configuration saved successfully');
                if (global.hubConfigSaved)
                    global.hubConfigSaved(this.config.nestedPath ? 'viewer' : 'engine',
                        { reloadWebviewer: this.reloadWebviewer, restartWebviewer: this.restartWebviewer });
                this.reloadWebviewer = false;
                this.restartWebviewer = false;
            } catch (e) {
                console.error(e);
                App.notify('error', 'Failed to save configuration: ' + e.message);
            } finally {
                if (saveBtn) {
                    saveBtn.textContent = 'Save';
                    saveBtn.disabled = false;
                }
            }
        }
    }

    global.App = App;
    global.Utils = Utils;
    global.ConfigStore = ConfigStore;
    global.ZoneColors = { badge: getZoneColor, css: getZoneCss };
    global.normalizeConfig = (cfg) => ConfigNormalizer.normalize(cfg);
    global.createSimpleConfigManager = (config) => { config.isList = false; return new ConfigManager(config); };
    global.createChannelManager = (config) => { config.isList = true; return new ConfigManager(config); };

    global.toggleJsonContent = (contentId, chevronId) => {
        const c = document.getElementById(contentId);
        const i = document.getElementById(chevronId);
        if (c) {
            const hidden = c.classList.toggle('hidden');
            if (i) i.classList.toggle('rotate-90', !hidden);
        }
    };

})(window);