// One place dialog on either host, backed by observed visits and destinations.
import { modal, flagHTML } from './components.js';
import { sanitizeString, formatDateTime, getDeltaTimeVal } from './core/text.js';
import { spriteFor } from './core/sprites.js';
const text = value => sanitizeString(String(value ?? ''));
const timeHTML = value => value > 0 ? `<span title="${text(formatDateTime(value))}">${text(new Date(value * 1000).toLocaleString([], {day:'numeric',month:'short',hour:'2-digit',minute:'2-digit'}))}</span>` : 'Unknown';

export function createPortDialog(host) {
    return function openPlaces(places) {
        const place = places[0];
        const isPort = !place.place_type || place.place_type === 'port';
        const tabs = ['Inside', 'Left', 'Arrived', 'Visits', ...(isPort ? ['Expected'] : [])];
        let selected = Number.isInteger(place.runtime_id) ? 'inside' : 'expected';
        const dlg = modal({id:'port-ships', title:place.label || place.code || 'Place', cardClass:'modal-port-ships'});
        dlg.setTitle(place.label || place.code || 'Place');
        dlg.card.setAttribute('role', 'dialog');
        dlg.card.setAttribute('aria-modal', 'true');
        const title = dlg.root.querySelector('.dialog-title');
        title.id = 'port-ships-title';
        // the code beside the name, subdued: the name is what a reader looks for, the code is what they quote
        if (place.code && place.code !== (place.label || ''))
            title.insertAdjacentHTML('beforeend', `<span class="dialog-title-hint">${sanitizeString(place.code)}</span>`);
        dlg.card.setAttribute('aria-labelledby', title.id);
        const close = dlg.root.querySelector('.close_icon');
        close.setAttribute('role', 'button');
        close.setAttribute('aria-label', 'Close place ships');
        const count = dlg.root.querySelector('.port-ship-count') || document.createElement('span');
        count.className = 'port-ship-count';
        count.setAttribute('aria-live', 'polite');
        close.before(count);
        close.tabIndex = 0;
        close.onkeydown = e => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); dlg.close(); } };
        dlg.body.innerHTML = '<div class="sc-tabs place-dialog-tabs" role="tablist" aria-label="Place activity">' + tabs.map(label =>
            `<button type="button" class="sc-tab" role="tab" id="place-tab-${label.toLowerCase()}" data-place-tab="${label.toLowerCase()}" aria-controls="place-results">${label}<span class="place-tab-count"></span></button>`).join('') + '</div>' +
            '<div class="place-dialog-note"></div><div id="place-results" class="port-results" role="tabpanel" aria-live="polite"><div class="tablecard_inner port-ship-list"><table><thead></thead><tbody></tbody></table><div class="port-status"></div></div></div>';
        const results = dlg.body.querySelector('.port-results'), body = results.querySelector('tbody'), status = results.querySelector('.port-status');
        const load = async () => {
            const request = {};
            dlg.root._portRequest = request;
            const visitRows = selected === 'visits';
            dlg.body.querySelectorAll('[data-place-tab]').forEach(button => {
                const active = button.dataset.placeTab === selected;
                button.setAttribute('aria-selected', String(active)); button.tabIndex = active ? 0 : -1;
            });
            results.setAttribute('aria-labelledby', 'place-tab-' + selected);
            results.querySelector('thead').innerHTML = '<tr><th scope="col" class="col-name">Name</th>' + (visitRows
                ? '<th scope="col">Entry</th><th scope="col">Exit</th><th scope="col">Duration</th>'
                : '<th scope="col" class="num col-spd">Spd <span class="dim">kts</span></th><th scope="col" class="col-type">Type</th><th scope="col" class="num col-last">' + ({arrived:'Arrived',left:'Left'}[selected] || 'Last') + '</th>') + '</tr>';
            count.textContent = '';
            results.setAttribute('aria-busy', 'true'); body.innerHTML = '';
            status.hidden = false;
            status.innerHTML = '<span class="port-loading" role="status" aria-label="Loading ships"><span class="spinner icon-sm" aria-hidden="true"></span></span>';
            dlg.body.querySelector('.place-dialog-note').textContent = '';
            const params = new URLSearchParams({tab:selected,hours:'0'});
            if (Number.isInteger(place.runtime_id)) { params.set('id', place.runtime_id); params.set('version', place.place_version || ''); }
            else params.set('code', place.code || String(place.id || '').replace(/^p/, ''));
            try {
                const url = host.placeShipsUrl ? host.placeShipsUrl(params) : 'ships_place.json?' + params;
                const data = await host.fetchJSON(url);
                if (dlg.root._portRequest !== request || !dlg.isOpen()) return;
                if (data?.error) throw new Error(data.error);
                if (!Array.isArray(data?.ships)) throw new Error('Invalid ship list');
                for (const button of dlg.body.querySelectorAll('[data-place-tab]')) {
                    const total = data.counts?.[button.dataset.placeTab];
                    button.querySelector('.place-tab-count').textContent = total == null ? '' : ` ${total}`;
                }
                const ships = data.ships.slice(0, 10);
                count.textContent = `${ships.length} out of ${data.total ?? ships.length}`;
                count.title = 'Newest first';
                dlg.body.querySelector('.place-dialog-note').textContent = selected === 'expected' && !data.has_geometry
                    ? 'No port boundary: ships already in port cannot be excluded.'
                    : '';
                if (!ships.length) { status.innerHTML = '<p>No ' + (selected === 'visits' ? 'visits' : 'ships') + ' in this view.</p>'; return; }
                const now = data.time || Math.floor(Date.now() / 1000);
                body.innerHTML = ships.map(ship => {
                    const id = Number(ship.mmsi), name = text(ship.shipname || `MMSI ${id}`);
                    const first = `<td class="col-name" title="${name} · MMSI ${id}"><span class="table-name">${flagHTML(ship.country)}<button type="button" class="port-ship-link">${name}</button></span></td>`;
                    if (visitRows) {
                        const end = ship.exited ?? (ship.inside && !ship.pending ? now : null);
                        const duration = ship.pending ? '—' : ship.entered && end != null ? getDeltaTimeVal(Math.max(0, end - ship.entered)) : 'Unknown';
                        return `<tr data-mmsi="${id}">${first}<td>${ship.pending && ship.inside ? 'Confirming…' : timeHTML(ship.entered)}</td><td>${ship.inside ? '—' : ship.pending ? 'Confirming…' : timeHTML(ship.exited)}</td><td>${text(duration)}</td></tr>`;
                    }
                    const speed = typeof ship.speed === 'number' && ship.speed >= 0 ? ship.speed.toFixed(1) : '—';
                    const sprite = spriteFor(ship.shipclass, ship.speed, ship.cog);
                    const icon = `<span class="table-shiptype-icon"><span class="sprites" style="background-position: -${sprite.cx}px -${sprite.cy}px; width: 20px; height: 20px; transform: rotate(${sprite.rot}rad)" title="${text(sprite.hint)}"></span></span>`;
                    const when = selected === 'arrived' ? timeHTML(ship.entered) : selected === 'left' ? timeHTML(ship.exited) : ship.timestamp > 0 ? text(getDeltaTimeVal(Math.max(0, now - ship.timestamp))) : '—';
                    return `<tr data-mmsi="${id}">${first}<td class="num col-spd">${speed}</td><td class="col-type">${icon}</td><td class="num col-last">${when}</td></tr>`;
                }).join('');
                status.innerHTML = ''; status.hidden = true;
            } catch (error) {
                if (dlg.root._portRequest === request && dlg.isOpen())
                    status.innerHTML = '<p role="status">Could not load this place. ' + text(error.message) + '</p><button type="button" class="btn port-retry">Try again</button>';
            } finally { if (dlg.root._portRequest === request) results.setAttribute('aria-busy', 'false'); }
        };
        dlg.body.onclick = e => {
            const tab = e.target.closest('[data-place-tab]');
            if (tab) { selected = tab.dataset.placeTab; load(); return; }
            if (e.target.closest('.port-retry')) { load(); return; }
            const row = e.target.closest('tr[data-mmsi]');
            if (row) { dlg.close(); host.openVessel(Number(row.dataset.mmsi)); }
        };
        dlg.body.querySelector('[role="tablist"]').onkeydown = e => {
            const buttons = [...dlg.body.querySelectorAll('[data-place-tab]')];
            let index = buttons.indexOf(document.activeElement);
            if (e.key === 'ArrowRight') index = (index + 1) % buttons.length;
            else if (e.key === 'ArrowLeft') index = (index + buttons.length - 1) % buttons.length;
            else if (e.key === 'Home') index = 0;
            else if (e.key === 'End') index = buttons.length - 1;
            else return;
            e.preventDefault(); buttons[index].focus(); buttons[index].click();
        };
        dlg.open(); close.focus(); return load();
    };
}
