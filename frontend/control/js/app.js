import { formatBytes } from '@aiscatcher/core/text.js';
import { createPlaceEditor } from './places-editor.js';
import { createStationsEditor } from './stations-editor.js';
import { createStationPicker } from './station-picker.js';
import { el } from './dom.js';
import { DOCS, docLink, inputDocs, outputDocs } from './docs.js';
import { createHistoryEditor } from './history-editor.js';
import { App, ConfigStore, Utils, createSimpleConfigManager, createChannelManager } from './config-manager.js';
import { SetupWizard } from './wizard.js';
import { receiverSchema, OUTPUT_TYPES, channelTitle } from './schema.js';
import { stationsOf, stationLabel } from './stations.js';
import { describeInput, inputGroup, INPUT_ESSENTIALS, SDR_INPUTS } from './inputs.js';
import { createFlowEditor, collectOutputs, inputRouting, outputRouting } from './flow-editor.js';
import { createServicesSection, servicesRailItems } from './services-pages.js';
import { createRail } from './rail.js';
import { createLogConsole } from './log-console.js';

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
    const logConsole = createLogConsole();

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
            ? 'Choose an admin password for this panel.'
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

    // after a save the receiver takes the configuration at its next start: the button says so
    window.hubConfigSaved = function () {
        App.notify('info', (engineRunning ? 'Restart' : 'Start') + ' the receiver to apply the new configuration', 8000);
        pendingApply = true;
        renderEngineButton();
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
    const INPUTS_CONTAINER = 'hub-receivers-container';
    const OUTPUTS_CONTAINER = 'hub-output-container';

    let currentTab = null;
    let placeReturnTab = 'services';
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
    let inputs = null, stationsEditor = null, outputs = null, services = null, flowPage = null, placeEditor = null;

    const rail = createRail(document.getElementById('settings-rail'), {
        sections: railSections,
        // the page, then the item on it; on a phone the page shows in place of the list
        open: (tab, key) => {
            if (!switchSystemTab(tab) || !showItem(tab, key)) return false;
            systemModal.dataset.view = 'page';
            return true;
        },
        shown: () => systemOverlay.classList.contains('open')
    });
    // open a page of the rail with the item on it; false when unsaved edits keep the page
    const selectPage = (tab, key) => rail.select(tab, key);

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
            { tab: 'input', label: 'Inputs', items: rows.map((r, i) => ({ key: i, label: describeInput(r, i, rows).title, dot: r.active !== false })), add: ['Add input', addInput] },
            { tab: 'output', label: 'Outputs', items: outputItems(cfg), add: ['Add output', () => selectPage('output-add')] },
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
            return showOutputTarget();
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

    function hasUnsaved() {
        if (currentTab === 'places') return !!placeEditor?.unsaved();
        return !!App.state.unsaved;
    }
    function confirmDiscardUnsaved(verb) {
        if (!hasUnsaved()) return true;
        if (!confirm(`You have unsaved changes. Are you sure you want to ${verb} without saving?`)) return false;
        if (currentTab === 'places') placeEditor.cancel();
        else App.setUnsaved(false);
        return true;
    }

    function switchSystemTab(tab) {
        if (!SYSTEM_TABS[tab]) return false;
        if (tab === currentTab) return true;

        // leaving a page with unsaved edits: once confirmed, they go with the page that holds them,
        // Flow's with the Inputs and Outputs pages it passed them to
        if (hasUnsaved()) {
            if (!confirmDiscardUnsaved('switch')) return false;
            if (currentTab === 'flow') unloadAll(); else unload(currentTab);
        }

        if (tab === 'places' && currentTab && currentTab !== 'places') placeReturnTab = currentTab;
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
        else if (tab === 'output-add') renderOutputAdd();
        else if (tab === 'flow') {
            load(tab, loadFlow);
            flowPage.load();
        } else if (tab === 'services') load(tab, loadServices);
        else if (tab === 'history') load(tab, loadHistory);
        else if (tab === 'places') {
            load(tab, loadPlaces);
            requestAnimationFrame(() => placeEditor?.resize());
        } else if (tab === 'status') paintStatusCounts();
        else if (tab === 'config') loadConfigJson();
        else if (tab === 'log') logConsole.scrollToEnd();
        return true;
    }

    // ---- the pages
    function loadInputs() {
        const container = el('div', '', { id: INPUTS_CONTAINER });
        systemBody.querySelector('#sys-input-body').replaceChildren(container);
        const m = inputs = createChannelManager({
            channelType: 'receiver',
            schema: receiverSchema,
            containerId: INPUTS_CONTAINER,
            title: 'Receiver',
            variant: 'inputs',
            essentials: INPUT_ESSENTIALS,
            describe: describeInput,
            routing: inputRouting,
            excludeFor: item => ['zone', ...(SDR_INPUTS.has(item.input) ? [] : ['engines'])],
            groupOf: inputGroup,
            docs: item => docLink(inputDocs(item.input)),
            stationDocs: () => docLink(DOCS.stations),
            // a station made here is new to the pages that list the stations
            stationPicker: (value, onPick) => createStationPicker(value, {
                stations: stationRecords, onPick,
                onCreated: () => { unload('stations'); unload('services'); rail.render(); }
            }),
            openFlow: () => selectPage('flow'),
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
                if (rewired) window.hubConfigSaved();
                else App.notify('info', 'The map follows the stations as saved', 5000);
                rail.render();
            }
        });
        return { destroy() { ed.destroy(); if (stationsEditor === ed) stationsEditor = null; } };
    }

    // ---- Outputs: one output to a page, the one the rail picks: "sharing", "screen", or a type and
    // its place ("udp:0"); "udp:new" while one is being added
    let outputType = null;
    let outputFocus = null;
    let outputHead = null;
    function outputKeyParts(key) {
        const [type, idx] = String(key ?? 'sharing').split(':');
        return { type: type || 'sharing', focus: idx === undefined || idx === 'new' ? null : Number(idx) };
    }
    // the outputs one by one: the community map first, then each configured one; the open type's from its editor
    function outputItems(cfg) {
        const open = OUTPUT_TYPES.find(o => o.value === outputType);
        const sharing = open?.value === 'sharing' && outputs?.loaded ? Utils.toBoolean(outputs.data.sharing) : cfg.sharing === true;
        const items = [{ key: 'sharing', label: 'Community map', dot: sharing }];
        OUTPUT_TYPES.forEach(t => {
            if (!t.configKey) return;
            const rows = open === t && outputs?.loaded ? outputs.data : listOf(cfg[t.configKey]);
            rows.forEach((item, i) => items.push({ key: `${t.value}:${i}`, label: (item.description || '').trim() || channelTitle(t.label, item), dot: item.active !== false }));
        });
        const screenOn = cfg.screen !== undefined && !['0', 'none'].includes(String(cfg.screen).toLowerCase());
        if (screenOn || (rail.tab === 'output' && rail.key === 'screen')) items.push({ key: 'screen', label: 'Screen', dot: screenOn });
        return items;
    }
    function setOutputType(value) {
        if (value === outputType) return true;
        if (!confirmDiscardUnsaved('switch')) return false;
        outputType = value;
        drawOutput();
        return true;
    }
    // show the output the rail has picked; false when the switch was refused for unsaved edits
    function showOutputTarget() {
        const { type, focus } = outputKeyParts(rail.key);
        outputFocus = focus;
        if (!outputHead) return true;
        if (outputType === type) {
            if (outputs?.loaded) outputs.render();
            return true;
        }
        return setOutputType(type);
    }
    function loadOutputs() {
        const host = systemBody.querySelector('#sys-output-body');
        const title = el('h2', 'st-main__title');
        const head = el('div', 'st-main__head', {}, el('div', '', {}, title));
        host.className = 'st st-single';
        host.replaceChildren(el('div', 'st-main', {}, head, el('div', '', { id: OUTPUTS_CONTAINER })));
        outputHead = { head, title };
        outputType = null;
        const { type, focus } = rail.tab === 'output' ? outputKeyParts(rail.key) : { type: 'sharing', focus: null };
        outputFocus = focus;
        setOutputType(type);
        return {
            destroy() {
                outputs?.destroy();
                outputs = null;
                outputHead = null;
                outputType = null;
            }
        };
    }
    function drawOutput() {
        const t = OUTPUT_TYPES.find(o => o.value === outputType);
        if (!t) return;
        const list = !!t.configKey;
        const { head, title } = outputHead;
        // an output of a list carries its own heading
        head.hidden = list;
        title.textContent = t.value === 'sharing' ? 'Community map' : t.label;
        // what Community put in the header (its switch and routing line) leaves with it
        head.querySelectorAll('.st-head-extra').forEach(e => e.remove());
        head.classList.remove('st-main__head--page');
        head.querySelector('.st-doc')?.remove();
        head.append(docLink(outputDocs(t.value)));
        // the rail follows the edits: a new row, a name typed in, one switched off
        const onChange = () => rail.render();
        outputs?.destroy();
        if (!list) {
            outputs = createSimpleConfigManager({
                schema: t.schema,
                containerId: OUTPUTS_CONTAINER,
                variant: t.value === 'sharing' ? 'community' : 'plain',
                excludeFor: () => ['sharing_zone'],
                routing: outputRouting,
                openFlow: () => selectPage('flow'),
                onChange
            });
            return;
        }
        outputs = createChannelManager({
            channelType: t.configKey,
            schema: t.schema,
            containerId: OUTPUTS_CONTAINER,
            title: t.label,
            variant: 'outputs',
            routing: outputRouting,
            // one output on a page of its own, the one the rail picks; its zones are drawn in Flow, and a
            // database's one station number for every row shows only while set once there are stations
            focus: () => outputFocus,
            excludeFor: item => ['zone', ...(t.value === 'db' && stationList().length > 1 && !item.station_id ? ['station_id'] : [])],
            nameOf: item => channelTitle(t.label, item),
            docs: () => docLink(outputDocs(t.value)),
            openFlow: () => selectPage('flow'),
            // what an output needs to work, up front; the rest in Advanced
            essentials: new Set(['description', ...t.essentials]),
            onChange
        });
    }

    // Add an output: the kinds there are, each with what it is for
    const ADDABLE = [
        ['udp', 'UDP', 'NMEA to another program on the network'],
        ['tcp', 'TCP client', 'Connect out to a server and send'],
        ['tcp-server', 'TCP server', 'Let programs connect in and read'],
        ['http', 'HTTP', 'Post batches to a web address'],
        ['mqtt', 'MQTT', 'Publish to a broker'],
        ['db', 'Database', 'SQLite, PostgreSQL or CSV files'],
        ['server', 'Web viewer', 'A map on a port of its own, with its own ships'],
        ['screen', 'Screen', 'Print the messages in the log']
    ];
    function renderOutputAdd() {
        const grid = el('div', 'st-addtypes', {}, ADDABLE.map(([value, label, desc]) =>
            el('button', 'st-addtype', { type: 'button', onClick: () => addOutput(value) }, el('strong', '', { text: label }), el('span', '', { text: desc }))));
        systemBody.querySelector('#sys-output-add-body').replaceChildren(el('div', 'st-main', {},
            el('div', 'st-main__head', {}, el('div', '', {},
                el('h2', 'st-main__title', { text: 'Add an output' }),
                el('p', 'st-main__desc', { text: 'Home Assistant and webhooks are under Services.' }))),
            grid));
    }
    // a new output of a kind: a row of its list, on its page, or the page of a kind there is one of
    function addOutput(value) {
        const t = OUTPUT_TYPES.find(o => o.value === value);
        if (!t || !t.configKey) return selectPage('output', value);
        if (!selectPage('output', value + ':new')) return;
        const m = outputs;
        m.ready.then(() => {
            if (!m.loaded || m !== outputs) return;
            outputFocus = m.data.length;
            rail.follow('output', `${value}:${outputFocus}`);
            m.addItem();
        });
    }

    function loadFlow() {
        const page = flowPage = createFlowEditor(pane('flow'), {
            open: selectPage,
            receivers: () => (inputs?.loaded ? inputs.data : null),
            pages: () => ({ inputs: inputs?.loaded ? inputs : null, outputs: outputs?.loaded ? outputs : null, outputType }),
            running: () => !!port && engineRunning,
            shown: () => currentTab === 'flow',
            onSaved: () => window.hubConfigSaved()
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
        const ed = placeEditor = createPlaceEditor(systemBody.querySelector('#sys-places-body'), {
            onCancelSetup: () => selectPage(placeReturnTab),
            // places switched on: the other pages read the configuration again
            onEnabled: () => [...loaded.keys()].filter(t => t !== 'places').forEach(unload)
        });
        return { destroy() { ed.destroy(); if (placeEditor === ed) placeEditor = null; } };
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
            systemOverlay.classList.add('open');
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
        systemOverlay.classList.remove('open');
        if (currentTab) rail.remember();
        currentTab = null;
        document.getElementById('nav-btn-settings').classList.remove('active');
    }

    // Cancel closes the panel, as the X does, with the edits not saved once confirmed
    function discardSettings() {
        if (hasUnsaved()) {
            if (!confirm('Discard your unsaved changes?')) return;
            if (currentTab === 'places') placeEditor.cancel();
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

        systemBody.querySelector('#password-form').addEventListener('submit', changePassword);
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
            const lines = [...systemBody.querySelectorAll('#hub-sysinfo > div')]
                .filter(r => !r.hidden).map(r => r.children[0].textContent + ': ' + r.children[1].textContent);
            lines.push('Receiver: ' + ENGINE_STATES[engineStateKey].label);
            const done = ok => { b.textContent = ok ? 'Copied' : 'Copy failed'; setTimeout(() => { b.textContent = 'Copy for bug report'; }, 1600); };
            (navigator.clipboard ? navigator.clipboard.writeText(lines.join('\n')) : Promise.reject())
                .then(() => done(true), () => done(false));
        });
        renderEngineButton();
        paintStatusCounts();
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
        document.getElementById('nav-btn-settings').addEventListener('click', () => openSystem());
        document.getElementById('status-dot-wrap').addEventListener('click', () => openSystem('log'));
        document.getElementById('rail-back').addEventListener('click', () => { systemModal.dataset.view = 'rail'; });
        document.getElementById('login-pill').addEventListener('click', () => { pendingAction = null; openLoginModal(); });
        document.getElementById('bar-collapse').addEventListener('click', () => setBarCollapsed(true));
        document.getElementById('bar-restore').addEventListener('click', () => setBarCollapsed(false));
        document.getElementById('system-close').addEventListener('click', closeSystem);
        const headerSave = document.getElementById('system-save');
        headerSave.addEventListener('click', async () => {
            headerSave.disabled = true;
            try { if (currentTab === 'places') await placeEditor.save(); else await App.saveDirty(); } finally { headerSave.disabled = false; }
        });
        systemOverlay.addEventListener('click', e => {
            if (e.target === systemOverlay) closeSystem();
        });
        document.addEventListener('keydown', e => {
            // an open list or menu on a page closes itself first, and keeps the key
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
