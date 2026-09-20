// A place in the shared side table, backed by observed visits and destinations.
import { flag, tabScroller } from './components.js';
import { createSideTable } from './side-table.js';
import { pageSize, renderPager, renderVesselRows, vesselNameCell, bindVesselHover, durationHTML } from './table.js';
import { sanitizeString, compactCount, getCountryName } from './core/text.js';
import { spriteFor } from './core/sprites.js';
const text = value => sanitizeString(String(value ?? ''));
function timeHTML(value, now, bound = '') {
    if (!(value > 0)) return 'Unknown';
    const date = new Date(value * 1000);
    const clock = date.toLocaleTimeString([], {hour:'2-digit', minute:'2-digit', hourCycle:'h23'});
    const label = date.toDateString() === new Date(now * 1000).toDateString()
        ? clock : date.toLocaleDateString([], {day:'numeric', month:'short'}) + ' ' + clock;
    const full = date.toLocaleString([], {year:'numeric', month:'short', day:'numeric', hour:'2-digit', minute:'2-digit', second:'2-digit', hourCycle:'h23', timeZoneName:'short'});
    const title = (bound === '<' ? 'Before ' : bound === '>' ? 'After ' : '') + full;
    return `<span class="place-time" title="${text(title)}">${bound ? text(bound) + ' ' : ''}${text(label)}</span>`;
}

function visitDuration(ship) {
    if (ship.pending) return '—';
    const start = ship.entered ?? ship.entered_before;
    const end = ship.inside ? ship.observed_until : ship.exited ?? ship.exited_after;
    if (!(start > 0) || !(end >= start)) return 'Unknown';
    const minimum = ship.inside || !ship.entered || !ship.exited;
    return durationHTML(end - start, minimum ? '> ' : '');
}

