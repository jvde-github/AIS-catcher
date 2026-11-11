import { el } from './dom.js';
import { icon } from './icons.js';

const PASSING_MS = 4000;

export function createMessageBar({ settings, page, inSettings }) {
    let msg = null, timer = null;

    function close() {
        clearTimeout(timer);
        msg = null;
        render();
    }

    function paint(node, m) {
        node.hidden = !m;
        if (!m) return node.replaceChildren();
        node.dataset.tone = m.tone;
        node.replaceChildren(
            el('span', 'msgbar__text', { text: m.text }),
            el('button', 'msgbar__close', { type: 'button', title: 'Close', 'aria-label': 'Close', html: icon('close', ''), onClick: close }));
    }

    function render() {
        const here = inSettings();
        paint(settings, here ? msg : null);
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

    return { show, render };
}
