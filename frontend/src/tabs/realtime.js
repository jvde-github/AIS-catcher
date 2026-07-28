import { settings } from '../core/state.js';

let viewer = null;
let savedState = null;
let keydownHandler = null;
let pendingFilters = null;

const FILTER_KINDS = {
    mmsi: { label: 'MMSI', get: (d) => String(d.mmsi) },
    type: { label: 'Type', get: (d) => String(d.type) },
    channel: { label: 'Ch', get: (d) => String(d.channel) },
};

function normalizeFilter(kind, value) {
    if (!FILTER_KINDS[kind]) return null;
    const v = String(value ?? '').trim();

    if (kind === 'channel') return /^[ABCD]$/.test(v.toUpperCase()) ? v.toUpperCase() : null;

    const n = parseInt(v, 10);
    if (!(n > 0) || (kind === 'type' && n > 27)) return null;
    return String(n);
}

class RealtimeViewer {
    constructor(filterMMSI = null) {

        this.nmeaContent = document.getElementById('realtime_nmea_content');
        this.eventSource = null;
        this.maxLines = 100;
        this.isPaused = false;
        this.filters = [];
        this.groups = {};
        if (filterMMSI) this.addFilter('mmsi', filterMMSI);
        this.activeMode = 'nmea';

        this.backgroundStreaming = settings.realtime_background_streaming === true;

        const checkbox = document.getElementById('realtime_background_streaming');
        if (checkbox) checkbox.checked = this.backgroundStreaming;

        this.boundVisibilityHandler = this.handleVisibilityChange.bind(this);
        document.addEventListener('visibilitychange', this.boundVisibilityHandler);
    }

    handleVisibilityChange() {
        if (document.hidden) {
            if (this.isPaused || !this.backgroundStreaming) {
                this.disconnectStreams();
            }
        } else {
            const isConnected = this.eventSource &&
                (this.eventSource.readyState === EventSource.OPEN ||
                    this.eventSource.readyState === EventSource.CONNECTING);
            if (!isConnected && !this.isPaused) {
                this.connectNmea();
            }
            syncPauseButton(this.isPaused);
        }
    }

    connectNmea() {
        if (document.hidden) return;
        if (this.eventSource && (this.eventSource.readyState === EventSource.OPEN || this.eventSource.readyState === EventSource.CONNECTING)) {
            return;
        }
        this.disconnectNmea();
        this.eventSource = new EventSource("api/sse");

        this.eventSource.addEventListener('open', () => {
            document.getElementById("realtime_tab_mini").classList.add('pulsating');
        });

        this.eventSource.addEventListener('nmea', (event) => {
            try {
                if (!this.isPaused) {
                    const data = JSON.parse(event.data);
                    this.addNmeaMessage(data);
                }
            } catch (error) {
                console.error('Error parsing NMEA data:', error);
            }
        });

        this.eventSource.addEventListener('error', () => {
            if (this.nmeaContent && !document.hidden) {
                this.nmeaContent.innerHTML = '<tr><td colspan="4" style="text-align: center; padding: 20px;">Connection error. Reconnecting...</td></tr>';
            }
            if (!this.isPaused && (!document.hidden || this.backgroundStreaming)) {
                if (this.reconnectTimeout) clearTimeout(this.reconnectTimeout);
                this.reconnectTimeout = setTimeout(() => {
                    if (!this.isPaused) {
                        this.disconnectNmea();
                        this.connectNmea();
                    }
                }, 3000);
            }
        });
    }

    disconnectNmea() {
        if (this.eventSource) {
            this.eventSource.close();
            this.eventSource = null;
        }
        document.getElementById("realtime_tab_mini").classList.remove('pulsating');
    }

    disconnectStreams() {
        if (this.reconnectTimeout) {
            clearTimeout(this.reconnectTimeout);
            this.reconnectTimeout = null;
        }
        this.disconnectNmea();
    }

    disconnect() {
        document.removeEventListener('visibilitychange', this.boundVisibilityHandler);
        this.disconnectStreams();
    }

