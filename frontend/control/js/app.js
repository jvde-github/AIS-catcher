import { formatBytes } from '@aiscatcher/core/text.js';
import { copyToClipboard } from '@aiscatcher/ui/components.js';
import { createPlaceEditor } from './places-editor.js';
import { createStationsEditor } from './stations-editor.js';
import { el, highlightJson, isActive } from './dom.js';
import { api } from './api.js';
import { wirePanelTLS } from './panel-tls.js';
import { icon } from './icons.js';
import { hub } from './hub.js';
import { createHistoryEditor } from './history-editor.js';
import { App, ConfigStore } from './config-store.js';
import { createInputsPage } from './inputs-page.js';
import { createOutputsPage, outputRailItems, renderAddOutput } from './outputs-page.js';
import { SetupWizard } from './wizard.js';
import { outputType as outputKind } from './schema.js';
import { stationsOf, stationLabel } from './stations.js';
import { describeInput } from './inputs.js';
import { createFlowEditor, collectOutputs, outputProblem, outputLiveProblem, statsByKey } from './flow-editor.js';
import { createServicesSection, servicesRailItems } from './services-pages.js';
import { createRail } from './rail.js';
import { createLogConsole } from './log-console.js';
import { createMessageBar } from './message-bar.js';

(function () {
    'use strict';

    let auth = 'login';
    let hasPassword = true;

    let port = 0;
    let viewerLoaded = false;
    let engineRunning = false;
    let engineDesired = false;
    let pendingApply = false;
    let pendingAction = null;
    let bar = null;

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

    function renderEngineButton() {
        const mode = engineButtonMode();
        const big = document.getElementById('hub-engine-btn');
        if (big && big.dataset.mode !== mode) {
            big.dataset.mode = mode;
            big.className = 'st-btn' + (mode === 'stop' ? '' : ' st-btn--primary');
            big.title = ENGINE_TITLES[mode];
            big.innerHTML = icon(mode) + '<span>' + ENGINE_LABELS[mode] + '</span>';
        }
        const btn = document.getElementById('nav-start-restart');
        const label = document.getElementById('nav-sr-label');
        const srIcon = document.getElementById('nav-sr-icon');
        if (btn) {
            btn.classList.toggle('attention', pendingApply);
            bar?.setRestart(pendingApply ? (engineRunning ? 'restart' : 'start') : null);
            btn.title = ENGINE_TITLES[mode];
        }
        if (srIcon && srIcon.dataset.mode !== mode) {
            srIcon.dataset.mode = mode;
            srIcon.className = ENGINE_ICONS[mode];
            if (label) label.textContent = mode.charAt(0).toUpperCase() + mode.slice(1);
        }
    }
    let streamRetryTimer = null;
    let streamWatchdog = null;
    let eventSource = null;
    // the receiver's warnings and errors since its log was last looked at, as a count beside its status
    const alerts = { warning: 0, error: 0 };
    const plural = (n, word) => `${n} ${word}${n === 1 ? '' : 's'}`;
    function paintAlerts() {
        const b = document.getElementById('log-alerts');
        if (!b) return;
        const parts = [alerts.error ? plural(alerts.error, 'error') : '', alerts.warning ? plural(alerts.warning, 'warning') : ''].filter(Boolean);
        b.hidden = !parts.length;
        b.textContent = parts.join(', ');
        b.title = 'Open the log';
        b.classList.toggle('is-error', alerts.error > 0);
    }
    function clearAlerts() {
        alerts.warning = alerts.error = 0;
        paintAlerts();
    }
    const logConsole = createLogConsole({ onAlert: level => { alerts[level]++; paintAlerts(); } });

    const iframe = document.getElementById('webviewer-frame');
    iframe.addEventListener('load', () => {
        try {
            const doc = iframe.contentDocument;
            const title = doc.querySelector('title') || doc.head.appendChild(doc.createElement('title'));
            const mirror = () => { if (doc.title) document.title = doc.title; };
            mirror();
            new MutationObserver(mirror).observe(title, { childList: true, characterData: true, subtree: true });
        } catch { /* a page of another origin keeps its title */ }
    });

    const fetchStatus = () => api('/api/status');
    // the login's own password is checked there: its 401 is a wrong password
    const postPassword = (endpoint, pw, headers) => api(endpoint, { body: pw, headers, login: endpoint === '/api/login' });
    const engineAction = action => api('/api/engine', { body: action });

    // the status as last read, the stream's updates merged in, for the pages that need the
    // receiver's port, address or listening servers; `statusRead` the read under way
    let lastStatus = {};
    let statusRead = Promise.resolve();
    hub.status = () => statusRead.then(() => lastStatus);
    hub.refreshStatus = () => refreshEngineStatus().then(() => lastStatus);

    // An expired session: ask for the password again, leaving unsaved edits intact.
    hub.authRequired = () => {
        if (isLoggedIn()) {
            auth = 'login';
            stopEventStream();
            updateBarVisibility();
        }
        openLoginModal();
    };

    // the wizard cancelled while no password is set: the modal is the backstop, so the
    // requirement cannot be dismissed
    hub.passwordBackstop = openLoginModal;

    // the wizard's password step has signed us in
    hub.authGranted = () => {
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
            ? 'Choose an admin password for this panel.'
            : 'Authentication required to continue.';
        document.getElementById('login-password').setAttribute('autocomplete', setup ? 'new-password' : 'current-password');
        document.getElementById('login-password2').classList.toggle('hidden', !setup);
        document.getElementById('login-setup-code').classList.toggle('hidden', auth !== 'setup');
        document.getElementById('login-submit').textContent = setup ? 'Set Password' : 'Sign In';
        document.getElementById('login-cancel').classList.toggle('hidden', setup);
        document.getElementById('login-overlay').classList.remove('hidden');
        setTimeout(() => document.getElementById('login-password').focus(), 50);
    }

    function closeLoginModal() {
        document.getElementById('login-overlay').classList.add('hidden');
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
        const headers = auth === 'setup' ? { 'X-Setup-Code': document.getElementById('login-code').value.trim() } : undefined;
        postPassword(endpoint, password, headers)
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
        connecting: { label: 'Connecting...', dot: 'dot-warn', text: 'sys-warn-ink', dotColor: 'var(--color-warning-ink)' },
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

        const waiting = Array.isArray(data.receivers) && data.receivers.some(r => r.state === 'starting');
        const state = running ? (waiting ? 'connecting' : 'running') : (data.retrying ? 'retrying' : (data.desired ? 'starting' : 'stopped'));
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
        lastStatus = { ...lastStatus, ...data };
        noteReceivers(data);
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
        if (fell) clearAlerts();
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
        const read = fetchStatus()
            .then(applyStatus)
            .catch(() => {
                setEngineButtonDisabled(false);
                return null;
            });
        statusRead = read;
        return read;
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

    // after a save the receiver takes the configuration at its next start: the button says so
    hub.configSaved = () => {
        pendingApply = true;
        renderEngineButton();
    };

    // the receiver's start, stop or restart, as the button says. `busy` greys the button pressed;
    // `untilReply`: only until the answer, not the status that follows it
    function engineClick(busy, untilReply = false) {
        const action = engineButtonMode();
        requireAuth(() => {
            busy(true);
            engineAction(action)
                .then(() => {
                    pendingApply = false;
                    renderEngineButton();
                    if (untilReply) busy(false);
                })
                .catch(e => {
                    busy(false);
                    if (e && e.message) App.notify('error', e.message);
                });
        });
    }
    const onStartRestartClick = () => engineClick(setEngineButtonDisabled);

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
        } catch { /* a line cut short: the next one follows */ }
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
                ${icon('alert', 'icon-2xl t-warn center-block')}
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
                <h3 class="t-strong t-lg">The map is not running</h3>
                <p class="t-muted">Main could not start: its port may be in use. See System › Log.</p>
            </div>
        `);
    }

    function loadWebviewer() {
        iframe.setAttribute('src', iframe.src);   // reloads the frame
        viewerLoaded = true;
        clearOverlayMessages();
    }

    function startEventStream() {
        if (eventSource || !isLoggedIn()) return;
        initChannelLeds();
        logConsole.starting();
        eventSource = new EventSource(logConsole.streamUrl());
        eventSource.addEventListener('log', logConsole.onEvent);
        eventSource.addEventListener('activity', onActivityEvent);
        eventSource.addEventListener('status', e => {
            try { applyStatus(JSON.parse(e.data)); } catch { /* a status line cut short: the next one follows */ }
        });
        eventSource.onopen = () => {
            clearTimeout(streamWatchdog);
            streamWatchdog = null;
            logConsole.opened();
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

    // ==== Settings: one left rail, one page at a time. A page is loaded when first shown and taken
    // down again when its edits are discarded or the panel closes, so what the rail and the other
    // pages read is the file, or a page that is open with its edits.
    const systemOverlay = document.getElementById('system-overlay');
    bar = createMessageBar({
        settings: document.getElementById('settings-bar'),
        page: document.getElementById('page-bar'),
        inSettings: () => !systemOverlay.classList.contains('hidden'),
        onRestart: onStartRestartClick
    });
    App.notify = (type, message) => bar.show(type, message);
    const systemBody = document.getElementById('system-body');
    const systemModal = document.getElementById('system-modal');

    // tab -> its title
    const SYSTEM_TABS = {
        input: 'Inputs',
        stations: 'Stations',
        output: 'Outputs',
        'output-add': 'Add an output',
        flow: 'Flow',
        services: 'Services',
        places: 'Places',
        status: 'Status',
        history: 'Data & history',
        log: 'Log',
        config: 'Config file',
        password: 'Access',
        about: 'About'
    };
    // System's pages, in the rail's order
    const SYSTEM_GROUP = ['status', 'history', 'log', 'config', 'password', 'about'];
    // the pages the footer saves
    const EDITABLE = ['input', 'stations', 'output', 'services', 'history', 'flow'];

    let currentTab = null;
    let previousTab = 'services';
    const listOf = v => (Array.isArray(v) ? v : []);
    const pane = name => systemBody.querySelector(`[data-pane="${name}"]`);

    // the pages loaded since the panel opened, each with what takes it down
    const loaded = new Map();
    function load(tab, make) {
        if (!loaded.has(tab)) loaded.set(tab, make());
    }
    function unload(tab) {
        loaded.get(tab)?.destroy();
        loaded.delete(tab);
    }
    const unloadAll = () => [...loaded.keys()].forEach(unload);

    // the pages' editors while they are loaded
    let inputs = null, stationsEditor = null, outputsPage = null, services = null, flowPage = null;

    const rail = createRail(document.getElementById('settings-rail'), {
        sections: railSections,
        // the page, then the item on it; on a phone the page shows in place of the list
        open: (tab, key) => {
            if (!switchSystemTab(tab) || !showItem(tab, key)) return false;
            systemModal.dataset.view = 'page';
            return true;
        },
        shown: () => !systemOverlay.classList.contains('hidden')
    });
    // open a page of the rail with the item on it; false when unsaved edits keep the page
    const selectPage = (tab, key) => rail.select(tab, key);

    // ---- what is wrong now, while the receiver runs: inputs by their place in the list, outputs by their rail key
    const live = { inputs: new Map(), outputs: new Map() };
    const sameMap = (a, b) => a.size === b.size && [...a].every(([k, v]) => b.get(k) === v);
    function setLive(part, next) {
        if (sameMap(next, live[part])) return;
        live[part] = next;
        if (!systemOverlay.classList.contains('hidden')) rail.render();
    }
    // the receivers the engine reports are the switched-on inputs of the file it started with, in order
    function noteReceivers(data) {
        const next = new Map();
        if (data.engine === 'running' && Array.isArray(data.receivers)) {
            const on = listOf(ConfigStore.cached?.receiver).map((r, i) => (isActive(r) ? i : -1)).filter(i => i >= 0);
            data.receivers.forEach(r => {
                const i = on[r.index - 1];
                if (i !== undefined && r.state === 'lost') next.set(i, 'The device stopped delivering: check that it is plugged in and free.');
                if (i !== undefined && r.state === 'starting') next.set(i, 'Not connected: trying again.');
            });
        }
        setLive('inputs', next);
    }
    const OUTPUT_STATS = '/viewer/api/output_stats.json';
    function pollOutputs() {
        if (document.hidden || !!systemOverlay.classList.contains('hidden')) return;
        if (!engineRunning || !ConfigStore.cached) return setLive('outputs', new Map());
        const outs = collectOutputs(ConfigStore.cached);
        api(OUTPUT_STATS)
            .then(stat => {
                const next = new Map();
                statsByKey(stat, outs).forEach((st, key) => {
                    const why = outputLiveProblem(outs.find(o => o.key === key)?.type, st);
                    if (why) next.set(key, why);
                });
                setLive('outputs', next);
            })
            .catch(() => {});
    }
    setInterval(pollOutputs, 10000);

    // what is wrong with a switched-on output: its settings first (amber), then what happens now (red)
    function outputMark(key, active, o) {
        if (!active) return {};
        const warn = outputProblem(o, inputs?.loaded ? inputs.data : listOf(ConfigStore.cached?.receiver));
        if (warn) return { warn };
        const bad = live.outputs.get(key);
        return bad ? { bad } : {};
    }

    // ---- the stations, the default one first: from the Stations page while it is loaded (its
    // edits count), else the configuration as last read. `key` is the page's, `id` what an
    // input or a filter names
    const stationRecords = () => (stationsEditor ? stationsEditor.stations() : stationsOf(ConfigStore.cached));
    function stationList() {
        const all = stationRecords();
        return all.map((s, i) => ({ key: i, id: Number(s.id), label: stationLabel(s, all.length > 1) }));
    }
    // the stations a server or a webhook can show, by what names them
    const stationChoices = () => stationList().map(st => ({ key: st.id, label: st.label }));

    // ---- what the rail lists, from the pages' editors where they are loaded, else the file
    function railSections(cfg) {
        const rows = inputs?.loaded ? inputs.data : listOf(cfg.receiver);
        return [
            { tab: 'stations', label: 'Stations', items: stationList(), add: ['Add station', addStation] },
            { tab: 'input', label: 'Inputs', items: rows.map((r, i) => ({ key: i, label: describeInput(r, i, rows).title, dot: isActive(r),
                bad: isActive(r) ? live.inputs.get(i) : undefined })), add: ['Add input', addInput] },
            { tab: 'output', label: 'Outputs', items: outputRailItems(cfg, outputsPage, { mark: outputMark, screenOpen: rail.tab === 'output' && rail.key === 'screen' }), add: ['Add output', () => selectPage('output-add')] },
            { tab: 'flow', label: 'Flow' },
            { tab: 'services', label: 'Services', items: servicesRailItems(cfg, services, { port, add: addServer }) },
            { tab: 'places', label: 'Places' },
            { tab: 'system', label: 'System', tabs: true, items: SYSTEM_GROUP.map(t => ({ key: t, label: SYSTEM_TABS[t] })) }
        ];
    }

    // the page's editor shows the item the rail picked, once it has loaded; else it reads the rail itself
    function showItem(tab, key) {
        if (tab === 'input') {
            if (inputs?.loaded && Number.isInteger(key) && inputs.selected !== key) { inputs.selected = key; inputs.render(); }
        } else if (tab === 'stations') {
            if (key !== undefined) stationsEditor?.select(key);
        } else if (tab === 'output') {
            return outputsPage ? outputsPage.show(rail.key) : true;
        } else if (tab === 'services') {
            services.show(key);
        }
        return true;
    }

    // something new, on its page: once the page has loaded, as its editor says
    function addInput() {
        if (!selectPage('input', rail.tab === 'input' ? rail.key : 0)) return;
        const m = inputs;
        m.ready.then(() => { if (m.loaded && m === inputs) m.addItem(); });
    }
    function addStation() {
        if (!selectPage('stations', rail.tab === 'stations' ? rail.key : 0)) return;
        const ed = stationsEditor;
        ed.ready.then(() => { if (ed === stationsEditor) ed.add(); });
    }
    function addServer() {
        if (selectPage('services', rail.tab === 'services' ? rail.key : 'main')) services.add();
    }

    const hasUnsaved = () => !!App.state.unsaved;
    function confirmDiscardUnsaved(verb) {
        if (!hasUnsaved()) return true;
        if (!confirm(`You have unsaved changes. Are you sure you want to ${verb} without saving?`)) return false;
        App.setUnsaved(false);
        return true;
    }

    function switchSystemTab(tab) {
        if (!SYSTEM_TABS[tab]) return false;
        if (tab === 'log') clearAlerts();
        if (tab === currentTab) return true;

        // leaving a page with unsaved edits: once confirmed, they go with the page that holds them,
        // Flow's with the Inputs and Outputs pages it passed them to
        if (hasUnsaved()) {
            if (!confirmDiscardUnsaved('switch')) return false;
            if (currentTab === 'flow') unloadAll(); else unload(currentTab);
        }

        if (currentTab) previousTab = currentTab;
        currentTab = tab;
        const group = systemBody.querySelector('#sys-group');
        const grouped = SYSTEM_GROUP.includes(tab);
        group.classList.toggle('hidden', !grouped);
        if (grouped) group.querySelector('.st-main__title').textContent = SYSTEM_TABS[tab];
        systemBody.querySelectorAll('.sys-pane').forEach(p => p.classList.toggle('hidden', p.dataset.pane !== tab));
        systemBody.querySelector('#st-footer').hidden = !EDITABLE.includes(tab);

        if (tab === 'input') load(tab, loadInputs);
        else if (tab === 'stations') load(tab, loadStations);
        else if (tab === 'output') load(tab, loadOutputs);
        else if (tab === 'output-add') renderAddOutput(systemBody.querySelector('#sys-output-add-body'), addOutput);
        else if (tab === 'flow') {
            load(tab, loadFlow);
            flowPage.load();
        } else if (tab === 'services') load(tab, loadServices);
        else if (tab === 'history') load(tab, loadHistory);
        else if (tab === 'places') load(tab, loadPlaces);
        else if (tab === 'status') paintStatusCounts();
        else if (tab === 'config') loadConfigJson();
        else if (tab === 'log') logConsole.scrollToEnd();
        return true;
    }

    // ---- the pages
    function loadInputs() {
        const m = inputs = createInputsPage(systemBody.querySelector('#sys-input-body'), {
            lost: item => live.inputs.get(m.data.indexOf(item)),
            stations: stationRecords,
            // a station made here is new to the pages that list the stations
            stationCreated: () => { unload('stations'); unload('services'); rail.render(); },
            open: selectPage,
            // the rail follows: a new input, a deleted one, a name typed in
            onChange: mgr => {
                if (Number.isInteger(mgr.selected)) rail.follow('input', mgr.selected);
                rail.render();
            }
        });
        // the input the rail asked for, before the first draw
        if (rail.tab === 'input' && Number.isInteger(rail.key)) m.selected = rail.key;
        return { destroy() { m.destroy(); if (inputs === m) inputs = null; } };
    }

    // Where the antennas stand: the list and the chosen one. A station saved under a new number
    // takes its inputs along.
    function loadStations() {
        const container = el('div');
        systemBody.querySelector('#sys-stations-body').replaceChildren(container);
        const ed = stationsEditor = createStationsEditor(container, {
            describeInput,
            initial: rail.tab === 'stations' ? rail.key : undefined,
            onList: () => {
                if (ed === stationsEditor) rail.follow('stations', ed.selected());
                rail.render();
            },
            onSaved: rewired => {
                // the inputs and the servers read them afresh
                unload('input');
                unload('services');
                if (rewired) hub.configSaved();
                else App.notify('info', 'The map follows the stations as saved', 5000);
                rail.render();
            }
        });
        return { destroy() { ed.destroy(); if (stationsEditor === ed) stationsEditor = null; } };
    }

    function loadOutputs() {
        const page = outputsPage = createOutputsPage(systemBody.querySelector('#sys-output-body'), {
            key: rail.tab === 'output' ? rail.key : undefined,
            mark: outputMark,
            stationCount: () => stationList().length,
            keepEdits: () => confirmDiscardUnsaved('switch'),
            open: selectPage,
            follow: key => rail.follow('output', key),
            // the rail follows the edits: a new row, a name typed in, one switched off
            changed: () => rail.render()
        });
        return { destroy() { page.destroy(); if (outputsPage === page) outputsPage = null; } };
    }
    // a new output of a kind: a row of its list, on its page, or the page of a kind there is one of
    function addOutput(value) {
        if (!outputKind(value)?.list) return selectPage('output', value);
        if (selectPage('output', value + ':new')) outputsPage.addRow();
    }

    function loadFlow() {
        const page = flowPage = createFlowEditor(pane('flow'), {
            open: selectPage,
            receivers: () => (inputs?.loaded ? inputs.data : null),
            pages: () => ({ inputs: inputs?.loaded ? inputs : null, outputs: outputsPage?.form?.loaded ? outputsPage.form : null, outputType: outputsPage?.type }),
            running: () => !!port && engineRunning,
            shown: () => currentTab === 'flow',
            onSaved: () => hub.configSaved()
        });
        return { destroy() { page.destroy(); if (flowPage === page) flowPage = null; } };
    }

    function loadServices() {
        const section = services = createServicesSection(pane('services'), {
            select: key => selectPage('services', key),
            follow: key => rail.follow('services', key),
            changed: () => rail.render(),
            stations: stationChoices,
            port: () => port,
            highlight: highlightJson
        });
        return { destroy() { section.destroy(); if (services === section) services = null; } };
    }

    // System › Data & history: how long ships and tracks are kept, and the backup that carries them
    // over a restart: the tracking section, which every server reads
    function loadHistory() {
        const host = systemBody.querySelector('#sys-history-body');
        host.replaceChildren();
        return createHistoryEditor(host);
    }

    function loadPlaces() {
        const ed = createPlaceEditor(systemBody.querySelector('#sys-places-body'), {
            onCancelSetup: () => selectPage(previousTab),
            // places switched on: the other pages read the configuration again
            onEnabled: () => [...loaded.keys()].filter(t => t !== 'places').forEach(unload)
        });
        return ed;
    }

    function loadConfigJson() {
        const pre = systemBody.querySelector('#config-json');
        ConfigStore.fetch()
            .then(cfg => { pre.innerHTML = highlightJson(JSON.stringify(cfg, null, 2)); })
            .catch(() => { pre.textContent = 'Could not load the configuration.'; });
    }

    function paintStatusCounts() {
        const box = systemBody.querySelector('#hub-counts');
        if (!box) return;
        const n = (k, w) => k + ' ' + w + (k === 1 ? '' : 's');
        ConfigStore.fetch().then(cfg => {
            const count = listOf(cfg.receiver).length;
            const active = collectOutputs(cfg).filter(o => o.active).length;
            box.textContent = n(count, 'input') + ' and ' + n(active, 'output') + ' configured';
        }).catch(() => { box.textContent = ''; });
    }

    // ---- opening and closing: Settings opens where it was left, or on a page asked for (the
    // status dot asks for the log)
    function openSystem(tab, key) {
        requireAuth(() => {
            buildPanel();
            systemOverlay.classList.remove('hidden');
            bar.render();
            document.getElementById('nav-btn-settings').classList.add('active');
            currentTab = null;
            const last = rail.last;
            selectPage(tab || last.tab, tab ? key : last.key);
            // on a phone the list comes first, unless a page was asked for
            systemModal.dataset.view = tab ? 'page' : 'rail';
        });
    }

    function closeSystem() {
        if (!confirmDiscardUnsaved('close')) return;
        App.setUnsaved(false);
        unloadAll();
        systemOverlay.classList.add('hidden');
        bar.render();
        if (currentTab) rail.remember();
        currentTab = null;
        document.getElementById('nav-btn-settings').classList.remove('active');
    }

    // Cancel closes the panel, as the X does, with the edits not saved once confirmed
    function discardSettings() {
        if (hasUnsaved()) {
            if (!confirm('Discard your unsaved changes?')) return;
            App.setUnsaved(false);
        }
        closeSystem();
    }

    // the panel's pages afresh, from the template, with one fresh read of the configuration
    function buildPanel() {
        unloadAll();
        App.setUnsaved(false);
        ConfigStore.invalidate();
        systemBody.replaceChildren(document.getElementById('system-body-template').content.cloneNode(true));
        // the password page as the panel asks for one or not
        const kind = auth === 'open' ? 'open' : 'login';
        systemBody.querySelectorAll(`[data-auth]:not([data-auth="${kind}"])`).forEach(n => n.remove());

        const footer = systemBody.querySelector('#st-footer');
        footer.querySelector('[data-st-cancel]').addEventListener('click', discardSettings);
        footer.querySelector('[data-st-save]').addEventListener('click', async e => {
            const b = e.currentTarget;
            b.disabled = true;
            b.textContent = 'Saving…';
            try { await App.saveDirty(); } finally {
                b.textContent = 'Save changes';
                b.disabled = !App.state.unsaved;
                rail.render();
            }
        });

        wireStatusPane();

        const communityLogo = document.querySelector('#community-link svg');
        const licenseLogo = systemBody.querySelector('#license-logo');
        if (communityLogo && licenseLogo) licenseLogo.innerHTML = communityLogo.outerHTML;

        currentPassword = systemBody.querySelector('#current-password');
        if (!hasPassword) currentPassword.remove();
        systemBody.querySelector('#password-form').addEventListener('submit', changePassword);
        wirePanelTLS(systemBody);
        systemBody.querySelector('#hub-btn-wizard').addEventListener('click', () => {
            closeSystem();
            openWizard();
        });
        systemBody.querySelector('#hub-btn-logout')?.addEventListener('click', logout);

        logConsole.mount(pane('log'));
        refreshEngineStatus();
    }

    // the Status page: Open log, the receiver button and the bug-report copy
    function wireStatusPane() {
        systemBody.querySelector('[data-open-log]').addEventListener('click', () => selectPage('log'));
        const btn = systemBody.querySelector('#hub-engine-btn');
        btn.addEventListener('click', () => engineClick(d => { btn.disabled = d; }, true));
        systemBody.querySelector('[data-copy-sysinfo]').addEventListener('click', e => {
            const b = e.currentTarget;
            const lines = [...systemBody.querySelectorAll('#hub-sysinfo > div')]
                .filter(r => !r.hidden).map(r => r.children[0].textContent + ': ' + r.children[1].textContent);
            lines.push('Receiver: ' + ENGINE_STATES[engineStateKey].label);
            const done = ok => { b.textContent = ok ? 'Copied' : 'Copy failed'; setTimeout(() => { b.textContent = 'Copy for bug report'; }, 1600); };
            copyToClipboard(lines.join('\n')).then(() => done(true), () => done(false));
        });
        renderEngineButton();
        paintStatusCounts();
    }

    let currentPassword = null;

    function changePassword(e) {
        e.preventDefault();
        const p1 = document.getElementById('new-password').value;
        const p2 = document.getElementById('new-password2').value;
        if (p1 !== p2) {
            App.notify('error', 'Passwords do not match');
            return;
        }
        // a password in place is changed with the current one first; a 401 then says that one is wrong
        const current = hasPassword ? currentPassword.value + '\n' : '';
        api('/api/password', { body: current + p1, login: hasPassword })
            .then(() => {
                hasPassword = true;
                App.notify('success', 'Password changed');
                document.getElementById('password-form').reset();
                document.getElementById('password-form').prepend(currentPassword);
            })
            .catch(err => App.notify('error', err.message || 'Failed to change password'));
    }

    function logout() {
        api('/api/logout', { method: 'POST' }).finally(() => window.location.reload());
    }

    function init() {
        document.getElementById('login-form').addEventListener('submit', submitLogin);
        document.getElementById('login-cancel').addEventListener('click', closeLoginModal);
        document.getElementById('nav-start-restart').addEventListener('click', onStartRestartClick);
        document.getElementById('nav-btn-settings').addEventListener('click', () => openSystem());
        document.getElementById('status-dot-wrap').addEventListener('click', () => openSystem('log'));
        document.getElementById('rail-back').addEventListener('click', () => { systemModal.dataset.view = 'rail'; });
        document.getElementById('login-pill').addEventListener('click', () => { pendingAction = null; openLoginModal(); });
        document.getElementById('bar-collapse').addEventListener('click', () => setBarCollapsed(true));
        document.getElementById('bar-restore').addEventListener('click', () => setBarCollapsed(false));
        document.getElementById('system-close').addEventListener('click', closeSystem);
        systemOverlay.addEventListener('click', e => {
            if (e.target === systemOverlay) closeSystem();
        });
        document.addEventListener('keydown', e => {
            // an open list or menu on a page closes itself first, and keeps the key
            if (e.key === 'Escape' && !systemOverlay.classList.contains('hidden') &&
                !document.querySelector('.modal-overlay:not(.hidden):not(#system-overlay)')) closeSystem();
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

        const first = fetchStatus()
            .then(data => {
                lastStatus = data;
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
        statusRead = first.catch(() => {});
    }

    document.addEventListener('DOMContentLoaded', init);
})();
