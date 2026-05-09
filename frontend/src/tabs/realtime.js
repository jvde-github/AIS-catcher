import { settings } from '../core/state.js';

let viewer = null;
let savedState = null;
let keydownHandler = null;

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
        if (this.filterMMSIs.length > 0 && !this.filterMMSIs.includes(data.mmsi.toString())) return;

        if (this.nmeaCount > this.maxLines) {
            const rows = this.nmeaContent.getElementsByTagName('tr');
            if (rows.length > 0) {
                this.nmeaContent.removeChild(rows[rows.length - 1]);
            }
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
            decoderIcon.innerHTML = '<i class="decode_icon"></i>';
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

        mmsiSpan.style.border = '1px solid #d0d0d0';
        mmsiSpan.style.padding = '2px 6px';
        mmsiSpan.style.borderRadius = '3px';
        mmsiSpan.style.display = 'inline-block';
        mmsiSpan.style.cursor = 'pointer';
        mmsiSpan.style.fontSize = '0.85em';
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
        this.nmeaCount++;
    }

    pause() { this.isPaused = true; }
    resume() {
        this.isPaused = false;
        this.connectNmea();
    }

    addFilterMMSI(mmsi) {
        const mmsiStr = mmsi.toString();
        if (!this.filterMMSIs.includes(mmsiStr)) this.filterMMSIs.push(mmsiStr);
    }

    removeFilterMMSI(mmsi) {
        const mmsiStr = mmsi.toString();
        this.filterMMSIs = this.filterMMSIs.filter((m) => m !== mmsiStr);
    }

    clearFilters() {
        this.filterMMSIs = [];
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

function updateFilterDisplay() {
    if (!viewer) return;
    const filterDisplay = document.getElementById('realtime_filter_display');
    const filterChips = document.getElementById('realtime_mmsi_filters');
    if (!filterDisplay || !filterChips) return;

    if (viewer.filterMMSIs.length > 0) {
        filterChips.textContent = '';
        viewer.filterMMSIs.forEach((mmsi) => {
            const chip = document.createElement('div');
            chip.className = 'filter-chip';

            const label = document.createElement('span');
            label.textContent = mmsi;

            const btn = document.createElement('button');
            btn.title = 'Remove filter';
            btn.dataset.action = 'removeRealtimeFilterMMSI';
            btn.dataset.mmsi = mmsi;
            const icon = document.createElement('i');
            icon.className = 'close_icon';
            btn.appendChild(icon);

            chip.appendChild(label);
            chip.appendChild(btn);
            filterChips.appendChild(chip);
        });
        filterDisplay.style.display = 'flex';
    } else {
        filterDisplay.style.display = 'none';
    }
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

        if (settings.realtime_filter_mmsis && Array.isArray(settings.realtime_filter_mmsis)) {
            viewer.filterMMSIs = [...settings.realtime_filter_mmsis];
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
        addFilter(mmsi);
        return;
    }

    const existingFilters = viewer ? [...viewer.filterMMSIs] : [];

    if (viewer) {
        viewer.disconnect();
        viewer = null;
    }
    savedState = null;

    document.getElementById('realtime_tab').click();

    setTimeout(() => {
        existingFilters.forEach((f) => addFilter(f));
        addFilter(mmsi);
    }, 50);
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
        viewer.nmeaCount = 0;
    }
}

export function promptFilter() {
    const mmsiInput = prompt('Enter MMSI to filter:');
    if (!mmsiInput) return;
    const mmsi = parseInt(mmsiInput.trim(), 10);
    if (!isNaN(mmsi) && mmsi > 0) {
        addFilter(mmsi);
    } else {
        alert('Please enter a valid MMSI number.');
    }
}

export function addFilter(mmsi) {
    if (!viewer) return;
    const filterValue = mmsi ? mmsi.toString().trim() : '';
    if (!filterValue) return;
    viewer.addFilterMMSI(filterValue);
    updateFilterDisplay();
    window.__app__.saveSettings();
}

export function removeFilter(mmsi) {
    if (!viewer) return;
    viewer.removeFilterMMSI(mmsi);
    updateFilterDisplay();
    window.__app__.saveSettings();
}

export function getFilterMMSIs() {
    return viewer ? viewer.filterMMSIs : null;
}

export function getBackgroundStreaming() {
    return viewer ? viewer.backgroundStreaming : null;
}
