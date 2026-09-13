// One nearby dialog on either host, over one answer: the hub returns ships,
// stations and places at once and the tabs choose which of the three to show.
import { modal, flagHTML, tabScroller } from './components.js';
import { sanitizeString, getDeltaTimeVal, compactCount } from './core/text.js';
import { spriteFor } from './core/sprites.js';

const text = value => sanitizeString(String(value ?? ''));
const POINTS = ['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW'];
const compass = bearing => Number.isFinite(bearing)
    ? POINTS[Math.round(((bearing % 360) + 360) % 360 / 45) % 8] : '';

/* an arrow points where the letters only name: the compass point stays as the
   label, for a reader who cannot see which way it turned */
const away = row => {
    if (typeof row.range !== 'number') return '—';
    const nm = `${row.range.toFixed(1)} nm`;
    if (!Number.isFinite(row.bearing)) return nm;
    const deg = Math.round(((row.bearing % 360) + 360) % 360);
    const point = compass(deg);
    return `${nm}<span class="nearby-bearing" style="transform: rotate(${deg}deg)" title="${point}" aria-label="${point}">&#8593;</span>`;
};

const TABS = [
    { key: 'ships', label: 'Ships' },
    { key: 'stations', label: 'Stations' },
    { key: 'places', label: 'Ports' },
];

