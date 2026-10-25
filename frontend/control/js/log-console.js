// The receiver's log in System › Log: the lines the event stream brings, kept in a buffer and
// filtered by level and words. `onAlert(level)` hears of each new warning or error, once the
// stream has replayed what came before it.

const LEVELS = ['debug', 'info', 'warning', 'error', 'critical'];
const BUFFER_MAX = 500;
const LEVEL_KEY = 'hub-log-level';

export function createLogConsole({ onAlert } = {}) {
    const buffer = [];
    let box = null;
    let replayDone = false;
    let lastTime = '', lastSeq = -1;
    let levelMin = 'info', search = '';
    try {
        const stored = localStorage.getItem(LEVEL_KEY);
        if (LEVELS.includes(stored)) levelMin = stored;
    } catch { /* private mode: the default level */ }

    function passes(m) {
        const i = LEVELS.indexOf(m.level);
        if (i >= 0 && i < LEVELS.indexOf(levelMin)) return false;
        return !search || String(m.message).toLowerCase().includes(search);
    }

    // a line the stream sent again after a reconnect
    function isReplayed(m) {
        if (typeof m.seq !== 'number') return false;
        const d = (m.seq - lastSeq) >>> 0;
        const ahead = lastSeq < 0 || (d > 0 && d < 0x80000000);
        if (!ahead && (!m.time || m.time <= lastTime)) return true;
        lastSeq = m.seq;
        return false;
    }

    function line(m) {
        const level = ['error', 'critical'].includes(m.level) ? 'error'
            : ['warning', 'info', 'debug'].includes(m.level) ? m.level : 'default';
        const row = document.createElement('div');
        row.className = 'log-line ' + level;
        const ts = document.createElement('span');
        ts.className = 'log-ts';
        const time = String(m.time || '');
        ts.textContent = time.length > 19 ? time.slice(11, 19) : time;
        ts.title = time;
        const msg = document.createElement('span');
        msg.textContent = m.message;
        row.append(ts, msg);
        return row;
    }

    function render() {
        if (!box) return;
        box.textContent = '';
        buffer.forEach(m => { if (passes(m)) box.appendChild(line(m)); });
        if (!box.childElementCount) {
            const empty = document.createElement('div');
            empty.className = 'log-empty';
            empty.textContent = buffer.length ? 'No lines match the filter.' : 'Waiting for log output...';
            box.appendChild(empty);
        }
        box.scrollTop = box.scrollHeight;
    }

    function onEvent(e) {
        let m;
        try { m = JSON.parse(e.data); } catch { return; }
        if (isReplayed(m)) return;
        if (m.time && m.time > lastTime) lastTime = m.time;
        buffer.push(m);
        if (buffer.length > BUFFER_MAX) buffer.shift();
        if (box && passes(m)) {
            box.querySelector('.log-empty')?.remove();
            box.appendChild(line(m));
            while (box.childElementCount > BUFFER_MAX) box.removeChild(box.firstChild);
            box.scrollTop = box.scrollHeight;
        }
        if (!replayDone) return;
        if (m.level === 'error' || m.level === 'critical' || m.level === 'warning')
            onAlert?.(m.level === 'warning' ? 'warning' : 'error');
    }

    // the console's toolbar and lines, in the Log pane of a panel just built
    function mount(pane) {
        box = pane.querySelector('#log-box');
        const level = pane.querySelector('#log-level');
        level.value = levelMin;
        level.addEventListener('change', () => {
            levelMin = level.value;
            try { localStorage.setItem(LEVEL_KEY, levelMin); } catch { /* private mode: for this visit only */ }
            render();
        });
        const words = pane.querySelector('#log-search');
        words.value = search;
        words.addEventListener('input', () => {
            search = words.value.trim().toLowerCase();
            render();
        });
        render();
    }

    return {
        onEvent,
        mount,
        // the stream to open: from where the last one left off
        streamUrl: () => '/api/stream' + (lastSeq >= 0 ? '?since=' + lastSeq : ''),
        // a stream opening replays the lines before it; the toasts start once those are through
        starting() { replayDone = false; },
        opened() { if (!replayDone) setTimeout(() => { replayDone = true; }, 500); },
        scrollToEnd() { if (box) box.scrollTop = box.scrollHeight; }
    };
}
