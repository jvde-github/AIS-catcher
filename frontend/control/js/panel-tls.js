// The panel's own HTTPS on the Access page. The station checks the files before it keeps them and
// the next connection is the new kind, so the page then goes to the other address. Set on the
// command line, it only shows. A path outside the configuration's folder stays as the file has it.

import { api } from './api.js';
import { App } from './config-store.js';
import { outsideFolder, tlsLine } from './services-editor.js';

// the same address over the other scheme
export function addressFor(tls, { hostname, port, pathname }) {
    const host = hostname.includes(':') && !hostname.startsWith('[') ? `[${hostname}]` : hostname;
    return `${tls ? 'https' : 'http'}://${host}${port ? ':' + port : ''}${pathname}`;
}

// what the page says: the files that would stop the next start come before anything else
export function panelTLSState(tls, now) {
    if (tls.problem) return { warn: true, text: `${tls.problem}. The next start stops until this is fixed.` };
    const line = tlsLine({ tls: tls.cert ? tls : undefined }, tls.until, now);
    const text = line.warn ? `${line.warn}: browsers refuse it, renew the files` : line.state;
    return { warn: !!line.warn, text: tls.fixed ? `${text}. Set on the command line for this run.` : text };
}

export async function wirePanelTLS(root) {
    const form = root.querySelector('#panel-tls-form');
    const cert = root.querySelector('#panel-tls-cert');
    const key = root.querySelector('#panel-tls-key');
    const state = root.querySelector('#panel-tls-state');
    const [status, tls] = await Promise.all([api('/api/status'), api('/api/panel-tls')]);
    cert.value = tls.cert || '';
    key.value = tls.key || '';

    const { text, warn } = panelTLSState(tls);
    state.textContent = text;
    state.classList.toggle('t-warn', warn);
    for (const input of [cert, key])
        input.readOnly = !!tls.fixed || outsideFolder(input.value, status.config_folder);
    root.querySelector('[form="panel-tls-form"]').disabled = !!tls.fixed;

    form.addEventListener('submit', async e => {
        e.preventDefault();
        const c = cert.value.trim(), k = key.value.trim();
        try {
            await api('/api/panel-tls', { body: c || k ? `${c}\n${k}` : '' });
            window.location.assign(addressFor(!!c, window.location));
        } catch (err) {
            App.notify('error', err.message || 'Failed to set HTTPS');
        }
    });
}
