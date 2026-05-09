export class LogViewer {
    constructor() {
        this.logState = document.getElementById('log_state');
        this.logScroll = document.getElementById('log_scroll');
        this.logContent = document.getElementById('log_content');
        this.eventSource = null;
    }

    connect() {
        if (this.eventSource) return;
        this.eventSource = new EventSource("api/log");

        this.eventSource.addEventListener('log', (event) => {
            try {
                const data = JSON.parse(event.data);
                this.appendFormattedLog(data);
                this.scrollToBottom();
            } catch (error) {
                console.error('Error handling log event:', error);
                this.appendRawLog(event.data);
            }
        });

        this.eventSource.addEventListener('open', () => {
            this.updateConnectionState(true);
        });

        this.eventSource.addEventListener('error', () => {
            this.updateConnectionState(false);
        });

        this.logContent.innerHTML = '';
    }

    appendFormattedLog(data) {
        const logEntry = document.createElement('div');
        logEntry.className = 'log-entry ' + (data.level || 'info');

        const icon = document.createElement('span');
        switch (data.level) {
            case 'error':
                icon.className = 'error_od_icon';
                break;
            case 'warning':
                icon.className = 'warning_od_icon';
                break;
            default:
                icon.className = '';
        }
        icon.style.flexShrink = '0';
        icon.style.marginTop = '0.2rem';
        icon.style.width = '1.2em';
        icon.style.height = '1.2em';

        const content = document.createElement('div');
        content.style.flex = '1';

        const timestamp = document.createElement('div');
        timestamp.className = 'timestamp';
        timestamp.textContent = data.time;
        timestamp.style.fontSize = '0.75rem';
        timestamp.style.marginBottom = '0.05rem';

        const message = document.createElement('div');
        message.textContent = data.message.replace(/^"|"$/g, '');

        content.appendChild(timestamp);
        content.appendChild(message);
        logEntry.appendChild(icon);
        logEntry.appendChild(content);

        this.logContent.appendChild(logEntry);
    }

    appendRawLog(message) {
        const logEntry = document.createElement('div');
        logEntry.textContent = message + '\n';
        this.logContent.appendChild(logEntry);
    }

    disconnect() {
        if (this.eventSource) {
            this.eventSource.close();
            this.eventSource = null;
        }
        this.updateConnectionState(false);
    }

    scrollToBottom() {
        requestAnimationFrame(() => {
            this.logScroll.scrollTop = this.logScroll.scrollHeight;
        });
    }

    updateConnectionState(connected) {
        const stateIcon = document.createElement('span');
        stateIcon.className = connected ? 'info_od_icon' : 'error_od_icon';
        stateIcon.style.display = 'inline-block';
        stateIcon.style.verticalAlign = 'middle';
        stateIcon.style.marginRight = '0.5em';
        stateIcon.style.width = '1em';
        stateIcon.style.height = '1em';

        const status = connected ? 'Connected' : 'Disconnected';
        this.logState.innerHTML = '';
        this.logState.display = 'none';
        if (!connected) {
            this.logState.appendChild(stateIcon);
            this.logState.appendChild(document.createTextNode(`Status: ${status}`));
        }
    }
}
