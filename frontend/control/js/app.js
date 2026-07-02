// Hub logic for the managed-mode control UI: permanent webviewer iframe,
// bottom bar with engine control, slide-in config panel. Adapted from
// AIS-catcher-control's webviewer hub to the in-process /api endpoints.

(function () {
    'use strict';

    let auth = 'login';           // 'setup' | 'login' | 'ok'
    let viewers = [];
    let port = '';
    let viewerActive = true;
    let engineRunning = false;
    let pendingAction = null;
    let currentLoadTimeout = null;
    let statusPollInterval = null;
    let logSource = null;
    let currentView = 'main';
    let currentOutputType = 'sharing';

    const iframe = document.getElementById('webviewer-frame');
    const loadingDiv = document.getElementById('loading');
    const configPanel = document.getElementById('config-panel');
    const configOverlay = document.getElementById('config-overlay');
    const configContent = document.getElementById('config-content');
    const configTitle = document.getElementById('config-title');

    // ------------------------------------------------------------------
    // API helpers
    // ------------------------------------------------------------------

    function fetchStatus() {
        return fetch('/api/status').then(r => r.json());
    }

    function engineAction(action) {
        return fetch('/api/engine', { method: 'POST', body: action }).then(r => r.json());
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

    // ------------------------------------------------------------------
    // Auth
    // ------------------------------------------------------------------

    function isLoggedIn() { return auth === 'ok'; }

    function setAuthButtons(loggedIn) {
        ['nav-btn-input', 'nav-btn-output', 'nav-btn-control'].forEach(id => {
            const btn = document.getElementById(id);
            if (!btn) return;
            if (loggedIn) {
                btn.classList.remove('opacity-40', 'pointer-events-none');
                btn.removeAttribute('title');
            } else {
                btn.classList.add('opacity-40', 'pointer-events-none');
                btn.title = 'Login required';
            }
        });
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
                setAuthButtons(true);
                const action = pendingAction;
                closeLoginModal();
                refreshEngineStatus();
                if (action) action();
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

    // ------------------------------------------------------------------
    // Engine status / bottom bar
    // ------------------------------------------------------------------

    function setNavBusy(busy) {
        const nav = document.getElementById('nav-buttons');
        const dot = document.getElementById('status-dot');
        const dotLabel = document.getElementById('status-dot-label');
        if (busy) {
            nav.classList.add('opacity-50', 'pointer-events-none');
            if (dot) dot.className = 'block w-2.5 h-2.5 rounded-full bg-amber-400 cursor-default';
            if (dotLabel) dotLabel.textContent = 'Working...';
        } else {
            nav.classList.remove('opacity-50', 'pointer-events-none');
        }
    }

    function updateStartRestartButton(running, uptime) {
        engineRunning = running;
        const label = document.getElementById('nav-sr-label');
        const icon = document.getElementById('nav-sr-icon');
        if (running) {
            label.textContent = 'Restart';
            icon.innerHTML = '<path d="M480-160q-134 0-227-93t-93-227q0-134 93-227t227-93q69 0 132 28.5T720-690v-110h80v280H520v-80h168q-32-56-87.5-88T480-720q-100 0-170 70t-70 170q0 100 70 170t170 70q77 0 139-44t87-116h84q-28 106-114 173t-196 67Z"/>';
        } else {
            label.textContent = 'Start';
            icon.innerHTML = '<path d="M320-200v-560l440 280-440 280Zm80-280Zm0 134 210-134-210-134v268Z"/>';
        }
        const dot = document.getElementById('status-dot');
        const dotLabel = document.getElementById('status-dot-label');
        const dotUptime = document.getElementById('status-dot-uptime');
        if (dot && dotLabel) {
            if (running) {
                dot.className = 'block w-2.5 h-2.5 rounded-full bg-emerald-500 cursor-default';
                dotLabel.textContent = 'Running';
            } else {
                dot.className = 'block w-2.5 h-2.5 rounded-full bg-slate-500 cursor-default';
                dotLabel.textContent = 'Stopped';
            }
        }
        if (dotUptime) dotUptime.textContent = running ? formatUptime(uptime) : '';
    }

    function refreshEngineStatus() {
        return fetchStatus()
            .then(data => {
                updateStartRestartButton(data.engine === 'running', data.uptime);
                return data;
            })
            .catch(() => null);
    }

    function startStatusPolling() {
        if (statusPollInterval) return;
        statusPollInterval = setInterval(refreshEngineStatus, 12000);
    }

    function pollUntilRunning(onReady, maxWaitMs = 30000) {
        const interval = 1000;
        let elapsed = 0;
        const timer = setInterval(() => {
            elapsed += interval;
            fetchStatus()
                .then(d => {
                    if (d.engine === 'running') {
                        clearInterval(timer);
                        onReady();
                    } else if (elapsed >= maxWaitMs) {
                        clearInterval(timer);
                        setNavBusy(false);
                    }
                })
                .catch(() => {
                    if (elapsed >= maxWaitMs) { clearInterval(timer); setNavBusy(false); }
                });
        }, interval);
    }

    function onStartRestartClick() {
        const action = engineRunning ? 'restart' : 'start';
        requireAuth(() => {
            setNavBusy(true);
            engineAction(action)
                .then(() => pollUntilRunning(() => window.location.reload()))
                .catch(() => setNavBusy(false));
        });
    }

    // ------------------------------------------------------------------
    // Webviewer iframe
    // ------------------------------------------------------------------

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
                <h3 class="text-xl font-semibold text-slate-800 mb-2">${title}</h3>
                <p class="text-slate-600 mb-4">${message}</p>
                ${showRestart ? '<button class="hub-restart-btn inline-flex items-center gap-2 bg-slate-800 hover:bg-slate-700 text-white px-6 py-2 rounded-lg transition font-medium">Restart</button>' : ''}
            </div>
        `);
        const btn = div.querySelector('.hub-restart-btn');
        if (btn)
            btn.addEventListener('click', () => {
                requireAuth(() => {
                    btn.textContent = 'Restarting...';
                    btn.disabled = true;
                    engineAction('restart')
                        .then(() => pollUntilRunning(() => window.location.reload()))
                        .catch(() => { btn.disabled = false; btn.textContent = 'Restart'; });
                });
            });
    }

    function showEngineStopped() {
        const div = showOverlayMessage(`
            <div class="max-w-md p-8 text-center">
                <svg class="h-16 w-16 text-slate-400 mx-auto mb-4" viewBox="0 -960 960 960" fill="currentColor">
                    <path d="M320-200v-560l440 280-440 280Zm80-280Zm0 134 210-134-210-134v268Z" />
                </svg>
                <h3 class="text-xl font-semibold text-slate-800 mb-2">Receiver Stopped</h3>
                <p class="text-slate-600 mb-4">The receiver is not running. Start it to see the map.</p>
                <button class="hub-start-btn inline-flex items-center gap-2 bg-emerald-600 hover:bg-emerald-700 text-white px-6 py-2 rounded-lg transition font-medium">Start</button>
            </div>
        `);
        div.querySelector('.hub-start-btn').addEventListener('click', function () {
            const btn = this;
            requireAuth(() => {
                btn.textContent = 'Starting...';
                btn.disabled = true;
                engineAction('start')
                    .then(() => pollUntilRunning(() => window.location.reload()))
                    .catch(() => { btn.disabled = false; btn.textContent = 'Start'; });
            });
        });
    }

    function showViewerDisabled() {
        const div = showOverlayMessage(`
            <div class="max-w-md p-8 text-center">
                <h3 class="text-xl font-semibold text-slate-800 mb-2">Webviewer Output Disabled</h3>
                <p class="text-slate-600 mb-1">The webviewer on port <strong>${port}</strong> is configured but <strong>inactive</strong>.</p>
                <p class="text-slate-500 text-sm mb-6">Enable it and restart the receiver.</p>
                <button class="hub-enable-btn inline-flex items-center justify-center gap-2 bg-emerald-600 hover:bg-emerald-700 text-white px-6 py-2.5 rounded-lg transition font-medium">Enable &amp; Restart</button>
            </div>
        `);
        div.querySelector('.hub-enable-btn').addEventListener('click', function () {
            const btn = this;
            requireAuth(() => {
                btn.disabled = true;
                btn.textContent = 'Saving...';
                fetch('/api/config')
                    .then(r => r.json())
                    .then(cfg => {
                        const entries = Array.isArray(cfg.server) ? cfg.server : (cfg.server ? [cfg.server] : []);
                        entries.forEach(srv => {
                            if (String(srv.port) === String(port)) srv.active = true;
                        });
                        return fetch('/api/config', {
                            method: 'POST',
                            headers: { 'Content-Type': 'application/json' },
                            body: JSON.stringify(cfg)
                        });
                    })
                    .then(r => r.json())
                    .then(data => {
                        if (!data.status) throw new Error(data.error || 'save failed');
                        btn.textContent = 'Restarting...';
                        return engineAction('restart');
                    })
                    .then(() => pollUntilRunning(() => window.location.reload()))
                    .catch(err => {
                        btn.disabled = false;
                        btn.textContent = 'Enable & Restart';
                        alert('Failed to enable webviewer: ' + err.message);
                    });
            });
        });
    }

    function loadWebviewer() {
        try {
            const url = new URL(window.location.href);
            url.port = port;
            url.pathname = '/';
            url.search = '';
            url.hash = '';

            iframe.src = url.toString();

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

    function initViewerSelect() {
        if (viewers.length <= 1) return;
        const sel = document.getElementById('viewer-select');
        sel.innerHTML = '';
        viewers.forEach(srv => {
            const opt = document.createElement('option');
            opt.value = srv.port;
            opt.textContent = ':' + srv.port;
            if (String(srv.port) === String(port)) opt.selected = true;
            sel.appendChild(opt);
        });
        sel.classList.remove('hidden');
    }

    function proceedWithViewer(selectedPort, selectedActive) {
        port = String(selectedPort);
        viewerActive = selectedActive;
        sessionStorage.setItem('webviewer_port', port);
        initViewerSelect();

        if (!engineRunning) {
            loadingDiv.classList.add('hidden');
            showEngineStopped();
        } else if (!viewerActive) {
            loadingDiv.classList.add('hidden');
            showViewerDisabled();
        } else {
            loadWebviewer();
        }
        startStatusPolling();
    }

    function switchViewer(selectedPort) {
        clearTimeout(currentLoadTimeout);
        const srv = viewers.find(s => String(s.port) === String(selectedPort));
        if (!srv) return;
        closeConfigPanel();
        clearOverlayMessages();
        iframe.classList.add('hidden');
        iframe.src = '';
        loadingDiv.classList.remove('hidden');
        proceedWithViewer(srv.port, srv.active);
    }

    // ------------------------------------------------------------------
    // Config panel
    // ------------------------------------------------------------------

    function openConfig(type) {
        requireAuth(() => _openConfig(type));
    }

    function _openConfig(type) {
        if (typeof App !== 'undefined' && App.setUnsaved) App.setUnsaved(false);
        const statusEl = document.getElementById('status-message');
        if (statusEl) {
            statusEl.className = 'hidden';
            statusEl.innerHTML = '';
        }

        currentView = type;

        document.querySelectorAll('.hub-button').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.view === type);
        });

        const titles = {
            input: 'Input Configuration',
            output: 'Output Configuration',
            'control-panel': 'Control'
        };
        configTitle.textContent = titles[type] || 'Configuration';

        if (type === 'input') loadSourceConfig();
        else if (type === 'output') loadOutputConfig();
        else if (type === 'control-panel') loadControlPanel();

        configPanel.classList.add('open');
        configOverlay.classList.add('open');
    }

    function closeConfigPanel() {
        if (typeof App !== 'undefined' && App.state && App.state.unsaved) {
            if (!confirm('You have unsaved changes. Are you sure you want to close without saving?')) return;
        }
        if (typeof App !== 'undefined' && App.setUnsaved) App.setUnsaved(false);

        const statusEl = document.getElementById('status-message');
        if (statusEl) {
            statusEl.className = 'hidden';
            statusEl.innerHTML = '';
        }

        stopLogStream();
        currentOutputType = 'sharing';
        configPanel.classList.remove('open');
        configOverlay.classList.remove('open');
        currentView = 'main';
        document.querySelectorAll('.hub-button').forEach(btn => btn.classList.remove('active'));
    }

    function loadingSpinner() {
        return '<div class="text-center p-8"><div class="animate-spin h-8 w-8 border-4 border-slate-300 border-t-slate-600 rounded-full mx-auto mb-2"></div><p class="text-sm text-slate-600">Loading...</p></div>';
    }

    function configLoadFailed(retry) {
        configContent.textContent = '';
        const errorDiv = document.createElement('div');
        errorDiv.className = 'text-center p-8';
        const p = document.createElement('p');
        p.className = 'text-slate-600';
        p.textContent = 'Failed to load configuration';
        const button = document.createElement('button');
        button.onclick = retry;
        button.className = 'mt-4 px-4 py-2 bg-slate-600 text-white rounded-lg hover:bg-slate-700';
        button.textContent = 'Retry';
        errorDiv.appendChild(p);
        errorDiv.appendChild(button);
        configContent.appendChild(errorDiv);
    }

    function loadSourceConfig() {
        configContent.innerHTML = loadingSpinner();
        fetch('/api/config')
            .then(res => {
                if (!res.ok) throw new Error('Failed to fetch configuration');
                return res.json();
            })
            .then(() => {
                configContent.textContent = '';
                const container = document.createElement('div');
                container.id = 'hub-receivers-container';
                container.className = 'space-y-4';
                configContent.appendChild(container);

                createChannelManager({
                    channelType: 'receiver',
                    schema: receiverSchema,
                    containerId: 'hub-receivers-container',
                    title: 'Receiver'
                });
            })
            .catch(() => configLoadFailed(loadSourceConfig));
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
        configContent.innerHTML = loadingSpinner();
        fetch('/api/config')
            .then(res => {
                if (!res.ok) throw new Error('Failed to fetch configuration');
                return res.json();
            })
            .then(() => renderOutputConfig())
            .catch(() => configLoadFailed(loadOutputConfig));
    }

    function renderOutputConfig() {
        currentOutputType = 'sharing';
        configContent.textContent = '';
        const wrapper = document.createElement('div');
        wrapper.className = 'space-y-4';

        const tabBar = document.createElement('div');
        tabBar.className = 'flex flex-wrap gap-1.5 p-1.5 bg-slate-100 rounded-xl mb-2';

        const activeClass = 'flex-1 px-2 py-1.5 rounded-lg text-xs font-semibold bg-white text-slate-800 shadow-sm transition-all';
        const inactiveClass = 'flex-1 px-2 py-1.5 rounded-lg text-xs font-medium text-slate-500 hover:text-slate-700 transition-all';

        outputTypes.forEach(({ value, label }) => {
            const btn = document.createElement('button');
            btn.type = 'button';
            btn.dataset.outputType = value;
            btn.textContent = label;
            btn.className = value === currentOutputType ? activeClass : inactiveClass;
            btn.addEventListener('click', () => {
                currentOutputType = value;
                tabBar.querySelectorAll('button').forEach(b => {
                    b.className = b.dataset.outputType === value ? activeClass : inactiveClass;
                });
                updateOutputView();
            });
            tabBar.appendChild(btn);
        });

        const container = document.createElement('div');
        container.id = 'hub-output-container';
        wrapper.appendChild(tabBar);
        wrapper.appendChild(container);
        configContent.appendChild(wrapper);

        updateOutputView();
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

    // ------------------------------------------------------------------
    // Control panel: engine, log stream, password
    // ------------------------------------------------------------------

    function stopLogStream() {
        if (logSource) {
            logSource.close();
            logSource = null;
        }
    }

    function startLogStream() {
        stopLogStream();
        const box = document.getElementById('log-box');
        if (!box) return;
        logSource = new EventSource('/api/log');
        logSource.addEventListener('log', e => {
            try {
                const m = JSON.parse(e.data);
                box.textContent += '[' + m.time + '] ' + m.message + '\n';
                box.scrollTop = box.scrollHeight;
            } catch (_) { }
        });
    }

    function loadControlPanel() {
        stopLogStream();
        configContent.innerHTML = `
            <div class="space-y-6">
                <div class="bg-slate-50 rounded-lg p-4 border border-slate-200">
                    <div class="flex items-center justify-between mb-2">
                        <span class="font-semibold text-slate-800">Receiver</span>
                        <div id="hub-status" class="flex items-center space-x-2">
                            <span class="text-sm font-medium text-slate-600">Checking...</span>
                        </div>
                    </div>
                    <div id="hub-uptime" class="text-sm text-slate-600"></div>
                </div>
                <div class="space-y-3">
                    <button id="hub-btn-start" class="w-full bg-emerald-600 hover:bg-emerald-700 text-white px-4 py-3 rounded-lg transition flex items-center justify-center gap-2 font-medium">Start</button>
                    <button id="hub-btn-stop" class="w-full bg-rose-600 hover:bg-rose-700 text-white px-4 py-3 rounded-lg transition flex items-center justify-center gap-2 font-medium">Stop</button>
                    <button id="hub-btn-restart" class="w-full bg-slate-600 hover:bg-slate-700 text-white px-4 py-3 rounded-lg transition flex items-center justify-center gap-2 font-medium">Restart</button>
                </div>
                <div>
                    <h4 class="font-semibold text-slate-800 mb-2">Log</h4>
                    <div id="log-box" class="bg-slate-900 text-slate-200 rounded-lg p-3 h-48 overflow-y-auto"></div>
                </div>
                <div>
                    <h4 class="font-semibold text-slate-800 mb-2">Change Password</h4>
                    <form id="password-form" class="space-y-2">
                        <input id="new-password" type="password" autocomplete="new-password" placeholder="New password"
                            class="w-full border border-slate-300 rounded-lg px-3 py-2 text-sm focus:outline-none focus:ring-2 focus:ring-slate-400" />
                        <input id="new-password2" type="password" autocomplete="new-password" placeholder="Confirm new password"
                            class="w-full border border-slate-300 rounded-lg px-3 py-2 text-sm focus:outline-none focus:ring-2 focus:ring-slate-400" />
                        <button type="submit" class="w-full bg-slate-800 hover:bg-slate-700 text-white px-4 py-2 rounded-lg transition text-sm font-medium">Change Password</button>
                    </form>
                </div>
                <button id="hub-btn-logout" class="w-full border border-slate-300 text-slate-600 hover:bg-slate-50 px-4 py-2 rounded-lg transition text-sm font-medium">Logout</button>
            </div>
        `;

        document.getElementById('hub-btn-start').addEventListener('click', () => controlEngine('start'));
        document.getElementById('hub-btn-stop').addEventListener('click', () => controlEngine('stop'));
        document.getElementById('hub-btn-restart').addEventListener('click', () => controlEngine('restart'));
        document.getElementById('hub-btn-logout').addEventListener('click', logout);
        document.getElementById('password-form').addEventListener('submit', changePassword);

        updateControlStatus();
        startLogStream();
    }

    function updateControlStatus() {
        refreshEngineStatus().then(data => {
            if (!data) return;
            const statusEl = document.getElementById('hub-status');
            const uptimeEl = document.getElementById('hub-uptime');
            const running = data.engine === 'running';

            if (statusEl) {
                statusEl.innerHTML = running
                    ? '<span class="text-sm font-medium text-emerald-600">Running</span><span class="relative flex h-2.5 w-2.5"><span class="animate-ping absolute inline-flex h-full w-full rounded-full bg-emerald-400 opacity-75"></span><span class="relative inline-flex rounded-full h-2.5 w-2.5 bg-emerald-500"></span></span>'
                    : '<span class="text-sm font-medium text-slate-600">Stopped</span><span class="relative flex h-2.5 w-2.5"><span class="relative inline-flex rounded-full h-2.5 w-2.5 bg-slate-400"></span></span>';
            }
            if (uptimeEl) uptimeEl.textContent = running ? 'Uptime: ' + formatUptime(data.uptime) : '';
        });
    }

    function controlEngine(action) {
        setNavBusy(true);
        engineAction(action)
            .then(() => setTimeout(() => {
                updateControlStatus();
                setNavBusy(false);
            }, 1000))
            .catch(() => setNavBusy(false));
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

    // ------------------------------------------------------------------
    // Init
    // ------------------------------------------------------------------

    function init() {
        document.getElementById('login-form').addEventListener('submit', submitLogin);
        document.getElementById('login-cancel').addEventListener('click', closeLoginModal);
        document.getElementById('config-close').addEventListener('click', closeConfigPanel);
        document.getElementById('config-overlay').addEventListener('click', closeConfigPanel);
        document.getElementById('nav-start-restart').addEventListener('click', onStartRestartClick);
        document.getElementById('nav-btn-input').addEventListener('click', () => openConfig('input'));
        document.getElementById('nav-btn-output').addEventListener('click', () => openConfig('output'));
        document.getElementById('nav-btn-control').addEventListener('click', () => openConfig('control-panel'));
        document.getElementById('viewer-select').addEventListener('change', e => switchViewer(e.target.value));

        fetchStatus()
            .then(data => {
                auth = data.auth;
                viewers = Array.isArray(data.viewers) ? data.viewers : [];
                updateStartRestartButton(data.engine === 'running', data.uptime);
                setAuthButtons(isLoggedIn());

                if (auth === 'setup') {
                    loadingDiv.classList.add('hidden');
                    openLoginModal();
                    return;
                }

                if (!viewers.length) {
                    showError('Webviewer Not Configured', 'No webviewer is configured. Add one under Output → Viewer.');
                    return;
                }

                const savedPort = sessionStorage.getItem('webviewer_port');
                const saved = savedPort && viewers.find(s => String(s.port) === String(savedPort));
                const srv = saved || viewers[0];
                proceedWithViewer(srv.port, srv.active);
            })
            .catch(() => {
                loadingDiv.classList.add('hidden');
                showError('Connection Error', 'Cannot reach the control server.');
            });
    }

    document.addEventListener('DOMContentLoaded', init);
})();