    addNmeaMessage(data) {
        if (!this.matches(data)) return;

        const rows = this.nmeaContent.getElementsByTagName('tr');
        if (rows.length >= this.maxLines) {
            this.nmeaContent.removeChild(rows[rows.length - 1]);
        }

        const config = window.AISCatcher.config;
        const row = document.createElement('tr');
        const time = new Date(data.timestamp * 1000).toLocaleTimeString();

        const channelIndicator = document.createElement('div');
        channelIndicator.className = 'channel-indicator';
        ['A', 'B', 'C', 'D'].forEach((ch) => {
            const box = document.createElement('span');
            box.className = 'channel-box' + (data.channel === ch ? ' active' : '');
            box.textContent = ch;
            channelIndicator.appendChild(box);
        });

        const channelCell = document.createElement('td');
        channelCell.appendChild(channelIndicator);

        const timeCell = document.createElement('td');
        timeCell.textContent = time;
        timeCell.style.fontSize = '0.85em';

        const mmsiCell = document.createElement('td');

        const nmeaCell = document.createElement('td');
        const nmeaArray = Array.isArray(data.nmea) ? data.nmea : [data.nmea];
        const nmeaText = nmeaArray.join('\n');

        const nmeaCellContent = document.createElement('div');
        nmeaCellContent.className = 'nmea-cell-content';

        if (config.features.decoder) {
            const decoderIcon = document.createElement('span');
            decoderIcon.className = 'nmea-decoder-icon';
            decoderIcon.innerHTML = '<i class="comment_icon"></i>';
            decoderIcon.title = 'Decode NMEA';
            decoderIcon.addEventListener('click', (e) => {
                e.stopPropagation();
                document.getElementById('decoder_tab').click();
                document.getElementById('decoder_input').value = nmeaText;
                window.AISCatcher.ACTIONS.decodeNMEA();
            });
            nmeaCellContent.appendChild(decoderIcon);
        }

        const nmeaTextSpan = document.createElement('span');
        nmeaTextSpan.className = 'nmea-text';
        nmeaTextSpan.textContent = nmeaText;
        nmeaCellContent.appendChild(nmeaTextSpan);

        nmeaCell.appendChild(nmeaCellContent);

        const mmsi = data.mmsi;
        const mmsiSpan = document.createElement('span');

        const hasShipname = data.shipname && data.shipname.trim() !== '';
        if (hasShipname) {
            mmsiSpan.textContent = data.shipname;
            mmsiSpan.title = 'MMSI: ' + data.mmsi;
        } else {
            mmsiSpan.textContent = data.mmsi;
        }

        mmsiSpan.className = 'nmea-mmsi';
        mmsiCell.appendChild(mmsiSpan);

        mmsiSpan.addEventListener('click', (e) => {
            e.stopPropagation();
            window.__app__.selectMapTab(mmsi);
        });

        mmsiSpan.addEventListener('contextmenu', (e) => {
            e.preventDefault();
            e.stopPropagation();
            window.AISCatcher.showContextMenu(e, mmsi, 'ship', ['ship']);
        });

        const typeCell = document.createElement('td');
        typeCell.textContent = data.type || '-';
        typeCell.style.fontSize = '0.85em';

        row.appendChild(channelCell);
        row.appendChild(typeCell);
        row.appendChild(timeCell);
        row.appendChild(mmsiCell);
        row.appendChild(nmeaCell);

        row.style.cursor = 'pointer';
        row.addEventListener('click', () => togglePause(true));

        this.nmeaContent.insertBefore(row, this.nmeaContent.firstChild);
    }

    pause() { this.isPaused = true; }
    resume() {
        this.isPaused = false;
        this.connectNmea();
    }

    reindex() {
        this.groups = {};
        for (const f of this.filters) (this.groups[f.kind] ||= []).push(f.value);
    }

    matches(data) {
        for (const kind in this.groups)
            if (!this.groups[kind].includes(FILTER_KINDS[kind].get(data))) return false;
        return true;
    }

