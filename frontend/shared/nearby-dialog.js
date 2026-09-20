// Nearby uses the same side table on either host. One answer supplies the
// nearest ships, stations and ports; tabs and pages select from that answer.
import { flag, flagHTML, tabScroller } from './components.js';
import { createSideTable } from './side-table.js';
import { pageSize, renderPager, renderVesselRows, bindVesselHover, durationHTML } from './table.js';
import { sanitizeString, compactCount, getCountryName, getShipTypeShort, getStatusVal } from './core/text.js';
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
        const shell = createSideTable(host, 'nearby'), root = shell.root;
        root.setAttribute('aria-labelledby', 'nearby-title');
        root.innerHTML = '<header class="place-panel-header">' +
            '<div class="place-panel-actions"><button type="button" class="place-overview">← In view</button><span class="side-table-label">Nearby</span><button type="button" class="table-collapse place-close" title="Close sidebar" aria-label="Close sidebar">Close <span aria-hidden="true">→</span></button></div>' +
            '<div class="place-panel-heading"><div class="place-panel-label"><h2 id="nearby-title"></h2><div class="place-panel-kind"></div></div></div></header>' +
            '<div class="sc-tabs place-dialog-tabs" role="tablist" aria-label="Nearby">' + TABS.map(tab =>
            `<button type="button" class="sc-tab" role="tab" id="nearby-tab-${tab.key}" data-nearby-tab="${tab.key}" aria-controls="nearby-results">${tab.label}<span class="place-tab-count"></span></button>`).join('') + '</div>' +
            '<div id="nearby-results" class="port-results" role="tabpanel" aria-live="polite"><div class="tablecard_inner port-ship-list"><table class="nearby"><thead></thead><tbody></tbody></table><div class="port-status"></div></div></div>' +
            '<footer class="table-footer"><span class="table-count" aria-live="polite" title="Nearest first"></span><div class="table-pager"><button type="button" class="nearby-prev" aria-label="Previous page">‹</button><span></span><button type="button" class="nearby-next" aria-label="Next page">›</button></div></footer>';
        const title = root.querySelector('#nearby-title');
        const name = anchor.title || (anchor.mmsi ? 'MMSI ' + anchor.mmsi : 'Nearby');
        const pageUrl = host.nearbyPageUrl?.(anchor);
        if (pageUrl) {
            const link = document.createElement('a');
            link.href = pageUrl;
            link.textContent = name;
            title.append(link);
        } else title.textContent = name;
        const type = anchor.mmsi ? (anchor.shiptype ? getShipTypeShort(anchor.shiptype) : 'Vessel') : anchor.station ? 'Station' : '';
        const state = anchor.mmsi
            ? (Number.isInteger(anchor.status) && anchor.status >= 0 && anchor.status < 15 ? getStatusVal(anchor) : '')
            : typeof anchor.online === 'boolean' ? (anchor.online ? 'Online' : 'Offline') : '';
        root.querySelector('.place-panel-kind').textContent = [type, state].filter(Boolean).join(' · ');
        if (anchor.country) root.querySelector('.place-panel-heading').prepend(flag(anchor.country, 'place-panel-flag', getCountryName(anchor.country)));
        const results = root.querySelector('.port-results');
        const body = results.querySelector('tbody');
        const status = results.querySelector('.port-status');
        const strip = tabScroller(root.querySelector('.place-dialog-tabs'), 'place-dialog-tabs-wrap');
        const hover = bindVesselHover(body, {
            enter: id => host.hoverVessel?.(Number(id)),
            leave: id => host.unhoverVessel?.(Number(id)),
        });
        const previous = root.querySelector('.nearby-prev'), next = root.querySelector('.nearby-next');
        let selected = 'ships';
        let answer = null, failed = '';
        let page = 0, perPage = 10, loading = false, selectedId = null;
        const capacity = () => pageSize(root.querySelector('.tablecard_inner'), root.querySelector('thead'), body.querySelector('tr')) || 10;
        const pager = (total, shown = 0) => renderPager(root.querySelector('.table-pager'), {page, perPage, total, shown, loading});

        const draw = () => {
            hover.clear();
            for (const button of root.querySelectorAll('[data-nearby-tab]')) {
                const active = button.dataset.nearbyTab === selected;
                button.setAttribute('aria-selected', String(active));
                button.tabIndex = active ? 0 : -1;
            }
            results.setAttribute('aria-labelledby', 'nearby-tab-' + selected);
            const heads = {
                ships: '<th scope="col" class="col-name">Name</th><th scope="col" class="num col-spd">Spd <span class="dim">kts</span></th><th scope="col" class="col-type">Type</th><th scope="col" class="num col-last">Range</th>',
                stations: '<th scope="col" class="col-name">Station</th><th scope="col" class="nearby-heard">Heard</th><th scope="col" class="num col-last">Range</th>',
                places: '<th scope="col" class="col-name">Port</th><th scope="col" class="nearby-code">LOCODE</th><th scope="col" class="num col-last">Range</th>',
            };
            results.querySelector('thead').innerHTML = '<tr>' + heads[selected] + '</tr>';
            const rows = answer?.[selected] || [];
            page = Math.min(page, Math.max(0, Math.ceil(rows.length / perPage) - 1));
            const visible = rows.slice(page * perPage, (page + 1) * perPage);
            pager(rows.length, visible.length);
            if (!rows.length) {
                body.innerHTML = '';
                status.hidden = false;
                status.innerHTML = loading ? '<span class="port-loading" role="status" aria-label="Loading nearby"><span class="spinner icon-sm" aria-hidden="true"></span></span>'
                    : failed ? '<p role="status">Could not look around here. ' + text(failed) + '</p><button type="button" class="btn nearby-retry">Try again</button>'
                    : '<p>No ' + (selected === 'places' ? 'ports' : selected) + ' within 99 nm.</p>';
                return;
            }
            const now = answer.time || Math.floor(Date.now() / 1000);
            body.innerHTML = selected === 'ships' ? renderVesselRows(visible.map(row => {
                const id = Number(row.mmsi), sprite = spriteFor(row.shipclass, row.speed, row.cog);
                return {...row, id, name: row.shipname || `MMSI ${id}`, selected: selectedId === 'ships:' + id,
                    speed: typeof row.speed === 'number' && row.speed >= 0 ? row.speed.toFixed(1) : '—',
                    typeHTML: `<span class="table-shiptype-icon"><span class="sprites" style="background-position: -${sprite.cx}px -${sprite.cy}px; width: 20px; height: 20px; transform: rotate(${sprite.rot}rad)" title="${text(sprite.hint)}"></span></span>`,
                    lastHTML: away(row)};
            })) : visible.map(row => {
                if (selected === 'stations') {
                    const id = Number(row.id), name = text(row.name || `Station ${id}`);
                    const heard = row.last > 0 ? durationHTML(now - row.last) : '—';
                    const dot = `<span class="nearby-dot${row.online ? ' on' : ''}" title="${row.online ? 'Online' : 'Offline'}"></span>`;
                    return `<tr data-station="${id}" class="${selectedId === 'stations:' + id ? 'selected' : ''}"><td class="col-name" title="${name} · ID ${id}"><span class="table-name">${flagHTML(row.country)}<button type="button" class="vessel-link port-ship-link" translate="no">${name}</button></span></td><td>${dot}${heard}</td><td class="num col-last">${away(row)}</td></tr>`;
                }
                const name = text(row.label || row.code || 'Port');
                /* a port inside a larger one says whose it is: the codes are
                   what a reader quotes, and the parent is what places it */
                const parent = row.parent_code && row.parent_code !== row.code
                    ? `<span class="nearby-parent">${text(row.parent_code)}</span>` : '';
                return `<tr data-place="${text(row.runtime_id)}"><td class="col-name" title="${name}${row.code ? ' · ' + text(row.code) : ''}"><span class="table-name">${flagHTML(row.country)}<button type="button" class="vessel-link port-ship-link" translate="no">${name}</button></span></td><td class="nearby-kind" translate="no">${text(row.code || '—')}${parent}</td><td class="num col-last">${away(row)}</td></tr>`;
            }).join('');
            status.innerHTML = '';
            status.hidden = true;
            fit();
        };

        const load = async () => {
            const request = {};
            root._nearbyRequest = request;
            loading = true;
            failed = '';
            results.setAttribute('aria-busy', 'true');
            draw();
            const params = new URLSearchParams({ lat: String(lat), lon: String(lon) });
            if (anchor.mmsi) params.set('mmsi', String(anchor.mmsi));
            if (anchor.station) params.set('station', String(anchor.station));
            try {
                const url = host.nearbyUrl ? host.nearbyUrl(params) : 'nearby.json?' + params;
                const data = await host.fetchJSON(url);
                if (root._nearbyRequest !== request) return;
                if (data?.error) throw new Error(data.error);
                if (!TABS.every(tab => Array.isArray(data?.[tab.key]))) throw new Error('Invalid nearby list');
                answer = data;
                /* every tab stays, a zero included: "Stations 0" says there is
                   nothing within reach, where a missing tab only looks broken */
                for (const button of root.querySelectorAll('[data-nearby-tab]')) {
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
                if (root._nearbyRequest === request) failed = error.message || 'No answer';
            } finally {
                if (root._nearbyRequest === request) {
                    loading = false;
                    results.setAttribute('aria-busy', 'false');
                    if (failed) draw();
                    pager(answer?.[selected]?.length || 0, body.children.length);
                }
            }
        };

        root.onclick = e => {
            if (e.target.closest('.place-overview')) { shell.overview(); return; }
            if (e.target.closest('.place-close')) { shell.close(); return; }
            if (e.target.closest('.nearby-prev') && !previous.disabled) { page--; draw(); return; }
            if (e.target.closest('.nearby-next') && !next.disabled) { page++; draw(); return; }
            const tab = e.target.closest('[data-nearby-tab]');
            if (tab) { selected = tab.dataset.nearbyTab; page = 0; draw(); return; }
            if (e.target.closest('.nearby-retry')) { load(); return; }
            const row = e.target.closest('tr[data-mmsi], tr[data-station], tr[data-place]');
            if (!row) return;
            selectedId = selected + ':' + (row.dataset.mmsi || row.dataset.station || row.dataset.place);
            body.querySelectorAll('tr').forEach(tr => tr.classList.toggle('selected', tr === row));
            if (row.dataset.mmsi) return host.openVessel(Number(row.dataset.mmsi));
            if (row.dataset.station) return host.openStation?.(Number(row.dataset.station));
            const place = (answer.places || []).find(p => String(p.runtime_id) === row.dataset.place);
            if (place) host.openPlace?.({ ...place, place_version: answer.place_version });
        };
        root.querySelector('[role="tablist"]').onkeydown = e => {
            const buttons = [...root.querySelectorAll('[data-nearby-tab]')].filter(b => !b.hidden);
            let index = buttons.indexOf(document.activeElement);
            if (e.key === 'ArrowRight') index = (index + 1) % buttons.length;
            else if (e.key === 'ArrowLeft') index = (index + buttons.length - 1) % buttons.length;
            else if (e.key === 'Home') index = 0;
            else if (e.key === 'End') index = buttons.length - 1;
            else return;
            e.preventDefault(); buttons[index].focus(); buttons[index].click();
        };
        let resizeTimer;
        const fit = () => {
            clearTimeout(resizeTimer);
            resizeTimer = setTimeout(() => {
                if (!shell.isOpen()) return;
                const size = capacity();
                if (size === perPage) return;
                page = Math.floor(page * perPage / size);
                perPage = size;
                draw();
            }, 150);
        };
        const resize = new ResizeObserver(fit);
        resize.observe(root.querySelector('.tablecard_inner'));
        root._dispose = () => {
            hover.destroy();
            root._nearbyRequest = null;
            clearTimeout(resizeTimer);
            resize.disconnect();
            strip.destroy();
            root._dispose = null;
        };
        shell.open();
        perPage = capacity();
        return load();
    };
}
