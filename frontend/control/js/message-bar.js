// The panel's messages, one at a time, in a bar: under the Settings header while Settings is open,
// else above the bottom bar. Errors and warnings stay until closed; the rest go by themselves.
// After a save, the bar in Settings asks for the restart that puts it to use.

import { el } from './dom.js';

const PASSING_MS = 4000;
const CLOSE_SVG = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" aria-hidden="true"><path d="M6 6l12 12M18 6L6 18"/></svg>';

// `settings` and `page` the bars, `inSettings()` whether Settings is open, `onRestart` the
// receiver's start or restart
export function createMessageBar({ settings, page, inSettings, onRestart }) {
    let msg = null, timer = null;
    let restart = null, restartClosed = false;

    function close() {
        if (msg) {
            clearTimeout(timer);
            msg = null;
        } else restartClosed = true;
        render();
    }

    function paint(node, m) {
        node.hidden = !m;
        if (!m) return node.replaceChildren();
        node.dataset.tone = m.tone;
        node.replaceChildren(...[
            el('span', 'msgbar__text', { text: m.text }),
            m.action ? el('button', 'st-btn st-btn--sm st-btn--primary msgbar__action', { type: 'button', text: m.action, onClick: onRestart }) : null,
            el('button', 'msgbar__close', { type: 'button', title: 'Close', 'aria-label': 'Close', html: CLOSE_SVG, onClick: close })].filter(Boolean));
    }

    function render() {
        const here = inSettings();
        paint(settings, here ? msg || (restart && !restartClosed ? restart : null) : null);
        paint(page, here ? null : msg);
    }

    function show(tone, text) {
        clearTimeout(timer);
        msg = { tone, text };
        if (tone === 'success' || tone === 'info') timer = setTimeout(() => { msg = null; render(); }, PASSING_MS);
        render();
        const shown = msg;
        return () => { if (msg === shown) close(); };
    }

    // 'restart' or 'start' while saved changes wait for the receiver, null once it has them
    function setRestart(state) {
        const was = restart && restart.state;
        if (state === was) return;
        restart = state ? { state, tone: 'info', text: `Changes saved. ${state === 'restart' ? 'Restart' : 'Start'} the receiver to use them.`,
            action: state === 'restart' ? 'Restart receiver' : 'Start receiver' } : null;
        restartClosed = false;
        if (restart && msg && (msg.tone === 'success' || msg.tone === 'info')) {
            clearTimeout(timer);
            msg = null;
        }
        render();
    }

    return { show, setRestart, render };
}
