import {createPlaceEditor} from './places-editor.js';
import * as AISComponents from '@aiscatcher/ui/components.js';
import { App, ConfigStore, ConfigManagers, ZoneColors, createSimpleConfigManager, createChannelManager } from './config-manager.js';
import { SetupWizard } from './wizard.js';
import { webviewerSchema, sharingSchema, receiverSchema, screenSchema, CHANNEL_REGISTRY, channelTitle } from './schema.js';

(function () {
    'use strict';

    let auth = 'login';
    let placeEditor = null;
    let hasPassword = true;
    // the viewer is mounted on this server, so one exposed port serves both
    const VIEWER_PATH = '/viewer/';

    let port = 0;
    let viewerLoaded = false;
    let engineRunning = false;
    let engineDesired = false;
    let pendingApply = false;
    let pendingAction = null;

    // shapes live in icons.css; swap the class rather than inlining SVG
    const ENGINE_ICONS = { start: 'engine_start_icon', stop: 'engine_stop_icon', restart: 'engine_restart_icon' };

    const ENGINE_TITLES = {
        start: 'Start AIS-catcher',
        stop: 'Stop AIS-catcher',
        restart: 'Restart AIS-catcher to apply configuration changes'
    };

    function engineButtonMode() {
        if (!engineRunning) return engineDesired ? 'stop' : 'start';
        return pendingApply ? 'restart' : 'stop';
    }

    const ENGINE_LABELS = { start: 'Start receiver', stop: 'Stop receiver', restart: 'Restart receiver' };
    const ENGINE_SVG = {
        start: '<path d="M8 5.5v13l10-6.5z" fill="currentColor" stroke="none"/>',
        stop: '<rect x="6.5" y="6.5" width="11" height="11" rx="1.5" fill="currentColor" stroke="none"/>',
        restart: '<path d="M20 12a8 8 0 1 1-2.34-5.66M20 4v5h-5"/>'
    };

    function renderEngineButton() {
        const mode = engineButtonMode();
        const big = document.getElementById('hub-engine-btn');
        if (big && big.dataset.mode !== mode) {
            big.dataset.mode = mode;
            big.className = 'st-btn' + (mode === 'stop' ? '' : ' st-btn--primary');
            big.title = ENGINE_TITLES[mode];
            big.innerHTML = '<svg class="st-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">' +
                ENGINE_SVG[mode] + '</svg><span>' + ENGINE_LABELS[mode] + '</span>';
        }
        const btn = document.getElementById('nav-start-restart');
        const label = document.getElementById('nav-sr-label');
        const icon = document.getElementById('nav-sr-icon');
        if (btn) {
            btn.classList.toggle('attention', pendingApply);
            btn.title = ENGINE_TITLES[mode];
        }
        if (icon && icon.dataset.mode !== mode) {
            icon.dataset.mode = mode;
            icon.className = ENGINE_ICONS[mode];
            if (label) label.textContent = mode.charAt(0).toUpperCase() + mode.slice(1);
        }
    }
    let streamRetryTimer = null;
    let streamWatchdog = null;
    let eventSource = null;
    let currentOutputType = 'sharing';
    let flowOutputTarget = null;

    const iframe = document.getElementById('webviewer-frame');
    iframe.addEventListener('load', () => {
        try {
            const doc = iframe.contentDocument;
            const title = doc.querySelector('title') || doc.head.appendChild(doc.createElement('title'));
            const mirror = () => { if (doc.title) document.title = doc.title; };
            mirror();
            new MutationObserver(mirror).observe(title, { childList: true, characterData: true, subtree: true });
        } catch (e) { }
    });
    const systemOverlay = document.getElementById('system-overlay');
    const systemBody = document.getElementById('system-body');
    const systemTabs = document.getElementById('system-tabs');
    const systemSubtabs = document.getElementById('system-subtabs');
    let currentSystemTab = null;
    let placeReturnTab = 'viewer';
    const loadedTabs = new Set();
    let flowResizeObserver = null;
    let flowRenderId = 0;
    let flowStatsTimer = null;

    // tab id -> header label / which nav-bar button to highlight
    const SYSTEM_TABS = {
        input: { label: 'Input', nav: 'input' },
        output: { label: 'Output', nav: 'output' },
        places: { label: 'Places', nav: 'control-panel' },
        flow: { label: 'Flow', nav: 'control-panel' },
        status: { label: 'Status', nav: 'control-panel' },
        viewer: { label: 'Viewer', nav: 'control-panel' },
        config: { label: 'Configuration', nav: 'control-panel' },
        log: { label: 'Log', nav: 'control-panel' },
        wizard: { label: 'Wizard', nav: 'control-panel' },
        password: { label: 'Access', nav: 'control-panel' },
        license: { label: 'License', nav: 'control-panel' }
    };

    const SYSTEM_GROUP = ['status', 'log', 'config', 'password', 'wizard', 'license'];
    let lastSystemLeaf = 'status';
    const TABS_WITH_SUBS = ['system', 'output'];

    function fetchStatus() {
        return fetch('/api/status').then(r => {
            if (!r.ok) throw new Error('Status request failed');
            return r.json();
        });
    }

    function postPassword(endpoint, pw) {
        return fetch(endpoint, { method: 'POST', body: pw })
            .catch(() => { throw new Error('Connection error. Please try again.'); })
            .then(r => {
                if (r.status === 401 && endpoint !== '/api/login') window.hubAuthRequired();
                return r.json().catch(() => { throw new Error('Connection error. Please try again.'); });
            })
            .then(data => {
                if (!data.status) throw new Error(data.error || 'Request failed');
                return data;
            });
    }

    function engineAction(action) {
        return fetch('/api/engine', { method: 'POST', body: action })
            .then(r => r.json().then(body => {
                if (!r.ok || !body.status) {
                    if (r.status === 401) window.hubAuthRequired();
                    throw new Error(body.error || 'Engine ' + action + ' failed');
                }
                return body;
            }));
    }

    // Central handler for expired sessions (used by config-manager and the
    // wizard too): ask for the password again, leaving unsaved edits intact.
    window.hubAuthRequired = function () {
        if (isLoggedIn()) {
            auth = 'login';
            stopEventStream();
            updateBarVisibility();
        }
        openLoginModal();
    };

    // Called by the wizard when it is cancelled while no password is set:
    // the modal is the backstop, so the requirement cannot be dismissed.
    window.hubPasswordBackstop = openLoginModal;

    // Called by the wizard once its password step has authenticated us.
    window.hubAuthGranted = function () {
        hasPassword = true;
        if (!isLoggedIn()) auth = 'ok';
        updateBarVisibility();
        startEventStream();
    };

    function formatUptime(seconds) {
        if (!seconds || seconds <= 0) return '';
        const d = Math.floor(seconds / 86400);
        const h = Math.floor((seconds % 86400) / 3600);
        const m = Math.floor((seconds % 3600) / 60);
        const s = seconds % 60;
        const hms = [h, m, s].map(n => String(n).padStart(2, '0')).join(':');
        return d > 0 ? d + 'd ' + hms : hms;
    }

    function isLoggedIn() { return auth === 'ok' || auth === 'open'; }

    let barCollapsed = false;

    function updateBarVisibility() {
        const bar = document.getElementById('bottom-bar');
        const loginPill = document.getElementById('login-pill');
        const restore = document.getElementById('bar-restore');
        const loggedIn = isLoggedIn();
        if (loginPill) loginPill.classList.toggle('show', !loggedIn);
        if (restore) restore.classList.toggle('show', loggedIn && barCollapsed);
        if (bar) bar.style.display = (loggedIn && !barCollapsed) ? '' : 'none';
    }


    // collapse is session-only: the bar always starts expanded
    function setBarCollapsed(collapsed) {
        barCollapsed = collapsed;
        updateBarVisibility();
    }

    // No password means one must be set, regardless of how the page is
    // reached; the modal is the backstop when the wizard does not run.
    function passwordSetupMode() {
        return !hasPassword;
    }

    function openWizard() {
        SetupWizard.open({ needed: passwordSetupMode(), setup: auth === 'setup' });
    }

    function openLoginModal() {
        const setup = passwordSetupMode();
        document.getElementById('login-title').textContent = setup ? 'Set Password' : 'Sign In';
        document.getElementById('login-subtitle').textContent = setup
            ? 'No password set: choose an admin password to protect this control page.'
            : 'Authentication required to continue.';
        document.getElementById('login-password').setAttribute('autocomplete', setup ? 'new-password' : 'current-password');
        document.getElementById('login-password2').classList.toggle('hidden', !setup);
        document.getElementById('login-submit').textContent = setup ? 'Set Password' : 'Sign In';
        document.getElementById('login-cancel').classList.toggle('hidden', setup);
        document.getElementById('login-overlay').classList.add('open');
        setTimeout(() => document.getElementById('login-password').focus(), 50);
    }

    function closeLoginModal() {
        document.getElementById('login-overlay').classList.remove('open');
        document.getElementById('login-error').classList.add('hidden');
        document.getElementById('login-form').reset();
        pendingAction = null;
    }

    function loginError(message) {
        const errorEl = document.getElementById('login-error');
        errorEl.textContent = message;
        errorEl.classList.remove('hidden');
    }

    function submitLogin(e) {
        e.preventDefault();
        const setup = passwordSetupMode();
        const password = document.getElementById('login-password').value;
        const submitBtn = document.getElementById('login-submit');
        document.getElementById('login-error').classList.add('hidden');

        if (setup && password !== document.getElementById('login-password2').value) {
            loginError('Passwords do not match');
            return;
        }

        submitBtn.disabled = true;
        const endpoint = auth === 'setup' ? '/api/setup' : setup ? '/api/password' : '/api/login';
        postPassword(endpoint, password)
            .then(() => {
                submitBtn.disabled = false;
                if (auth === 'setup') {
                    window.location.reload();
                    return;
                }
                if (setup) {
                    hasPassword = true;
                    App.notify('success', 'Password set');
                    closeLoginModal();
                    return;
                }
                auth = 'ok';
                updateBarVisibility();
                startEventStream();
                const action = pendingAction;
                closeLoginModal();
                refreshEngineStatus().then(st => {
                    if (!action && st && st.wizard) openWizard();
                });
                if (action) action();
            })
            .catch(err => {
                submitBtn.disabled = false;
                loginError(err.message || 'Login failed');
            });
    }

    function requireAuth(callback) {
        if (isLoggedIn()) {
            callback();
            return;
        }
        pendingAction = callback;
        openLoginModal();
    }

    function setEngineButtonDisabled(disabled) {
        const btn = document.getElementById('nav-start-restart');
        if (btn) {
            btn.disabled = disabled;
            btn.classList.toggle('sys-o50', disabled);
        }
    }

    const ENGINE_STATES = {
        running: { label: 'Running', dot: 'dot-ok', text: 'sys-ok', dotColor: 'var(--color-on)' },
        starting: { label: 'Starting...', dot: 'dot-warn', text: 'sys-warn-ink', dotColor: 'var(--color-warning-ink)' },
        retrying: { label: 'Retrying...', dot: 'dot-warn', text: 'sys-warn-ink', dotColor: 'var(--color-warning-ink)' },
        stopped: { label: 'Stopped', dot: '', text: 't-subtle', dotColor: 'var(--chrome-dot)' }
    };

    let engineStateKey = 'stopped';
    let uptimeBase = 0;
    let uptimeStamp = 0;

    function updateUptimeDisplay() {
        const s = ENGINE_STATES[engineStateKey];
        const up = engineRunning ? formatUptime(uptimeBase + Math.floor((Date.now() - uptimeStamp) / 1000)) : '';
        const dotUptime = document.getElementById('status-dot-uptime');
        if (dotUptime) dotUptime.textContent = up;
        const dotText = document.getElementById('status-dot-text');
        if (dotText) dotText.textContent = s.label + (up ? ' · ' + up : '');
        const hubUptime = document.getElementById('hub-uptime');
        if (hubUptime) hubUptime.textContent = up ? 'up ' + up : '';
    }

    function renderEngineState(data) {
        if (!data) return;
        const running = data.engine === 'running';
        engineRunning = running;
        engineDesired = !!data.desired;
        renderEngineButton();

        const state = running ? 'running' : (data.retrying ? 'retrying' : (data.desired ? 'starting' : 'stopped'));
        const s = ENGINE_STATES[state];
        engineStateKey = state;
        if (data.uptime !== undefined) {
            uptimeBase = data.uptime;
            uptimeStamp = Date.now();
        }

        const dot = document.getElementById('status-dot');
        if (dot) dot.className = 'dot ' + s.dot;
        const dotLabel = document.getElementById('status-dot-label');
        if (dotLabel) dotLabel.textContent = s.label;
        const dotText = document.getElementById('status-dot-text');
        if (dotText) dotText.className = 't-small sys-wide-only ' + s.text;
        const restoreDot = document.getElementById('restore-dot');
        if (restoreDot) restoreDot.style.background = s.dotColor;

        const hubStatus = document.getElementById('hub-status');
        if (hubStatus && hubStatus.dataset.state !== state) {
            hubStatus.dataset.state = state;
            hubStatus.className = 'st-chip st-state st-state--' + state;
            hubStatus.textContent = s.label.replace('...', '\u2026');
        }

        updateUptimeDisplay();

        const sysinfo = document.getElementById('hub-sysinfo');
        if (sysinfo && data.version) {
            if (!sysinfo._memEl) {
                const rows = [
                    ['Version', data.version],
                    ['Build date', data.build_date],
                    ['Operating system', data.os],
                    ['Hardware', data.hardware]
                ].filter(r => r[1]);
                sysinfo.textContent = '';
                const fact = (k, v, cls) => {
                    const row = document.createElement('div');
                    const dt = document.createElement('dt');
                    const dd = document.createElement('dd');
                    dt.textContent = k;
                    dd.textContent = v;
                    if (cls) dd.className = cls;
                    row.append(dt, dd);
                    sysinfo.appendChild(row);
                    return row;
                };
                rows.forEach(r => fact(r[0], r[1], r[0] === 'Version' ? 'st-facts__mono' : ''));
                sysinfo._memRow = fact('Memory usage', '');
                sysinfo._memEl = sysinfo._memRow.lastChild;
                const card = document.getElementById('hub-sysinfo-card');
                if (card) card.hidden = false;
            }
            sysinfo._memEl.textContent = data.memory ? formatBytes(data.memory) : '';
            sysinfo._memRow.hidden = !data.memory;
        }
    }

    let reloadUntil = 0;
    let reloadSawDown = false;
    let overlayRestartBtn = null;
    let lastUptime = Infinity;

    function restartTimedOut() {
        reloadUntil = 0;
        if (overlayRestartBtn) {
            overlayRestartBtn.disabled = false;
            overlayRestartBtn.textContent = 'Restart';
        }
    }

    function applyStatus(data) {
        if (data.auth && data.auth !== auth) {
            auth = data.auth;
            updateBarVisibility();
            startEventStream();
        }
        if (data.has_password !== undefined) hasPassword = !!data.has_password;
        renderEngineState(data);
        setEngineButtonDisabled(false);
        // a restart can complete between two status updates, so a falling
        // uptime is the signal, not a stopped state we may never observe
        const uptime = data.engine === 'running' && typeof data.uptime === 'number' ? data.uptime : Infinity;
        const fell = uptime < lastUptime;
        lastUptime = uptime;
        if (reloadUntil) {
            if (data.engine === 'running' && (reloadSawDown || fell)) {
                reloadUntil = 0;
                window.location.reload();
            } else if (data.desired === false || Date.now() > reloadUntil) {
                restartTimedOut();
            } else if (data.engine !== 'running') {
                reloadSawDown = true;
            }
        }

        if (data.viewer && (!viewerLoaded || data.viewer !== port)) {
            port = data.viewer;
            clearOverlayMessages();
            loadWebviewer();
        }
        return data;
    }

    window.addEventListener('message', (e) => {
        if (e.origin !== window.location.origin) return;
        if (!e.data || e.data.type !== 'aiscatcher:sharing') return;

        const link = document.getElementById('community-link');
        if (!link) return;

        const SHARING = {
            on: { title: 'Sharing with the community map' },
            anon: { title: 'Sharing anonymously — register to claim your station' },
            off: { title: 'Not sharing — put your station on the community map', href: 'https://aiscatcher.org/addstation_ac' },
            stopped: { title: 'Receiver stopped' },
        };
        const state = e.data.state in SHARING ? e.data.state : 'off';
        link.classList.remove('sharing-on', 'sharing-anon', 'sharing-off', 'sharing-stopped');
        link.classList.add('sharing-' + state);
        link.href = SHARING[state].href || 'https://www.aiscatcher.org';
        link.title = SHARING[state].title;
    });

    function refreshEngineStatus() {
        return fetchStatus()
            .then(applyStatus)
            .catch(() => {
                setEngineButtonDisabled(false);
                return null;
            });
    }

    function scheduleStreamRetry() {
        if (streamRetryTimer) return;
        streamRetryTimer = setTimeout(() => {
            streamRetryTimer = null;
            refreshEngineStatus().then(data => {
                startEventStream();
                if (!data) scheduleStreamRetry();
            });
        }, 5000);
    }

    function cancelStreamRetry() {
        clearTimeout(streamRetryTimer);
        streamRetryTimer = null;
    }

    function armStreamWatchdog() {
        clearTimeout(streamWatchdog);
        streamWatchdog = setTimeout(() => {
            streamWatchdog = null;
            if (eventSource && eventSource.readyState === EventSource.CONNECTING) {
                stopEventStream();
                scheduleStreamRetry();
            }
        }, 10000);
    }

    window.hubConfigSaved = function (kind) {
        // Viewer polls configuration versions and applies changes in place.
        if (kind === 'viewer') {
            App.notify('info', 'Map settings saved; the viewer updates automatically', 5000);
        } else {
            App.notify('info', (engineRunning ? 'Restart' : 'Start') + ' the receiver to apply the new configuration', 8000);
            pendingApply = true;
            renderEngineButton();
        }
    };

    function onStartRestartClick() {
        const action = engineButtonMode();
        requireAuth(() => {
            setEngineButtonDisabled(true);
            engineAction(action)
                .then(() => { pendingApply = false; renderEngineButton(); })
                .catch(e => {
                    setEngineButtonDisabled(false);
                    if (e && e.message) App.notify('error', e.message);
                });
        });
    }

    const CHANNELS = ['A', 'B', 'C', 'D'];
    let channelPrev = null;

    function initChannelLeds() {
        const wrap = document.getElementById('channel-leds');
        if (!wrap || wrap.childElementCount > 0) return;

        wrap.innerHTML = CHANNELS.map((c, i) =>
            `<div class="ch-led-item" id="ch-item-${i}" style="${i >= 2 ? 'display:none' : ''}">
                <span class="ch-led" id="ch-led-${i}"></span>
                <span class="ch-led-label">${c}</span>
            </div>`).join('');
    }

    function onActivityEvent(e) {
        try {
            const ch = JSON.parse(e.data);
            ch.forEach((count, i) => {
                if (i >= 2 && count > 0)
                    document.getElementById('ch-item-' + i).style.display = '';

                if (channelPrev && count > channelPrev[i]) {
                    const led = document.getElementById('ch-led-' + i);
                    led.classList.add('flash');
                    setTimeout(() => led.classList.remove('flash'), 500);
                }
            });
            channelPrev = ch.slice();
        } catch (_) { }
    }

    function showOverlayMessage(html) {
        clearOverlayMessages();
        const hubContainer = document.getElementById('hub-container');
        const div = document.createElement('div');
        div.className = 'hub-overlay-msg row row-center';
        div.innerHTML = html;
        hubContainer.insertBefore(div, hubContainer.firstChild);
        return div;
    }

    function clearOverlayMessages() {
        document.querySelectorAll('.hub-overlay-msg').forEach(el => el.remove());
    }

    function showError(title, message, showRestart = false) {
        const div = showOverlayMessage(`
            <div class="t-center sys-splash-box stack-loose">
                <svg class="icon-2xl t-warn center-block" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z"></path>
                </svg>
                <h3 data-role="err-title" class="t-strong t-lg"></h3>
                <p data-role="err-msg" class="t-muted"></p>
                ${showRestart ? '<button data-role="err-restart" class="btn">Restart</button>' : ''}
            </div>
        `);
        div.querySelector('[data-role="err-title"]').textContent = title;
        div.querySelector('[data-role="err-msg"]').textContent = message;
        const btn = div.querySelector('[data-role="err-restart"]');
        if (btn)
            btn.addEventListener('click', () => {
                requireAuth(() => {
                    btn.textContent = 'Restarting...';
                    btn.disabled = true;
                    overlayRestartBtn = btn;
                    reloadUntil = Date.now() + 90000;
                    reloadSawDown = false;
                    engineAction('restart')
                        .catch(() => { reloadUntil = 0; btn.disabled = false; btn.textContent = 'Restart'; });
                });
            });
    }

    function showNoViewer() {
        showOverlayMessage(`
            <div class="t-center sys-splash-box stack">
                <h3 class="t-strong t-lg">Viewer Not Running</h3>
                <p class="t-muted">The built-in viewer could not be started &mdash; its port may be in use. Check the log in the Control panel.</p>
            </div>
        `);
    }

    function loadWebviewer() {
        iframe.setAttribute('src', iframe.src);   // reloads the frame
        viewerLoaded = true;
        clearOverlayMessages();
    }

    function loadSourceConfig() {
        const host = document.getElementById('sys-input-body');
        if (!host) return;
        host.textContent = '';
        const container = document.createElement('div');
        container.id = 'hub-receivers-container';
        container.className = 'stack stack-loose';
        host.appendChild(container);

        createChannelManager({
            channelType: 'receiver',
            schema: receiverSchema,
            containerId: 'hub-receivers-container',
            title: 'Receiver',
            variant: 'inputs',
            essentials: INPUT_ESSENTIALS,
            describe: (item, i) => describeInput(item, i, ConfigManagers?.get('hub-receivers-container')?.data),
            routing: inputRouting,
            excludeFor: item => SDR_INPUTS.has(item.input) ? [] : ['engines'],
            groupOf: inputGroup,
            openFlow: () => switchSystemTab('flow')
        });
    }

    // what an input needs to work: the device, which one, and how to reach it; the rest is Advanced
    const INPUT_ESSENTIALS = new Set(['input', 'serial', 'serialport_port', 'serialport_baudrate', 'rtltcp_protocol', 'rtltcp_host', 'rtltcp_port', 'rtltcp_url',
        'udpserver_server', 'udpserver_port', 'spyserver_host', 'spyserver_port', 'nmea2000_interface']);
    const INPUT_DESCRIPTIONS = {
        RTLSDR: 'An RTL-SDR dongle on this computer.',
        AIRSPY: 'An Airspy receiver on this computer.',
        AIRSPYHF: 'An Airspy HF+ receiver on this computer.',
        HACKRF: 'A HackRF on this computer.',
        HYDRASDR: 'A HydraSDR on this computer.',
        SERIALPORT: 'An AIS receiver connected by USB or serial cable.',
        UDPSERVER: 'NMEA received over the network by UDP.',
        RTLTCP: 'A radio or feed reached over the network.',
        SPYSERVER: 'A remote radio shared by SpyServer.',
        NMEA2000: 'An NMEA 2000 network interface.'
    };
    // the device as people write it, for an input's default name
    // the receiver's fields carry no section: group them by what they are about
    function inputGroup(f) {
        const n = f.name;
        if (n === 'engines' || n === 'fp_ds') return 'Engines';
        if (n === 'channel' || n === 'nmea_channel') return 'Channels';
        if (n === 'verbose' || n === 'screen' || n === 'verbose_time') return 'Logging';
        if (n.startsWith('timeout')) return 'Timeouts';
        if (n.startsWith('serialport_')) return 'Serial';
        if (n.startsWith('rtltcp_') || n.startsWith('udpserver_')) return 'Connection';
        if (n.startsWith('rtlsdr_') || n === 'tuner' || n.startsWith('airspyhf_') || n.startsWith('hackrf_') || n.startsWith('spyserver_')) return 'Tuning';
        return 'Other';
    }
    const SDR_INPUTS = new Set(['RTLSDR', 'AIRSPY', 'AIRSPYHF', 'HACKRF', 'HYDRASDR', 'SPYSERVER', 'RTLTCP', 'SDRPLAY', 'SOAPYSDR']);
    const DEVICE_NAMES = { RTLSDR: 'RTL-SDR', AIRSPY: 'Airspy', AIRSPYHF: 'Airspy HF+', HACKRF: 'HackRF', HYDRASDR: 'HydraSDR',
        SERIALPORT: 'Serial', UDPSERVER: 'UDP', RTLTCP: 'Network', SPYSERVER: 'SpyServer', NMEA2000: 'NMEA 2000' };
    // An input's name: its description, or else the device with its serial ("RTL-SDR 00000001"),
    // or with its place among the inputs of that device ("Serial #1"). Underneath: the device when
    // the description took the title, and the engines.
    function describeInput(item, i, all) {
        const device = item.input ? (DEVICE_NAMES[item.input] || item.input) : '';
        const list = all || [];
        const nth = list.slice(0, i + 1).filter(r => r.input === item.input).length || 1;
        // a dongle's serial tells it apart; a serial port's or a network feed's is only a number
        const sdr = SDR_INPUTS.has(item.input);
        // a serial receiver is told apart by the port it sits on
        const path = item.input === 'SERIALPORT' ? String(item.serialport?.port || '').trim() : '';
        const fallback = !item.input ? 'New input' : sdr && item.serial ? `${device} ${item.serial}`
            : path ? `${device} ${path}` : `${device} #${nth}`;
        const name = (item.description || '').trim();
        // the second line: the device and what tells it apart (the port's last part, the serial, or the place)
        const id = path ? path.split('/').pop() : sdr && item.serial ? item.serial : `#${nth}`;
        const sub = item.input ? `${device} · ${id}` : '';
        return {
            title: name || fallback,
            sub,
            desc: INPUT_DESCRIPTIONS[item.input] || 'Pick the device this input reads from.'
        };
    }

    const OUTPUT_TYPES = [
        { value: 'sharing', label: 'Community', schema: sharingSchema },
        ...CHANNEL_REGISTRY.filter(c => c.key !== 'receiver').map(c => ({
            value: c.key === 'tcp_listener' ? 'tcp-server' : c.key,
            // the built-in viewer is the Viewer tab; the ones added here are the extra ones
            label: c.key === 'server' ? 'Extra Viewers' : c.label, title: c.key === 'server' ? 'Extra Viewers' : null,
            addLabel: c.key === 'server' ? 'Add viewer' : null,
            schema: c.schema, configKey: c.configKey,
            flowLabel: c.flowLabel, statType: c.statType, statTypes: c.statTypes
        })),
        { value: 'screen', label: 'Screen', schema: screenSchema }
    ];

    // how many outputs of a type are configured, and whether one of them is on
    function outputTypeState(t, cfg) {
        if (t.value === 'sharing') return { count: 0, on: cfg.sharing === true };
        if (t.value === 'screen') return { count: 0, on: cfg.screen !== undefined && !['0', 'none'].includes(String(cfg.screen).toLowerCase()) };
        const list = Array.isArray(cfg[t.configKey]) ? cfg[t.configKey] : [];
        return { count: list.length, on: list.some(i => i.active !== false) };
    }
    // the list follows the edits on screen: the open type's settings come from its manager, not the file
    function paintOutputNav(saved, mgr) {
        let cfg = saved;
        if (mgr && saved) {
            const t = OUTPUT_TYPES.find(o => o.value === currentOutputType);
            cfg = { ...saved };
            if (t && t.configKey) cfg[t.configKey] = mgr.data;
            else if (t && t.value === 'sharing') cfg.sharing = mgr.data.sharing;
            else if (t && t.value === 'screen') cfg.screen = mgr.data.screen;
        }
        document.querySelectorAll('.st-nav__item[data-output-type]').forEach(b => {
            const t = OUTPUT_TYPES.find(o => o.value === b.dataset.outputType);
            if (!t || !cfg) return;
            const { count, on } = outputTypeState(t, cfg);
            const dot = b.querySelector('.st-nav__dot');
            // Community is one switch: it always shows, green when sharing, red when not
            const always = t.value === 'sharing';
            dot.hidden = !on && !always;
            dot.classList.toggle('st-nav__dot--bad', always && !on);
            dot.title = always ? (on ? 'Sharing' : 'Not sharing') : 'At least one is active';
            const c = b.querySelector('.st-nav__count');
            c.hidden = !count;
            c.textContent = count;
        });
    }
    function enableTabScroll(el) {
        return AISComponents.tabScroller(el);
    }

    function hasUnsaved() {
        if (currentSystemTab === 'places') return !!placeEditor?.unsaved();
        return typeof App !== 'undefined' && !!App.state?.unsaved;
    }
    function confirmDiscardUnsaved(verb) {
        if (!hasUnsaved()) return true;
        if (!confirm(`You have unsaved changes. Are you sure you want to ${verb} without saving?`)) return false;
        if (currentSystemTab === 'places') placeEditor.cancel();
        else App.setUnsaved(false);
        return true;
    }

    function setOutputType(value) {
        if (value === currentOutputType) return;
        if (!confirmDiscardUnsaved('switch')) return;
        currentOutputType = value;
        document.querySelectorAll('.st-nav__item[data-output-type]').forEach(b => {
            const on = b.dataset.outputType === value;
            if (on) b.setAttribute('aria-current', 'true'); else b.removeAttribute('aria-current');
            if (on) b.scrollIntoView({ block: 'nearest', inline: 'nearest' });
        });
        updateOutputView();
    }

    // Open the Output tab on a specific sub-type (used by the Data Flow nodes).
    function selectOutputTab(value) {
        const wasLoaded = loadedTabs.has('output');
        flowOutputTarget = value;
        if (!switchSystemTab('output')) {
            flowOutputTarget = null;
            return;
        }
        if (wasLoaded) {
            setOutputType(value);
            flowOutputTarget = null;
        }
    }

    function renderOutputConfig(host) {
        const initial = flowOutputTarget || 'sharing';
        flowOutputTarget = null;
        currentOutputType = null;
        host.textContent = '';
        host.className = 'st st-split';
        host.innerHTML = `
            <nav class="st-nav" aria-label="Output types"><span class="st-nav__title">Output types</span></nav>
            <div class="st-main">
                <div class="st-main__head">
                    <div><h2 class="st-main__title"></h2><p class="st-main__desc"></p></div>
                    <button class="st-btn st-btn--sm" type="button" data-st-add hidden>
                        <svg class="st-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" aria-hidden="true"><path d="M12 5v14M5 12h14"/></svg><span></span>
                    </button>
                </div>
                <div id="hub-output-container"></div>
            </div>`;
        const nav = host.querySelector('.st-nav');
        OUTPUT_TYPES.forEach(({ value, label }) => {
            const btn = document.createElement('button');
            btn.type = 'button';
            btn.className = 'st-nav__item';
            btn.dataset.outputType = value;
            // the count before the dot, so the dots line up at the right edge
            btn.innerHTML = '<span class="st-nav__label"></span><span class="st-nav__count" hidden></span><span class="st-nav__dot" title="At least one is active" hidden></span>';
            btn.querySelector('.st-nav__label').textContent = label;
            btn.addEventListener('click', () => setOutputType(value));
            nav.appendChild(btn);
        });
        host.querySelector('[data-st-add]').addEventListener('click', () => {
            const m = ConfigManagers?.get('hub-output-container');
            if (m && m.config.isList) m.addItem();
        });
        ConfigStore.fetch().then(paintOutputNav).catch(() => {});
        setOutputType(initial);
    }

    function updateOutputView() {
        const t = OUTPUT_TYPES.find(o => o.value === currentOutputType);
        if (!t) return;
        const host = document.getElementById('sys-output-body');
        const list = !!t.configKey;
        host.querySelector('.st-main__title').textContent = t.title || (list ? `${t.label} outputs` : t.label);
        host.querySelector('.st-main__desc').hidden = true; // the title says enough
        // what Community put in the header (its switch and routing line) leaves with it
        host.querySelectorAll('.st-head-extra').forEach(e => e.remove());
        host.querySelector('.st-main__head').classList.remove('st-main__head--page');
        const add = host.querySelector('[data-st-add]');
        add.hidden = !list;
        add.querySelector('span').textContent = t.addLabel || `Add ${t.label} output`;
        // the nav follows the edits: a new row counts at once, a switched-off one drops the dot
        const onChange = mgr => ConfigStore.fetch().then(cfg => paintOutputNav(cfg, mgr)).catch(() => {});

        if (!list) {
            createSimpleConfigManager({
                schema: t.schema,
                containerId: 'hub-output-container',
                variant: t.value === 'sharing' ? 'community' : t.value === 'screen' ? 'plain' : undefined,
                routing: outputRouting,
                openFlow: () => switchSystemTab('flow'),
                onChange
            });
        } else {
            createChannelManager({
                channelType: t.configKey,
                schema: t.schema,
                containerId: 'hub-output-container',
                title: t.label,
                variant: 'outputs',
                routing: outputRouting,
                openFlow: () => switchSystemTab('flow'),
                // what an output needs to work, up front; the rest in Advanced
                essentials: new Set(['description', ...((CHANNEL_REGISTRY.find(c => c.configKey === t.configKey) || {}).essentials || [])]),
                onChange
            });
        }
    }

    let logReplayDone = false;
    let lastLogTime = '';
    let lastLogSeq = -1;
    const logBuffer = [];
    const LOG_BUFFER_MAX = 500;
    let lastToast = { message: '', showing: false };

    const LOG_LEVELS = ['debug', 'info', 'warning', 'error', 'critical'];
    let logLevelMin = 'info';
    let logSearch = '';
    try {
        const stored = localStorage.getItem('hub-log-level');
        if (LOG_LEVELS.indexOf(stored) >= 0) logLevelMin = stored;
    } catch (_) { }

    function passesLogFilter(m) {
        const i = LOG_LEVELS.indexOf(m.level);
        if (i >= 0 && i < LOG_LEVELS.indexOf(logLevelMin)) return false;
        return !logSearch || m.message.toLowerCase().indexOf(logSearch) >= 0;
    }

    function isReplayedLog(m) {
        if (typeof m.seq !== 'number') return false;
        const d = (m.seq - lastLogSeq) >>> 0;
        const ahead = lastLogSeq < 0 || (d > 0 && d < 0x80000000);
        if (!ahead && (!m.time || m.time <= lastLogTime)) return true;
        lastLogSeq = m.seq;
        return false;
    }

    function buildLogLine(m) {
        const level = ['error', 'critical'].indexOf(m.level) >= 0 ? 'error'
            : ['warning', 'info', 'debug'].indexOf(m.level) >= 0 ? m.level : 'default';
        const line = document.createElement('div');
        line.className = 'log-line ' + level;
        const ts = document.createElement('span');
        ts.className = 'log-ts';
        ts.textContent = m.time.length > 19 ? m.time.slice(11, 19) : m.time;
        ts.title = m.time;
        const msg = document.createElement('span');
        msg.textContent = m.message;
        line.appendChild(ts);
        line.appendChild(msg);
        return line;
    }

    function renderLogBox() {
        const box = document.getElementById('log-box');
        if (!box) return;
        box.textContent = '';
        logBuffer.forEach(m => { if (passesLogFilter(m)) box.appendChild(buildLogLine(m)); });
        if (!box.childElementCount) {
            const empty = document.createElement('div');
            empty.className = 'log-empty';
            empty.textContent = logBuffer.length ? 'No lines match the filter.' : 'Waiting for log output...';
            box.appendChild(empty);
        }
        box.scrollTop = box.scrollHeight;
    }

    function onLogEvent(e) {
        try {
            const m = JSON.parse(e.data);
            if (isReplayedLog(m)) return;
            if (m.time && m.time > lastLogTime) lastLogTime = m.time;

            logBuffer.push(m);
            if (logBuffer.length > LOG_BUFFER_MAX) logBuffer.shift();
            const box = document.getElementById('log-box');
            if (box && passesLogFilter(m)) {
                const placeholder = box.querySelector('.log-empty');
                if (placeholder) placeholder.remove();
                box.appendChild(buildLogLine(m));
                while (box.childElementCount > LOG_BUFFER_MAX)
                    box.removeChild(box.firstChild);
                box.scrollTop = box.scrollHeight;
            }

            if (!logReplayDone) return;
            if (m.level === 'error' || m.level === 'critical' || m.level === 'warning') {
                // a crash-looping engine repeats the same line: suppress a repeat
                // only while its toast is still on screen, so dismissing one lets
                // the next occurrence through instead of muting it for a fixed window
                if (m.message === lastToast.message && lastToast.showing) return;
                const rec = { message: m.message, showing: true };
                lastToast = rec;
                App.notify(m.level === 'warning' ? 'warning' : 'error', m.message,
                    undefined, () => { rec.showing = false; });
            }
        } catch (_) { }
    }

    function startEventStream() {
        if (eventSource || !isLoggedIn()) return;
        initChannelLeds();
        logReplayDone = false;
        eventSource = new EventSource('/api/stream' + (lastLogSeq >= 0 ? '?since=' + lastLogSeq : ''));
        eventSource.addEventListener('log', onLogEvent);
        eventSource.addEventListener('activity', onActivityEvent);
        eventSource.addEventListener('status', e => {
            try { applyStatus(JSON.parse(e.data)); } catch (_) { }
        });
        eventSource.onopen = () => {
            clearTimeout(streamWatchdog);
            streamWatchdog = null;
            if (!logReplayDone)
                setTimeout(() => { logReplayDone = true; }, 500);
        };
        eventSource.onerror = () => {
            if (eventSource && eventSource.readyState === EventSource.CLOSED) {
                stopEventStream();
                scheduleStreamRetry();
            } else {
                armStreamWatchdog();
            }
        };
        armStreamWatchdog();
    }

    function stopEventStream() {
        clearTimeout(streamWatchdog);
        streamWatchdog = null;
        if (eventSource) {
            eventSource.close();
            eventSource = null;
        }
    }

    function openSystem(tab) {
        requireAuth(() => _openSystem(tab));
    }

    function _openSystem(tab) {
        loadSystemPanel();
        systemOverlay.classList.add('open');
        switchSystemTab(tab || 'flow', true);
    }

    function closeSystem() {
        if (!confirmDiscardUnsaved('close')) return;
        if (typeof App !== 'undefined' && App.setUnsaved) App.setUnsaved(false);

        stopFlowObserver();
        systemOverlay.classList.remove('open');
        currentSystemTab = null;
        document.querySelectorAll('.hub-button').forEach(btn => btn.classList.remove('active'));
    }

    function switchSystemTab(tab, force) {
        if (!SYSTEM_TABS[tab]) return false;
        if (!force && tab === currentSystemTab) return true;

        // Guard against losing unsaved edits when leaving an editable tab.
        if (!force && hasUnsaved()) {
            if (!confirmDiscardUnsaved('switch')) return false;
            loadedTabs.delete(currentSystemTab);
        }

        if (tab === 'places' && currentSystemTab && currentSystemTab !== 'places') placeReturnTab = currentSystemTab;
        currentSystemTab = tab;
        const grouped = SYSTEM_GROUP.indexOf(tab) !== -1;
        if (grouped) lastSystemLeaf = tab;
        const top = grouped ? 'system' : tab;

        const nav = SYSTEM_TABS[tab].nav;
        document.querySelectorAll('.hub-button').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.view === nav);
        });
        document.querySelectorAll('#system-tabs .sys-tab').forEach(b => {
            const active = (b.dataset.top || b.dataset.tab) === top;
            b.classList.toggle('active', active);
            b.classList.toggle('has-sub', active && TABS_WITH_SUBS.indexOf(top) !== -1);
            if (active) b.scrollIntoView({ block: 'nearest', inline: 'nearest' });
        });
        paintTabCounts();
        // the System pages have their own nav on the left, not a second row of tabs
        systemSubtabs.classList.add('hidden');
        const group = document.getElementById('sys-group');
        if (group) {
            group.classList.toggle('hidden', !grouped);
            group.querySelectorAll('[data-sys-tab]').forEach(b => b.setAttribute('aria-current', String(b.dataset.sysTab === tab)));
            if (grouped) {
                group.querySelector('.st-main__title').textContent = SYSTEM_TABS[tab].label;
                group.querySelector('.st-main__desc').hidden = true; // the title says enough
            }
        }
        systemBody.querySelectorAll('.sys-pane').forEach(p => {
            p.classList.toggle('hidden', p.dataset.pane !== tab);
        });
        const footer = document.getElementById('st-footer');
        if (footer) footer.hidden = !['input', 'output', 'viewer'].includes(tab);

        if (tab === 'input') {
            if (!loadedTabs.has(tab)) { loadedTabs.add(tab); loadSourceConfig(); }
        } else if (tab === 'output') {
            if (!loadedTabs.has(tab)) {
                loadedTabs.add(tab);
                const host = document.getElementById('sys-output-body');
                if (host) renderOutputConfig(host);
            }
        } else if (tab === 'viewer') {
            if (!loadedTabs.has(tab)) { loadedTabs.add(tab); loadViewerConfig(); }
        } else if (tab === 'places') {
            if (!loadedTabs.has(tab)) {
                loadedTabs.add(tab);
                placeEditor?.destroy();
                placeEditor = createPlaceEditor(document.getElementById('sys-places-body'), {onCancelSetup: () => switchSystemTab(placeReturnTab), onEnabled: () => { loadedTabs.clear(); loadedTabs.add('places'); }});
            }
            requestAnimationFrame(() => placeEditor.resize());
        } else if (tab === 'flow') {
            loadDataFlow();
        } else if (tab === 'status') {
            paintStatusCounts();
        } else if (tab === 'config') {
            loadConfigJson();
        } else if (tab === 'log') {
            const box = document.getElementById('log-box');
            if (box) box.scrollTop = box.scrollHeight;
        }
        return true;
    }

    // Cancel throws the edits away: the cached copy is dropped and the tab built again from the file
    function discardSettings() {
        if (!hasUnsaved() || !confirm('Discard your unsaved changes?')) return;
        const tab = currentSystemTab;
        if (tab === 'output') flowOutputTarget = currentOutputType;
        App.setUnsaved(false);
        ConfigStore.invalidate();
        loadedTabs.clear();
        switchSystemTab(tab, true);
    }
    function wireSettingsFooter() {
        const footer = document.getElementById('st-footer');
        if (!footer) return;
        footer.querySelector('[data-st-cancel]').addEventListener('click', discardSettings);
        footer.querySelector('[data-st-save]').addEventListener('click', async e => {
            const b = e.currentTarget;
            b.disabled = true;
            b.textContent = 'Saving…';
            try { await App.saveDirty(); } finally {
                b.textContent = 'Save changes';
                b.disabled = !App.state.unsaved;
                if (currentSystemTab === 'output') ConfigStore.fetch().then(paintOutputNav).catch(() => {});
                paintTabCounts();
            }
        });
    }

    // the Status page: nav, Open log, the receiver button and the bug-report copy
    function wireStatusPane() {
        systemBody.querySelectorAll('[data-sys-tab]').forEach(b =>
            b.addEventListener('click', () => switchSystemTab(b.dataset.sysTab)));
        systemBody.querySelector('[data-open-log]').addEventListener('click', () => switchSystemTab('log'));
        const btn = document.getElementById('hub-engine-btn');
        btn.addEventListener('click', () => {
            const action = engineButtonMode();
            requireAuth(() => {
                btn.disabled = true;
                engineAction(action)
                    .then(() => { pendingApply = false; renderEngineButton(); })
                    .catch(e => { if (e && e.message) App.notify('error', e.message); })
                    .finally(() => { btn.disabled = false; });
            });
        });
        systemBody.querySelector('[data-copy-sysinfo]').addEventListener('click', e => {
            const b = e.currentTarget;
            const lines = [...document.querySelectorAll('#hub-sysinfo > div')]
                .filter(r => !r.hidden).map(r => r.children[0].textContent + ': ' + r.children[1].textContent);
            lines.push('Receiver: ' + ENGINE_STATES[engineStateKey].label);
            const done = ok => { b.textContent = ok ? 'Copied' : 'Copy failed'; setTimeout(() => { b.textContent = 'Copy for bug report'; }, 1600); };
            (navigator.clipboard ? navigator.clipboard.writeText(lines.join('\n')) : Promise.reject())
                .then(() => done(true), () => done(false));
        });
        renderEngineButton();
        paintStatusCounts();
    }

    // a small count after Input and Output in the top tabs: inputs configured, outputs switched on
    function paintTabCounts() {
        ConfigStore.fetch().then(cfg => {
            const counts = { input: (cfg.receiver || []).length, output: collectOutputs(cfg).filter(o => o.active).length };
            Object.keys(counts).forEach(tab => {
                const btn = systemTabs.querySelector('.sys-tab[data-tab="' + tab + '"]');
                if (!btn) return;
                let b = btn.querySelector('.sys-tab__count');
                if (!b) { b = document.createElement('span'); b.className = 'sys-tab__count'; btn.appendChild(b); }
                b.textContent = counts[tab];
            });
        }).catch(() => {});
    }

    function paintStatusCounts() {
        const box = document.getElementById('hub-counts');
        if (!box) return;
        const n = (k, w) => k + ' ' + w + (k === 1 ? '' : 's');
        ConfigStore.fetch().then(cfg => {
            const inputs = (cfg.receiver || []).length;
            const outputs = collectOutputs(cfg).filter(o => o.active).length;
            box.textContent = n(inputs, 'input') + ' and ' + n(outputs, 'output') + ' configured';
        }).catch(() => { box.textContent = ''; });
    }

    function loadSystemPanel() {
        stopFlowObserver();
        if (typeof App !== 'undefined' && App.setUnsaved) App.setUnsaved(false);
        // one fresh config fetch per panel open; the tabs share it from here
        ConfigStore.invalidate();
        loadedTabs.clear();
        placeEditor?.destroy();
        systemBody.innerHTML = `
            <div id="status-message" class="hidden"></div>
            <div class="sys-pane hidden" data-pane="input"><div id="sys-input-body"></div></div>
            <div class="sys-pane hidden" data-pane="output"><div id="sys-output-body"></div></div>
            <div class="sys-pane hidden" data-pane="places"><div id="sys-places-body"></div></div>
            <div class="sys-pane hidden st st-flowpage" data-pane="flow">
                <div class="st-flowpage__intro">
                    <p>Signal routing between inputs and outputs, based on shared zones.</p>
                    <div id="flow-filter" class="st-filter" role="group" aria-label="Highlight zone"></div>
                </div>
                <div id="flow-loading" class="st-flow__note">Loading&hellip;</div>
                <div id="flow-empty" class="st-flow__note hidden">No receivers or outputs configured.</div>
                <div id="flow-patch" class="st-flow hidden">
                    <div><h2 class="st-col__title">Inputs</h2><div id="flow-inputs" class="st-stack"></div></div>
                    <svg id="flow-svg" class="st-routes" aria-hidden="true"></svg>
                    <div><h2 class="st-col__title">Outputs</h2><div id="flow-outputs"></div></div>
                </div>
            </div>
            <div class="sys-pane hidden" data-pane="viewer"><div><div id="viewer-config-container"></div></div></div>
            <div id="sys-group" class="st st-split sys-group hidden">
                <nav class="st-nav" aria-label="System">
                    <span class="st-nav__title">System</span>
                    ${SYSTEM_GROUP.map(t => `<button type="button" class="st-nav__item" data-sys-tab="${t}"><span class="st-nav__label">${SYSTEM_TABS[t].label}</span></button>`).join('')}
                </nav>
                <div class="st-main sys-group__main">
                    <div class="st-main__head"><div><h2 class="st-main__title"></h2><p class="st-main__desc"></p></div></div>
            <div class="sys-pane hidden" data-pane="status">
                <div class="st-sys">
                    <section class="st-sc st-recv">
                        <span class="st-recv__icon" aria-hidden="true"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M4.9 4.9a10 10 0 0 0 0 14.2M19.1 4.9a10 10 0 0 1 0 14.2M7.8 7.8a6 6 0 0 0 0 8.4M16.2 7.8a6 6 0 0 1 0 8.4"/><circle cx="12" cy="12" r="1.6"/></svg></span>
                        <div class="st-recv__body">
                            <div class="st-recv__title"><h3>Receiver</h3><span id="hub-status" class="st-chip st-state st-state--stopped">Checking&hellip;</span></div>
                            <div class="st-recv__line"><span id="hub-counts"></span><span id="hub-uptime"></span><button type="button" class="st-link" data-open-log>Open log</button></div>
                        </div>
                        <button id="hub-engine-btn" type="button" class="st-btn st-btn--primary"></button>
                    </section>
                    <section id="hub-sysinfo-card" class="st-sc" hidden>
                        <div class="st-sc__head"><h3>System</h3><button type="button" class="st-btn st-btn--sm" data-copy-sysinfo>Copy for bug report</button></div>
                        <dl id="hub-sysinfo" class="st-facts"></dl>
                    </section>
                </div>
            </div>
            <div class="sys-pane hidden" data-pane="config">
                <pre id="config-json" class="term-pane">Loading...</pre>
            </div>
            <div class="sys-pane hidden" data-pane="log">
                <div class="log-console">
                    <div class="log-toolbar">
                        <span class="log-prompt">&gt;_</span>
                        <select id="log-level" class="log-control"
                                title="Filters this console; the receiver records down to control.level (debug by default)">
                            <option value="debug">debug</option>
                            <option value="info">info</option>
                            <option value="warning">warning</option>
                            <option value="error">error</option>
                            <option value="critical">critical</option>
                        </select>
                        <input id="log-search" class="log-control log-search" type="search"
                               placeholder="search&hellip;" autocomplete="off" spellcheck="false">
                        <span class="log-dots" aria-hidden="true"><i></i><i></i><i></i></span>
                    </div>
                    <div id="log-box"></div>
                </div>
            </div>
            <div class="sys-pane hidden" data-pane="wizard">
                <div class="sys-pane-inner">
                    <div class="card">
                        <div class="card-header">
                            <span class="t-strong">Setup Wizard</span>
                        </div>
                        <div class="card-body">
                            <p class="t-small t-muted">Step through the guided setup to configure your receiver, sharing and viewer.</p>
                        </div>
                    </div>
                    <div class="row row-wrap row-end row-loose sys-actions">
                        <button id="hub-btn-wizard" class="btn">Open Setup Wizard</button>
                    </div>
                </div>
            </div>
            <div class="sys-pane hidden" data-pane="password">
                <div class="sys-pane-inner">
                    <div class="card">
                        <div class="card-header">
                            <span class="t-strong">Reset Password</span>
                        </div>
                        <div class="card-body">
                            <form id="password-form" class="stack">
                                <input id="new-password" type="password" autocomplete="new-password" placeholder="New password"
                                    class="input" />
                                <input id="new-password2" type="password" autocomplete="new-password" placeholder="Confirm new password"
                                    class="input" />
                            </form>
                            ${auth === 'open' ? '<p class="t-small t-muted">Local access needs no password; this one is used when AIS-catcher is started with LAN access (bind 0.0.0.0).</p>' : ''}
                        </div>
                    </div>
                    <div class="row row-wrap row-end row-loose sys-actions">
                        ${auth === 'open' ? '' : '<button id="hub-btn-logout" type="button" class="btn sys-save">Logout</button>'}
                        <button type="submit" form="password-form" class="btn sys-save">Reset</button>
                    </div>
                </div>
            </div>
            <div class="sys-pane hidden" data-pane="license"></div>
                </div>
            </div>
                    <footer id="st-footer" class="st st-footer" hidden>
                <span class="st-spacer"></span>
                <span class="st-dirty" role="status" hidden>Unsaved changes</span>
                <button class="st-btn" type="button" data-st-cancel>Cancel</button>
                <button class="st-btn st-btn--primary" type="button" data-st-save disabled>Save changes</button>
            </footer>
`;
        wireSettingsFooter();
        wireStatusPane();

        // static license content lives in index.html
        systemBody.querySelector('[data-pane="license"]')
            .appendChild(document.getElementById('license-pane-content').content.cloneNode(true));

        const communityLogo = document.querySelector('#community-link svg');
        const licenseLogo = document.getElementById('license-logo');
        if (communityLogo && licenseLogo) licenseLogo.innerHTML = communityLogo.outerHTML;

        document.getElementById('password-form').addEventListener('submit', changePassword);
        document.getElementById('hub-btn-wizard').addEventListener('click', () => {
            closeSystem();
            openWizard();
        });
        if (auth !== 'open')
            document.getElementById('hub-btn-logout').addEventListener('click', logout);

        const levelSelect = document.getElementById('log-level');
        levelSelect.value = logLevelMin;
        levelSelect.addEventListener('change', () => {
            logLevelMin = levelSelect.value;
            try { localStorage.setItem('hub-log-level', logLevelMin); } catch (_) { }
            renderLogBox();
        });

        const searchInput = document.getElementById('log-search');
        searchInput.value = logSearch;
        searchInput.addEventListener('input', () => {
            logSearch = searchInput.value.trim().toLowerCase();
            renderLogBox();
        });

        refreshEngineStatus();
        renderLogBox();
    }

    // the Map tab: one settings object over five pages; labels name the option, for the owner
    const MAP_LABELS = {
        station: ['Station Name', 'Shown in the viewer'],
        station_link: ['Station Page', 'Opens from the station name'],
        use_gps: ['Use GPS', 'Take the position from a GPS input'],
        share_loc: ['Show Station and Range', 'Station position and reception range on the map'],
        backup: ['Save Every (min)', 'Statistics survive restarts; 0 is off'],
        file: ['File', 'Where the statistics are kept'],
        webcontrol_http: ['Control Link', 'Opens from the viewer menu'],
        split: ['Per-Receiver Views', 'A view for each receiver'],
        realtime: ['Live NMEA', 'Stream received messages to the viewer'],
        msg: ['Last Message', 'Recent NMEA on the ship card'],
        decoder: ['NMEA Decoder', 'A tab to decode messages by hand'],
        log: ['Log Tab', 'Show the receiver log in the viewer'],
        track_memory: ['Track Memory (KB)', 'More memory, longer tracks'],
        track_time: ['Max Track Age (s)', '0 keeps tracks without a time limit'],
        history: ['Hide Ships After (s)', 'Ships silent this long leave the map'],
        expire: ['Expire Data', 'Clear ship details that are not repeated within the timeout'],
        replay: ['Replay', 'Replay recent tracks on the map'],
        geojson: ['GeoJSON', 'Ships as GeoJSON for other tools'],
        prome: ['Prometheus', 'Metrics served at /metrics'],
        places: ['Place Directory', 'Folder with place definitions'],
        plugin_dir: ['Plugin Directory', 'Folder with viewer plugins'],
        context: ['Context', 'Keeps settings of viewers apart']
    };
    const TRACK_PRESETS = [
        { value: 'compact', label: 'Compact', track_memory: 512, track_time: 1800 },
        { value: 'standard', label: 'Standard', track_memory: 1024, track_time: 3600 },
        { value: 'long', label: 'Long', track_memory: 4096, track_time: 21600 }
    ];
    // one "Track length" choice on the right; memory and age only show for Custom
    function trackPresets(node, mgr) {
        const sec = [...node.querySelectorAll('.st-sec')].find(x => x.querySelector('.st-section__title')?.textContent === 'Tracks');
        if (!sec) return;
        const f = n => mgr.fields.find(x => x.name === n);
        // unset means the default, which is Standard
        const val = n => { const v = mgr.fieldValue(mgr.data, f(n)); return Number(v === undefined || v === '' ? f(n).defaultValue : v); };
        const match = TRACK_PRESETS.find(p => p.track_memory === val('track_memory') && p.track_time === val('track_time'));
        const custom = mgr.trackCustom || !match;
        const select = document.createElement('select');
        select.className = 'select';
        TRACK_PRESETS.concat([{ value: 'custom', label: 'Custom' }]).forEach(p => {
            const o = document.createElement('option');
            o.value = p.value;
            o.textContent = p.label;
            select.appendChild(o);
        });
        select.value = custom ? 'custom' : match.value;
        select.addEventListener('change', () => {
            const p = TRACK_PRESETS.find(x => x.value === select.value);
            mgr.trackCustom = !p;
            if (p) {
                mgr.updateValue(0, f('track_memory'), p.track_memory);
                mgr.updateValue(0, f('track_time'), p.track_time);
            }
            mgr.render();
        });
        const row = document.createElement('div');
        row.className = 'field st-opt';
        row.innerHTML = '<label class="field-label">Track Length</label><p class="field-desc">Longer tracks and replay use more memory</p>';
        row.appendChild(select);
        // inside the grid, so the section counts it and stays visible when the two values are hidden
        sec.querySelector('.st-sec__grid').prepend(row);
        ['track_memory', 'track_time'].forEach(n => sec.querySelector(`[data-st-field="${n}"]`)?.classList.toggle('hidden', !custom));
    }
    const MAP_PAGES = [
        { key: 'general', label: 'General', sub: 'Station and location', desc: 'The basics most stations need. Everything else has sensible defaults.',
          fields: ['station', 'station_link', 'use_gps', 'lat', 'lon', 'share_loc'],
          sections: { station: 'Station', station_link: 'Station', use_gps: 'Location', lat: 'Location', lon: 'Location', share_loc: 'Location' } },
        { key: 'viewer', label: 'Features', sub: 'Tabs and extras', desc: 'Extra tabs and live data in the web viewer.',
          fields: ['split', 'realtime', 'msg', 'decoder', 'log', 'webcontrol_http', 'geojson', 'prome'],
          sections: { split: 'Features', realtime: 'Features', msg: 'Features', decoder: 'Features', log: 'Features', webcontrol_http: 'Links', geojson: 'Endpoints', prome: 'Endpoints' } },
        { key: 'tracks', label: 'Tracks & Data', sub: 'History and retention', desc: 'How long ships and their tracks stay on the map.',
          fields: ['track_memory', 'track_time', 'history', 'expire', 'replay'],
          sections: { track_memory: 'Tracks', track_time: 'Tracks', history: 'Retention', expire: 'Retention', replay: 'Replay' },
          extra: trackPresets },
        { key: 'tiles', label: 'Map Tiles', sub: 'MBTiles and tile folders', desc: 'Your own base maps and overlays, served from this machine.',
          fields: ['mbtiles', 'mboverlay', 'fstiles', 'fsoverlay'] },
        { key: 'server', label: 'Server', sub: 'Endpoints, backup, storage, zones', desc: 'Data endpoints, where files live, and which inputs the map shows.',
          fields: ['backup', 'file', 'places', 'plugin_dir', 'context', 'zone'],
          sections: { backup: 'Backup', file: 'Backup', places: 'Storage', plugin_dir: 'Storage', context: 'Storage', zone: 'Zones' } }
    ];

    function loadViewerConfig() {
        // every viewer setting except the port, which follows the control port
        const keys = ['station', 'station_link', 'webcontrol_http',
                      'lat', 'lon', 'share_loc', 'use_gps',
                      'history', 'track_memory', 'track_time', 'expire',
                      'replay', 'split',
                      'file', 'backup',
                      'places', 'plugin_dir', 'context',
                      'mbtiles', 'mboverlay', 'fstiles', 'fsoverlay',
                      'realtime', 'msg', 'decoder', 'log', 'geojson', 'prome',
                      'zones'];
        const schema = {};
        keys.forEach(k => {
            schema[k] = Object.assign({}, webviewerSchema[k]);
            const l = MAP_LABELS[k];
            if (l) { schema[k].label = l[0]; schema[k].tooltip = l[1]; }
        });
        const keepPage = ConfigManagers?.get('viewer-config-container')?.page;
        const mgr = createSimpleConfigManager({
            schema: schema,
            containerId: 'viewer-config-container',
            nestedPath: ['control', 'viewer'],
            title: 'Viewer',
            variant: 'pages',
            pages: MAP_PAGES
        });
        if (keepPage && mgr) mgr.page = keepPage;
    }

    function highlightJson(text) {
        const esc = text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
        return esc.replace(/("(?:\\.|[^"\\])*")(\s*:)?|\b(?:true|false|null)\b|-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?/g, (m, str, colon) => {
            if (str) return colon ? `<span class="j-key">${str}</span>${colon}` : `<span class="j-str">${str}</span>`;
            if (m === 'true' || m === 'false') return `<span class="j-bool">${m}</span>`;
            if (m === 'null') return `<span class="j-null">${m}</span>`;
            return `<span class="j-num">${m}</span>`;
        });
    }

    window.highlightJson = highlightJson;

    function loadConfigJson() {
        const pre = document.getElementById('config-json');
        ConfigStore.fetch()
            .then(cfg => { pre.innerHTML = highlightJson(JSON.stringify(cfg, null, 2)); })
            .catch(() => { pre.textContent = 'Could not load the configuration.'; });
    }

    // Data Flow tab: a patch-bay view of signal routing. Inputs (left) and
    // outputs (right); an output with zones is drawn connected to every input
    // sharing one, an output without zones takes every input and says so on a
    // chip instead of a line. Zone colours match the config chips.
    const SVG_NS = 'http://www.w3.org/2000/svg';
    function flowChip(zone) {
        const s = document.createElement('span');
        s.className = 'st-chip st-chip--zone';
        s.style.setProperty('--z', ZoneColors.css(zone));
        s.textContent = zone;
        return s;
    }

    function safeLink(url) {
        return url && /^https?:\/\//i.test(url) ? url : null;
    }

    // one card: name and status on top, an optional subtitle, then the zones and the live figures
    function flowNode({ name, sub, zones, isInput, onClick, link }) {
        const card = document.createElement('article');
        const zoned = zones && zones.length > 0;
        card.className = 'st-node ' + (isInput ? 'st-node--in' : 'st-node--out') + (!isInput && zoned ? ' st-node--zone' : '');
        card.setAttribute('role', 'button');
        card.tabIndex = 0;
        card.addEventListener('click', onClick);
        card.addEventListener('keydown', e => {
            if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); onClick(e); }
        });
        const top = document.createElement('div');
        top.className = 'st-node__top';
        const url = safeLink(link);
        const nm = document.createElement(url ? 'a' : 'span');
        nm.className = 'st-node__name';
        nm.textContent = name;
        top.appendChild(nm);
        if (url) {
            nm.href = url; nm.target = '_blank'; nm.rel = 'noopener'; nm.title = url;
            nm.addEventListener('click', e => e.stopPropagation());
            top.insertAdjacentHTML('beforeend', '<svg class="st-node__link" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-label="has link"><path d="M7 17L17 7M9 7h8v8"/></svg>');
        }
        const spacer = document.createElement('span');
        spacer.className = 'st-spacer';
        const status = document.createElement('span');
        top.append(spacer, status);
        card.appendChild(top);
        if (sub) {
            const s = document.createElement('div');
            s.className = 'st-node__sub';
            s.textContent = sub;
            card.appendChild(s);
        }
        const bottom = document.createElement('div');
        bottom.className = 'st-node__bottom';
        if (zoned) zones.forEach(z => bottom.appendChild(flowChip(z)));
        else if (!isInput) {
            const all = document.createElement('span');
            all.className = 'st-chip st-chip--all';
            all.textContent = 'All inputs';
            bottom.appendChild(all);
        }
        const stats = document.createElement('span');
        stats.className = 'st-node__stats';
        bottom.appendChild(stats);
        card.appendChild(bottom);
        return { card, status, stats };
    }

    function setFlowStatus(el, tone, text) {
        el.className = 'st-status st-status--' + tone;
        el.textContent = text;
    }

    // one curve per input and zone to every output in that zone, from the card edges
    function drawFlowConnections(routes, svg) {
        svg.innerHTML = '';
        const box = svg.getBoundingClientRect();
        const graph = svg.parentElement.getBoundingClientRect();
        svg.setAttribute('height', graph.height);
        svg.style.height = graph.height + 'px';
        const w = box.width;
        routes.forEach(({ from, to, zone }) => {
            const a = from.getBoundingClientRect(), b = to.getBoundingClientRect();
            if (!a.height || !b.height) return;
            const y1 = a.top + a.height / 2 - box.top, y2 = b.top + b.height / 2 - box.top;
            const p = document.createElementNS(SVG_NS, 'path');
            p.setAttribute('d', `M0 ${y1} C${w / 2} ${y1} ${w / 2} ${y2} ${w} ${y2}`);
            p.setAttribute('class', 'st-route');
            p.style.stroke = ZoneColors.css(zone);
            p.dataset.zone = zone;
            svg.appendChild(p);
        });
        applyFlowFilter();
    }

    // the zone filter: everything outside the chosen zone fades
    let flowZone = '';
    function applyFlowFilter() {
        document.querySelectorAll('#flow-filter .st-filter__btn').forEach(b => b.setAttribute('aria-pressed', String(b.dataset.zone === flowZone)));
        document.querySelectorAll('#flow-patch .st-node').forEach(n => {
            const zones = (n.dataset.zones || '').split(' ').filter(Boolean);
            n.dataset.dim = String(!!flowZone && (n.classList.contains('st-node--in') || n.classList.contains('st-node--zone')) && !zones.includes(flowZone));
        });
        document.querySelectorAll('#flow-svg .st-route').forEach(r => { r.dataset.dim = String(!!flowZone && r.dataset.zone !== flowZone); });
    }

    function flowOutputLabel(type, item) {
        if (item && item.description) return `${type} · ${item.description}`;
        return channelTitle(type, item);
    }

    function stopFlowObserver() {
        if (flowResizeObserver) {
            flowResizeObserver.disconnect();
            flowResizeObserver = null;
        }
        if (flowStatsTimer) {
            clearInterval(flowStatsTimer);
            flowStatsTimer = null;
        }
    }

    function formatBytes(b) {
        if (!b || b < 0) return '0 B';
        const units = ['B', 'KB', 'MB', 'GB', 'TB'];
        let i = 0;
        while (b >= 1024 && i < units.length - 1) { b /= 1024; i++; }
        return (i === 0 ? b : b.toFixed(1)) + ' ' + units[i];
    }

    // stat.json output types -> Data Flow node sub types
    const FLOW_STAT_TYPES = {};
    OUTPUT_TYPES.forEach(o => {
        if (o.statType) FLOW_STAT_TYPES[o.statType] = o.value;
        (o.statTypes || []).forEach(t => { FLOW_STAT_TYPES[t] = o.value; });
    });

    function flowStatParts(sub, s) {
        const parts = [`${formatBytes(s.bytes_out)} out`];
        if (s.bytes_in > 0) parts.push(`${formatBytes(s.bytes_in)} in`);
        if (sub !== 'udp') parts.push(`ok/fail ${s.connect_ok}/${s.connect_fail}`);
        if (s.reconnects > 0) parts.push(`${s.reconnects} reconnects`);
        if (s.dropped > 0) parts.push(`${s.dropped} dropped`);
        return parts;
    }
    function putStats(el, parts) {
        el.replaceChildren(...parts.map(t => { const n = document.createElement('span'); n.className = 'st-num'; n.textContent = t; return n; }));
    }

    function paintFlowInput(n) {
        if (!n.active) setFlowStatus(n.status, 'off', 'Off');
        else if (engineRunning) setFlowStatus(n.status, 'ok', 'Running');
        else setFlowStatus(n.status, 'off', 'Stopped');
    }
    function paintFlowOutput(o) {
        setFlowStatus(o.status, o.active ? 'ok' : 'off', o.active ? 'Active' : 'Off');
    }

    function updateFlowStats(outputs, inputs = []) {
        // the receiver can stop or start while the page is open: the inputs follow it
        inputs.forEach(paintFlowInput);
        if (!port || !engineRunning) {
            // nothing is connected while it is stopped: back to what the configuration says
            outputs.forEach(o => { putStats(o.stats, []); paintFlowOutput(o); });
            return;
        }
        fetch(VIEWER_PATH + 'api/output_stats.json')
            .then(r => { if (!r.ok) throw new Error(); return r.json(); })
            .then(stat => {
                const pools = {};
                (stat.outputs || []).forEach(o => {
                    const sub = o.description === 'Community Feed' ? 'sharing' : FLOW_STAT_TYPES[o.type];
                    if (sub) (pools[sub] = pools[sub] || []).push(o);
                });
                outputs.forEach(o => {
                    if (o.sub === 'server') {
                        putStats(o.stats, [`${stat.tcp_clients} connection${stat.tcp_clients === 1 ? '' : 's'}`]);
                        return;
                    }
                    if (!o.active) { putStats(o.stats, []); return; }
                    const m = pools[o.sub] && pools[o.sub].shift();
                    if (!m) { putStats(o.stats, []); return; }
                    putStats(o.stats, flowStatParts(o.sub, m.stats));
                    // a connection type says whether it is connected; UDP and HTTP have no connection to report
                    if (o.sub !== 'udp' && o.sub !== 'http')
                        setFlowStatus(o.status, m.stats.connected ? 'ok' : 'bad', m.stats.connected ? 'Connected' : 'Not connected');
                });
            })
            .catch(() => outputs.forEach(o => putStats(o.stats, [])));
    }

    function engineTypeLabel(type) {
        const opt = receiverSchema.engines.options.find(o => o.value === (type || 'auto'));
        return opt ? opt.label : (type || 'auto').replace(/_/g, ' ');
    }

    // every output a receiver can feed, with its zones: what Flow draws and the routing line counts
    function collectOutputs(cfg) {
        const outputs = [];
        const mapCfg = (cfg.control && cfg.control.viewer) || null;
        if (mapCfg)
            outputs.push({ name: 'Viewer', zones: Array.isArray(mapCfg.zone) ? mapCfg.zone : [], active: true, tab: 'viewer' });
        if (cfg.sharing !== undefined)
            outputs.push({ name: 'Community', zones: Array.isArray(cfg.sharing_zone) ? cfg.sharing_zone : [], active: cfg.sharing === true, sub: 'sharing' });
        // stdout takes every receiver; "0" and "none" are the silent levels
        if (cfg.screen !== undefined && !['0', 'none'].includes(String(cfg.screen).toLowerCase()))
            outputs.push({ name: 'Screen', zones: [], active: true, sub: 'screen' });
        OUTPUT_TYPES.forEach(({ value, configKey, flowLabel }) => {
            if (!configKey) return;
            (cfg[configKey] || []).forEach(item => {
                outputs.push({ name: flowOutputLabel(flowLabel, item), zones: item.zone || [], active: item.active !== false, sub: value, link: item.link });
            });
        });
        return outputs;
    }
    // how many switched-on outputs take this input: those sharing a zone with it, and those with no zones at all
    function inputRouting(item) {
        const zones = Array.isArray(item.zone) ? item.zone : [];
        return ConfigStore.fetch().then(cfg => collectOutputs(cfg).filter(o => o.active &&
            (!o.zones.length || o.zones.some(z => zones.includes(z)))).length).catch(() => null);
    }

    // how many switched-on inputs feed this output: those sharing a zone with it, or all when it has none
    function outputRouting(item) {
        const zones = Array.isArray(item.zone) ? item.zone : Array.isArray(item.sharing_zone) ? item.sharing_zone : [];
        return ConfigStore.fetch().then(cfg => (cfg.receiver || []).filter(r => r.active !== false &&
            (!zones.length || (r.zone || []).some(z => zones.includes(z)))).length).catch(() => null);
    }

    function loadDataFlow() {
        stopFlowObserver();
        const renderId = ++flowRenderId;
        const patchEl = document.getElementById('flow-patch');
        if (!patchEl) return;
        const loadingEl = document.getElementById('flow-loading');
        const emptyEl = document.getElementById('flow-empty');
        const inputsEl = document.getElementById('flow-inputs');
        const outputsEl = document.getElementById('flow-outputs');
        const filterEl = document.getElementById('flow-filter');
        const svg = document.getElementById('flow-svg');

        inputsEl.innerHTML = '';
        outputsEl.innerHTML = '';
        filterEl.innerHTML = '';
        svg.innerHTML = '';
        emptyEl.classList.add('hidden');
        patchEl.classList.add('hidden');
        loadingEl.textContent = 'Loading…';
        loadingEl.classList.remove('hidden');

        ConfigStore.fetch()
            .then(cfg => {
                if (currentSystemTab !== 'flow' || renderId !== flowRenderId) return;
                // one node per engine, with the receiver's zones plus its own
                const receivers = [];
                (cfg.receiver || []).forEach((item, i) => {
                    const d = describeInput(item, i, cfg.receiver);
                    const named = (item.description || '').trim();
                    const fallback = named ? d.sub : '';
                    const engines = SDR_INPUTS.has(item.input) && Array.isArray(item.engines) && item.engines.length ? item.engines : [{}];
                    engines.forEach(e => receivers.push({
                        name: d.title,
                        sub: [fallback, engines.length > 1 || e.type ? engineTypeLabel(e.type) : ''].filter(Boolean).join(' · '),
                        zones: [...new Set([...(item.zone || []), ...(Array.isArray(e.zone) ? e.zone : [])])],
                        active: item.active !== false
                    }));
                });

                const outputs = collectOutputs(cfg);
                loadingEl.classList.add('hidden');
                if (receivers.length === 0 && outputs.length === 0) {
                    emptyEl.classList.remove('hidden');
                    return;
                }
                patchEl.classList.remove('hidden');

                // the filter: one button per zone in use, in its colour
                const allZones = new Set();
                [...receivers, ...outputs].forEach(n => n.zones.forEach(z => allZones.add(z)));
                if (!allZones.has(flowZone)) flowZone = '';
                filterEl.hidden = allZones.size === 0;
                const filterBtn = (zone, label) => {
                    const b = document.createElement('button');
                    b.type = 'button';
                    b.className = 'st-filter__btn';
                    b.dataset.zone = zone;
                    if (zone) { const dot = document.createElement('i'); dot.style.background = ZoneColors.css(zone); b.appendChild(dot); }
                    b.appendChild(document.createTextNode(label));
                    b.addEventListener('click', () => { flowZone = zone; applyFlowFilter(); });
                    filterEl.appendChild(b);
                };
                filterBtn('', 'All zones');
                allZones.forEach(z => filterBtn(z, z));

                const inputNodes = receivers.map(r => {
                    const n = flowNode({ ...r, isInput: true, onClick: () => switchSystemTab('input') });
                    n.card.dataset.zones = r.zones.join(' ');
                    const node = { ...r, ...n };
                    paintFlowInput(node);
                    inputsEl.appendChild(n.card);
                    return node;
                });

                // zone-routed outputs first, with lines; then the ones that take every input
                const zoned = outputs.filter(o => o.zones.length), all = outputs.filter(o => !o.zones.length);
                const outputNodes = [];
                const group = (title, list) => {
                    if (!list.length) return;
                    const h = document.createElement('div');
                    h.className = 'st-col__group';
                    h.textContent = title;
                    const stack = document.createElement('div');
                    stack.className = 'st-stack st-stack--out';
                    list.forEach(o => {
                        const open = o.tab ? () => switchSystemTab(o.tab) : () => selectOutputTab(o.sub);
                        const n = flowNode({ name: o.name, zones: o.zones, isInput: false, onClick: open, link: o.link });
                        n.card.dataset.zones = o.zones.join(' ');
                        const node = { ...o, ...n };
                        paintFlowOutput(node);
                        stack.appendChild(n.card);
                        outputNodes.push(node);
                    });
                    outputsEl.append(h, stack);
                };
                group('Zone routed', zoned);
                group('Receive all inputs', all);

                updateFlowStats(outputNodes, inputNodes);
                flowStatsTimer = setInterval(() => {
                    if (currentSystemTab === 'flow') updateFlowStats(outputNodes, inputNodes);
                }, 5000);

                const routes = [];
                inputNodes.forEach(i => i.zones.forEach(z =>
                    outputNodes.forEach(o => { if (o.zones.includes(z)) routes.push({ from: i.card, to: o.card, zone: z }); })));
                const redraw = () => drawFlowConnections(routes, svg);
                requestAnimationFrame(redraw);
                stopFlowObserver();
                flowResizeObserver = new ResizeObserver(redraw);
                flowResizeObserver.observe(patchEl);
            })
            .catch(() => {
                if (currentSystemTab !== 'flow' || renderId !== flowRenderId) return;
                loadingEl.textContent = 'Failed to load configuration.';
                loadingEl.classList.remove('hidden');
            });
    }

    function changePassword(e) {
        e.preventDefault();
        const p1 = document.getElementById('new-password').value;
        const p2 = document.getElementById('new-password2').value;
        if (p1 !== p2) {
            App.notify('error', 'Passwords do not match');
            return;
        }
        postPassword('/api/password', p1)
            .then(() => {
                hasPassword = true;
                App.notify('success', 'Password changed');
                document.getElementById('password-form').reset();
            })
            .catch(err => App.notify('error', err.message || 'Failed to change password'));
    }

    function logout() {
        fetch('/api/logout', { method: 'POST' }).then(() => window.location.reload());
    }

    function init() {
        document.getElementById('login-form').addEventListener('submit', submitLogin);
        document.getElementById('login-cancel').addEventListener('click', closeLoginModal);
        document.getElementById('nav-start-restart').addEventListener('click', onStartRestartClick);
        document.getElementById('nav-btn-input').addEventListener('click', () => openSystem('input'));
        document.getElementById('nav-btn-output').addEventListener('click', () => openSystem('output'));
        document.getElementById('nav-btn-control').addEventListener('click', () => openSystem(lastSystemLeaf));
        document.getElementById('status-dot-wrap').addEventListener('click', () => openSystem('log'));
        document.getElementById('login-pill').addEventListener('click', () => { pendingAction = null; openLoginModal(); });
        document.getElementById('bar-collapse').addEventListener('click', () => setBarCollapsed(true));
        document.getElementById('bar-restore').addEventListener('click', () => setBarCollapsed(false));
        document.getElementById('system-close').addEventListener('click', closeSystem);
        const headerSave = document.getElementById('system-save');
        headerSave.addEventListener('click', async () => {
            headerSave.disabled = true;
            try { if (currentSystemTab === 'places') await placeEditor.save(); else await App.saveDirty(); } finally { headerSave.disabled = false; }
        });
        systemOverlay.addEventListener('click', e => {
            if (e.target === systemOverlay) closeSystem();
        });
        systemTabs.addEventListener('click', e => {
            const btn = e.target.closest('.sys-tab');
            if (btn) switchSystemTab(btn.dataset.top === 'system' ? lastSystemLeaf : btn.dataset.tab);
        });
        systemSubtabs.addEventListener('click', e => {
            const btn = e.target.closest('.sys-tab');
            if (btn) switchSystemTab(btn.dataset.tab);
        });
        enableTabScroll(systemTabs);
        enableTabScroll(systemSubtabs);
        document.addEventListener('keydown', e => {
            if (e.key === 'Escape' && systemOverlay.classList.contains('open') &&
                !document.getElementById('login-overlay').classList.contains('open') &&
                !document.querySelector('.modal-overlay:not(.hidden)') &&
                !document.querySelector('#wizard-overlay.open')) closeSystem();
        });
        document.addEventListener('visibilitychange', () => {
            if (document.hidden) {
                cancelStreamRetry();
                stopEventStream();
            } else {
                refreshEngineStatus();
                startEventStream();
            }
        });
        setInterval(() => {
            if (document.hidden) return;
            updateUptimeDisplay();
            if (reloadUntil && Date.now() > reloadUntil) restartTimedOut();
        }, 1000);

        fetchStatus()
            .then(data => {
                auth = data.auth;
                hasPassword = !!data.has_password || auth === 'login';
                port = data.viewer || 0;
                renderEngineState(data);
                updateBarVisibility();
                startEventStream();

                // the wizard leads with its password step; without a wizard
                // run, a missing password is prompted via the modal instead
                if (data.wizard && (isLoggedIn() || auth === 'setup')) {
                    openWizard();
                } else if (passwordSetupMode()) {
                    openLoginModal();
                }

                if (port)
                    loadWebviewer();
                else if (auth !== 'setup') {
                    showNoViewer();
                }
            })
            .catch(() => {
                showError('Connection Error', 'Cannot reach the control server.');
                scheduleStreamRetry();
            });
    }

    document.addEventListener('DOMContentLoaded', init);
})();