    addFilter(kind, value) {
        const v = normalizeFilter(kind, value);
        if (v === null) return false;
        if (!this.filters.some((f) => f.kind === kind && f.value === v)) {
            this.filters.push({ kind, value: v });
            this.reindex();
        }
        return true;
    }

    removeFilter(kind, value) {
        this.filters = this.filters.filter((f) => !(f.kind === kind && f.value === String(value)));
        this.reindex();
    }

    setFilters(list) {
        this.clearFilters();
        for (const f of list || []) this.addFilter(f.kind, f.value);
    }

    clearFilters() {
        this.filters = [];
        this.reindex();
    }
}


function syncPauseButton(isPaused) {
    const button = document.getElementById('realtime_pause_button');
    if (!button) return;
    const icon = button.querySelector('span');
    if (icon) {
        icon.className = isPaused ? 'play_arrow_icon' : 'pause_icon';
        button.title = isPaused ? 'Resume stream' : 'Pause stream';
    }
}

let dialogKind = 'mmsi';

function chipsHTML() {
    return viewer.filters.map((f) => `<div class="filter-chip">
        <span class="filter-chip-kind">${FILTER_KINDS[f.kind].label}</span><span>${f.value}</span>
        <button title="Remove filter" data-action="removeRealtimeFilter" data-kind="${f.kind}"
            data-value="${f.value}"><i class="close_icon"></i></button></div>`).join('');
}

function updateFilterDisplay() {
    if (!viewer) return;
    const n = viewer.filters.length;

    document.getElementById('realtime_mmsi_filters').innerHTML = chipsHTML();
    document.getElementById('realtime_filter_display').style.display = n ? 'flex' : 'none';
    document.getElementById('realtime_filter_count').textContent = n || '';
    document.getElementById('realtime_filter_button').classList.toggle('active', n > 0);

    if (document.getElementById('realtime_filter_kind')) renderFilterDialog();
}

function valueFieldHTML() {
    if (dialogKind === 'channel')
        return `<select id="realtime_filter_value" class="realtime-filter-input">
            ${['A', 'B', 'C', 'D'].map((c) => `<option>${c}</option>`).join('')}</select>`;

    const hint = dialogKind === 'type' ? 'Message type 1 - 27' : 'MMSI number';
    return `<input id="realtime_filter_value" class="realtime-filter-input" placeholder="${hint}">`;
}

function filterDialogHTML() {
    const kinds = Object.entries(FILTER_KINDS).map(([k, v]) =>
        `<option value="${k}"${k === dialogKind ? ' selected' : ''}>${v.label}</option>`).join('');

    const active = viewer.filters.length
        ? `<div class="realtime-filter-active"><div class="filter-chips">${chipsHTML()}</div>
            <button class="realtime-filter-clear" data-action="clearRealtimeFilters">Remove all</button></div>`
        : '<div class="realtime-filter-none">No filters. Every message is shown.</div>';

    return `<div class="realtime-filter-editor">
        <div class="realtime-filter-row">
            <select id="realtime_filter_kind" class="realtime-filter-input"
                data-on-change="realtimeFilterKindChanged">${kinds}</select>
            ${valueFieldHTML()}
            <button class="realtime-filter-add" data-action="addRealtimeFilter">Add</button>
        </div>${active}
        <div class="realtime-filter-hint">Values of the same kind widen the selection, different kinds narrow it.</div>
        </div>`;
}

function renderFilterDialog() {
    window.AISCatcher.showDialog('NMEA filters', filterDialogHTML());
    document.getElementById('dialog-box').style.maxWidth = '520px';

    const field = document.getElementById('realtime_filter_value');
    field.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') addFilterFromInput();
    });
    field.focus();
}

function handleKeydown(event) {
    if (event.code === 'Space' && !['INPUT', 'TEXTAREA'].includes(event.target.tagName)) {
        event.preventDefault();
        togglePause(true);
    }
}



