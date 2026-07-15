
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

    // Shared shell for the device and serial-port pickers; the list lives in
    // '<id>-list' and is rebuilt on every open.
    function ensurePickerModal(id, title) {
        let modal = document.getElementById(id);
        if (modal) return modal;
        modal = el('div', 'fixed inset-0 flex items-center justify-center hidden z-[100] p-4',
            { id, style: 'background-color: rgba(0,0,0,0.3)' },
            el('div', 'bg-white rounded-lg shadow-xl w-full max-w-2xl max-h-[90vh] flex flex-col relative', {},
                el('div', 'flex justify-between items-center p-3 md:p-4 border-b flex-shrink-0', {},
                    el('h3', 'text-base md:text-lg font-medium text-gray-800', {}, title),
                    el('button', 'text-gray-600 hover:text-gray-800', { type: 'button', onClick: () => modal.classList.add('hidden') },
                        el('svg', 'h-6 w-6', { fill: 'none', viewBox: '0 0 24 24', stroke: 'currentColor' },
                            el('path', '', { 'stroke-linecap': 'round', 'stroke-linejoin': 'round', 'stroke-width': '2', d: 'M6 18L18 6M6 6l12 12' })
                        )
                    )
                ),
                el('div', 'p-3 md:p-4 overflow-y-auto flex-1', { id: id + '-list' }),
                el('div', 'flex justify-end p-3 md:p-4 border-t flex-shrink-0', {},
                    el('button', 'bg-white border border-slate-300 text-slate-700 px-4 py-2 rounded-lg hover:bg-slate-50 shadow-sm transition duration-200 text-sm', {
                        type: 'button', onClick: () => modal.classList.add('hidden')
                    }, 'Close')
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
        App.setUnsaved(true);
        mgr.updateJsonDebug();
        mgr.render();
    }

    const ZONE_COLORS = [
        'bg-blue-100 text-blue-700 border border-blue-200',
        'bg-emerald-100 text-emerald-700 border border-emerald-200',
        'bg-violet-100 text-violet-700 border border-violet-200',
        'bg-amber-100 text-amber-700 border border-amber-200',
        'bg-rose-100 text-rose-700 border border-rose-200',
        'bg-cyan-100 text-cyan-700 border border-cyan-200',
        'bg-orange-100 text-orange-700 border border-orange-200',
        'bg-teal-100 text-teal-700 border border-teal-200',
    ];
    const ZONE_HEX = ['#93c5fd', '#6ee7b7', '#c4b5fd', '#fcd34d', '#fca5a5', '#67e8f9', '#fdba74', '#5eead4'];

    function zoneHash(zone) {
        let hash = 0;
        for (let i = 0; i < zone.length; i++) hash = (hash * 31 + zone.charCodeAt(i)) & 0xFFFF;
        return hash;
    }
    const getZoneColor = zone => ZONE_COLORS[zoneHash(zone) % ZONE_COLORS.length];
    const getZoneHex = zone => ZONE_HEX[zoneHash(zone) % ZONE_HEX.length];

    async function fetchAllZones() {
        try {
            const cfg = await ConfigStore.fetch();
            const zones = new Set();
            ['receiver', 'udp', 'tcp', 'http', 'mqtt', 'tcp_listener', 'server'].forEach(key => {
                (cfg[key] || []).forEach(item => { if (Array.isArray(item.zone)) item.zone.forEach(z => zones.add(z)); });
            });
            if (Array.isArray(cfg.sharing_zone)) cfg.sharing_zone.forEach(z => zones.add(z));
            return [...zones].sort();
        } catch { return []; }
    }

    let activeZoneEdit = { zones: [], onUpdate: null, allZones: [] };

    function ensureZoneModal() {
        if (document.getElementById('cm-zone-modal')) return;
        const modal = el('div', 'fixed inset-0 flex items-center justify-center hidden z-[100] p-4',
            { id: 'cm-zone-modal', style: 'background-color: rgba(0,0,0,0.3)' },
            el('div', 'bg-white rounded-lg shadow-xl w-full max-w-sm flex flex-col relative', {},
                el('div', 'flex justify-between items-center p-4 border-b flex-shrink-0', {},
                    el('h3', 'text-base font-medium text-gray-800', {}, 'Manage Zones'),
                    el('button', 'text-gray-600 hover:text-gray-800', { type: 'button', onClick: () => modal.classList.add('hidden') },
                        el('svg', 'h-5 w-5', { fill: 'none', viewBox: '0 0 24 24', stroke: 'currentColor' },
                            el('path', '', { 'stroke-linecap': 'round', 'stroke-linejoin': 'round', 'stroke-width': '2', d: 'M6 18L18 6M6 6l12 12' })
                        )
                    )
                ),
                el('div', 'p-4 flex flex-col gap-4', { id: 'cm-zone-body' }),
                el('div', 'flex justify-end p-4 border-t flex-shrink-0', {},
                    el('button', 'bg-white border border-slate-300 text-slate-700 px-4 py-2 rounded-lg hover:bg-slate-50 shadow-sm transition text-sm', {
                        type: 'button', onClick: () => modal.classList.add('hidden')
                    }, 'Done')
                )
            )
        );
        document.body.appendChild(modal);
    }

    function openZoneModal(currentZones, onUpdate) {
        ensureZoneModal();
        activeZoneEdit.zones = [...currentZones];
        activeZoneEdit.onUpdate = onUpdate;
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

        const chipsWrap = el('div', 'flex flex-wrap gap-1.5 min-h-[2rem]');
        if (activeZoneEdit.zones.length === 0) {
            chipsWrap.appendChild(el('span', 'text-xs text-slate-400 italic', {}, 'No zones assigned'));
        } else {
            activeZoneEdit.zones.forEach((zone, i) => {
                const chip = el('span', `inline-flex items-center gap-1 px-2 py-0.5 rounded-full text-xs font-medium ${getZoneColor(zone)}`, {},
                    zone,
                    el('button', 'ml-0.5 hover:opacity-60 font-bold leading-none', {
                        type: 'button',
                        onClick: () => {
                            activeZoneEdit.zones.splice(i, 1);
                            activeZoneEdit.onUpdate([...activeZoneEdit.zones]);
                            renderZoneModalBody();
                        }
                    }, '×')
                );
                chipsWrap.appendChild(chip);
            });
        }
        body.appendChild(chipsWrap);

        const addSection = el('div', 'flex flex-col gap-1');
        const inputRow = el('div', 'flex gap-2');
        const input = el('input', 'flex-1 px-3 py-1.5 border border-slate-300 rounded-lg text-sm focus:outline-none focus:ring-2 focus:ring-slate-500', {
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
        inputRow.appendChild(el('button', 'px-3 py-1.5 bg-white border border-slate-300 text-slate-700 rounded-lg text-sm hover:bg-slate-50 shadow-sm transition', {
            type: 'button', onClick: addZoneFromInput
        }, 'Add'));
        addSection.appendChild(inputRow);

        const suggestionsDiv = el('div', 'flex flex-wrap gap-1 mt-1', { id: 'cm-zone-suggestions' });
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
                suggestionsDiv.appendChild(el('button', `text-xs px-2 py-0.5 rounded-full ${getZoneColor(z)} hover:opacity-80 transition`, {
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
        debounce: (fn, ms) => { let t; return (...a) => { clearTimeout(t); t = setTimeout(() => fn(...a), ms); }; },
        getNested: (obj, path) => path?.split('.').reduce((a, p) => a?.[p], obj),
        setNested: (obj, path, val) => {
            const keys = path.split('.'), last = keys.pop();
            keys.reduce((a, k) => a[k] = a[k] || {}, obj)[last] = val;
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
        arraySchemas: {
            receiver: () => receiverSchema,
            server: () => webviewerSchema,
            http: () => httpSchema,
            mqtt: () => mqttSchema,
            tcp: () => tcpSchema,
            udp: () => udpSchema,
            tcp_listener: () => tcpServerSchema,
        },
        topLevelSchemas: () => [sharingSchema, generalSettingsSchema],

        receiverKeys: ['serial', 'input', 'verbose', 'model', 'meta', 'own_mmsi',
            'rtlsdr', 'rtltcp', 'airspy', 'airspyhf', 'hydrasdr', 'sdrplay', 'serialport',
            'hackrf', 'udpserver', 'soapysdr', 'nmea2000', 'file', 'zmq',
            'spyserver', 'wavfile'],

        migrate(cfg) {
            let changed = false;

            if (cfg.server && !Array.isArray(cfg.server) && typeof cfg.server === 'object') {
                cfg.server = [cfg.server];
                changed = true;
            }

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

            for (const [key, getSchema] of Object.entries(this.arraySchemas)) {
                if (!Array.isArray(cfg[key])) continue;
                const fields = Object.values(getSchema());
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

    const ActionRegistry = {
        openSerialDeviceModal: (index, containerId) => {
            const modal = ensurePickerModal('cm-serial-modal', 'Select Serial Device');
            const list = document.getElementById('cm-serial-modal-list');
            list.innerHTML = '<div class="text-center p-4 text-slate-500">Loading...</div>';
            modal.classList.remove('hidden');
            fetch('/api/serial')
                .then(r => { if (authFailed(r)) throw new Error(); return r.json(); })
                .then(data => {
                    list.innerHTML = '';
                    const devices = Array.isArray(data) ? data : [];
                    if (devices.length === 0) {
                        list.innerHTML = '<li class="text-gray-500 p-2">No devices found</li>';
                        return;
                    }
                    devices.forEach(device => {
                        list.appendChild(el('li', 'p-2 hover:bg-gray-100 cursor-pointer rounded', {
                            onClick: () => {
                                applyToManager(containerId, index, target => {
                                    if (!target.serialport) target.serialport = {};
                                    target.serialport.port = device;
                                });
                                modal.classList.add('hidden');
                            }
                        }, device));
                    });
                })
                .catch(() => { list.innerHTML = '<li class="text-red-500 p-2">Error loading devices</li>'; });
        },

        openDeviceSelectionModal: (index, containerId) => {
            const modal = ensurePickerModal('cm-device-modal', 'Select Device');
            const list = document.getElementById('cm-device-modal-list');
            list.innerHTML = '<div class="text-center p-4 text-slate-500">Loading...</div>';
            modal.classList.remove('hidden');
            fetch('/api/devices')
                .then(r => { if (authFailed(r)) throw new Error(); return r.json(); })
                .then(data => {
                    list.innerHTML = '';
                    const devices = Array.isArray(data.devices) ? data.devices : [];
                    if (devices.length === 0) {
                        list.innerHTML = '<li class="text-gray-500 p-2">No devices found</li>';
                        return;
                    }
                    devices.forEach(device => {
                        list.appendChild(el('li', 'flex items-center justify-between p-2 border rounded-md hover:bg-gray-100 mb-2', {},
                            el('span', '', {}, device.name),
                            el('button', 'bg-white border border-slate-300 text-slate-700 px-2 py-1 rounded-md hover:bg-slate-50 shadow-sm text-sm', {
                                type: 'button',
                                onClick: () => {
                                    applyToManager(containerId, index, target => {
                                        target.input = device.input ? device.input.toUpperCase() : '';
                                        target.serial = device.serial || '';
                                    });
                                    modal.classList.add('hidden');
                                }
                            }, 'Select')
                        ));
                    });
                })
                .catch(() => { list.innerHTML = '<li class="text-red-500 p-2">Error loading devices</li>'; });
        },

        openRegistration: () => window.open('https://aiscatcher.org/register', '_blank'),

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

    const Styles = {
        input: 'w-full px-2 sm:px-3 py-1.5 sm:py-2 bg-white border border-slate-300 rounded-lg text-slate-700 placeholder-slate-400 focus:outline-none focus:ring-2 focus:ring-slate-500 focus:border-transparent transition-all duration-200 shadow-sm text-xs sm:text-sm',
        select: 'appearance-none pr-8',
        toggle: "relative w-11 h-6 bg-slate-200 peer-focus:outline-none peer-focus:ring-2 peer-focus:ring-blue-300 rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-[#0857b1]",
        slider: 'flex-1 h-2 bg-slate-200 rounded-lg appearance-none cursor-pointer accent-[#0857b1]',
        sliderContainer: 'flex items-center gap-2 sm:gap-3 p-1.5 sm:p-2 bg-slate-50 rounded-lg border border-slate-100',
        sliderDisplay: 'text-xs sm:text-sm font-mono font-medium text-slate-600 min-w-[2.5rem] sm:min-w-[3rem] text-center px-1.5 sm:px-2 py-0.5 sm:py-1 bg-slate-100 rounded border border-slate-200',
        label: 'block text-slate-700 text-xs sm:text-sm font-semibold mb-1 sm:mb-2 ml-0.5',
        description: 'mt-0.5 text-[10px] sm:text-xs text-slate-500 ml-0.5',
        button: 'w-full sm:w-auto bg-white border border-slate-300 text-slate-700 px-3 sm:px-4 py-1.5 sm:py-2 rounded-lg hover:bg-slate-50 transition duration-200 shadow-sm inline-flex items-center justify-center gap-2 text-xs sm:text-sm font-medium',
        buttonPrimary: 'px-2.5 sm:px-3 py-1.5 sm:py-2 bg-white border border-slate-300 text-slate-700 rounded-lg hover:bg-slate-50 transition duration-200 shadow-sm flex items-center justify-center min-w-[40px] sm:min-w-[44px] text-xs sm:text-sm',
        card: 'bg-white rounded-none sm:rounded-xl border-x-0 sm:border-x border-slate-200 shadow-sm sm:max-w-2xl sm:mx-auto mb-2 sm:mb-6 relative hover:shadow-md transition-shadow duration-300 overflow-hidden',
        cardHeader: 'flex justify-between items-center px-3 sm:px-5 py-2 sm:py-2.5 bg-slate-100 border-b border-slate-200',
        cardBody: 'p-3 sm:p-5',
        deleteBtn: 'text-slate-400 hover:text-rose-500 hover:bg-rose-50 p-1 sm:p-1.5 rounded-lg transition duration-200',
        chevron: 'pointer-events-none absolute inset-y-0 right-0 flex items-center px-2 text-slate-500',
        saveActive: 'w-auto sm:w-32 bg-[#0857b1] text-white px-4 sm:px-6 py-1.5 sm:py-2 rounded-lg hover:bg-[#0a63c6] shadow-md transition-all duration-200 text-xs sm:text-sm font-medium transform hover:-translate-y-0.5',
        saveInactive: 'w-auto sm:w-32 bg-white border border-slate-300 text-slate-400 px-4 sm:px-6 py-1.5 sm:py-2 rounded-lg hover:bg-slate-50 shadow-sm transition-all duration-200 text-xs sm:text-sm font-medium cursor-default',
    };

    const Icons = {
        chevronDown: () => el('svg', 'h-4 w-4', { fill: 'none', stroke: 'currentColor', viewBox: '0 0 24 24' },
            el('path', '', { 'stroke-linecap': 'round', 'stroke-linejoin': 'round', 'stroke-width': '2', d: 'M19 9l-7 7-7-7' })
        ),
        delete: () => el('svg', 'h-5 w-5', { fill: 'none', viewBox: '0 0 24 24', stroke: 'currentColor' },
            el('path', '', { 'stroke-linecap': 'round', 'stroke-linejoin': 'round', 'stroke-width': '2', d: 'M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16' })
        ),
        plus: () => el('svg', 'w-4 h-4 text-slate-500', { fill: 'none', stroke: 'currentColor', viewBox: '0 0 24 24' },
            el('path', '', { 'stroke-linecap': 'round', 'stroke-linejoin': 'round', 'stroke-width': '2', d: 'M12 4v16m8-8H4' })
        ),
    };

    const App = {
        state: { unsaved: false },

        notify(type, message, duration = 6000) {
            const container = document.getElementById('toast-container') || this.createToastContainer();
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
            }, duration);
        },

        createToastContainer() {
            const div = el('div', 'fixed bottom-6 right-6 z-50 flex flex-col items-end', { id: 'toast-container' });
            document.body.appendChild(div);
            return div;
        },

        notifyWarnings(warnings) {
            if (!warnings || !warnings.length) return;
            const msgs = warnings.map(w => `${w.path} = ${JSON.stringify(w.value)} is not a valid ${w.type}`);
            console.warn('Config normalization warnings:', msgs);
            let banner = document.getElementById('normalize-warning-banner');
            if (!banner) {
                banner = el('div', 'fixed bottom-6 right-6 z-50 max-w-md w-full bg-amber-50 border border-amber-200 text-amber-800 rounded-lg shadow-sm px-4 py-3 text-sm', { id: 'normalize-warning-banner' });
                document.body.appendChild(banner);
            }
            banner.innerHTML = '';
            banner.appendChild(el('div', 'flex items-start justify-between gap-3', {},
                el('div', '', {},
                    el('p', 'font-semibold mb-1', {}, `${msgs.length} config value(s) could not be normalized and were left unchanged:`),
                    el('div', 'space-y-1', {}, ...msgs.slice(0, 12).map(m => el('div', '', {}, m))),
                    msgs.length > 12 ? el('p', 'mb-1 text-amber-700', {}, `…and ${msgs.length - 12} more (see console).`) : null
                ),
                el('button', 'text-amber-700 font-bold leading-none', { type: 'button', title: 'Dismiss', onClick: () => banner.remove() }, '✕')
            ));
        },

        setUnsaved(bool) {
            this.state.unsaved = bool;
            const statusEl = document.getElementById('status-message');

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

            document.querySelectorAll('[data-save-btn]').forEach(btn => {
                btn.className = bool ? Styles.saveActive : Styles.saveInactive;
            });
            window.onbeforeunload = bool ? () => true : null;
        }
    };

    const Renderer = {
        createInput(field, currentValue, onUpdate, index = 0, containerId = '') {
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
                    ),
                    el('div', Styles.chevron, {}, Icons.chevronDown())
                );
            }

            if (type === 'toggle') {
                // match Styles.input height so the pill centers on the same midline as inputs in the row
                return el('label', 'relative inline-flex items-center cursor-pointer group h-[30px] sm:h-[38px]', {},
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
                    onInput: (e) => {
                        const n = parseInt(e.target.value, 10);
                        if (!isNaN(n)) onUpdate(n);
                    }
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

            if (type === 'off-number') {
                const isOff = currentValue === false || currentValue === 'false';
                const numVal = isOff ? (field.defaultNumber || field.min || 0) : parseFloat(currentValue);
                const display = el('span', Styles.sliderDisplay, {}, isNaN(numVal) ? 0 : numVal);
                const unit = field.unit ? el('span', 'text-xs sm:text-sm text-slate-600 ml-1', {}, field.unit) : null;

                const slider = el('input', Styles.slider, {
                    type: 'range',
                    min: field.min, max: field.max, step: field.step || 1,
                    value: isNaN(numVal) ? (field.defaultNumber || field.min || 0) : numVal,
                    onInput: (e) => {
                        display.textContent = e.target.value;
                        if (selector.querySelector('select').value === 'custom') onUpdate(parseFloat(e.target.value));
                    }
                });

                const sliderChildren = [slider, display];
                if (unit) sliderChildren.push(unit);
                const sliderContainer = el('div', `${Styles.sliderContainer} mt-2`, { style: isOff ? 'display: none' : 'display: flex' }, ...sliderChildren);

                const selector = el('div', 'relative', {},
                    el('select', `${Styles.input} ${Styles.select}`, {
                        onChange: (e) => {
                            const isNowOff = e.target.value === 'off';
                            sliderContainer.style.display = isNowOff ? 'none' : 'flex';
                            onUpdate(isNowOff ? false : parseFloat(slider.value));
                        }
                    },
                        el('option', '', { value: 'off', selected: isOff }, 'Off'),
                        el('option', '', { value: 'custom', selected: !isOff }, `Custom`)
                    ),
                    el('div', Styles.chevron, {}, Icons.chevronDown())
                );
                return el('div', '', {}, selector, sliderContainer);
            }

            if (type === 'switch-integer') {
                const isOff = currentValue === false || currentValue === 'false';
                const numVal = isOff ? (field.defaultInteger || field.min || 0) : parseInt(currentValue, 10);
                const display = el('span', Styles.sliderDisplay, {}, isNaN(numVal) ? 0 : numVal);
                const unit = el('span', 'text-xs sm:text-sm text-slate-600 ml-1', {}, 's');

                const slider = el('input', Styles.slider, {
                    type: 'range',
                    min: field.min, max: field.max, step: field.step || 1,
                    value: isNaN(numVal) ? (field.defaultInteger || field.min || 0) : numVal,
                    onInput: (e) => {
                        display.textContent = e.target.value;
                        onUpdate(parseInt(e.target.value, 10));
                    }
                });

                const sliderContainer = el('div', `${Styles.sliderContainer} mt-2`, { style: isOff ? 'display: none' : 'display: flex' }, slider, display, unit);

                const toggleContainer = el('label', 'relative inline-flex items-center cursor-pointer h-[30px] sm:h-[38px]', {});
                const checkbox = el('input', 'sr-only peer', {
                    type: 'checkbox',
                    checked: !isOff,
                    onChange: (e) => {
                        const isNowOff = !e.target.checked;
                        sliderContainer.style.display = isNowOff ? 'none' : 'flex';
                        onUpdate(isNowOff ? false : parseInt(slider.value, 10));
                    }
                });
                const toggleSwitch = el('div', Styles.toggle, {});

                toggleContainer.appendChild(checkbox);
                toggleContainer.appendChild(toggleSwitch);

                return el('div', '', {}, toggleContainer, sliderContainer);
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

            if (type === 'zones') {
                const zones = Array.isArray(currentValue) ? [...currentValue] : [];

                const wrapper = el('div', 'flex flex-col gap-1');
                const badgesDiv = el('div', 'flex flex-wrap items-center gap-1');
                const manageBtn = el('button', 'inline-flex items-center gap-1 px-2 py-0.5 rounded-full text-xs font-medium bg-slate-100 text-slate-500 border border-slate-200 hover:bg-slate-200 transition', {
                    type: 'button',
                    onClick: () => openZoneModal(zones, (updated) => {
                        zones.length = 0;
                        updated.forEach(z => zones.push(z));
                        onUpdate([...zones]);
                        renderZoneBadges();
                    })
                },
                    el('svg', 'w-3 h-3', { fill: 'none', stroke: 'currentColor', viewBox: '0 0 24 24' },
                        el('path', '', { 'stroke-linecap': 'round', 'stroke-linejoin': 'round', 'stroke-width': '2', d: 'M12 4v16m8-8H4' })
                    ),
                    'Add Zone'
                );

                function renderZoneBadges() {
                    badgesDiv.innerHTML = '';
                    zones.forEach((zone, i) => {
                        badgesDiv.appendChild(
                            el('span', `inline-flex items-center gap-0.5 px-2 py-0.5 rounded-full text-xs font-medium ${getZoneColor(zone)}`, {},
                                zone,
                                el('button', 'ml-0.5 hover:opacity-60 font-bold leading-none', {
                                    type: 'button',
                                    onClick: () => {
                                        zones.splice(i, 1);
                                        onUpdate([...zones]);
                                        renderZoneBadges();
                                    }
                                }, '×')
                            )
                        );
                    });
                    badgesDiv.appendChild(manageBtn);
                    if (field.hint) {
                        const hintText = zones.length === 0 ? field.hint : '';
                        hintEl.textContent = hintText;
                    }
                }

                const hintEl = el('span', 'text-xs text-slate-400 italic');
                renderZoneBadges();
                wrapper.appendChild(badgesDiv);
                if (field.hint) wrapper.appendChild(hintEl);
                return wrapper;
            }

            return el('input', Styles.input, {
                type: type === 'integer' ? 'text' : (type || 'text'),
                value: currentValue !== undefined ? currentValue : '',
                placeholder: field.placeholder || '',
                onInput: Utils.debounce((e) => {
                    let val = e.target.value;
                    if (field.type === 'number' || field.type === 'integer') {
                        // a cleared field removes the key (never NaN -> null in
                        // the saved JSON); unparsable input keeps the last value
                        if (val.trim() === '') {
                            val = undefined;
                        } else {
                            const n = parseNumberLike(val);
                            if (n === null) return;
                            val = field.type === 'integer' ? Math.floor(n) : n;
                        }
                    }
                    onUpdate(val);
                }, 200)
            });
        },

        renderField(field, index, dataContext, onUpdateCallback, onDepencyCheck, containerId = '') {
            let val = field.jsonpath ? Utils.getNested(dataContext, field.jsonpath) : dataContext[field.name];
            if (val === undefined) val = field.defaultValue;

            let inputEl = field.type === 'button'
                ? el('button', Styles.button, {
                    type: 'button',
                    onClick: () => (typeof field.onClick === 'function' ? field.onClick : ActionRegistry[field.onClick])?.(index, containerId)
                }, field.buttonIcon && el('span', '', { innerHTML: field.buttonIcon }),
                typeof field.buttonLabel === 'function' ? field.buttonLabel(dataContext) : (field.buttonLabel || 'Action'))
                : this.createInput(field, val, newValue => { onUpdateCallback(field, newValue); onDepencyCheck(); }, index, containerId);

            if (field.withButton && field.type !== 'button') {
                const action = typeof field.withButton.onClick === 'function'
                    ? field.withButton.onClick
                    : ActionRegistry[field.withButton.onClick] || (() => { });
                inputEl = el('div', 'flex items-center gap-2', {},
                    el('div', 'flex-1', {}, inputEl),
                    el('button', Styles.buttonPrimary, { type: 'button', onClick: () => action(index, containerId) },
                        el('span', '', { innerHTML: field.withButton.icon || 'Action' }))
                );
            }

            const container = el('div', `mb-1.5 sm:mb-2 transition-all duration-200${field.width ? ' field-sized' : ''}`, {
                dataset: { field: field.name, dependsOn: field.dependsOn ? JSON.stringify(field.dependsOn) : null },
                style: field.width ? `flex: 0 1 calc(${field.width}% - 0.375rem); min-width: 0;` : 'flex: 1 1 100%;'
            });

            if (field.type !== 'button') container.appendChild(el('label', Styles.label, {}, field.label));
            container.appendChild(inputEl);
            
            if (field.type === 'button' && typeof field.buttonLabel === 'function') {
                container._buttonEl = inputEl;
                container._field = field;
                container._dataContext = dataContext;
            }
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

    class ConfigManager {
        constructor(config) {
            this.config = config;
            this.container = document.getElementById(config.containerId);
            this.fields = Object.values(config.schema);
            this.data = config.isList ? [] : {};

            if (!this.container) return;

            ManagerRegistry.set(config.containerId, this);
            this.load();
        }

        load() {
            this.container.textContent = '';
            this.container.appendChild(el('div', 'text-center p-8', {},
                el('div', 'animate-spin h-8 w-8 border-4 border-slate-300 border-t-slate-600 rounded-full mx-auto mb-2'),
                el('p', 'text-sm text-slate-600', {}, 'Loading...')));

            this.loadData().then(() => {
                this.render();
                this.updateJsonDebug();
            }).catch(() => {
                this.container.textContent = '';
                this.container.appendChild(el('div', 'text-center p-8', {},
                    el('p', 'text-slate-600', {}, 'Failed to load configuration'),
                    el('button', 'mt-4 px-4 py-2 bg-slate-600 text-white rounded-lg hover:bg-slate-700', {
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
                this.fields.forEach(f => this.data[f.name] ??= f.defaultValue);
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
                    const toggleInput = activeField ? el('input', 'sr-only peer', {
                        type: 'checkbox',
                        checked: isActive,
                        onChange: e => this.updateValue(index, activeField, e.target.checked)
                    }) : null;

                    const header = el('div', Styles.cardHeader, {},
                        el('div', 'flex items-center gap-2', {},
                            el('span', 'flex items-center justify-center w-5 h-5 text-xs font-bold bg-slate-200 text-slate-600 rounded-full', {}, `${index + 1}`),
                            el('h4', 'text-sm sm:text-sm font-semibold text-slate-700', {}, this.config.title)
                        ),
                        el('div', 'flex items-center gap-2 sm:gap-3', {},
                            activeField ? el('label', 'flex items-center gap-1.5 cursor-pointer select-none', {},
                                el('span', 'text-xs font-medium text-slate-400', {}, 'Active'),
                                el('div', 'relative inline-flex items-center', {},
                                    toggleInput,
                                    el('div', "w-9 h-5 bg-slate-300 peer-focus:outline-none rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-white after:border after:rounded-full after:h-4 after:w-4 after:transition-all peer-checked:bg-emerald-500")
                                )
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
                        el('h4', 'text-sm sm:text-sm font-semibold text-slate-700', {}, this.config.title)
                    );
                    wrapper.appendChild(header);
                }

                const fieldsDiv = el('div', this.config.isList ? Styles.cardBody : 'p-3 sm:p-5');
                const innerFields = el('div', 'flex flex-wrap gap-x-3 gap-y-2');
                this.fields.forEach(field => {
                    if (activeField && field.name === 'active') return;
                    const fieldEl = Renderer.renderField(field, index, item,
                        (fld, val) => this.updateValue(index, fld, val),
                        () => Renderer.updateVisibility(innerFields, item),
                        this.config.containerId
                    );
                    innerFields.appendChild(fieldEl);
                });
                fieldsDiv.appendChild(innerFields);

                wrapper.appendChild(fieldsDiv);
                this.container.appendChild(wrapper);

                Renderer.updateVisibility(innerFields, item);
            });

            this.renderControls();
        }

        renderControls() {
            const containerIdClass = `controls-${this.config.containerId}`;
            const existing = this.container.parentElement.querySelector('.' + containerIdClass);
            if (existing) existing.remove();

            this.ensureJsonUI();

            const btnGroup = el('div', `${containerIdClass} mt-6 sm:mt-8 px-4 sm:px-0 flex flex-row flex-wrap justify-end items-center gap-2 sm:gap-3 max-w-2xl sm:mx-auto`);

            if (this.config.isList) {
                btnGroup.appendChild(el('button', 'w-auto sm:w-32 bg-white border border-slate-300 text-slate-700 px-4 py-1.5 sm:py-2 rounded-lg hover:bg-slate-50 transition duration-200 shadow-sm inline-flex items-center justify-center gap-2 text-xs sm:text-sm font-medium', {
                    type: 'button', onClick: () => this.addItem()
                },
                    Icons.plus(),
                    'Add Item'));
            }

            btnGroup.appendChild(el('button', 'w-auto sm:w-32 bg-white border border-slate-300 text-slate-400 px-4 sm:px-6 py-1.5 sm:py-2 rounded-lg hover:bg-slate-50 shadow-sm transition-all duration-200 text-xs sm:text-sm font-medium cursor-default', {
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

            const toggleDiv = el('div', 'mt-6 sm:mt-8 px-4 sm:px-0 max-w-2xl sm:mx-auto border-t border-slate-200 pt-6', { dataset: { jsonSection: '' } });
            const btn = el('button', 'flex items-center text-slate-500 hover:text-slate-800 transition-colors focus:outline-none group text-sm font-medium', {
                type: 'button', onClick: () => global.toggleJsonContent(contentId, chevronId)
            });

            const chevron = el('svg', 'h-4 w-4 mr-2 transform transition-transform duration-200 text-slate-400 group-hover:text-slate-600', {
                id: chevronId, fill: 'none', viewBox: '0 0 24 24', stroke: 'currentColor'
            }, el('path', '', { 'stroke-linecap': 'round', 'stroke-linejoin': 'round', 'stroke-width': '2', d: 'M19 9l-7 7-7-7' }));

            btn.appendChild(chevron);
            btn.appendChild(el('span', '', {}, 'JSON'));

            const contentDiv = el('div', 'mt-3 hidden transition-all', { id: contentId });
            const pre = el('pre', 'w-full p-4 rounded-lg bg-slate-900 text-slate-200 text-xs font-mono overflow-auto custom-scrollbar border border-slate-700 shadow-inner', {
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
            let o = obj;
            for (const p of this.config.nestedPath)
                o = (o && typeof o === 'object') ? o[p] : undefined;
            return o;
        }

        setNested(obj) {
            const path = this.config.nestedPath;
            let o = obj;
            for (let i = 0; i < path.length - 1; i++) {
                if (!o[path[i]] || typeof o[path[i]] !== 'object') o[path[i]] = {};
                o = o[path[i]];
            }
            o[path[path.length - 1]] = this.data;
        }

        updateJsonDebug() {
            const el = document.getElementById(this.jsonPreId());
            if (!el) return;
            const cfg = {};
            if (this.config.nestedPath) {
                this.setNested(cfg);
            } else if (this.config.channelType) {
                cfg[this.config.channelType] = this.data;
            } else {
                // Top-level section (e.g. sharing): this.data is the whole
                // config, so include only the fields declared in its schema.
                this.fields.forEach(f => {
                    const val = f.jsonpath ? Utils.getNested(this.data, f.jsonpath) : this.data[f.name];
                    if (val === undefined) return;
                    if (f.jsonpath) Utils.setNested(cfg, f.jsonpath, val);
                    else cfg[f.name] = val;
                });
            }
            el.textContent = JSON.stringify(cfg, null, 2);
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

                if (this.config.nestedPath) {
                    this.setNested(fullConfig);
                } else if (this.config.channelType) {
                    fullConfig[this.config.channelType] = this.data;
                } else {
                    this.fields.forEach(f => {
                        if (f.skipSave) return;
                        const val = f.jsonpath ? Utils.getNested(this.data, f.jsonpath) : this.data[f.name];
                        if (val === undefined) return;
                        if (f.jsonpath) Utils.setNested(fullConfig, f.jsonpath, val);
                        else fullConfig[f.name] = val;
                    });
                }

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
                App.setUnsaved(false);
                App.notify('success', 'Configuration saved successfully');
                if (global.hubConfigSaved)
                    global.hubConfigSaved(this.config.nestedPath ? 'viewer' : 'engine');
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
    global.ConfigStore = ConfigStore;
    global.ZoneColors = { badge: getZoneColor, hex: getZoneHex };
    global.normalizeConfig = (cfg) => ConfigNormalizer.normalize(cfg);
    global.createConfigManager = (config) => new ConfigManager(config);
    global.createSimpleConfigManager = (config) => { config.isList = false; return new ConfigManager(config); };
    global.createChannelManager = (config) => { config.isList = true; return new ConfigManager(config); };

    global.toggleJsonContent = (contentId, chevronId) => {
        const c = document.getElementById(contentId);
        const i = document.getElementById(chevronId);
        if (c) {
            const hidden = c.classList.toggle('hidden');
            if (i) i.setAttribute('class', `h-4 w-4 mr-2 transform transition-transform duration-200 text-slate-400 group-hover:text-slate-600 ${hidden ? '' : 'rotate-180'}`);
        }
    };

})(window);