export function createPortDialog(host) {
    return function openPlaces(places) {
        const place = places[0];
        const isPort = !place.place_type || place.place_type === 'port';
        // Nearby answers for every place, including the thousands that are a
        // point and can never hold a visit; Expected reads a port's reported
        // destinations. Whether the four visit tabs are worth showing follows
        // from the outline, which the marker row already knows - a count of
        // zero is a quiet hour, not a reason to take a tab away mid-read.
        const drawn = place.has_geometry !== false;
        const tabs = [...(drawn ? ['Inside', 'Left', 'Arrived', 'Visits'] : []),
                      'Closest', ...(isPort ? ['Expected'] : [])];
        let selected = drawn && Number.isInteger(place.runtime_id) ? 'inside' : 'closest';
        const shell = createSideTable(host, 'port-ships');
        const root = shell.root;
        const listLimit = Number.isInteger(host.placeListLimit) ? Math.max(0, Math.min(100, host.placeListLimit)) : 0;
        root.classList.toggle('port-list-preview', listLimit > 0);
        let page = 0, perPage = 10, total = 0, shown = 0, selectedMmsi = null, loading = false;
        const kind = place.place_type === 'custom' && place.category
            ? String(place.category).replace(/_/g, ' ').replace(/^./, c => c.toUpperCase())
            : {port:'Port', anchorage:'Anchorage', berth:'Berth', custom:'Area'}[place.place_type || 'port'] || 'Place';
        root.setAttribute('aria-labelledby', 'port-ships-title');
        root.innerHTML = '<header class="place-panel-header">' +
            '<div class="place-panel-actions"><button type="button" class="place-overview">← In view</button><span class="side-table-label"></span><button type="button" class="table-collapse place-close" title="Close sidebar" aria-label="Close sidebar">Close <span aria-hidden="true">→</span></button></div>' +
            '<div class="place-panel-heading"><div class="place-panel-label"><h2 id="port-ships-title"></h2><div class="place-panel-kind"></div></div></div></header>' +
            '<div class="sc-tabs place-dialog-tabs" role="tablist" aria-label="Place activity">' + tabs.map(label =>
            `<button type="button" class="sc-tab" role="tab" id="place-tab-${label.toLowerCase()}" data-place-tab="${label.toLowerCase()}" aria-controls="place-results">${label === 'Closest' ? 'Nearby' : label}<span class="place-tab-count"></span></button>`).join('') + '</div>' +
            '<div class="place-dialog-note" hidden></div><div id="place-results" class="port-results" role="tabpanel" aria-live="polite"><div class="tablecard_inner port-ship-list"><table><thead></thead><tbody></tbody></table><div class="port-status"></div></div></div>' +
            '<footer class="table-footer"><span class="table-count port-ship-count" aria-live="polite"></span><div class="table-pager"><button type="button" class="place-prev" aria-label="Previous page">‹</button><span class="place-page"></span><button type="button" class="place-next" aria-label="Next page">›</button></div></footer>';
        root.querySelector('#port-ships-title').textContent = place.label || place.code || 'Place';
        root.querySelector('.side-table-label').textContent = kind;
        const code = String(place.code || '').trim().toUpperCase();
        const country = /^[A-Z]{2}[A-Z0-9]{3}$/.test(code) ? code.slice(0, 2) : '';
        const countryName = country ? getCountryName(country) : '';
        const detail = root.querySelector('.place-panel-kind');
        detail.textContent = [countryName, code].filter(Boolean).join(' · ');
        detail.hidden = !detail.textContent;
        if (country) root.querySelector('.place-panel-heading').prepend(flag(country, 'place-panel-flag', countryName));
        const results = root.querySelector('.port-results'), body = results.querySelector('tbody'), status = results.querySelector('.port-status');
        const hover = bindVesselHover(body, {
            enter: id => host.hoverVessel?.(Number(id)),
            leave: id => host.unhoverVessel?.(Number(id)),
        });
        const count = root.querySelector('.port-ship-count'), note = root.querySelector('.place-dialog-note');
        const previous = root.querySelector('.place-prev'), next = root.querySelector('.place-next');
        const pagination = root.querySelector('.table-pager');
        const more = document.createElement('a');
        more.className = 'table-more';
        more.hidden = true;
        root.querySelector('.table-footer').append(more);
        const strip = tabScroller(root.querySelector('.place-dialog-tabs'), 'place-dialog-tabs-wrap');
        const capacity = () => listLimit || Math.min(100, pageSize(root.querySelector('.tablecard_inner'), root.querySelector('thead'), body.querySelector('tr')) || 10);
        const pager = () => {
            if (!listLimit) return renderPager(pagination, { page, perPage, total, shown, loading });
            pagination.hidden = true;
            count.textContent = loading ? '' : `${shown.toLocaleString()} shown`;
            const link = !loading ? host.placeListLink?.(place, selected) : null;
            const showLink = link?.href && (link.always || (total > shown && shown > 0));
            more.hidden = !showLink;
            if (showLink) { more.href = link.href; more.textContent = link.label || 'View all →'; }
            else more.removeAttribute('href');
        };
        const load = async (refresh = false) => {
            const request = {};
            loading = true;
            pager();
            root._portRequest = request;
            const visitRows = selected === 'visits';
            root.querySelectorAll('[data-place-tab]').forEach(button => {
                const active = button.dataset.placeTab === selected;
                button.setAttribute('aria-selected', String(active)); button.tabIndex = active ? 0 : -1;
            });
            results.setAttribute('aria-labelledby', 'place-tab-' + selected);
            // Arrived and Left put a date where Inside puts "2s", and the column
            // is pinned narrow: say which it is so the width can follow.
            results.querySelector('table').classList.toggle('dated', selected === 'arrived' || selected === 'left');
            results.querySelector('table').classList.toggle('visits', visitRows);
            results.querySelector('thead').innerHTML = '<tr><th scope="col" class="col-name">Name</th>' + (visitRows
                ? '<th scope="col">Entry</th><th scope="col">Exit</th><th scope="col">Duration</th>'
                : '<th scope="col" class="num col-spd">Spd <span class="dim">kts</span></th><th scope="col" class="col-type">Type</th><th scope="col" class="num col-last">' + ({arrived:'Arrived',left:'Left',closest:'Range'}[selected] || 'Last') + '</th>') + '</tr>';
            results.setAttribute('aria-busy', 'true');
            if (!refresh) {
                hover.clear();
                shown = 0;
                count.textContent = '';
                body.innerHTML = '';
                status.hidden = false;
                status.innerHTML = '<span class="port-loading" role="status" aria-label="Loading ships"><span class="spinner icon-sm" aria-hidden="true"></span></span>';
            }
            const params = new URLSearchParams({tab:selected,hours:'0',offset:String(page * perPage),limit:String(perPage)});
            if (Number.isInteger(place.runtime_id)) { params.set('id', place.runtime_id); params.set('version', place.place_version || ''); }
            else params.set('code', place.code || String(place.id || '').replace(/^p/, ''));
            try {
                const url = host.placeShipsUrl ? host.placeShipsUrl(params) : 'ships_place.json?' + params;
                const data = await host.fetchJSON(url);
                if (root._portRequest !== request || !shell.isOpen()) return;
                if (data?.error) throw new Error(data.error);
                if (!Array.isArray(data?.ships)) throw new Error('Invalid ship list');
                const visitTabs = ['inside', 'left', 'arrived', 'visits'];
                for (const button of root.querySelectorAll('[data-place-tab]')) {
                    const tab = button.dataset.placeTab;
                    const total = data.counts?.[tab];
                    // a busy place runs to five figures; the tab has room for four
                    button.querySelector('.place-tab-count').textContent = listLimit || total == null ? '' : ` ${compactCount(total)}`;
                    if (!listLimit && total != null) button.title = `${total.toLocaleString()} ${tab === 'closest' ? 'nearby' : tab}`;
                    // opened by code, the outline was unknown until this answer:
                    // drop the tabs a place without one can never fill
                    if (data.has_geometry === false && visitTabs.includes(tab))
                        button.hidden = true;
                }
                strip.update(); // the counts have just changed every label's width
                const paged = data.offset !== undefined;
                if (!paged) page = 0;
                total = Math.max(0, paged ? data.total ?? data.ships.length : data.ships.length);
                if (page > 0 && page * perPage >= total) {
                    page = Math.max(0, Math.ceil(total / perPage) - 1);
                    return load();
                }
                const ships = listLimit ? data.ships.slice(0, listLimit) : data.ships;
                shown = ships.length;
                count.title = selected === 'closest' ? 'Nearest first' : 'Newest first';
                note.textContent = selected === 'expected' && !data.has_geometry
                    ? 'No port boundary: ships already in port cannot be excluded.'
                    : '';
                note.hidden = !note.textContent;
                if (!ships.length) { body.innerHTML = ''; status.hidden = false; status.innerHTML = '<p>No ' + (selected === 'visits' ? 'visits' : 'ships') + ' in this view.</p>'; return; }
                const now = data.time || Math.floor(Date.now() / 1000);
                const rows = ships.map(ship => {
                    const id = Number(ship.mmsi);
                    return { ...ship, id, name: ship.shipname || `MMSI ${id}`, selected: id === selectedMmsi };
                });
                body.innerHTML = visitRows ? rows.map(ship => {
                    const entry = ship.pending && ship.inside ? 'Confirming…' : timeHTML(ship.entered ?? ship.entered_before, now, ship.entered ? '' : '<');
                    const exit = ship.inside ? '—' : ship.pending ? 'Confirming…' : timeHTML(ship.exited ?? ship.exited_after, now, ship.exited ? '' : '>');
                    return `<tr data-mmsi="${ship.id}" class="${ship.selected ? 'selected' : ''}">${vesselNameCell(ship)}<td>${entry}</td><td>${exit}</td><td>${visitDuration(ship)}</td></tr>`;
                }).join('') : renderVesselRows(rows.map(ship => {
                    const sprite = spriteFor(ship.shipclass, ship.speed, ship.cog);
                    return {
                        ...ship,
                        speed: typeof ship.speed === 'number' && ship.speed >= 0 ? ship.speed.toFixed(1) : '—',
                        typeHTML: `<span class="table-shiptype-icon"><span class="sprites" style="background-position: -${sprite.cx}px -${sprite.cy}px; width: 20px; height: 20px; transform: rotate(${sprite.rot}rad)" title="${text(sprite.hint)}"></span></span>`,
                        lastHTML: selected === 'arrived' ? timeHTML(ship.entered, now)
                            : selected === 'left' ? timeHTML(ship.exited, now)
                            : selected === 'closest' ? (typeof ship.range === 'number' ? text(ship.range.toFixed(1) + ' nm') : '—')
                            : ship.timestamp > 0 ? durationHTML(now - ship.timestamp) : '—',
                    };
                }));
                status.innerHTML = ''; status.hidden = true;
            } catch (error) {
                if (root._portRequest === request && shell.isOpen())
                    { status.hidden = false; status.innerHTML = '<p role="status">Could not load this place. ' + text(error.message) + '</p><button type="button" class="btn port-retry">Try again</button>'; }
            } finally {
                if (root._portRequest === request) {
                    hover.sync();
                    loading = false;
                    results.setAttribute('aria-busy', 'false');
                    pager();
                    fit();
                }
            }
        };
        root.onclick = e => {
            const tab = e.target.closest('[data-place-tab]');
            if (tab) { selected = tab.dataset.placeTab; page = 0; load(); return; }
            if (e.target.closest('.place-overview')) { shell.overview(); return; }
            if (e.target.closest('.place-close')) { shell.close(); return; }
            if (e.target.closest('.place-prev') && !previous.disabled) { page--; load(); return; }
            if (e.target.closest('.place-next') && !next.disabled) { page++; load(); return; }
            if (e.target.closest('.port-retry')) { load(); return; }
            const row = e.target.closest('tr[data-mmsi]');
            if (row) {
                selectedMmsi = Number(row.dataset.mmsi);
                body.querySelectorAll('tr').forEach(tr => tr.classList.toggle('selected', Number(tr.dataset.mmsi) === selectedMmsi));
                host.openVessel(selectedMmsi);
            }
        };
        root.querySelector('[role="tablist"]').onkeydown = e => {
            const buttons = [...root.querySelectorAll('[data-place-tab]')].filter(button => !button.hidden);
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
            if (listLimit) return;
            clearTimeout(resizeTimer);
            resizeTimer = setTimeout(() => {
                if (!shell.isOpen()) return;
                const size = capacity();
                if (size === perPage) return;
                page = Math.floor(page * perPage / size);
                perPage = size;
                load();
            }, 150);
        };
        const resize = new ResizeObserver(fit);
        resize.observe(root.querySelector('.tablecard_inner'));
        const refresh = setInterval(() => { if (shell.isOpen() && !document.hidden && !loading) load(true); }, 60000);
        refresh.unref?.();
        root._dispose = () => {
            hover.destroy();
            root._portRequest = null;
            clearInterval(refresh);
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