export function activate(filterMMSI = null) {
    const config = window.AISCatcher.config;

    if (!config.features.realtime) return;

    if (!viewer) {
        viewer = new RealtimeViewer(filterMMSI);

        if (Array.isArray(settings.realtime_filters) && settings.realtime_filters.length) {
            viewer.setFilters(settings.realtime_filters);
        } else if (Array.isArray(settings.realtime_filter_mmsis) && settings.realtime_filter_mmsis.length) {
            viewer.setFilters(settings.realtime_filter_mmsis.map((m) => ({ kind: 'mmsi', value: m })));
        }
        if (savedState && savedState.isPaused) {
            viewer.isPaused = true;
        }
        viewer.connectNmea();
    } else {
        if (viewer.eventSource && viewer.eventSource.readyState === EventSource.OPEN) {
            document.getElementById("realtime_tab_mini").classList.add('pulsating');
        } else if (!viewer.isPaused) {
            viewer.connectNmea();
        }
    }

    if (pendingFilters) {
        pendingFilters.forEach((f) => viewer.addFilter(f.kind, f.value));
        pendingFilters = null;
        window.__app__.saveSettings();
    }

    updateFilterDisplay();
    syncPauseButton(viewer.isPaused);

    keydownHandler = handleKeydown;
    document.addEventListener('keydown', keydownHandler);
}

export function deactivate() {
    if (keydownHandler) {
        document.removeEventListener('keydown', keydownHandler);
        keydownHandler = null;
    }
    if (!viewer) return;

    savedState = { isPaused: viewer.isPaused };
    window.__app__.saveSettings();

    if (viewer.isPaused || !viewer.backgroundStreaming) {
        viewer.disconnect();
        viewer = null;
    }
}

export function openForMMSI(mmsi) {
    const config = window.AISCatcher.config;
    if (!config.features.realtime) return;

    if (viewer && document.getElementById('realtime').style.display === 'block') {
        addFilter('mmsi', mmsi);
        return;
    }

    const existingFilters = viewer ? [...viewer.filters] : [];

    if (viewer) {
        viewer.disconnect();
        viewer = null;
    }
    savedState = null;

    pendingFilters = [...existingFilters, { kind: 'mmsi', value: mmsi }];
    document.getElementById('realtime_tab').click();
}

export function togglePause(showMessage = false) {
    if (!viewer) return;
    if (viewer.isPaused) {
        viewer.resume();
        if (showMessage) window.AISCatcher.showNotification("Streaming resumed");
    } else {
        viewer.pause();
        if (showMessage) window.AISCatcher.showNotification("Streaming paused");
    }
    syncPauseButton(viewer.isPaused);
}

export function toggleBackgroundStreaming() {
    const checkbox = document.getElementById('realtime_background_streaming');
    if (checkbox && viewer) {
        viewer.backgroundStreaming = checkbox.checked;
        settings.realtime_background_streaming = checkbox.checked;
        window.__app__.saveSettings();
    }
}

export function clear() {
    if (viewer && viewer.nmeaContent) {
        viewer.nmeaContent.innerHTML = '';
    }
}

export function addFilter(kind, value) {
    if (!viewer) return;
    if (!viewer.addFilter(kind, value)) {
        window.AISCatcher.showNotification('Not a valid ' + (FILTER_KINDS[kind] ? FILTER_KINDS[kind].label : kind));
        return;
    }
    updateFilterDisplay();
    window.__app__.saveSettings();
}

export function addFilterFromInput() {
    addFilter(dialogKind, document.getElementById('realtime_filter_value').value);
}

export function openFilters() {
    if (viewer) renderFilterDialog();
}

export function removeFilter(kind, value) {
    if (!viewer) return;
    viewer.removeFilter(kind, value);
    updateFilterDisplay();
    window.__app__.saveSettings();
}

export function clearFilters() {
    if (!viewer) return;
    viewer.clearFilters();
    updateFilterDisplay();
    window.__app__.saveSettings();
}

export function filterKindChanged() {
    dialogKind = document.getElementById('realtime_filter_kind').value;
    renderFilterDialog();
}

export function getFilters() {
    return viewer ? viewer.filters : null;
}

export function getBackgroundStreaming() {
    return viewer ? viewer.backgroundStreaming : null;
}
