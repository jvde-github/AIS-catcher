
(function () {
    'use strict';

    let auth = 'login';
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

    function renderEngineButton() {
        const mode = engineButtonMode();
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
    const systemOverlay = document.getElementById('system-overlay');
    const systemBody = document.getElementById('system-body');
    const systemTabs = document.getElementById('system-tabs');
    const systemSubtabs = document.getElementById('system-subtabs');
    let currentSystemTab = null;
    const loadedTabs = new Set();
    let flowResizeObserver = null;
    let flowRenderId = 0;
    let flowStatsTimer = null;

    // tab id -> header label / which nav-bar button to highlight
    const SYSTEM_TABS = {
        input: { label: 'Input', nav: 'input' },
        output: { label: 'Output', nav: 'output' },
        flow: { label: 'Flow', nav: 'control-panel' },
        status: { label: 'Status', nav: 'control-panel' },
        viewer: { label: 'Map', nav: 'control-panel' },
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
        if (hubUptime) hubUptime.textContent = up ? 'Uptime: ' + up : '';
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
            hubStatus.innerHTML = '<span class="t-small ' + s.text + '">' + s.label + '</span>' +
                '<span class="relative row">' +
                '<span class="dot ' + s.dot + (running ? ' dot-pulse' : '') + '"></span></span>';
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
                rows.forEach(r => sysinfo.appendChild(
                    Utils.el('div', 'row row-between row-loose', {},
                        Utils.el('span', 't-muted', {}, r[0]),
                        Utils.el('span', 't-strong t-right t-truncate', {}, r[1])
                    )
                ));
                sysinfo._memEl = Utils.el('span', 't-strong t-right t-truncate', {}, '');
                sysinfo._memRow = Utils.el('div', 'row row-between row-loose hidden', {},
                    Utils.el('span', 't-muted', {}, 'Memory usage'), sysinfo._memEl);
                sysinfo.appendChild(sysinfo._memRow);
                const card = document.getElementById('hub-sysinfo-card');
                if (card) card.classList.remove('hidden');
            }
            sysinfo._memEl.textContent = data.memory ? formatBytes(data.memory) : '';
            sysinfo._memRow.classList.toggle('hidden', !data.memory);
        }
    }

    let reloadUntil = 0;
    let reloadSawDown = false;
    let overlayRestartBtn = null;
    let pendingViewerReload = false;
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
        const reloadFrame = pendingViewerReload && fell;
        if (reloadFrame) pendingViewerReload = false;

        if (data.viewer && (!viewerLoaded || data.viewer !== port)) {
            port = data.viewer;
            clearOverlayMessages();
            loadWebviewer();
        } else if (reloadFrame && viewerLoaded) {
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

    window.hubConfigSaved = function (kind, opts) {
        // a running receiver picks the settings up when it restarts
        if (opts && opts.reloadWebviewer) pendingViewerReload = true;
        if (opts && opts.restartWebviewer) {
            if (engineRunning) pendingViewerReload = true;
            else if (viewerLoaded) loadWebviewer();
        }

        if (kind === 'viewer' && !engineRunning) {
            App.notify('info', 'Map viewer settings applied', 5000);
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
        iframe.src = iframe.src;
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
            title: 'Receiver'
        });
    }

    const OUTPUT_TYPES = [
        { value: 'sharing', label: 'Community', schema: sharingSchema },
        ...CHANNEL_REGISTRY.filter(c => c.key !== 'receiver').map(c => ({
            value: c.key === 'tcp_listener' ? 'tcp-server' : c.key,
            label: c.label, schema: c.schema, configKey: c.configKey,
            flowLabel: c.flowLabel, statType: c.statType, statTypes: c.statTypes
        }))
    ];

    const OUTPUT_TAB_ACTIVE = 'sys-tab active';
    const OUTPUT_TAB_INACTIVE = 'sys-tab';

    function enableTabScroll(el) {
        return window.AISComponents.tabScroller(el);
    }

    function confirmDiscardUnsaved(verb) {
        if (typeof App === 'undefined' || !App.state || !App.state.unsaved) return true;
        if (!confirm(`You have unsaved changes. Are you sure you want to ${verb} without saving?`)) return false;
        App.setUnsaved(false);
        return true;
    }

    function setOutputType(value) {
        if (value === currentOutputType) return;
        if (!confirmDiscardUnsaved('switch')) return;
        currentOutputType = value;
        document.querySelectorAll('[data-output-type]').forEach(b => {
            b.className = b.dataset.outputType === value ? OUTPUT_TAB_ACTIVE : OUTPUT_TAB_INACTIVE;
            if (b.dataset.outputType === value) b.scrollIntoView({ block: 'nearest', inline: 'nearest' });
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
        const wrapper = document.createElement('div');
        wrapper.className = 'stack stack-loose';

        const tabBar = document.createElement('div');
        tabBar.className = 'sys-subtabs';

        OUTPUT_TYPES.forEach(({ value, label }) => {
            const btn = document.createElement('button');
            btn.type = 'button';
            btn.dataset.outputType = value;
            btn.textContent = label;
            btn.className = OUTPUT_TAB_INACTIVE;
            btn.addEventListener('click', () => setOutputType(value));
            tabBar.appendChild(btn);
        });

        const container = document.createElement('div');
        container.id = 'hub-output-container';
        wrapper.appendChild(tabBar);
        wrapper.appendChild(container);
        host.appendChild(wrapper);
        enableTabScroll(tabBar);

        setOutputType(initial);
    }

    function updateOutputView() {
        const t = OUTPUT_TYPES.find(o => o.value === currentOutputType);
        if (!t) return;

        if (!t.configKey) {
            createSimpleConfigManager({
                schema: t.schema,
                containerId: 'hub-output-container'
            });
        } else {
            createChannelManager({
                channelType: t.configKey,
                schema: t.schema,
                containerId: 'hub-output-container',
                title: t.label
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
        if (!force && typeof App !== 'undefined' && App.state && App.state.unsaved) {
            if (!confirmDiscardUnsaved('switch')) return false;
            loadedTabs.delete(currentSystemTab);
        }

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
        systemSubtabs.classList.toggle('hidden', !grouped);
        systemSubtabs.querySelectorAll('.sys-tab').forEach(b => {
            const active = b.dataset.tab === tab;
            b.classList.toggle('active', active);
            if (active && grouped) b.scrollIntoView({ block: 'nearest', inline: 'nearest' });
        });
        systemBody.querySelectorAll('.sys-pane').forEach(p => {
            p.classList.toggle('hidden', p.dataset.pane !== tab);
        });

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
        } else if (tab === 'flow') {
            loadDataFlow();
        } else if (tab === 'config') {
            loadConfigJson();
        } else if (tab === 'log') {
            const box = document.getElementById('log-box');
            if (box) box.scrollTop = box.scrollHeight;
        }
        return true;
    }

    function loadSystemPanel() {
        stopFlowObserver();
        if (typeof App !== 'undefined' && App.setUnsaved) App.setUnsaved(false);
        // one fresh config fetch per panel open; the tabs share it from here
        ConfigStore.invalidate();
        loadedTabs.clear();
        systemBody.innerHTML = `
            <div id="status-message" class="hidden"></div>
            <div class="sys-pane hidden" data-pane="input"><div id="sys-input-body"></div></div>
            <div class="sys-pane hidden" data-pane="output"><div id="sys-output-body"></div></div>
            <div class="sys-pane hidden" data-pane="flow">
                <p class="t-small t-muted">Signal routing between inputs and outputs based on shared zones.</p>
                <div id="flow-loading" class="t-center t-subtle sys-empty">Loading&hellip;</div>
                <div id="flow-empty" class="t-center t-subtle sys-empty hidden">No receivers or outputs configured.</div>
                <div id="flow-patch" class="hidden stack-loose">
                    <div id="flow-legend" class="row row-wrap"></div>
                    <div class="col-header sys-flow-grid">
                        <div>Input</div><div></div><div>Output</div>
                    </div>
                    <div id="flow-graph" class="relative" style="min-height:60px">
                        <div class="sys-flow-grid">
                            <div id="flow-inputs" class="col col-loose"></div>
                            <div></div>
                            <div id="flow-outputs" class="col col-loose"></div>
                        </div>
                        <svg id="flow-svg" class="sys-overlay-fill" style="width:100%;overflow:visible"></svg>
                    </div>
                </div>
            </div>
            <div class="sys-pane hidden" data-pane="status">
                <div class="stack stack-loose sys-narrow">
                    <div class="box stack-tight">
                        <div class="row row-between">
                            <span class="t-strong">Receiver</span>
                            <div id="hub-status" class="row">
                                <span class="t-small t-muted">Checking...</span>
                            </div>
                        </div>
                        <div id="hub-uptime" class="t-muted"></div>
                    </div>
                    <div id="hub-sysinfo-card" class="box hidden stack">
                        <div class="t-strong">System</div>
                        <div id="hub-sysinfo" class="stack stack-tight t-small"></div>
                    </div>
                </div>
            </div>
            <div class="sys-pane hidden" data-pane="viewer"><div><div id="viewer-config-container"></div></div></div>
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
        `;

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

    function loadViewerConfig() {
        // every viewer setting except the port, which follows the control port
        const keys = ['station', 'station_link', 'webcontrol_http',
                      'lat', 'lon', 'share_loc', 'use_gps',
                      'history', 'track_memory', 'track_time', 'expire',
                      'replay', 'split',
                      'file', 'backup',
                      'plugin_dir', 'context',
                      'realtime', 'msg', 'decoder', 'log', 'geojson', 'prome',
                      'zones'];
        const schema = {};
        keys.forEach(k => { schema[k] = Object.assign({}, webviewerSchema[k]); });
        schema.use_gps.label = 'GPS';
        createSimpleConfigManager({
            schema: schema,
            containerId: 'viewer-config-container',
            nestedPath: ['control', 'viewer'],
            title: 'Map'
        });
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
    // outputs (right) are linked wherever they share a zone name; outputs with
    // no zone receive from every input. Zone colours match the config chips.
    function flowBadge(zone) {
        const s = document.createElement('span');
        s.className = `chip ${ZoneColors.badge(zone)}`;
        s.textContent = zone;
        return s;
    }

    function safeLink(url) {
        return url && /^https?:\/\//i.test(url) ? url : null;
    }

    function flowNode(label, zones, active, isInput, onClick, link) {
        const border = active ? 'sys-node-on' : 'sys-node-off';
        const div = document.createElement('div');
        div.setAttribute('role', 'button');
        div.tabIndex = 0;
        div.className = `box sys-flow-node stack-tight ${border}`;
        div.addEventListener('click', onClick);
        div.addEventListener('keydown', e => {
            if (e.key === 'Enter' || e.key === ' ') {
                e.preventDefault();
                onClick(e);
            }
        });

        const url = safeLink(link);
        const lbl = document.createElement(url ? 'a' : 'div');
        lbl.className = 't-small t-strong t-truncate';
        lbl.textContent = label;
        if (url) {
            lbl.href = url;
            lbl.target = '_blank';
            lbl.rel = 'noopener';
            lbl.title = url;
            lbl.addEventListener('click', e => e.stopPropagation());
            const icon = document.createElement('span');
            icon.className = 't-subtle sys-inline';
            icon.innerHTML = '<svg xmlns="http://www.w3.org/2000/svg" width="12" height="12" viewBox="0 -960 960 960" fill="currentColor"><path d="M200-120q-33 0-56.5-23.5T120-200v-560q0-33 23.5-56.5T200-840h280v80H200v560h560v-280h80v280q0 33-23.5 56.5T760-120H200Zm188-212-56-56 372-372H520v-80h320v320h-80v-184L388-332Z"/></svg>';
            lbl.appendChild(icon);
        }
        div.appendChild(lbl);

        if (zones && zones.length > 0) {
            const row = document.createElement('div');
            row.className = 'row row-wrap row-tight';
            zones.forEach(z => row.appendChild(flowBadge(z)));
            div.appendChild(row);
        } else if (!isInput) {
            const note = document.createElement('div');
            note.className = 't-small t-subtle t-italic';
            note.textContent = 'all inputs';
            div.appendChild(note);
        }
        return div;
    }

    function drawFlowConnections(inputEls, outputEls, connections, svg, graphEl) {
        svg.innerHTML = '';
        svg.setAttribute('height', graphEl.offsetHeight);
        const gr = graphEl.getBoundingClientRect();

        connections.forEach(({ ii, oi, zones, isAll }) => {
            const iEl = inputEls[ii];
            const oEl = outputEls[oi];
            if (!iEl || !oEl) return;
            const ir = iEl.getBoundingClientRect();
            const or = oEl.getBoundingClientRect();
            const x1 = ir.right - gr.left;
            const y1 = ir.top + ir.height / 2 - gr.top;
            const x2 = or.left - gr.left;
            const y2 = or.top + or.height / 2 - gr.top;
            const cx = (x1 + x2) / 2;

            const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
            path.setAttribute('d', `M ${x1} ${y1} C ${cx} ${y1}, ${cx} ${y2}, ${x2} ${y2}`);
            path.setAttribute('fill', 'none');
            if (isAll) {
                path.style.stroke = 'var(--color-field-border)';
                path.setAttribute('stroke-width', '1.5');
                path.setAttribute('stroke-dasharray', '5 3');
            } else {
                path.style.stroke = ZoneColors.css(zones[0]);
                path.setAttribute('stroke-width', '2.5');
                path.setAttribute('opacity', '0.75');
            }
            svg.appendChild(path);
        });
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

    function flowStatDetails(sub, s) {
        const parts = [];
        if (sub !== 'udp' && sub !== 'http')
            parts.push(`<span class="t-strong ${s.connected ? 'sys-ok' : 't-danger'}">${s.connected ? 'Connected' : 'Not connected'}</span>`);
        parts.push(`<span>${formatBytes(s.bytes_out)} out</span>`);
        if (s.bytes_in > 0) parts.push(`<span>${formatBytes(s.bytes_in)} in</span>`);
        if (sub !== 'udp') parts.push(`<span>ok/fail ${s.connect_ok}/${s.connect_fail}</span>`);
        if (s.reconnects > 0) parts.push(`<span>${s.reconnects} reconnects</span>`);
        if (s.dropped > 0) parts.push(`<span class="t-warn">${s.dropped} dropped</span>`);
        return parts.join('');
    }

    function updateFlowStats(outputs, statEls) {
        if (!port || !engineRunning) {
            statEls.forEach(el => { el.innerHTML = ''; });
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
                outputs.forEach((o, i) => {
                    const el = statEls[i];
                    if (!el) return;
                    if (o.sub === 'server') {
                        el.innerHTML = `<span>${stat.tcp_clients} connection${stat.tcp_clients === 1 ? '' : 's'}</span>`;
                        return;
                    }
                    if (!o.active) { el.innerHTML = ''; return; }
                    const m = pools[o.sub] && pools[o.sub].shift();
                    el.innerHTML = m ? flowStatDetails(o.sub, m.stats) : '';
                });
            })
            .catch(() => statEls.forEach(el => { el.innerHTML = ''; }));
    }

    function engineTypeLabel(type) {
        const opt = receiverSchema.engines.options.find(o => o.value === (type || 'auto'));
        return opt ? opt.label : (type || 'auto').replace(/_/g, ' ');
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
        const legendEl = document.getElementById('flow-legend');
        const svg = document.getElementById('flow-svg');
        const graphEl = document.getElementById('flow-graph');

        inputsEl.innerHTML = '';
        outputsEl.innerHTML = '';
        legendEl.innerHTML = '';
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
                    const label = `${item.input || 'Unknown'} ${item.serial ? '#' + item.serial : '#' + (i + 1)}`;
                    const engines = Array.isArray(item.engines) && item.engines.length ? item.engines : [{}];
                    engines.forEach(e => receivers.push({
                        label: engines.length > 1 ? `${label} · ${engineTypeLabel(e.type)}` : label,
                        zones: [...(item.zone || []), ...(Array.isArray(e.zone) ? e.zone : [])],
                        active: item.active !== false
                    }));
                });

                const outputs = [];
                const mapCfg = (cfg.control && cfg.control.viewer) || null;
                if (mapCfg)
                    outputs.push({
                        label: 'Map',
                        zones: Array.isArray(mapCfg.zone) ? mapCfg.zone : [],
                        active: true,
                        tab: 'viewer'
                    });

                if (cfg.sharing !== undefined)
                    outputs.push({
                        label: 'Community',
                        zones: Array.isArray(cfg.sharing_zone) ? cfg.sharing_zone : [],
                        active: cfg.sharing === true,
                        sub: 'sharing'
                    });

                OUTPUT_TYPES.forEach(({ value, configKey, flowLabel }) => {
                    if (!configKey) return;
                    (cfg[configKey] || []).forEach(item => {
                        outputs.push({ label: flowOutputLabel(flowLabel, item), zones: item.zone || [], active: item.active !== false, sub: value, link: item.link });
                    });
                });
                loadingEl.classList.add('hidden');
                if (receivers.length === 0 && outputs.length === 0) {
                    emptyEl.classList.remove('hidden');
                    return;
                }
                patchEl.classList.remove('hidden');

                const allZones = new Set();
                [...receivers, ...outputs].forEach(n => n.zones.forEach(z => allZones.add(z)));
                allZones.forEach(z => {
                    const item = document.createElement('span');
                    item.className = `chip ${ZoneColors.badge(z)}`;
                    const dot = document.createElement('span');
                    dot.className = 'dot dot-sm';
                    dot.style.background = ZoneColors.css(z);
                    item.appendChild(dot);
                    item.appendChild(document.createTextNode(z));
                    legendEl.appendChild(item);
                });

                const inputEls = receivers.map(r => {
                    const n = flowNode(r.label, r.zones, r.active, true, () => switchSystemTab('input'));
                    inputsEl.appendChild(n);
                    return n;
                });
                const outputEls = outputs.map(o => {
                    const open = o.tab ? () => switchSystemTab(o.tab) : () => selectOutputTab(o.sub);
                    const n = flowNode(o.label, o.zones, o.active, false, open, o.link);
                    outputsEl.appendChild(n);
                    return n;
                });

                const statEls = outputEls.map(n => {
                    const d = document.createElement('div');
                    d.className = 'row row-wrap t-small t-subtle';
                    n.appendChild(d);
                    return d;
                });
                updateFlowStats(outputs, statEls);
                flowStatsTimer = setInterval(() => {
                    if (currentSystemTab === 'flow') updateFlowStats(outputs, statEls);
                }, 5000);

                const connections = [];
                outputs.forEach((output, oi) => {
                    if (output.zones.length === 0) {
                        receivers.forEach((_, ii) => connections.push({ ii, oi, zones: [], isAll: true }));
                    } else {
                        receivers.forEach((receiver, ii) => {
                            const shared = output.zones.filter(z => receiver.zones.includes(z));
                            if (shared.length > 0) connections.push({ ii, oi, zones: shared, isAll: false });
                        });
                    }
                });

                const redraw = () => drawFlowConnections(inputEls, outputEls, connections, svg, graphEl);
                requestAnimationFrame(redraw);
                stopFlowObserver();
                flowResizeObserver = new ResizeObserver(redraw);
                flowResizeObserver.observe(graphEl);
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
            try { await App.saveDirty(); } finally { headerSave.disabled = false; }
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
