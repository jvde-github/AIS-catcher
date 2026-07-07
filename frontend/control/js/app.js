
(function () {
    'use strict';

    let auth = 'login';
    let port = 0;
    let viewerLoaded = false;
    let engineRunning = false;
    let engineDesired = false;
    let pendingApply = false;
    let pendingAction = null;

    const ENGINE_ICONS = {
        start: '<path d="M320-200v-560l440 280-440 280Zm80-280Zm0 134 210-134-210-134v268Z"/>',
        stop: '<path d="M240-240v-480h480v480H240Z"/>',
        restart: '<path d="M440-122q-121-15-200.5-105.5T160-440q0-66 26-126.5T260-672l57 57q-38 34-57.5 79T240-440q0 88 56 155.5T440-203v81Zm80 0v-81q87-16 143.5-83T720-440q0-100-70-170t-170-70h-3l44 44-56 56-140-140 140-140 56 56-44 44h3q134 0 227 93t93 227q0 121-79.5 211.5T520-122Z"/>'
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
        if (btn) btn.classList.toggle('attention', pendingApply);
        if (icon && icon.dataset.mode !== mode) {
            icon.dataset.mode = mode;
            icon.innerHTML = ENGINE_ICONS[mode];
            if (label) label.textContent = mode.charAt(0).toUpperCase() + mode.slice(1);
        }
    }
    let currentLoadTimeout = null;
    let statusPollInterval = null;
    let logSource = null;
    let currentOutputType = 'sharing';
    let flowOutputTarget = null;

    const iframe = document.getElementById('webviewer-frame');
    const loadingDiv = document.getElementById('loading');
    const systemOverlay = document.getElementById('system-overlay');
    const systemBody = document.getElementById('system-body');
    const systemTitle = document.getElementById('system-title');
    let currentSystemTab = null;
    let systemInputLoaded = false;
    let systemOutputLoaded = false;
    let systemViewerLoaded = false;
    let flowResizeObserver = null;

    // tab id -> header label / which nav-bar button to highlight
    const SYSTEM_TABS = {
        input: { label: 'Input', nav: 'input' },
        output: { label: 'Output', nav: 'output' },
        flow: { label: 'Data Flow', nav: 'control-panel' },
        status: { label: 'System', nav: 'control-panel' },
        viewer: { label: 'Map', nav: 'control-panel' },
        config: { label: 'Configuration', nav: 'control-panel' },
        log: { label: 'Log', nav: 'control-panel' },
        wizard: { label: 'Wizard', nav: 'control-panel' },
        password: { label: 'Access', nav: 'control-panel' }
    };

    function fetchStatus() {
        return fetch('/api/status').then(r => r.json());
    }

    function engineAction(action) {
        return fetch('/api/engine', { method: 'POST', body: action })
            .then(r => r.json().then(body => {
                if (!r.ok || !body.status) {
                    if (r.status === 401) {
                        auth = 'login';
                        updateBarVisibility();
                        openLoginModal();
                    }
                    throw new Error(body.error || 'Engine ' + action + ' failed');
                }
                return body;
            }));
    }

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


    function setBarCollapsed(collapsed) {
        barCollapsed = collapsed;
        try { localStorage.setItem('hubBarCollapsed', collapsed ? '1' : '0'); } catch (e) { /* ignore */ }
        updateBarVisibility();
    }

    function openLoginModal() {
        const setup = auth === 'setup';
        document.getElementById('login-title').textContent = setup ? 'Set Password' : 'Sign In';
        document.getElementById('login-subtitle').textContent = setup
            ? 'First run: choose an admin password to protect this control page.'
            : 'Authentication required to continue.';
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
        const setup = auth === 'setup';
        const password = document.getElementById('login-password').value;
        const submitBtn = document.getElementById('login-submit');
        document.getElementById('login-error').classList.add('hidden');

        if (setup && password !== document.getElementById('login-password2').value) {
            loginError('Passwords do not match');
            return;
        }

        submitBtn.disabled = true;
        fetch(setup ? '/api/setup' : '/api/login', { method: 'POST', body: password })
            .then(r => r.json())
            .then(data => {
                submitBtn.disabled = false;
                if (!data.status) {
                    loginError(data.error || 'Login failed');
                    return;
                }
                if (setup) {
                    window.location.reload();
                    return;
                }
                auth = 'ok';
                updateBarVisibility();
                startLogStream();
                startChannelLeds();
                const action = pendingAction;
                closeLoginModal();
                refreshEngineStatus();
                if (action) action();
                else SetupWizard.maybeAutoOpen();
            })
            .catch(() => {
                submitBtn.disabled = false;
                loginError('Connection error. Please try again.');
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
            btn.classList.toggle('opacity-50', disabled);
        }
    }

    const ENGINE_STATES = {
        running: { label: 'Running', dot: 'bg-emerald-500', text: 'text-emerald-600', hex: '#10b981' },
        starting: { label: 'Starting...', dot: 'bg-amber-400', text: 'text-amber-400', hex: '#fbbf24' },
        retrying: { label: 'Reconnecting...', dot: 'bg-amber-400', text: 'text-amber-400', hex: '#fbbf24' },
        stopped: { label: 'Stopped', dot: 'bg-slate-500', text: 'text-slate-400', hex: '#64748b' }
    };

    function renderEngineState(data) {
        if (!data) return;
        const running = data.engine === 'running';
        engineRunning = running;
        engineDesired = !!data.desired;
        renderEngineButton();

        const state = running ? 'running' : (data.retrying ? 'retrying' : (data.desired ? 'starting' : 'stopped'));
        const s = ENGINE_STATES[state];
        const up = running ? formatUptime(data.uptime) : '';

        const dot = document.getElementById('status-dot');
        if (dot) dot.className = 'block w-2.5 h-2.5 rounded-full cursor-default ' + s.dot;
        const dotLabel = document.getElementById('status-dot-label');
        if (dotLabel) dotLabel.textContent = s.label;
        const dotUptime = document.getElementById('status-dot-uptime');
        if (dotUptime) dotUptime.textContent = up;
        const dotText = document.getElementById('status-dot-text');
        if (dotText) {
            dotText.className = 'hidden sm:inline text-xs font-medium ' + s.text;
            dotText.textContent = s.label + (up ? ' · ' + up : '');
        }
        const restoreDot = document.getElementById('restore-dot');
        if (restoreDot) restoreDot.style.background = s.hex;

        const hubStatus = document.getElementById('hub-status');
        if (hubStatus && hubStatus.dataset.state !== state) {
            hubStatus.dataset.state = state;
            hubStatus.innerHTML = '<span class="text-sm font-medium ' + s.text + '">' + s.label + '</span>' +
                '<span class="relative flex h-2.5 w-2.5">' +
                (running ? '<span class="animate-ping absolute inline-flex h-full w-full rounded-full bg-emerald-400 opacity-75"></span>' : '') +
                '<span class="relative inline-flex rounded-full h-2.5 w-2.5 ' + s.dot + '"></span></span>';
        }
        const hubUptime = document.getElementById('hub-uptime');
        if (hubUptime) hubUptime.textContent = up ? 'Uptime: ' + up : '';
    }

    let reloadUntil = 0;
    let overlayRestartBtn = null;

    function refreshEngineStatus() {
        return fetchStatus()
            .then(data => {
                if (data.auth !== auth) {
                    auth = data.auth;
                    updateBarVisibility();
                    startLogStream();
                    startChannelLeds();
                }
                renderEngineState(data);
                setEngineButtonDisabled(false);
                if (reloadUntil) {
                    if (data.engine === 'running') {
                        reloadUntil = 0;
                        window.location.reload();
                    } else if (data.desired === false || Date.now() > reloadUntil) {
                        reloadUntil = 0;
                        if (overlayRestartBtn) {
                            overlayRestartBtn.disabled = false;
                            overlayRestartBtn.textContent = 'Restart';
                        }
                    }
                }
                if (data.viewer && !viewerLoaded) {
                    port = data.viewer;
                    clearOverlayMessages();
                    loadWebviewer();
                }
                return data;
            })
            .catch(() => {
                setEngineButtonDisabled(false);
                return null;
            });
    }

    function startStatusPolling() {
        if (statusPollInterval) return;
        statusPollInterval = setInterval(refreshEngineStatus, 1000);
    }

    function stopStatusPolling() {
        clearInterval(statusPollInterval);
        statusPollInterval = null;
    }

    window.hubConfigSaved = function (kind) {
        if (kind === 'viewer') {
            App.notify('info', 'Reload the page to apply the map viewer changes', 8000);
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
    let activitySource = null;

    function startChannelLeds() {
        if (activitySource || !isLoggedIn()) return;

        const wrap = document.getElementById('channel-leds');
        if (!wrap) return;

        wrap.innerHTML = CHANNELS.map((c, i) =>
            `<div class="ch-led-item" id="ch-item-${i}" style="${i >= 2 ? 'display:none' : ''}">
                <span class="ch-led" id="ch-led-${i}"></span>
                <span class="ch-led-label">${c}</span>
            </div>`).join('');

        activitySource = new EventSource('/api/activity');
        activitySource.addEventListener('activity', e => {
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
        });
    }

    function showOverlayMessage(html) {
        const hubContainer = document.getElementById('hub-container');
        const div = document.createElement('div');
        div.className = 'hub-overlay-msg flex items-center justify-center h-full bg-slate-50 absolute inset-0 z-10';
        div.innerHTML = html;
        loadingDiv.classList.add('hidden');
        hubContainer.insertBefore(div, hubContainer.firstChild);
        return div;
    }

    function clearOverlayMessages() {
        document.querySelectorAll('.hub-overlay-msg').forEach(el => el.remove());
    }

    function showError(title, message, showRestart = false) {
        const div = showOverlayMessage(`
            <div class="max-w-md p-8 text-center">
                <svg class="h-16 w-16 text-amber-500 mx-auto mb-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z"></path>
                </svg>
                <h3 class="hub-err-title text-xl font-semibold text-slate-800 mb-2"></h3>
                <p class="hub-err-msg text-slate-600 mb-4"></p>
                ${showRestart ? '<button class="hub-restart-btn inline-flex items-center gap-2 bg-white border border-slate-300 text-slate-700 hover:bg-slate-50 shadow-sm px-6 py-2 rounded-lg transition font-medium">Restart</button>' : ''}
            </div>
        `);
        div.querySelector('.hub-err-title').textContent = title;
        div.querySelector('.hub-err-msg').textContent = message;
        const btn = div.querySelector('.hub-restart-btn');
        if (btn)
            btn.addEventListener('click', () => {
                requireAuth(() => {
                    btn.textContent = 'Restarting...';
                    btn.disabled = true;
                    overlayRestartBtn = btn;
                    reloadUntil = Date.now() + 90000;
                    engineAction('restart')
                        .catch(() => { reloadUntil = 0; btn.disabled = false; btn.textContent = 'Restart'; });
                });
            });
    }

    function showNoViewer() {
        showOverlayMessage(`
            <div class="max-w-md p-8 text-center">
                <h3 class="text-xl font-semibold text-slate-800 mb-2">Viewer Not Running</h3>
                <p class="text-slate-600 mb-4">The built-in viewer could not be started &mdash; its port may be in use. Check the log in the Control panel.</p>
            </div>
        `);
    }

    function loadWebviewer() {
        try {
            const url = new URL(window.location.href);
            url.port = port;
            url.pathname = '/';
            url.search = '?welcome=false';
            url.hash = '';

            iframe.src = url.toString();
            viewerLoaded = true;

            currentLoadTimeout = setTimeout(() => {
                showError('Webviewer Not Responding', 'Port ' + port + ' is not responding.', true);
            }, 8000);

            iframe.onload = function () {
                clearTimeout(currentLoadTimeout);
                loadingDiv.classList.add('hidden');
                iframe.classList.remove('hidden');
            };

            iframe.onerror = function () {
                clearTimeout(currentLoadTimeout);
                showError('Webviewer Load Error', 'Failed to load the webviewer on port ' + port + '.');
            };
        } catch (e) {
            showError('Configuration Error', 'There was an error processing the webviewer URL.');
        }
    }

    function loadSourceConfig() {
        const host = document.getElementById('sys-input-body');
        if (!host) return;
        host.textContent = '';
        const container = document.createElement('div');
        container.id = 'hub-receivers-container';
        container.className = 'space-y-4';
        host.appendChild(container);

        createChannelManager({
            channelType: 'receiver',
            schema: receiverSchema,
            containerId: 'hub-receivers-container',
            title: 'Receiver'
        });
    }

    const outputTypes = [
        { value: 'sharing', label: 'Community' },
        { value: 'udp', label: 'UDP' },
        { value: 'http', label: 'HTTP' },
        { value: 'mqtt', label: 'MQTT' },
        { value: 'tcp', label: 'TCP Client' },
        { value: 'tcp-server', label: 'TCP Server' },
        { value: 'server', label: 'Viewer' },
    ];

    function loadOutputConfig() {
        const host = document.getElementById('sys-output-body');
        if (!host) return;
        renderOutputConfig(host);
    }

    const OUTPUT_TAB_ACTIVE = 'flex-1 px-2 py-1.5 rounded-lg text-xs font-semibold bg-white text-slate-800 shadow-sm transition-all';
    const OUTPUT_TAB_INACTIVE = 'flex-1 px-2 py-1.5 rounded-lg text-xs font-medium text-slate-500 hover:text-slate-700 transition-all';

    function setOutputType(value) {
        if (value !== currentOutputType && typeof App !== 'undefined' && App.state && App.state.unsaved) {
            if (!confirm('You have unsaved changes. Are you sure you want to switch without saving?')) return;
            App.setUnsaved(false);
        }
        currentOutputType = value;
        document.querySelectorAll('[data-output-type]').forEach(b => {
            b.className = b.dataset.outputType === value ? OUTPUT_TAB_ACTIVE : OUTPUT_TAB_INACTIVE;
        });
        updateOutputView();
    }

    // Open the Output tab on a specific sub-type (used by the Data Flow nodes).
    function selectOutputTab(value) {
        const wasLoaded = systemOutputLoaded;
        flowOutputTarget = value;
        switchSystemTab('output');
        if (wasLoaded) {
            setOutputType(value);
            flowOutputTarget = null;
        }
    }

    function renderOutputConfig(host) {
        const initial = flowOutputTarget || 'sharing';
        flowOutputTarget = null;
        host.textContent = '';
        const wrapper = document.createElement('div');
        wrapper.className = 'space-y-4';

        const tabBar = document.createElement('div');
        tabBar.className = 'flex flex-wrap gap-1.5 p-1.5 bg-slate-100 rounded-xl mb-2';

        outputTypes.forEach(({ value, label }) => {
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

        setOutputType(initial);
    }

    function updateOutputView() {
        const schemaMap = {
            sharing: sharingSchema,
            udp: udpSchema,
            http: httpSchema,
            mqtt: mqttSchema,
            tcp: tcpSchema,
            'tcp-server': tcpServerSchema,
            server: webviewerSchema
        };

        if (currentOutputType === 'sharing') {
            createSimpleConfigManager({
                schema: schemaMap.sharing,
                containerId: 'hub-output-container'
            });
        } else {
            const channelTypeMap = { 'tcp-server': 'tcp_listener' };
            createChannelManager({
                channelType: channelTypeMap[currentOutputType] || currentOutputType,
                schema: schemaMap[currentOutputType],
                containerId: 'hub-output-container',
                title: currentOutputType.charAt(0).toUpperCase() + currentOutputType.slice(1)
            });
        }
    }

    let logReplayDone = false;
    let logReplayUntil = 0;
    let lastLogTime = '';
    const logBuffer = [];
    const LOG_BUFFER_MAX = 500;

    function buildLogLine(m) {
        const level = ['error', 'critical'].indexOf(m.level) >= 0 ? 'error'
            : ['warning', 'info', 'debug'].indexOf(m.level) >= 0 ? m.level : 'default';
        const line = document.createElement('div');
        line.className = 'log-line ' + level;
        const ts = document.createElement('span');
        ts.className = 'log-ts';
        ts.textContent = m.time;
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
        logBuffer.forEach(m => box.appendChild(buildLogLine(m)));
        box.scrollTop = box.scrollHeight;
    }

    function startLogStream() {
        if (logSource || !isLoggedIn()) return;
        logReplayDone = false;
        logSource = new EventSource('/api/log');
        logSource.addEventListener('log', e => {
            try {
                const m = JSON.parse(e.data);
                if (Date.now() < logReplayUntil && m.time && m.time <= lastLogTime) return;
                if (m.time && m.time > lastLogTime) lastLogTime = m.time;

                logBuffer.push(m);
                if (logBuffer.length > LOG_BUFFER_MAX) logBuffer.shift();
                const box = document.getElementById('log-box');
                if (box) {
                    box.appendChild(buildLogLine(m));
                    while (box.childElementCount > LOG_BUFFER_MAX)
                        box.removeChild(box.firstChild);
                    box.scrollTop = box.scrollHeight;
                }

                if (!logReplayDone) return;
                if (m.level === 'error' || m.level === 'critical') {
                    App.notify('error', m.message);
                } else if (m.level === 'warning')
                    App.notify('warning', m.message);
            } catch (_) { }
        });
        logSource.onopen = () => {
            logReplayUntil = Date.now() + 500;
            if (!logReplayDone)
                setTimeout(() => { logReplayDone = true; }, 500);
        };
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
        if (typeof App !== 'undefined' && App.state && App.state.unsaved) {
            if (!confirm('You have unsaved changes. Are you sure you want to close without saving?')) return;
        }
        if (typeof App !== 'undefined' && App.setUnsaved) App.setUnsaved(false);

        stopFlowObserver();
        systemOverlay.classList.remove('open');
        currentSystemTab = null;
        document.querySelectorAll('.hub-button').forEach(btn => btn.classList.remove('active'));
    }

    function switchSystemTab(tab, force) {
        if (!SYSTEM_TABS[tab] || (!force && tab === currentSystemTab)) return;

        // Guard against losing unsaved edits when leaving an editable tab.
        if (!force && typeof App !== 'undefined' && App.state && App.state.unsaved) {
            if (!confirm('You have unsaved changes. Are you sure you want to switch without saving?')) return;
            App.setUnsaved(false);
            if (currentSystemTab === 'input') systemInputLoaded = false;
            else if (currentSystemTab === 'output') systemOutputLoaded = false;
            else if (currentSystemTab === 'viewer') systemViewerLoaded = false;
        }

        currentSystemTab = tab;
        systemTitle.textContent = SYSTEM_TABS[tab].label;

        const nav = SYSTEM_TABS[tab].nav;
        document.querySelectorAll('.hub-button').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.view === nav);
        });
        document.querySelectorAll('#system-tabs .sys-tab').forEach(b => {
            b.classList.toggle('active', b.dataset.tab === tab);
        });
        systemBody.querySelectorAll('.sys-pane').forEach(p => {
            p.classList.toggle('hidden', p.dataset.pane !== tab);
        });

        if (tab === 'input') {
            if (!systemInputLoaded) { systemInputLoaded = true; loadSourceConfig(); }
        } else if (tab === 'output') {
            if (!systemOutputLoaded) { systemOutputLoaded = true; loadOutputConfig(); }
        } else if (tab === 'viewer') {
            if (!systemViewerLoaded) { systemViewerLoaded = true; loadViewerConfig(); }
        } else if (tab === 'flow') {
            loadDataFlow();
        } else if (tab === 'config') {
            loadConfigJson();
        } else if (tab === 'log') {
            const box = document.getElementById('log-box');
            if (box) box.scrollTop = box.scrollHeight;
        }
    }

    function loadSystemPanel() {
        stopFlowObserver();
        if (typeof App !== 'undefined' && App.setUnsaved) App.setUnsaved(false);
        systemInputLoaded = false;
        systemOutputLoaded = false;
        systemViewerLoaded = false;
        systemBody.innerHTML = `
            <div id="status-message" class="hidden"></div>
            <div class="sys-pane hidden" data-pane="input"><div id="sys-input-body"></div></div>
            <div class="sys-pane hidden" data-pane="output"><div id="sys-output-body"></div></div>
            <div class="sys-pane hidden" data-pane="flow">
                <p class="text-xs text-slate-500 mb-4">Signal routing between inputs and outputs based on shared zones.</p>
                <div id="flow-loading" class="text-center text-slate-400 py-16">Loading&hellip;</div>
                <div id="flow-empty" class="hidden text-center text-slate-400 py-16">No receivers or outputs configured.</div>
                <div id="flow-patch" class="hidden">
                    <div id="flow-legend" class="flex flex-wrap gap-2 mb-5"></div>
                    <div class="grid mb-2 text-xs font-semibold text-slate-400 uppercase tracking-wider" style="grid-template-columns:1fr 100px 1fr">
                        <div>Input</div><div></div><div>Output</div>
                    </div>
                    <div id="flow-graph" class="relative" style="min-height:60px">
                        <div class="grid gap-0" style="grid-template-columns:1fr 100px 1fr">
                            <div id="flow-inputs" class="flex flex-col gap-3 pr-2"></div>
                            <div></div>
                            <div id="flow-outputs" class="flex flex-col gap-3 pl-2"></div>
                        </div>
                        <svg id="flow-svg" class="absolute inset-0 pointer-events-none" style="width:100%;overflow:visible"></svg>
                    </div>
                </div>
            </div>
            <div class="sys-pane hidden" data-pane="status">
                <div class="bg-slate-50 rounded-lg p-4 border border-slate-200 w-full max-w-md mx-auto">
                    <div class="flex items-center justify-between mb-2">
                        <span class="font-semibold text-slate-800">Receiver</span>
                        <div id="hub-status" class="flex items-center space-x-2">
                            <span class="text-sm font-medium text-slate-600">Checking...</span>
                        </div>
                    </div>
                    <div id="hub-uptime" class="text-sm text-slate-600"></div>
                </div>
            </div>
            <div class="sys-pane hidden" data-pane="viewer">
                <div>
                    <p class="text-xs text-slate-500 mb-3">Settings for the built-in map viewer. Changes take effect on the next receiver restart; reload the page to see new tabs.</p>
                    <div id="viewer-config-container"></div>
                </div>
            </div>
            <div class="sys-pane hidden" data-pane="config">
                <pre id="config-json" class="rounded-lg">Loading...</pre>
            </div>
            <div class="sys-pane hidden" data-pane="log">
                <div id="log-box" class="rounded-lg"></div>
            </div>
            <div class="sys-pane hidden" data-pane="wizard">
                <div class="max-w-2xl mx-auto px-4 sm:px-0">
                    <div class="bg-white rounded-xl border border-slate-200 shadow-sm overflow-hidden">
                        <div class="flex justify-between items-center px-5 py-2.5 bg-slate-100 border-b border-slate-200">
                            <span class="font-semibold text-slate-800">Setup Wizard</span>
                        </div>
                        <div class="p-5">
                            <p class="text-xs text-slate-500">Step through the guided setup to configure your receiver, sharing and viewer.</p>
                        </div>
                    </div>
                    <div class="mt-6 sm:mt-8 flex flex-row flex-wrap justify-end items-center gap-2 sm:gap-3">
                        <button id="hub-btn-wizard" class="w-auto bg-white border border-slate-300 text-slate-700 px-4 py-1.5 sm:py-2 rounded-lg hover:bg-slate-50 transition duration-200 shadow-sm inline-flex items-center justify-center gap-2 text-xs sm:text-sm font-medium">Open Setup Wizard</button>
                    </div>
                </div>
            </div>
            <div class="sys-pane hidden" data-pane="password">
                <div class="max-w-2xl mx-auto px-4 sm:px-0">
                    <div class="bg-white rounded-xl border border-slate-200 shadow-sm overflow-hidden">
                        <div class="flex justify-between items-center px-5 py-2.5 bg-slate-100 border-b border-slate-200">
                            <span class="font-semibold text-slate-800">Reset Password</span>
                        </div>
                        <div class="p-5">
                            <form id="password-form" class="space-y-2">
                                <input id="new-password" type="password" autocomplete="new-password" placeholder="New password"
                                    class="w-full border border-slate-300 rounded-lg px-3 py-2 text-sm focus:outline-none focus:ring-2 focus:ring-slate-400" />
                                <input id="new-password2" type="password" autocomplete="new-password" placeholder="Confirm new password"
                                    class="w-full border border-slate-300 rounded-lg px-3 py-2 text-sm focus:outline-none focus:ring-2 focus:ring-slate-400" />
                            </form>
                            ${auth === 'open' ? '<p class="text-xs text-slate-500 mt-3">Local access needs no password; this one is used when AIS-catcher is started with LAN access (bind 0.0.0.0).</p>' : ''}
                        </div>
                    </div>
                    <div class="mt-6 sm:mt-8 flex flex-row flex-wrap justify-end items-center gap-2 sm:gap-3">
                        ${auth === 'open' ? '' : '<button id="hub-btn-logout" type="button" class="w-auto sm:w-32 bg-white border border-slate-300 text-slate-700 px-4 py-1.5 sm:py-2 rounded-lg hover:bg-slate-50 transition duration-200 shadow-sm inline-flex items-center justify-center gap-2 text-xs sm:text-sm font-medium">Logout</button>'}
                        <button type="submit" form="password-form" class="w-auto sm:w-32 bg-white border border-slate-300 text-slate-700 px-4 sm:px-6 py-1.5 sm:py-2 rounded-lg hover:bg-slate-50 shadow-sm transition-all duration-200 text-xs sm:text-sm font-medium">Reset</button>
                    </div>
                </div>
            </div>
        `;

        document.getElementById('password-form').addEventListener('submit', changePassword);
        document.getElementById('hub-btn-wizard').addEventListener('click', () => {
            closeSystem();
            SetupWizard.open();
        });
        if (auth !== 'open')
            document.getElementById('hub-btn-logout').addEventListener('click', logout);

        refreshEngineStatus();
        renderLogBox();
    }

    function loadViewerConfig() {
        const keys = ['lat', 'lon', 'use_gps'];
        const widths = { lat: 38, lon: 38, use_gps: 18 };
        const schema = {};
        keys.forEach(k => {
            schema[k] = Object.assign({}, webviewerSchema[k]);
            delete schema[k].required;
            schema[k].width = widths[k];
        });
        schema.use_gps.label = 'GPS';
        createSimpleConfigManager({
            schema: schema,
            containerId: 'viewer-config-container',
            nestedPath: ['control', 'viewer'],
            title: 'Map'
        });
    }

    function loadConfigJson() {
        const pre = document.getElementById('config-json');
        fetch('/api/config')
            .then(r => { if (!r.ok) throw new Error(); return r.json(); })
            .then(cfg => { pre.textContent = JSON.stringify(cfg, null, 2); })
            .catch(() => { pre.textContent = 'Could not load the configuration.'; });
    }

    // Data Flow tab: a patch-bay view of signal routing. Inputs (left) and
    // outputs (right) are linked wherever they share a zone name; outputs with
    // no zone receive from every input. Zone colours match the config chips.
    function flowBadge(zone) {
        const s = document.createElement('span');
        s.className = `inline-flex items-center px-2 py-0.5 rounded-full text-xs font-medium ${ZoneColors.badge(zone)}`;
        s.textContent = zone;
        return s;
    }

    function flowNode(label, zones, active, isInput, onClick) {
        const border = active ? 'border-r-emerald-400' : 'border-r-rose-400';
        const div = document.createElement('button');
        div.type = 'button';
        div.className = `text-left bg-white border border-slate-200 border-r-[6px] ${border} rounded-lg px-3 py-2.5 shadow-sm min-w-0 cursor-pointer hover:bg-slate-50 transition-colors`;
        div.addEventListener('click', onClick);

        const lbl = document.createElement('div');
        lbl.className = 'text-sm font-medium text-slate-700 truncate';
        lbl.textContent = label;
        div.appendChild(lbl);

        if (zones && zones.length > 0) {
            const row = document.createElement('div');
            row.className = 'flex flex-wrap gap-1 mt-1';
            zones.forEach(z => row.appendChild(flowBadge(z)));
            div.appendChild(row);
        } else if (!isInput) {
            const note = document.createElement('div');
            note.className = 'text-xs text-slate-400 italic mt-0.5';
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
                path.setAttribute('stroke', '#cbd5e1');
                path.setAttribute('stroke-width', '1.5');
                path.setAttribute('stroke-dasharray', '5 3');
            } else {
                path.setAttribute('stroke', ZoneColors.hex(zones[0]));
                path.setAttribute('stroke-width', '2.5');
                path.setAttribute('opacity', '0.75');
            }
            svg.appendChild(path);
        });
    }

    function flowOutputLabel(type, item) {
        if (item && item.description) return `${type} · ${item.description}`;
        if (item && item.host && item.port) return `${type} · ${item.host}:${item.port}`;
        if (item && item.url) return `${type} · ${item.url}`;
        if (item && item.host) return `${type} · ${item.host}`;
        if (item && item.port) return `${type} · :${item.port}`;
        return type;
    }

    function stopFlowObserver() {
        if (flowResizeObserver) {
            flowResizeObserver.disconnect();
            flowResizeObserver = null;
        }
    }

    function loadDataFlow() {
        stopFlowObserver();
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

        fetch('/api/config')
            .then(r => { if (!r.ok) throw new Error(); return r.json(); })
            .then(cfg => {
                const receivers = (cfg.receiver || []).map((item, i) => ({
                    label: `${item.input || 'Unknown'} ${item.serial ? '#' + item.serial : '#' + (i + 1)}`,
                    zones: item.zone || [],
                    active: item.active !== false
                }));

                const outputs = [];
                [
                    { type: 'UDP', key: 'udp', sub: 'udp' },
                    { type: 'TCP', key: 'tcp', sub: 'tcp' },
                    { type: 'HTTP', key: 'http', sub: 'http' },
                    { type: 'MQTT', key: 'mqtt', sub: 'mqtt' },
                    { type: 'TCP Server', key: 'tcp_listener', sub: 'tcp-server' },
                    { type: 'Webviewer', key: 'server', sub: 'server' }
                ].forEach(({ type, key, sub }) => {
                    (cfg[key] || []).forEach(item => {
                        outputs.push({ label: flowOutputLabel(type, item), zones: item.zone || [], active: item.active !== false, sub });
                    });
                });
                if (cfg.sharing !== undefined) {
                    outputs.push({
                        label: 'Community',
                        zones: Array.isArray(cfg.sharing_zone) ? cfg.sharing_zone : [],
                        active: cfg.sharing === true,
                        sub: 'sharing'
                    });
                }

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
                    item.className = `inline-flex items-center gap-1.5 px-2.5 py-1 rounded-full text-xs font-medium ${ZoneColors.badge(z)}`;
                    const dot = document.createElement('span');
                    dot.className = 'w-2 h-2 rounded-full flex-shrink-0';
                    dot.style.background = ZoneColors.hex(z);
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
                    const n = flowNode(o.label, o.zones, o.active, false, () => selectOutputTab(o.sub));
                    outputsEl.appendChild(n);
                    return n;
                });

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
        fetch('/api/password', { method: 'POST', body: p1 })
            .then(r => r.json())
            .then(data => {
                if (data.status) {
                    App.notify('success', 'Password changed');
                    document.getElementById('password-form').reset();
                } else {
                    App.notify('error', data.error || 'Failed to change password');
                }
            })
            .catch(() => App.notify('error', 'Connection error'));
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
        document.getElementById('nav-btn-control').addEventListener('click', () => openSystem('flow'));
        document.getElementById('login-pill').addEventListener('click', () => { pendingAction = null; openLoginModal(); });
        document.getElementById('bar-collapse').addEventListener('click', () => setBarCollapsed(true));
        document.getElementById('bar-restore').addEventListener('click', () => setBarCollapsed(false));
        try { if (localStorage.getItem('hubBarCollapsed') === '1') setBarCollapsed(true); } catch (e) { /* ignore */ }
        document.getElementById('system-close').addEventListener('click', closeSystem);
        systemOverlay.addEventListener('click', e => {
            if (e.target === systemOverlay) closeSystem();
        });
        document.getElementById('system-tabs').addEventListener('click', e => {
            const btn = e.target.closest('.sys-tab');
            if (btn) switchSystemTab(btn.dataset.tab);
        });
        document.addEventListener('keydown', e => {
            if (e.key === 'Escape' && systemOverlay.classList.contains('open')) closeSystem();
        });
        document.addEventListener('visibilitychange', () => {
            if (document.hidden) {
                stopStatusPolling();
            } else {
                refreshEngineStatus();
                startStatusPolling();
            }
        });

        fetchStatus()
            .then(data => {
                auth = data.auth;
                port = data.viewer || 0;
                renderEngineState(data);
                updateBarVisibility();
                startStatusPolling();
                startLogStream();
                startChannelLeds();

                if (auth === 'setup') {
                    loadingDiv.classList.add('hidden');
                    openLoginModal();
                }

                if (isLoggedIn())
                    SetupWizard.maybeAutoOpen();

                if (port)
                    loadWebviewer();
                else if (auth !== 'setup') {
                    loadingDiv.classList.add('hidden');
                    showNoViewer();
                }
            })
            .catch(() => {
                loadingDiv.classList.add('hidden');
                showError('Connection Error', 'Cannot reach the control server.');
                startStatusPolling();
            });
    }

    document.addEventListener('DOMContentLoaded', init);
})();