export function createNearbyDialog(host) {
    return function openNearby(anchor) {
        const { lat, lon } = anchor;
        if (!Number.isFinite(lat) || !Number.isFinite(lon)) return;
        const dlg = modal({ id: 'nearby', title: anchor.title || 'Nearby', cardClass: 'modal-port-ships' });
        dlg.setTitle(anchor.title || 'Nearby');
        dlg.card.setAttribute('role', 'dialog');
        dlg.card.setAttribute('aria-modal', 'true');
        const title = dlg.root.querySelector('.dialog-title');
        title.id = 'nearby-title';
        title.insertAdjacentHTML('beforeend', '<span class="dialog-title-hint">nearby</span>');
        dlg.card.setAttribute('aria-labelledby', title.id);
        const close = dlg.root.querySelector('.close_icon');
        close.setAttribute('role', 'button');
        close.setAttribute('aria-label', 'Close nearby');
        close.tabIndex = 0;
        close.onkeydown = e => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); dlg.close(); } };
        // the dialog is reused between openings: take the count that is already
        // there, or a fresh one adds itself beside the last on every open
        const count = dlg.root.querySelector('.port-ship-count') || document.createElement('span');
        count.className = 'port-ship-count';
        count.setAttribute('aria-live', 'polite');
        close.before(count);

        dlg.body.innerHTML = '<div class="sc-tabs place-dialog-tabs" role="tablist" aria-label="Nearby">' + TABS.map(tab =>
            `<button type="button" class="sc-tab" role="tab" id="nearby-tab-${tab.key}" data-nearby-tab="${tab.key}" aria-controls="nearby-results">${tab.label}<span class="place-tab-count"></span></button>`).join('') + '</div>' +
            '<div id="nearby-results" class="port-results" role="tabpanel" aria-live="polite"><div class="tablecard_inner port-ship-list"><table><thead></thead><tbody></tbody></table><div class="port-status"></div></div></div>';
        const results = dlg.body.querySelector('.port-results');
        const body = results.querySelector('tbody');
        const status = results.querySelector('.port-status');
        const strip = tabScroller(dlg.body.querySelector('.place-dialog-tabs'), 'place-dialog-tabs-wrap');
        let selected = 'ships';
        let answer = null;

        const draw = () => {
            for (const button of dlg.body.querySelectorAll('[data-nearby-tab]')) {
                const active = button.dataset.nearbyTab === selected;
                button.setAttribute('aria-selected', String(active));
                button.tabIndex = active ? 0 : -1;
            }
            results.setAttribute('aria-labelledby', 'nearby-tab-' + selected);
            const heads = {
                ships: '<th scope="col" class="col-name">Name</th><th scope="col" class="num col-spd">Spd <span class="dim">kts</span></th><th scope="col" class="col-type">Type</th><th scope="col" class="num col-last">Range</th>',
                stations: '<th scope="col" class="col-name">Station</th><th scope="col">Heard</th><th scope="col" class="num col-last">Range</th>',
                places: '<th scope="col" class="col-name">Place</th><th scope="col">Kind</th><th scope="col" class="num col-last">Range</th>',
            };
            results.querySelector('thead').innerHTML = '<tr>' + heads[selected] + '</tr>';
            const rows = answer?.[selected] || [];
            count.textContent = rows.length ? `${rows.length} nearest` : '';
            if (!rows.length) {
                body.innerHTML = '';
                status.hidden = false;
                status.innerHTML = '<p>No ' + (selected === 'places' ? 'ports' : selected) + ' within 99 nm.</p>';
                return;
            }
            const now = answer.time || Math.floor(Date.now() / 1000);
            body.innerHTML = rows.map(row => {
                if (selected === 'ships') {
                    const id = Number(row.mmsi), name = text(row.shipname || `MMSI ${id}`);
                    const speed = typeof row.speed === 'number' && row.speed >= 0 ? row.speed.toFixed(1) : '—';
                    const sprite = spriteFor(row.shipclass, row.speed, row.cog);
                    const icon = `<span class="table-shiptype-icon"><span class="sprites" style="background-position: -${sprite.cx}px -${sprite.cy}px; width: 20px; height: 20px; transform: rotate(${sprite.rot}rad)" title="${text(sprite.hint)}"></span></span>`;
                    return `<tr data-mmsi="${id}"><td class="col-name" title="${name} · MMSI ${id}"><span class="table-name">${flagHTML(row.country)}<button type="button" class="port-ship-link" translate="no">${name}</button></span></td><td class="num col-spd">${speed}</td><td class="col-type">${icon}</td><td class="num col-last">${away(row)}</td></tr>`;
                }
                if (selected === 'stations') {
                    const id = Number(row.id), name = text(row.name || `Station ${id}`);
                    const heard = row.last > 0 ? text(getDeltaTimeVal(Math.max(0, now - row.last))) : '—';
                    const dot = `<span class="nearby-dot${row.online ? ' on' : ''}" title="${row.online ? 'Online' : 'Offline'}"></span>`;
                    return `<tr data-station="${id}"><td class="col-name" title="${name} · ID ${id}"><span class="table-name">${flagHTML(row.country)}<button type="button" class="port-ship-link" translate="no">${name}</button></span></td><td>${dot}${heard}</td><td class="num col-last">${away(row)}</td></tr>`;
                }
                const name = text(row.label || row.code || 'Place');
                const kind = text(row.place_type === 'custom' ? 'area' : row.place_type || 'place');
                return `<tr data-place="${row.runtime_id}"><td class="col-name" title="${name}${row.code ? ' · ' + text(row.code) : ''}"><span class="table-name">${flagHTML(row.country)}<button type="button" class="port-ship-link" translate="no">${name}</button></span></td><td class="nearby-kind">${kind}</td><td class="num col-last">${away(row)}</td></tr>`;
            }).join('');
            status.innerHTML = '';
            status.hidden = true;
        };

        const load = async () => {
            const request = {};
            dlg.root._nearbyRequest = request;
            results.setAttribute('aria-busy', 'true');
            body.innerHTML = '';
            status.hidden = false;
            status.innerHTML = '<span class="port-loading" role="status" aria-label="Loading"><span class="spinner icon-sm" aria-hidden="true"></span></span>';
            const params = new URLSearchParams({ lat: String(lat), lon: String(lon) });
            if (anchor.mmsi) params.set('mmsi', String(anchor.mmsi));
            if (anchor.station) params.set('station', String(anchor.station));
            try {
                const url = host.nearbyUrl ? host.nearbyUrl(params) : 'nearby.json?' + params;
                const data = await host.fetchJSON(url);
                if (dlg.root._nearbyRequest !== request || !dlg.isOpen()) return;
                if (data?.error) throw new Error(data.error);
                answer = data;
                /* every tab stays, a zero included: "Stations 0" says there is
                   nothing within reach, where a missing tab only looks broken */
                for (const button of dlg.body.querySelectorAll('[data-nearby-tab]')) {
                    const rows = data[button.dataset.nearbyTab];
                    button.querySelector('.place-tab-count').textContent =
                        ` ${compactCount(Array.isArray(rows) ? rows.length : 0)}`;
                }
                strip.update();
                // but open on one that has something to show
                const first = TABS.find(tab => (data[tab.key] || []).length);
                if (first && !(answer[selected] || []).length) selected = first.key;
                draw();
            } catch (error) {
                if (dlg.root._nearbyRequest === request && dlg.isOpen())
                    status.innerHTML = '<p role="status">Could not look around here. ' + text(error.message) + '</p><button type="button" class="btn nearby-retry">Try again</button>';
            } finally { if (dlg.root._nearbyRequest === request) results.setAttribute('aria-busy', 'false'); }
        };

        dlg.body.onclick = e => {
            const tab = e.target.closest('[data-nearby-tab]');
            if (tab) { selected = tab.dataset.nearbyTab; draw(); return; }
            if (e.target.closest('.nearby-retry')) { load(); return; }
            const row = e.target.closest('tr[data-mmsi], tr[data-station], tr[data-place]');
            if (!row) return;
            dlg.close();
            if (row.dataset.mmsi) return host.openVessel(Number(row.dataset.mmsi));
            if (row.dataset.station) return host.openStation?.(Number(row.dataset.station));
            const place = (answer.places || []).find(p => String(p.runtime_id) === row.dataset.place);
            if (place) host.openPlace?.({ ...place, place_version: answer.place_version });
        };
        dlg.body.querySelector('[role="tablist"]').onkeydown = e => {
            const buttons = [...dlg.body.querySelectorAll('[data-nearby-tab]')].filter(b => !b.hidden);
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
