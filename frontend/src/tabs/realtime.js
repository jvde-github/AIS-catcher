import { registerActions, registerChanges } from '../events.js';
import { settings, persist, onSettingsChange } from '../settings.js';
import { showNotification, showContextMenu } from '../ui.js';
import { decodeNMEA } from './decoder.js';

let viewer = null;
let selectMapTab = () => {};

// ── RealtimeViewer ────────────────────────────────────────────────────────────

class RealtimeViewer {
    constructor(filterMMSI = null) {
        this.nmeaContent = document.getElementById('realtime_nmea_content');
        this.eventSource = null;
        this.nmeaCount = 0;
        this.maxLines = 100;
        this.isPaused = false;
        this.filterMMSIs = filterMMSI ? [filterMMSI] : [];
        this.activeMode = 'nmea';
        this.backgroundStreaming = settings.realtime_background_streaming === true;

        const checkbox = document.getElementById('realtime_background_streaming');
        if (checkbox) checkbox.checked = this.backgroundStreaming;

        this.boundVisibilityHandler = this.handleVisibilityChange.bind(this);
        document.addEventListener('visibilitychange', this.boundVisibilityHandler);
    }

    handleVisibilityChange() {
        if (document.hidden) {
            if (this.isPaused || !this.backgroundStreaming) this.disconnectStreams();
        } else {
            const isConnected = this.eventSource &&
                (this.eventSource.readyState === EventSource.OPEN ||
                    this.eventSource.readyState === EventSource.CONNECTING);
            if (!isConnected && !this.isPaused) this.connectNmea();

            const button = document.getElementById('realtime_pause_button');
            if (button) {
                const icon = button.querySelector('span');
                if (icon) {
                    icon.className = this.isPaused ? 'play_arrow_icon' : 'pause_icon';
                    button.title = this.isPaused ? 'Resume stream' : 'Pause stream';
                }
            }
        }
    }

