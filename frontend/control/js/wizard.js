// First-run setup wizard: welcome dialog plus a small step-by-step flow that
// writes a simple configuration (input device, community sharing, AISHub).
// Steps are described declaratively in WIZARD_STEPS; output services are pure
// data so new ones can be added without touching the engine.

(function (global) {
    'use strict';

    const AISCATCHER_SVG =
        '<svg viewBox="0 0 106.63 101.41" xmlns="http://www.w3.org/2000/svg"><g transform="translate(-2.8209 -5.1046)" fill="#0857b1">' +
        '<path d="m100.94 53.199h-3.5e-4c-0.0018 12.359-5.0165 23.535-13.138 31.663-8.1284 8.1213-19.304 13.136-31.663 13.138-12.359-0.00176-23.535-5.0168-31.663-13.138-8.1216-8.1284-13.136-19.304-13.138-31.663h-8.5132c-0.0019 14.692 5.9867 28.045 15.632 37.683 9.6383 9.6449 22.991 15.634 37.683 15.632 14.692 2e-3 28.045-5.9866 37.683-15.632 9.6446-9.6379 15.633-22.991 15.632-37.683h-8.5132"/>' +
        '<path d="m56.136 93.173c11.013 0.0018 21.031-4.4905 28.254-11.72 7.2291-7.2224 11.722-17.24 11.72-28.254h-8.5136c-0.0018 8.6801-3.5204 16.521-9.2262 22.233-5.7125 5.7058-13.553 9.2248-22.234 9.2262-8.6808-0.0014-16.521-3.52-22.234-9.2262-5.7059-5.7122-9.2243-13.553-9.2262-22.233h-8.5134c-0.0019 11.013 4.4909 21.031 11.72 28.254 7.2226 7.2291 17.24 11.721 28.254 11.72"/>' +
        '<path d="m29.503 53.199c-0.0017 7.3353 2.9947 14.018 7.8078 18.825 4.8065 4.8129 11.489 7.8098 18.825 7.8077 7.3357 0.0021 14.018-2.9947 18.825-7.8077 4.8129-4.807 7.8094-11.489 7.8077-18.825h-8.5136c-0.0018 5.0024-2.0242 9.5081-5.3142 12.804-3.2967 3.2904-7.802 5.3125-12.805 5.3142-5.0027-0.0018-9.5081-2.0239-12.805-5.3142-3.2904-3.2964-5.3125-7.802-5.3145-12.804h-8.5134"/>' +
        '<path d="m56.136 47.791c2.9866 0 5.4077 2.4215 5.4077 5.4084 0 2.9859-2.4211 5.4074-5.4077 5.4074s-5.4077-2.4215-5.4077-5.4074c0-2.987 2.4211-5.4084 5.4077-5.4084zm0 18.247c7.0905 0 12.838-5.7482 12.838-12.838 0-7.0908-5.7478-12.839-12.838-12.839s-12.839 5.7478-12.839 12.839c0 7.0901 5.7482 12.838 12.839 12.838"/>' +
        '<path d="m56.093 35.218c9.377 0 17.181 6.6996 18.644 15.45h21.047v-20.877s-24.146-2.7524-39.691-8.1104c-15.544 5.358-39.691 8.1104-39.691 8.1104v20.877h21.047c1.4628-8.7499 9.2663-15.45 18.644-15.45"/>' +
        '<path d="m55.832 18.164 1.182 0.393c7.4393 2.4518 16.912 4.3557 24.848 5.6822v-13.731s-16.507-1.9276-26.03-5.4042c-9.5227 3.4766-26.03 5.4042-26.03 5.4042v13.731c7.9439-1.3264 17.425-3.2381 24.848-5.6822l1.182-0.393"/></g></svg>';

    const UUID_RE = /^[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;

    const BAUD_RATES = [4800, 9600, 38400, 115200];

    // only true SDR hardware is offered directly; serial devices are covered
    // by the dAISy option below the list
    const SDR_TYPES = {
        RTLSDR: 'RTL-SDR',
        AIRSPY: 'Airspy',
        AIRSPYHF: 'Airspy HF+',
        HACKRF: 'HackRF',
        SDRPLAY: 'SDRplay',
        HYDRASDR: 'HydraSDR',
        SOAPYSDR: 'SoapySDR'
    };

    // ------------------------------------------------------------------
    // Step schema
    // ------------------------------------------------------------------

    const WIZARD_STEPS = [
        {
            id: 'welcome',
            title: 'Welcome to AIS-catcher',
            type: 'welcome',
            nextLabel: 'Get started'
        },
        {
            id: 'input',
            title: 'Input Device',
            type: 'input',
            intro: 'Select the device that receives AIS. SDR devices below were detected on this machine; a dAISy or other NMEA receiver connects via a serial port.'
        },
        {
            id: 'community',
            title: 'AIS-catcher Community',
            type: 'service',
            service: {
                icon: 'aiscatcher',
                name: 'AIS-catcher Community Station',
                blurb: 'Your reception is shared with the AIS-catcher community map by default. Register a station to claim it: your station gets a name, coverage statistics and a public page on aiscatcher.org.',
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

    // ------------------------------------------------------------------
    // State
    // ------------------------------------------------------------------

    let stepIndex = 0;
    let sel = {};
    let overlay = null;
    let devices = null;      // /api/devices result
    let serialPorts = null;  // /api/serial result

    function resetState() {
        stepIndex = 0;
        sel = { device: null, daisy: false, daisy_port: '', daisy_baudrate: 38400, community_uid: '', aishub_port: '' };
    }

    // the wizard is shown while control.wizard is true in the config (set
    // when the default config is created) and switched off once completed
    // or dismissed, so the state is shared by all browsers
    function wizardFlagOn(cfg) {
        const v = cfg && cfg.control && cfg.control.wizard;
        return v === true || v === 'true' || v === 'on';
    }

    function postConfig(cfg) {
        return fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(cfg, null, 2)
        }).then(r => r.json());
    }

    function clearWizardFlag() {
        fetch('/api/config')
            .then(r => { if (!r.ok) throw new Error(); return r.json(); })
            .then(cfg => {
                if (!wizardFlagOn(cfg)) return;
                cfg.control.wizard = false;
                return postConfig(cfg);
            })
            .catch(() => { });
    }

    // ------------------------------------------------------------------
    // Rendering
    // ------------------------------------------------------------------

    function icon(name, cls) {
        if (name === 'aiscatcher') return '<span class="' + cls + '">' + AISCATCHER_SVG + '</span>';
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
    }

    function close(clearFlag) {
        if (clearFlag) clearWizardFlag();
        if (overlay) overlay.classList.remove('open');
    }

    function renderStep() {
        const step = WIZARD_STEPS[stepIndex];
        const card = document.getElementById('wizard-card');

        const dots = WIZARD_STEPS.map((s, i) =>
            '<span class="wz-dot' + (i === stepIndex ? ' active' : i < stepIndex ? ' past' : '') + '"></span>').join('');

        const showBack = stepIndex > 0;
        const showSkip = step.type === 'input' || step.type === 'service';
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
        else if (step.type === 'input') renderInput(body, step);
        else if (step.type === 'service') renderService(body, step);
        else if (step.type === 'finish') renderFinish(body, step);

        document.getElementById('wz-cancel').addEventListener('click', () => {
            close(true);
            if (global.App && App.notify) App.notify('info', 'Setup cancelled — run the wizard anytime from the System panel');
        });
        const back = document.getElementById('wz-back');
        if (back) back.addEventListener('click', () => { stepIndex--; renderStep(); });
        const skip = document.getElementById('wz-skip');
        if (skip) skip.addEventListener('click', () => { clearStep(step); stepIndex++; renderStep(); });
        document.getElementById('wz-next').addEventListener('click', () => onNext(step));
    }

    function stepError(message) {
        const el = document.getElementById('wz-error');
        if (!message) { el.classList.add('hidden'); return; }
        el.textContent = message;
        el.classList.remove('hidden');
    }

    function clearStep(step) {
        if (step.type === 'input') { sel.device = null; sel.daisy = false; }
        else if (step.type === 'service') sel[step.service.field.key] = '';
    }

    // --- Welcome ---

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

    // --- Input device ---

    function renderInput(body, step) {
        body.innerHTML = '<h3>' + esc(step.title) + '</h3><p class="wz-intro">' + esc(step.intro) + '</p><div id="wz-devices" class="wz-options"><div class="wz-loading">Scanning devices&hellip;</div></div>';

        const fetches = [
            devices ? Promise.resolve(devices) : fetch('/api/devices').then(r => r.json()).then(d => (devices = d)),
            serialPorts ? Promise.resolve(serialPorts) : fetch('/api/serial').then(r => r.json()).then(d => (serialPorts = d))
        ];

        Promise.all(fetches).then(([dev, ports]) => {
            const list = ((dev && dev.devices) || []).filter(d => SDR_TYPES[d.input]);
            const box = document.getElementById('wz-devices');
            if (!box) return;

            let html = '';
            if (!list.length)
                html += '<p class="wz-none">No SDR devices detected. Connect an RTL-SDR or similar dongle, or use a serial device below.</p>';

            list.forEach((d, i) => {
                const active = sel.device && sel.device.input === d.input && sel.device.serial === d.serial;
                html +=
                    '<label class="wz-option' + (active ? ' selected' : '') + '" data-dev="' + i + '">' +
                    '<input type="radio" name="wz-dev" ' + (active ? 'checked' : '') + '>' +
                    '<span class="wz-option-main"><span class="wz-option-title">' + esc(SDR_TYPES[d.input]) + '</span>' +
                    '<span class="wz-option-sub">Serial ' + esc(d.serial || '&mdash;') + '</span></span>' +
                    '</label>';
            });

            const portOptions = (ports || []).map(p => '<option value="' + esc(p) + '">').join('');
            html +=
                '<label class="wz-option' + (sel.daisy ? ' selected' : '') + '" data-dev="daisy">' +
                '<input type="radio" name="wz-dev" ' + (sel.daisy ? 'checked' : '') + '>' +
                '<span class="wz-option-main"><span class="wz-option-title">dAISy / serial NMEA receiver</span>' +
                '<span class="wz-option-sub">Hardware AIS receiver connected via a serial port</span>' +
                '<span class="wz-daisy' + (sel.daisy ? '' : ' hidden') + '" id="wz-daisy-fields">' +
                '<input id="wz-daisy-port" list="wz-serial-list" placeholder="Serial port, e.g. /dev/serial0" value="' + esc(sel.daisy_port) + '">' +
                '<datalist id="wz-serial-list">' + portOptions + '</datalist>' +
                '<select id="wz-daisy-baud">' +
                BAUD_RATES.map(b => '<option value="' + b + '"' + (b === sel.daisy_baudrate ? ' selected' : '') + '>' + b + ' baud</option>').join('') +
                '</select>' +
                '</span></span>' +
                '</label>';

            box.innerHTML = html;

            box.querySelectorAll('.wz-option').forEach(opt => {
                opt.addEventListener('click', () => {
                    box.querySelectorAll('.wz-option').forEach(o => o.classList.remove('selected'));
                    opt.classList.add('selected');
                    opt.querySelector('input[type=radio]').checked = true;
                    if (opt.dataset.dev === 'daisy') {
                        sel.device = null;
                        sel.daisy = true;
                        document.getElementById('wz-daisy-fields').classList.remove('hidden');
                    } else {
                        sel.daisy = false;
                        sel.device = list[+opt.dataset.dev];
                        const f = document.getElementById('wz-daisy-fields');
                        if (f) f.classList.add('hidden');
                    }
                    stepError();
                });
            });

            const portInput = document.getElementById('wz-daisy-port');
            const baudInput = document.getElementById('wz-daisy-baud');
            if (portInput) portInput.addEventListener('input', () => { sel.daisy_port = portInput.value.trim(); });
            if (baudInput) baudInput.addEventListener('change', () => { sel.daisy_baudrate = +baudInput.value; });
        }).catch(() => {
            const box = document.getElementById('wz-devices');
            if (box) box.innerHTML = '<p class="wz-none">Could not scan for devices.</p>';
        });
    }

    // --- Output services (schema-driven) ---

    function renderService(body, step) {
        const s = step.service;
        body.innerHTML =
            '<h3>' + esc(step.title) + '</h3>' +
            '<div class="wz-service">' +
            icon(s.icon, 'wz-service-icon') +
            '<div class="wz-service-main">' +
            '<span class="wz-option-title">' + esc(s.name) + '</span>' +
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

    // --- Summary ---

    function changeList() {
        const items = [];
        if (sel.device)
            items.push('Input device: <b>' + esc(SDR_TYPES[sel.device.input] || sel.device.input) + ' [' + esc(sel.device.serial || '') + ']</b>');
        else if (sel.daisy && sel.daisy_port)
            items.push('Input device: <b>serial receiver on ' + esc(sel.daisy_port) + '</b> at ' + sel.daisy_baudrate + ' baud');
        if (sel.community_uid)
            items.push('Community sharing with your registered station key');
        if (sel.aishub_port)
            items.push('UDP feed to <b>data.aishub.net:' + esc(sel.aishub_port) + '</b> (AISHub)');
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

    // ------------------------------------------------------------------
    // Navigation and apply
    // ------------------------------------------------------------------

    function onNext(step) {
        stepError();

        if (step.type === 'input' && sel.daisy && !sel.daisy_port) {
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
        stepIndex++;
        renderStep();
    }

    function applyAndClose() {
        const items = changeList();
        if (!items.length) { close(true); return; }

        const startAfter = !!(document.getElementById('wz-start') && document.getElementById('wz-start').checked);
        const nextBtn = document.getElementById('wz-next');
        nextBtn.disabled = true;
        nextBtn.textContent = 'Saving...';

        fetch('/api/config')
            .then(r => { if (!r.ok) throw new Error('failed to load configuration'); return r.json(); })
            .then(cfg => {
                if (sel.device) {
                    cfg.input = sel.device.input;
                    cfg.serial = sel.device.serial;
                } else if (sel.daisy && sel.daisy_port) {
                    cfg.input = 'SERIALPORT';
                    cfg.serialport = Object.assign({}, cfg.serialport, {
                        active: true,
                        port: sel.daisy_port,
                        baudrate: sel.daisy_baudrate
                    });
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
                if (cfg.control) cfg.control.wizard = false;
                return postConfig(cfg);
            })
            .then(data => {
                if (!data.status) throw new Error(data.error || 'save failed');
                close(false);
                if (global.App && App.notify) App.notify('success', 'Configuration saved');
                if (startAfter)
                    return fetch('/api/status').then(r => r.json())
                        .then(st => fetch('/api/engine', { method: 'POST', body: st.engine === 'running' ? 'restart' : 'start' }));
            })
            .catch(e => {
                nextBtn.disabled = false;
                nextBtn.textContent = 'Save & Close';
                stepError('Could not save: ' + e.message);
            });
    }

    // ------------------------------------------------------------------
    // Public API
    // ------------------------------------------------------------------

    const SetupWizard = {
        open() {
            ensureOverlay();
            resetState();
            renderStep();
            overlay.classList.add('open');
        },

        maybeAutoOpen() {
            fetch('/api/config')
                .then(r => { if (!r.ok) throw new Error(); return r.json(); })
                .then(cfg => { if (wizardFlagOn(cfg)) SetupWizard.open(); })
                .catch(() => { });
        }
    };

    global.SetupWizard = SetupWizard;
})(window);
