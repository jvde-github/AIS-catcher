
(function (global) {
    'use strict';

    const UUID_RE = /^[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;

    const BAUD_RATES = [4800, 9600, 38400, 115200];

    const SDR_TYPES = {
        RTLSDR: 'RTL-SDR',
        AIRSPY: 'Airspy',
        AIRSPYHF: 'Airspy HF+',
        HACKRF: 'HackRF',
        SDRPLAY: 'SDRplay',
        HYDRASDR: 'HydraSDR',
        SOAPYSDR: 'SoapySDR'
    };

    const WIZARD_STEPS = [
        {
            id: 'welcome',
            title: 'Welcome to AIS-catcher',
            type: 'welcome',
            nextLabel: 'Get started'
        },
        {
            id: 'password',
            title: 'Set Password',
            type: 'password'
        },
        {
            id: 'import',
            title: 'Previous Settings',
            type: 'import',
            intro: 'A configuration from a previous installation was found. Select what to carry over; steps already covered by the import are skipped.'
        },
        {
            id: 'input',
            title: 'Input Device',
            type: 'input',
            coveredBy: 'receiver',
            intro: 'Select the device that receives AIS: a dAISy-catcher, one of the SDR devices detected on this machine, or another NMEA receiver on a serial port.'
        },
        {
            id: 'community',
            title: 'AIS-catcher Community',
            type: 'service',
            coveredBy: 'sharing',
            service: {
                icon: 'aiscatcher',
                name: 'AIS-catcher Community Station',
                blurb: 'Share your reception with the AIS-catcher community map. Registering a station gives you a station key: paste it below to link this receiver. Your station gets a name, coverage statistics and a public page on aiscatcher.org.',
                linkLabel: 'Create station on aiscatcher.org',
                url: 'https://www.aiscatcher.org/addstation_ac',
                field: {
                    key: 'community_uid',
                    label: 'Station key',
                    placeholder: 'Paste the station key (UUID) here',
                    validate: v => UUID_RE.test(v) || 'The station key should be a UUID like 123e4567-e89b-12d3-a456-426614174000'
                }
            }
        },
        {
            id: 'aishub',
            title: 'AISHub',
            type: 'service',
            coveredBy: 'udp',
            service: {
                icon: 'aishub.png',
                name: 'AISHub',
                blurb: 'AISHub is a free AIS data exchange: feed your data and get access to the combined worldwide feed. After joining, AISHub assigns you a dedicated UDP port on data.aishub.net.',
                linkLabel: 'Join at aishub.net',
                url: 'https://www.aishub.net/join-us',
                field: {
                    key: 'aishub_port',
                    label: 'Assigned UDP port',
                    placeholder: 'Paste your AISHub UDP port here',
                    validate: v => (/^\d+$/.test(v) && +v > 0 && +v < 65536) || 'The port should be a number between 1 and 65535'
                }
            }
        },
        {
            id: 'finish',
            title: 'Summary',
            type: 'finish',
            nextLabel: 'Save & Close'
        }
    ];

    let stepIndex = 0;
    let sel = {};
    let overlay = null;
    let devices = null;
    let serialPorts = null;
    let legacyOutputs = null; // null = still fetching, [] = nothing to import
    let legacyPromise = null;
    // Password-step state, passed in by app.js at open(): shown (and
    // mandatory) whenever no password is set; `setup` selects the endpoint
    // for the LAN-bound first run, where nothing else works until it is set.
    let pwState = { needed: false, setup: false };

    // Legacy sections the wizard can carry over; entries are reduced to the
    // fields their schema declares and coerced by normalizeConfig. Other
    // root fields stay behind on purpose.
    const IMPORT_CHANNELS = [
        { key: 'udp', label: 'UDP', schema: udpSchema, essentials: ['host', 'port'] },
        { key: 'tcp', label: 'TCP Client', schema: tcpSchema, essentials: ['host', 'port'] },
        { key: 'http', label: 'HTTP', schema: httpSchema, essentials: ['url'] },
        { key: 'mqtt', label: 'MQTT', schema: mqttSchema, essentials: ['url'] },
        { key: 'tcp_listener', label: 'TCP Server', schema: tcpServerSchema, essentials: ['port'] },
        { key: 'server', label: 'Webviewer', schema: webviewerSchema, essentials: ['port'] }
    ];

    function channelTitle(label, item) {
        if (item.host && item.port) return label + ' · ' + item.host + ':' + item.port;
        if (item.url) return label + ' · ' + item.url;
        if (item.port) return label + ' · :' + item.port;
        return label;
    }

    // copy only the fields the schema declares, at their jsonpath (receiver
    // settings nest per device, e.g. rtlsdr.tuner)
    function sanitizeBySchema(item, schema) {
        const clean = {};
        Object.values(schema).forEach(f => {
            if (f.type === 'button') return;
            const path = (f.jsonpath || f.name || '').split('.');
            if (!path[0]) return;
            let s = item;
            for (const p of path) {
                if (!s || typeof s !== 'object') return;
                s = s[p];
            }
            if (s === undefined) return;
            let d = clean;
            for (let i = 0; i < path.length - 1; i++)
                d = d[path[i]] = d[path[i]] || {};
            d[path[path.length - 1]] = s;
        });
        return clean;
    }

    // cfg comes from normalizeConfig with the channel arrays pre-wrapped in
    // fetchLegacy, so every section here is an array (or absent)
    function buildLegacyOutputs(cfg) {
        const items = [];

        (cfg.receiver || []).forEach(item => {
            if (!item || typeof item !== 'object') return;
            const clean = sanitizeBySchema(item, receiverSchema);
            if (!clean.input && !clean.serial) return;
            if (clean.input) clean.input = String(clean.input).toUpperCase();
            items.push({
                kind: 'receiver', key: 'receiver', idFields: ['input', 'serial'], item: clean,
                checked: clean.active !== false,
                title: 'Receiver · ' + (SDR_TYPES[clean.input] || clean.input || 'device'),
                subtitle: clean.serial ? 'Serial ' + clean.serial : ((clean.serialport && clean.serialport.port) || '')
            });
        });

        if (typeof cfg.sharing_key === 'string' && UUID_RE.test(cfg.sharing_key)) {
            const checked = cfg.sharing !== false;
            items.push({ kind: 'sharing', uuid: cfg.sharing_key, checked, title: 'Community station key', subtitle: cfg.sharing_key });
            if (checked && !sel.community_uid) sel.community_uid = cfg.sharing_key;
        }

        IMPORT_CHANNELS.forEach(({ key, label, schema, essentials }) => {
            (cfg[key] || []).forEach(item => {
                if (!item || typeof item !== 'object') return;
                const clean = sanitizeBySchema(item, schema);
                if (!essentials.every(f => clean[f] !== undefined && clean[f] !== ''))
                    return;
                // entries disabled in the legacy config default to unchecked
                items.push({ kind: 'channel', key, idFields: essentials, item: clean, checked: clean.active !== false, title: channelTitle(label, clean), subtitle: clean.description || '' });
            });
        });

        return items;
    }

    function fetchLegacy() {
        legacyOutputs = null;
        legacyPromise = fetch('/api/legacy_config')
            .then(r => { if (!r.ok) throw new Error(); return r.json(); })
            .then(cfg => {
                // single-object channels predate the array form; wrap them so
                // normalizeConfig's migrations apply to them too
                IMPORT_CHANNELS.forEach(({ key }) => {
                    if (cfg[key] && !Array.isArray(cfg[key])) cfg[key] = [cfg[key]];
                });
                if (cfg.receiver && !Array.isArray(cfg.receiver)) cfg.receiver = [cfg.receiver];
                legacyOutputs = buildLegacyOutputs(normalizeConfig(cfg).config);
            })
            .catch(() => { legacyOutputs = []; });
    }

    const ROOT_DEVICE_KEYS = ['input', 'serial', 'serialport', 'rtlsdr', 'rtltcp', 'airspy', 'airspyhf',
        'hydrasdr', 'sdrplay', 'hackrf', 'udpserver', 'soapysdr', 'nmea2000', 'file', 'zmq', 'spyserver', 'wavfile'];

    const DAISY_CATCHER = {
        baudrate: 115200,
        init_seq: 'co2,v',
        url: 'https://shop.wegmatt.com/products/daisy-catcher-high-performance-ais-receiver'
    };

    const USB_ICON =
        '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M15 7v4h1v2h-3V5h2l-3-4-3 4h2v8H8v-2.07c.7-.37 1.2-1.08 1.2-1.93 0-1.21-.99-2.2-2.2-2.2-1.21 0-2.2.99-2.2 2.2 0 .85.5 1.56 1.2 1.93V13c0 1.11.89 2 2 2h3v3.05c-.71.37-1.2 1.1-1.2 1.95 0 1.22.99 2.2 2.2 2.2 1.21 0 2.2-.98 2.2-2.2 0-.85-.49-1.58-1.2-1.95V15h3c1.11 0 2-.89 2-2v-2h1V7h-4z"/></svg>';

    const CABLE_ICON =
        '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M20,5V4c0-0.55-0.45-1-1-1h-2c-0.55,0-1,0.45-1,1v1h-1v4c0,0.55,0.45,1,1,1h1v7c0,1.1-0.9,2-2,2s-2-0.9-2-2V7 c0-2.21-1.79-4-4-4S5,4.79,5,7v7H4c-0.55,0-1,0.45-1,1v4h1v1c0,0.55,0.45,1,1,1h2c0.55,0,1-0.45,1-1v-1h1v-4c0-0.55-0.45-1-1-1H7 V7c0-1.1,0.9-2,2-2s2,0.9,2,2v10c0,2.21,1.79,4,4,4s4-1.79,4-4v-7h1c0.55,0,1-0.45,1-1V5H20z"/></svg>';

    function resetState() {
        stepIndex = 0;
        // rescan devices on every wizard open
        devices = null;
        serialPorts = null;
        sel = { kind: null, device: null, dc_port: '', serial_port: '', serial_baudrate: 38400, community_uid: '', aishub_port: '' };
    }

    function wizardFlagOn(cfg) {
        const v = cfg && cfg.control && cfg.control.wizard;
        return v === true || v === 'true' || v === 'on';
    }

    function postConfig(cfg) {
        return fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(cfg, null, 2)
        }).then(r => {
            ConfigStore.invalidate();
            if (r.status === 401 && global.hubAuthRequired) global.hubAuthRequired();
            return r.json();
        });
    }

    function clearWizardFlag() {
        ConfigStore.fetch(true)
            .then(cfg => {
                if (!wizardFlagOn(cfg)) return;
                cfg.control.wizard = false;
                return postConfig(cfg);
            })
            .catch(() => { });
    }

    function icon(name, cls) {
        if (name === 'aiscatcher') {
            const logo = document.querySelector('#community-link svg');
            return '<span class="' + cls + '">' + (logo ? logo.outerHTML : '') + '</span>';
        }
        return '<span class="' + cls + '"><img src="' + name + '" alt=""></span>';
    }

    function esc(s) {
        return String(s).replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
    }

    function ensureOverlay() {
        if (overlay) return;
        overlay = document.createElement('div');
        overlay.id = 'wizard-overlay';
        overlay.innerHTML = '<div id="wizard-card"></div>';
        document.body.appendChild(overlay);
        document.addEventListener('keydown', e => {
            if (e.key === 'Escape' && overlay.classList.contains('open')) cancelWizard();
        });
    }

    function close(clearFlag) {
        if (clearFlag) clearWizardFlag();
        if (overlay) overlay.classList.remove('open');
    }

    function cancelWizard() {
        close(true);
        // cancelling must not bypass the password requirement: hand over to
        // the set-password modal
        if (pwState.needed && global.hubPasswordBackstop) {
            global.hubPasswordBackstop();
            return;
        }
        if (global.App && App.notify) App.notify('info', 'Setup cancelled — run the wizard anytime from the System panel');
    }

    // A step is hidden when a checked import covers it (its coveredBy matches
    // the import's kind or config key); unchecking in the import step brings
    // it back. The import step itself is hidden when there is nothing to
    // import.
    function stepHidden(step) {
        if (step.type === 'password')
            return !pwState.needed;
        if (step.type === 'import')
            return legacyOutputs !== null && legacyOutputs.length === 0;
        if (!step.coveredBy || legacyOutputs === null)
            return false;
        return legacyOutputs.some(it => it.checked && (it.kind === step.coveredBy || it.key === step.coveredBy));
    }

    // step forward/back, jumping over hidden steps; welcome and finish are
    // never hidden so the loop always terminates
    function gotoStep(dir) {
        do
            stepIndex += dir;
        while (stepHidden(WIZARD_STEPS[stepIndex]));
        renderStep();
    }

    function renderStep() {
        const step = WIZARD_STEPS[stepIndex];
        const card = document.getElementById('wizard-card');

        const dots = WIZARD_STEPS.map((s, i) => {
            if (stepHidden(s)) return '';
            return '<span class="wz-dot' + (i === stepIndex ? ' active' : i < stepIndex ? ' past' : '') + '"></span>';
        }).join('');

        const showBack = stepIndex > 0;
        // no skipping past the import step while its content is still loading:
        // the items arrive pre-checked and must not be applied unseen
        const showSkip = step.type === 'input' || step.type === 'service' ||
            (step.type === 'import' && legacyOutputs !== null);
        const nextLabel = step.nextLabel || 'Next';

        card.innerHTML =
            '<div class="wz-head">' +
            '  <div class="wz-dots">' + dots + '</div>' +
            '  <button id="wz-cancel" title="Cancel setup" class="wz-x">' +
            '    <svg class="h-6 w-6" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12"/></svg>' +
            '  </button>' +
            '</div>' +
            '<div class="wz-body" id="wz-body"></div>' +
            '<div class="wz-error hidden" id="wz-error"></div>' +
            '<div class="wz-foot">' +
            (showBack ? '<button id="wz-back" class="wz-btn ghost">Back</button>' : '<span></span>') +
            '  <div class="wz-foot-right">' +
            (showSkip ? '<button id="wz-skip" class="wz-btn ghost">Skip</button>' : '') +
            '    <button id="wz-next" class="wz-btn primary">' + esc(nextLabel) + '</button>' +
            '  </div>' +
            '</div>';

        const body = document.getElementById('wz-body');
        if (step.type === 'welcome') renderWelcome(body, step);
        else if (step.type === 'password') renderPassword(body, step);
        else if (step.type === 'input') renderInput(body, step);
        else if (step.type === 'import') renderImport(body, step);
        else if (step.type === 'service') renderService(body, step);
        else if (step.type === 'finish') renderFinish(body, step);

        document.getElementById('wz-cancel').addEventListener('click', cancelWizard);
        const back = document.getElementById('wz-back');
        if (back) back.addEventListener('click', () => gotoStep(-1));
        const skip = document.getElementById('wz-skip');
        if (skip) skip.addEventListener('click', () => { clearStep(step); gotoStep(1); });
        document.getElementById('wz-next').addEventListener('click', () => onNext(step));
    }

    function stepError(message) {
        const el = document.getElementById('wz-error');
        if (!message) { el.classList.add('hidden'); return; }
        el.textContent = message;
        el.classList.remove('hidden');
    }

    function clearStep(step) {
        if (step.type === 'input') { sel.kind = null; sel.device = null; }
        else if (step.type === 'service') sel[step.service.field.key] = '';
        else if (step.type === 'import') {
            (legacyOutputs || []).forEach(it => {
                it.checked = false;
                if (it.kind === 'sharing' && sel.community_uid === it.uuid) sel.community_uid = '';
            });
        }
    }

    function renderWelcome(body, step) {
        body.innerHTML =
            '<div class="wz-welcome">' +
            icon('aiscatcher', 'wz-logo') +
            '<h2>' + esc(step.title) + '</h2>' +
            '<p class="wz-sub">&copy; 2021-2026 jvde.github@gmail.com</p>' +
            '<p>This wizard sets up a simple receiving station in a few steps: pick an input device and choose where to share your data. ' +
            'Every step can be skipped and all settings can be changed later under <b>Input</b> and <b>Output</b>.</p>' +
            '<p class="wz-fineprint">AIS-catcher is a research and educational tool provided under the ' +
            '<a href="https://github.com/jvde-github/AIS-catcher/blob/main/LICENSE" target="_blank" rel="noopener">GNU GPL v3 license</a>. ' +
            'It is not reliable for navigation or safety of life or property. Radio reception regulations vary by region; check your local rules before use.</p>' +
            '</div>';
    }

    function renderInput(body, step) {
        body.innerHTML = '<h3>' + esc(step.title) + '</h3><p class="wz-intro">' + esc(step.intro) + '</p><div id="wz-devices" class="wz-options"><div class="wz-loading">Scanning devices&hellip;</div></div>';

        const getJSON = url => fetch(url).then(r => {
            if (r.status === 401 && global.hubAuthRequired) global.hubAuthRequired();
            if (!r.ok) throw new Error();
            return r.json();
        });
        const fetches = [
            devices ? Promise.resolve(devices) : getJSON('/api/devices').then(d => (devices = d)),
            serialPorts ? Promise.resolve(serialPorts) : getJSON('/api/serial').then(d => (serialPorts = d))
        ];

        Promise.all(fetches).then(([dev, ports]) => {
            const list = ((dev && dev.devices) || []).filter(d => SDR_TYPES[d.input]);
            const box = document.getElementById('wz-devices');
            if (!box) return;

            const portOptions = (ports || []).map(p => '<option value="' + esc(p) + '">').join('');

            let html =
                '<label class="wz-option' + (sel.kind === 'dc' ? ' selected' : '') + '" data-dev="dc">' +
                '<input type="radio" name="wz-dev" ' + (sel.kind === 'dc' ? 'checked' : '') + '>' +
                '<span class="wz-option-icon">' + CABLE_ICON + '</span>' +
                '<span class="wz-option-main"><span class="wz-option-title">dAISy-catcher</span>' +
                '<span class="wz-option-sub">High-performance AIS receiver &mdash; <a class="wz-mini-link" href="' + DAISY_CATCHER.url + '" target="_blank" rel="noopener">shop.wegmatt.com</a></span>' +
                '<span class="wz-daisy' + (sel.kind === 'dc' ? '' : ' hidden') + '" id="wz-dc-fields">' +
                '<input id="wz-dc-port" list="wz-serial-list" placeholder="Serial port, e.g. /dev/serial0" value="' + esc(sel.dc_port) + '">' +
                '</span></span>' +
                '</label>';

            if (!list.length)
                html += '<p class="wz-none">No SDR devices detected. Connect an RTL-SDR or similar dongle, or use a serial device.</p>';

            list.forEach((d, i) => {
                const active = sel.kind === 'sdr' && sel.device && sel.device.input === d.input && sel.device.serial === d.serial;
                html +=
                    '<label class="wz-option' + (active ? ' selected' : '') + '" data-dev="' + i + '">' +
                    '<input type="radio" name="wz-dev" ' + (active ? 'checked' : '') + '>' +
                    '<span class="wz-option-icon">' + USB_ICON + '</span>' +
                    '<span class="wz-option-main"><span class="wz-option-title">' + esc(SDR_TYPES[d.input]) + '</span>' +
                    '<span class="wz-option-sub">Serial ' + esc(d.serial || '—') + '</span></span>' +
                    '</label>';
            });

            html +=
                '<label class="wz-option' + (sel.kind === 'serial' ? ' selected' : '') + '" data-dev="serial">' +
                '<input type="radio" name="wz-dev" ' + (sel.kind === 'serial' ? 'checked' : '') + '>' +
                '<span class="wz-option-icon">' + CABLE_ICON + '</span>' +
                '<span class="wz-option-main"><span class="wz-option-title">Serial</span>' +
                '<span class="wz-option-sub">NMEA receiver connected via a serial port</span>' +
                '<span class="wz-daisy' + (sel.kind === 'serial' ? '' : ' hidden') + '" id="wz-serial-fields">' +
                '<input id="wz-serial-port" list="wz-serial-list" placeholder="Serial port, e.g. /dev/serial0" value="' + esc(sel.serial_port) + '">' +
                '<select id="wz-serial-baud">' +
                BAUD_RATES.map(b => '<option value="' + b + '"' + (b === sel.serial_baudrate ? ' selected' : '') + '>' + b + ' baud</option>').join('') +
                '</select>' +
                '</span></span>' +
                '</label>' +
                '<datalist id="wz-serial-list">' + portOptions + '</datalist>';

            box.innerHTML = html;

            box.querySelectorAll('.wz-option').forEach(opt => {
                opt.addEventListener('click', e => {
                    if (e.target.closest('.wz-mini-link')) return;
                    box.querySelectorAll('.wz-option').forEach(o => o.classList.remove('selected'));
                    opt.classList.add('selected');
                    opt.querySelector('input[type=radio]').checked = true;
                    const kind = opt.dataset.dev;
                    sel.kind = kind === 'dc' ? 'dc' : kind === 'serial' ? 'serial' : 'sdr';
                    sel.device = sel.kind === 'sdr' ? list[+kind] : null;
                    document.getElementById('wz-dc-fields').classList.toggle('hidden', sel.kind !== 'dc');
                    document.getElementById('wz-serial-fields').classList.toggle('hidden', sel.kind !== 'serial');
                    stepError();
                });
            });

            const dcPort = document.getElementById('wz-dc-port');
            const serialPort = document.getElementById('wz-serial-port');
            const serialBaud = document.getElementById('wz-serial-baud');
            dcPort.addEventListener('input', () => { sel.dc_port = dcPort.value.trim(); });
            serialPort.addEventListener('input', () => { sel.serial_port = serialPort.value.trim(); });
            serialBaud.addEventListener('change', () => { sel.serial_baudrate = +serialBaud.value; });
        }).catch(() => {
            const box = document.getElementById('wz-devices');
            if (box) box.innerHTML = '<p class="wz-none">Could not scan for devices.</p>';
        });
    }

    function renderPassword(body, step) {
        body.innerHTML =
            '<h3>' + esc(step.title) + '</h3>' +
            '<p class="wz-intro">Choose an admin password to protect this control page.</p>' +
            '<label class="wz-field-label">Password</label>' +
            '<input id="wz-pw1" class="wz-input" type="password" autocomplete="new-password">' +
            '<label class="wz-field-label">Confirm password</label>' +
            '<input id="wz-pw2" class="wz-input" type="password" autocomplete="new-password">';
    }

    function submitPassword() {
        const p1 = document.getElementById('wz-pw1').value;
        const p2 = document.getElementById('wz-pw2').value;

        if (p1.length < 6) { stepError('The password needs at least 6 characters.'); return; }
        if (p1 !== p2) { stepError('Passwords do not match.'); return; }

        const nextBtn = document.getElementById('wz-next');
        nextBtn.disabled = true;
        fetch(pwState.setup ? '/api/setup' : '/api/password', { method: 'POST', body: p1 })
            .then(r => r.json())
            .then(res => {
                nextBtn.disabled = false;
                if (!res.status) { stepError(res.error || 'Could not set the password.'); return; }
                const wasSetup = pwState.setup;
                pwState = { needed: false, setup: false };
                if (global.hubAuthGranted) global.hubAuthGranted();
                // before setup the legacy config was not accessible yet
                if (wasSetup) fetchLegacy();
                gotoStep(1);
            })
            .catch(() => { nextBtn.disabled = false; stepError('Connection error. Please try again.'); });
    }

    function renderImport(body, step) {
        if (legacyOutputs === null) {
            body.innerHTML = '<h3>' + esc(step.title) + '</h3><div class="wz-loading">Looking for previous settings&hellip;</div>';
            legacyPromise.then(() => {
                if (WIZARD_STEPS[stepIndex].type !== 'import') return;
                if (legacyOutputs.length) renderStep();
                else gotoStep(1);
            });
            return;
        }

        body.innerHTML =
            '<h3>' + esc(step.title) + '</h3>' +
            '<p class="wz-intro">' + esc(step.intro) + '</p>' +
            '<div class="wz-options">' +
            legacyOutputs.map((it, i) =>
                '<label class="wz-option' + (it.checked ? ' selected' : '') + '">' +
                '<input type="checkbox" data-imp="' + i + '"' + (it.checked ? ' checked' : '') + '>' +
                '<span class="wz-option-main"><span class="wz-option-title">' + esc(it.title) + '</span>' +
                (it.subtitle ? '<span class="wz-option-sub">' + esc(it.subtitle) + '</span>' : '') +
                '</span></label>').join('') +
            '</div>' +
            '<p class="wz-fineprint">Only settings recognised by this version are imported.</p>';

        body.querySelectorAll('[data-imp]').forEach(cb => {
            cb.addEventListener('change', () => {
                const it = legacyOutputs[+cb.dataset.imp];
                it.checked = cb.checked;
                cb.closest('.wz-option').classList.toggle('selected', cb.checked);
                if (it.kind === 'sharing') {
                    if (cb.checked) sel.community_uid = it.uuid;
                    else if (sel.community_uid === it.uuid) sel.community_uid = '';
                }
            });
        });
    }

    function renderService(body, step) {
        const s = step.service;
        body.innerHTML =
            '<h3>' + esc(step.title) + '</h3>' +
            '<div class="wz-service">' +
            icon(s.icon, 'wz-service-icon') +
            '<div class="wz-service-main">' +
            '<p class="wz-intro">' + esc(s.blurb) + '</p>' +
            '</div>' +
            '</div>' +
            '<a class="wz-btn link" href="' + esc(s.url) + '" target="_blank" rel="noopener">' + esc(s.linkLabel) +
            ' <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M18 13v6a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h6"/><polyline points="15 3 21 3 21 9"/><line x1="10" y1="14" x2="21" y2="3"/></svg></a>' +
            '<label class="wz-field-label">' + esc(s.field.label) + '</label>' +
            '<input id="wz-service-field" class="wz-input" placeholder="' + esc(s.field.placeholder) + '" value="' + esc(sel[s.field.key] || '') + '">' +
            '<p class="wz-fineprint">Leave empty and press Next (or Skip) if you do not want to set this up now.</p>';

        const field = document.getElementById('wz-service-field');
        field.addEventListener('input', () => { sel[s.field.key] = field.value.trim(); stepError(); });
    }

    function changeList() {
        const items = [];
        if (sel.kind === 'sdr' && sel.device)
            items.push('Input device: <b>' + esc(SDR_TYPES[sel.device.input] || sel.device.input) + ' [' + esc(sel.device.serial || '') + ']</b>');
        else if (sel.kind === 'dc' && sel.dc_port)
            items.push('Input device: <b>dAISy-catcher on ' + esc(sel.dc_port) + '</b>');
        else if (sel.kind === 'serial' && sel.serial_port)
            items.push('Input device: <b>serial receiver on ' + esc(sel.serial_port) + '</b> at ' + sel.serial_baudrate + ' baud');
        if (sel.community_uid)
            items.push('Community sharing with your registered station key');
        if (sel.aishub_port)
            items.push('UDP feed to <b>data.aishub.net:' + esc(sel.aishub_port) + '</b> (AISHub)');
        (legacyOutputs || []).forEach(it => {
            if (it.checked && it.kind !== 'sharing')
                items.push('Import <b>' + esc(it.title) + (it.subtitle ? ' (' + esc(it.subtitle) + ')' : '') + '</b>');
        });
        return items;
    }

    function renderFinish(body, step) {
        const items = changeList();
        body.innerHTML =
            '<h3>' + esc(step.title) + '</h3>' +
            (items.length
                ? '<p class="wz-intro">The following will be added to the configuration:</p><ul class="wz-summary">' +
                items.map(i => '<li>' + i + '</li>').join('') + '</ul>' +
                '<label class="wz-check"><input type="checkbox" id="wz-start" checked> Start the receiver after saving</label>'
                : '<p class="wz-intro">Nothing selected &mdash; no changes will be made. You can run this wizard again from the <b>System</b> panel.</p>');
    }

    function onNext(step) {
        stepError();

        if (step.type === 'password') {
            submitPassword();
            return;
        }
        // wait for the legacy scan; its promise callback advances or re-renders
        if (step.type === 'import' && legacyOutputs === null)
            return;
        if (step.type === 'input' && sel.kind === 'dc' && !sel.dc_port) {
            stepError('Enter the serial port of the dAISy-catcher, or press Skip.');
            return;
        }
        if (step.type === 'input' && sel.kind === 'serial' && !sel.serial_port) {
            stepError('Enter the serial port of the receiver, or press Skip.');
            return;
        }
        if (step.type === 'service') {
            const v = sel[step.service.field.key];
            if (v) {
                const ok = step.service.field.validate(v);
                if (ok !== true) { stepError(ok); return; }
            }
        }
        if (step.type === 'finish') {
            applyAndClose();
            return;
        }
        gotoStep(1);
    }

    function applyAndClose() {
        const items = changeList();
        if (!items.length) { close(true); return; }

        const startAfter = !!(document.getElementById('wz-start') && document.getElementById('wz-start').checked);
        const nextBtn = document.getElementById('wz-next');
        nextBtn.disabled = true;
        nextBtn.textContent = 'Saving...';

        ConfigStore.fetch(true)
            .then(cfg => {
                const hasDevice = (sel.kind === 'sdr' && sel.device) ||
                    (sel.kind === 'dc' && sel.dc_port) ||
                    (sel.kind === 'serial' && sel.serial_port);
                if (hasDevice) {
                    ROOT_DEVICE_KEYS.forEach(k => { delete cfg[k]; });
                    if (!Array.isArray(cfg.receiver) || !cfg.receiver.length) cfg.receiver = [{}];
                    const rx = cfg.receiver[0];
                    if (sel.kind === 'sdr') {
                        rx.input = String(sel.device.input || '').toUpperCase();
                        rx.serial = sel.device.serial;
                    } else if (sel.kind === 'dc') {
                        rx.input = 'SERIALPORT';
                        delete rx.serial;
                        rx.serialport = Object.assign({}, rx.serialport, {
                            port: sel.dc_port,
                            baudrate: DAISY_CATCHER.baudrate,
                            init_seq: DAISY_CATCHER.init_seq
                        });
                    } else {
                        rx.input = 'SERIALPORT';
                        delete rx.serial;
                        rx.serialport = Object.assign({}, rx.serialport, {
                            port: sel.serial_port,
                            baudrate: sel.serial_baudrate
                        });
                        delete rx.serialport.init_seq;
                    }
                }
                if (sel.community_uid) {
                    cfg.sharing = true;
                    cfg.sharing_key = sel.community_uid;
                }
                if (sel.aishub_port) {
                    let udp = cfg.udp || [];
                    if (!Array.isArray(udp)) udp = [udp];
                    const dup = udp.some(u => u && u.host === 'data.aishub.net' && String(u.port) === String(sel.aishub_port));
                    if (!dup) udp.push({ active: true, host: 'data.aishub.net', port: +sel.aishub_port, description: 'AISHub' });
                    cfg.udp = udp;
                }
                (legacyOutputs || []).forEach(it => {
                    if (!it.checked || it.kind === 'sharing') return;
                    let arr = cfg[it.key] || [];
                    if (!Array.isArray(arr)) arr = [arr];
                    const dup = arr.some(x => x && typeof x === 'object' &&
                        it.idFields.every(f => String(x[f]) === String(it.item[f])));
                    if (!dup) arr.push(it.item);
                    cfg[it.key] = arr;
                });
                if (cfg.control) cfg.control.wizard = false;
                return postConfig(cfg);
            })
            .then(data => {
                if (!data.status) throw new Error(data.error || 'save failed');
                close(false);
                if (global.App && App.notify) App.notify('success', 'Configuration saved');
                if (startAfter)
                    return fetch('/api/status').then(r => { if (!r.ok) throw new Error(); return r.json(); })
                        .then(st => fetch('/api/engine', { method: 'POST', body: st.engine === 'running' ? 'restart' : 'start' }))
                        .then(r => r.json())
                        .then(res => {
                            if (!res.status && global.App && App.notify)
                                App.notify('error', 'Could not start the receiver: ' + (res.error || 'request failed'));
                        })
                        .catch(() => { if (global.App && App.notify) App.notify('error', 'Could not start the receiver'); });
            })
            .catch(e => {
                nextBtn.disabled = false;
                nextBtn.textContent = 'Save & Close';
                stepError('Could not save: ' + e.message);
            });
    }

    const SetupWizard = {
        open(pw) {
            ensureOverlay();
            resetState();
            pwState = { needed: !!(pw && pw.needed), setup: !!(pw && pw.setup) };
            fetchLegacy();
            renderStep();
            overlay.classList.add('open');
        }
    };

    global.SetupWizard = SetupWizard;
})(window);
