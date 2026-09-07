// Destination lists use the host's transport and vessel navigation on either map.
import { modal } from './components.js';
import { sanitizeString, formatDateTime, getDeltaTimeVal } from './core/text.js';

const text = value => sanitizeString(String(value ?? ''));

export function createPortDialog(host) {
    return function openPorts(ports) {
        const dlg = modal({ id: 'port-ships', title: 'Ships bound for port', cardClass: 'modal-port-ships' });
        dlg.card.setAttribute('role', 'dialog');
        dlg.card.setAttribute('aria-modal', 'true');
        const title = dlg.root.querySelector('.dialog-title');
        title.id = 'port-ships-title';
        dlg.card.setAttribute('aria-labelledby', title.id);
        const close = dlg.root.querySelector('.close_icon');
        close.setAttribute('role', 'button');
        close.setAttribute('aria-label', 'Close port ships');
        close.tabIndex = 0;
        close.onkeydown = e => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); dlg.close(); } };

        const choices = ports.length > 1
            ? `<label class="port-choice">Port <select aria-label="Port">${ports.map((p, i) => `<option value="${i}">${text(p.label || p.code)} · ${text(p.code)}</option>`).join('')}</select></label>` : '';
        dlg.body.innerHTML = choices + '<div class="port-results" aria-live="polite"></div>';
        const results = dlg.body.querySelector('.port-results');
        const load = async port => {
            const request = {};
            dlg.root._portRequest = request;
            dlg.setTitle(port.label || port.code || 'Port');
            results.innerHTML = '<p class="dim" role="status">Loading ships…</p>';
            const code = port.code || String(port.id || '').replace(/^p/, '');
            try {
                if (!code) throw new Error('Missing port code');
                const url = host.portShipsUrl ? host.portShipsUrl(code) : `ships_port.json?code=${encodeURIComponent(code)}`;
                const data = await host.fetchJSON(url);
                if (dlg.root._portRequest !== request || !dlg.isOpen()) return;
                if (!data || !Array.isArray(data.ships)) throw new Error('Invalid ship list');
                const ships = data.ships.slice(0, 10);
                const heading = `<p class="dim">Ships reporting ${text(code)} as their destination.</p>`;
                if (!ships.length) {
                    results.innerHTML = heading + '<p>No ships currently report this destination.</p>';
                    return;
                }
                const now = Math.floor(Date.now() / 1000);
                const rows = ships.map(ship => {
                    const id = Number(ship.mmsi);
                    const name = text(ship.shipname || `MMSI ${id}`);
                    const speed = typeof ship.speed === 'number' && ship.speed >= 0 ? `${ship.speed.toFixed(1)} kn` : '—';
                    const age = ship.timestamp > 0 ? text(getDeltaTimeVal(Math.max(0, now - ship.timestamp))) : '—';
                    const when = ship.timestamp > 0 ? text(formatDateTime(ship.timestamp)) : '';
                    return `<tr data-mmsi="${id}"><td class="col-name" title="${name} · MMSI ${id}"><button type="button" class="port-ship-link">${name}</button></td><td class="num col-spd">${speed}</td><td class="num col-last" title="${when}">${age}</td></tr>`;
                }).join('');
                results.innerHTML = heading + `<p class="dim">${ships.length} of ${text(data.total ?? ships.length)} ships · latest reports first</p>` +
                    `<div class="tablecard_inner port-ship-list"><table><thead><tr><th scope="col" class="col-name">Vessel</th><th scope="col" class="num col-spd">Speed</th><th scope="col" class="num col-last" title="Last report">Last</th></tr></thead><tbody>${rows}</tbody></table></div>`;
            } catch (_) {
                if (dlg.root._portRequest === request && dlg.isOpen())
                    results.innerHTML = '<p role="status">Could not load ships for this port.</p><button type="button" class="btn port-retry">Try again</button>';
            }
        };
        let selected = ports[0];
        dlg.body.onchange = e => {
            if (e.target.matches('.port-choice select')) {
                selected = ports[Number(e.target.value)];
                load(selected);
            }
        };
        dlg.body.onclick = e => {
            if (e.target.closest('.port-retry')) { load(selected); return; }
            const link = e.target.closest('.port-ship-list tr[data-mmsi]');
            if (link) { dlg.close(); host.openVessel(Number(link.dataset.mmsi)); }
        };
        dlg.open();
        close.focus();
        return load(selected);
    };
}