    connectNmea() {
        if (document.hidden) return;
        if (this.eventSource && (this.eventSource.readyState === EventSource.OPEN || this.eventSource.readyState === EventSource.CONNECTING)) return;

        this.disconnectNmea();
        this.eventSource = new EventSource("api/sse");

        this.eventSource.addEventListener('open', () => {
            document.getElementById("realtime_tab_mini").classList.add('pulsating');
        });

        this.eventSource.addEventListener('nmea', (event) => {
            try {
                if (!this.isPaused) this.addNmeaMessage(JSON.parse(event.data));
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
                    if (!this.isPaused) { this.disconnectNmea(); this.connectNmea(); }
                }, 3000);
            }
        });
    }

    disconnectNmea() {
        if (this.eventSource) { this.eventSource.close(); this.eventSource = null; }
        document.getElementById("realtime_tab_mini").classList.remove('pulsating');
    }

    disconnectStreams() {
        if (this.reconnectTimeout) { clearTimeout(this.reconnectTimeout); this.reconnectTimeout = null; }
        this.disconnectNmea();
    }

    disconnect() {
        document.removeEventListener('visibilitychange', this.boundVisibilityHandler);
        this.disconnectStreams();
    }

    addNmeaMessage(data) {
        if (this.filterMMSIs.length > 0 && !this.filterMMSIs.includes(data.mmsi.toString())) return;

        if (this.nmeaCount > this.maxLines) {
            const rows = this.nmeaContent.getElementsByTagName('tr');
            if (rows.length > 0) this.nmeaContent.removeChild(rows[rows.length - 1]);
        }

        const row = document.createElement('tr');
        const time = new Date(data.timestamp * 1000).toLocaleTimeString();

        const channelIndicator = document.createElement('div');
        channelIndicator.className = 'channel-indicator';
        ['A', 'B', 'C', 'D'].forEach(ch => {
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

        if (typeof decoder_enabled !== 'undefined' && decoder_enabled) {
            const decoderIcon = document.createElement('span');
            decoderIcon.className = 'nmea-decoder-icon';
            decoderIcon.innerHTML = '<i class="decode_icon"></i>';
            decoderIcon.title = 'Decode NMEA';
            decoderIcon.addEventListener('click', function (e) {
                e.stopPropagation();
                document.getElementById('decoder_input').value = nmeaText;
                document.getElementById('decoder_tab').click();
                decodeNMEA();
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
        mmsiSpan.textContent = hasShipname ? data.shipname : data.mmsi;
        if (hasShipname) mmsiSpan.title = 'MMSI: ' + data.mmsi;
        mmsiSpan.style.cssText = 'border:1px solid #d0d0d0;padding:2px 6px;border-radius:3px;display:inline-block;cursor:pointer;font-size:0.85em';
        mmsiCell.appendChild(mmsiSpan);

        mmsiSpan.addEventListener('click', (e) => { e.stopPropagation(); selectMapTab(mmsi); });
        mmsiSpan.addEventListener('contextmenu', (e) => {
            e.preventDefault(); e.stopPropagation();
            showContextMenu(e, mmsi, 'ship', ['ship']);
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
        row.addEventListener('click', () => toggleRealtimePause(true));

        this.nmeaContent.insertBefore(row, this.nmeaContent.firstChild);
        this.nmeaCount++;
    }

    pause()  { this.isPaused = true; }
    resume() { this.isPaused = false; this.connectNmea(); }

    addFilterMMSI(mmsi) {
        const s = mmsi.toString();
        if (!this.filterMMSIs.includes(s)) this.filterMMSIs.push(s);
    }
    removeFilterMMSI(mmsi) { this.filterMMSIs = this.filterMMSIs.filter(m => m !== mmsi.toString()); }
    clearFilters()          { this.filterMMSIs = []; }
    setFilterMMSI(mmsi)     { this.filterMMSIs = mmsi ? [mmsi.toString()] : []; }
}

// ── Actions ───────────────────────────────────────────────────────────────────

function toggleRealtimePause(showMessage = false) {
    if (!viewer) return;
    const button = document.getElementById('realtime_pause_button');
    const icon = button?.querySelector('span');
    if (viewer.isPaused) {
        viewer.resume();
        if (showMessage) showNotification("Streaming resumed");
        if (icon) { icon.className = 'pause_icon'; button.title = 'Pause stream'; }
    } else {
        viewer.pause();
        if (showMessage) showNotification("Streaming paused");
        if (icon) { icon.className = 'play_arrow_icon'; button.title = 'Resume stream'; }
    }
}

function toggleBackgroundStreaming() {
    const checkbox = document.getElementById('realtime_background_streaming');
    if (checkbox && viewer) {
        viewer.backgroundStreaming = checkbox.checked;
        settings.realtime_background_streaming = checkbox.checked;
        persist();
    }
}

function clearRealtimeTable() {
    if (viewer && viewer.nmeaContent) { viewer.nmeaContent.innerHTML = ''; viewer.nmeaCount = 0; }
}

function promptFilterMMSI() {
    const mmsiInput = prompt('Enter MMSI to filter:');
    if (mmsiInput) {
        const mmsi = parseInt(mmsiInput.trim(), 10);
        if (!isNaN(mmsi) && mmsi > 0) addRealtimeFilterMMSI(mmsi);
        else alert('Please enter a valid MMSI number.');
    }
}

function addRealtimeFilterMMSI(mmsi) {
    if (!viewer) return;
    const v = mmsi ? mmsi.toString().trim() : '';
    if (!v) return;
    viewer.addFilterMMSI(v);
    updateFilterDisplay();
    settings.realtime_filter_mmsis = viewer.filterMMSIs;
    persist();
}

function removeRealtimeFilterMMSI(mmsi) {
    if (!viewer) return;
    viewer.removeFilterMMSI(mmsi);
    updateFilterDisplay();
    settings.realtime_filter_mmsis = viewer.filterMMSIs;
    persist();
}

function updateFilterDisplay() {
    if (!viewer) return;
    const filterDisplay = document.getElementById('realtime_filter_display');
    const filterChips = document.getElementById('realtime_mmsi_filters');
    if (!filterDisplay || !filterChips) return;

    if (viewer.filterMMSIs.length > 0) {
        filterChips.innerHTML = '';
        viewer.filterMMSIs.forEach(mmsi => {
            const chip = document.createElement('div');
            chip.className = 'filter-chip';
            chip.innerHTML = `
                <span>${mmsi}</span>
                <button onclick="removeRealtimeFilterMMSI('${mmsi}')" title="Remove filter">
                    <i class="close_icon"></i>
                </button>`;
            filterChips.appendChild(chip);
        });
        filterDisplay.style.display = 'flex';
    } else {
        filterDisplay.style.display = 'none';
    }
}

function clearAllRealtimeFilters() {
    if (!viewer) return;
    viewer.clearFilters();
    updateFilterDisplay();
    settings.realtime_filter_mmsis = [];
    persist();
}

function handleRealtimeKeydown(event) {
    if (event.code === 'Space' && !['INPUT', 'TEXTAREA'].includes(event.target.tagName)) {
        event.preventDefault();
        toggleRealtimePause(true);
    }
}

// ── Tab lifecycle ─────────────────────────────────────────────────────────────

export function onActivate() {
    if (!realtime_enabled) return;

    if (!viewer) {
        const savedState = window.realtimeViewerState;
        viewer = new RealtimeViewer(savedState?.filterMMSI ?? null);

        if (settings.realtime_filter_mmsis?.length) {
            viewer.filterMMSIs = [...settings.realtime_filter_mmsis];
        }
        if (savedState?.isPaused) viewer.isPaused = true;

        viewer.connectNmea();
    } else {
        if (viewer.eventSource?.readyState === EventSource.OPEN) {
            document.getElementById("realtime_tab_mini").classList.add('pulsating');
        } else if (!viewer.isPaused) {
            viewer.connectNmea();
        }
    }

    updateFilterDisplay();

    const button = document.getElementById('realtime_pause_button');
    if (button) {
        const icon = button.querySelector('span');
        if (icon) {
            icon.className = viewer.isPaused ? 'play_arrow_icon' : 'pause_icon';
            button.title = viewer.isPaused ? 'Resume stream' : 'Pause stream';
        }
    }

    document.addEventListener('keydown', handleRealtimeKeydown);
}

export function onDeactivate() {
    document.removeEventListener('keydown', handleRealtimeKeydown);
    if (!viewer) return;

    window.realtimeViewerState = { isPaused: viewer.isPaused };
    settings.realtime_filter_mmsis = viewer.filterMMSIs;
    settings.realtime_background_streaming = viewer.backgroundStreaming;
    persist();

    if (viewer.isPaused || !viewer.backgroundStreaming) {
        viewer.disconnect();
        viewer = null;
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

export function openRealtimeForMMSI(mmsi) {
    if (!realtime_enabled) return;

    if (viewer && document.getElementById('realtime').style.display === 'block') {
        addRealtimeFilterMMSI(mmsi);
        return;
    }

    const existingFilters = viewer ? [...viewer.filterMMSIs] : [];
    if (viewer) { viewer.disconnect(); viewer = null; }
    window.realtimeViewerState = null;

    document.getElementById('realtime_tab').click();

    setTimeout(() => {
        existingFilters.forEach(f => addRealtimeFilterMMSI(f));
        addRealtimeFilterMMSI(mmsi);
    }, 50);
}

export function getRealtimeState() {
    if (!viewer) return null;
    return { filterMMSIs: viewer.filterMMSIs, backgroundStreaming: viewer.backgroundStreaming };
}

export function init({ selectMapTab: _selectMapTab }) {
    selectMapTab = _selectMapTab;

    // Sync background streaming checkbox if settings are changed externally.
    onSettingsChange(() => {
        const checkbox = document.getElementById('realtime_background_streaming');
        if (checkbox) checkbox.checked = settings.realtime_background_streaming;
        if (viewer) viewer.backgroundStreaming = settings.realtime_background_streaming;
    });

    if (typeof realtime_enabled === 'undefined' || !realtime_enabled) {
        document.getElementById('realtime_tab').style.display = 'none';
        document.getElementById('realtime_tab_mini').style.display = 'none';
        document.querySelectorAll('.ctx-realtime').forEach(el => el.style.display = 'none');
        const shipcardRealtime = document.getElementById('shipcard_realtime_option');
        if (shipcardRealtime) shipcardRealtime.style.display = 'none';
        return;
    }

    // Expose for inline onclick in filter chips (until updateFilterDisplay is refactored)
    window.removeRealtimeFilterMMSI = removeRealtimeFilterMMSI;

    registerActions({ promptFilterMMSI, toggleRealtimePause, clearRealtimeTable });
    registerChanges({ toggleBackgroundStreaming });
}
