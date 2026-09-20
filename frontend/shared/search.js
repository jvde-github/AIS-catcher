// Shared search panel. Hosts supply results and decide how a chosen item opens.
import { ANCHOR_SVG } from './icons.js';
import { chipStyle } from './components.js';
import { spriteFor } from './core/sprites.js';
import { sanitizeString } from './core/text.js';
import { beginLoading } from './loading.js';

const text = value => sanitizeString(String(value ?? ''));
const defaultTypes = [
    { id: 'all', label: 'Everything', placeholder: 'Search vessels, stations and ports…' },
    { id: 'ships', label: 'Vessels', placeholder: 'Name, MMSI, callsign or IMO…' },
    { id: 'stations', label: 'Stations', placeholder: 'Station name or identifier…' },
    { id: 'ports', label: 'Ports', placeholder: 'Port name or UN/LOCODE…' },
];

export function createSearch(opts) {
    const root = opts.root, scrim = opts.scrim;
    const types = opts.types || defaultTypes;
    let type = types[0].id, timer, request = 0, controller, opener;
    let results = [], selected = -1, stopLoading = () => {};
    try {
        const saved = localStorage.getItem(opts.storageKey || 'currentSearchType');
        if (types.some(t => t.id === saved)) type = saved;
    } catch { /* Search also works with storage disabled. */ }
    root.classList.add('map-search');
    root.hidden = scrim.hidden = true;
    root.setAttribute('role', 'search');
    root.setAttribute('aria-label', 'Map search');
    root.innerHTML = '<div class="search-field"><i class="search_icon search-glyph" aria-hidden="true"></i>' +
        '<input type="search" id="map-search" autocomplete="off" spellcheck="false" role="combobox" aria-label="Search" aria-autocomplete="list" aria-controls="search-results" aria-expanded="false">' +
        '<span class="search-spinner"></span><button type="button" id="clear-search" hidden aria-label="Clear search"><i class="close_icon" aria-hidden="true"></i></button></div>' +
        '<div class="search-chips" role="group" aria-label="Search category">' + types.map(t =>
            '<button type="button" id="search-' + text(t.id) + '" class="search-chip" data-search-type="' + text(t.id) + '">' + text(t.label) + '</button>').join('') + '</div>' +
        '<div class="search-status" role="status" aria-live="polite"></div>' +
        '<div id="search-results" class="search-results" role="listbox" aria-label="Search results" hidden></div>';
    const input = root.querySelector('input'), list = root.querySelector('.search-results');
    const status = root.querySelector('.search-status'), clear = root.querySelector('#clear-search');
    const triggers = Array.from(opts.triggers || []);
    for (const button of triggers) {
        button.setAttribute('aria-controls', root.id);
        button.setAttribute('aria-expanded', 'false');
    }

    function cancel() {
        clearTimeout(timer);
        ++request;
        controller?.abort();
        stopLoading();
    }

    function resetResults() {
        results = []; selected = -1;
        list.replaceChildren(); list.hidden = true;
        input.setAttribute('aria-expanded', 'false');
        input.removeAttribute('aria-activedescendant');
        status.textContent = '';
    }

    function syncType() {
        input.placeholder = types.find(t => t.id === type).placeholder;
        root.querySelectorAll('[data-search-type]').forEach(button => {
            const active = button.dataset.searchType === type;
            button.classList.toggle('is-on', active);
            button.setAttribute('aria-pressed', String(active));
        });
    }

    function close(restoreFocus = false) {
        cancel(); resetResults();
        input.value = ''; clear.hidden = true;
        const wasOpen = !root.hidden;
        root.hidden = scrim.hidden = true;
        document.body.classList.remove('search-open');
        triggers.forEach(button => {
            button.classList.remove('is-active');
            button.setAttribute('aria-expanded', 'false');
        });
        if (wasOpen) opts.onClose?.();
        if (restoreFocus && opener?.isConnected) opener.focus();
    }

    function open() {
        if (!root.hidden) { input.focus(); return; }
        opener = document.activeElement;
        opts.onOpen?.();
        root.hidden = scrim.hidden = false;
        document.body.classList.add('search-open');
        triggers.forEach(button => {
            button.classList.add('is-active');
            button.setAttribute('aria-expanded', 'true');
        });
        input.focus();
    }

    function select(index) {
        selected = index;
        Array.from(list.children).forEach((row, i) => row.setAttribute('aria-selected', String(i === index)));
        const row = list.children[index];
        if (row) {
            input.setAttribute('aria-activedescendant', row.id);
            row.scrollIntoView?.({ block: 'nearest' });
        }
    }

    async function choose(index) {
        const result = results[index];
        if (!result) return;
        close();
        try { await opts.onSelect(result); }
        catch (error) { opts.onError?.(error); }
    }

    function render(items) {
        resetResults();
        results = items.slice(0, 50);
        status.textContent = items.length > 50 ? 'Showing 50 of ' + items.length + ' matches. Refine your search for more.' :
            items.length ? items.length + (items.length === 1 ? ' result' : ' results') : 'No results found';
        for (const [index, result] of results.entries()) {
            const row = document.createElement('div');
            row.className = 'search-result'; row.id = 'search-result-' + index;
            row.setAttribute('role', 'option'); row.setAttribute('aria-selected', 'false');
            let icon = '<i class="search_icon" aria-hidden="true"></i>';
            if (result.type === 'station') icon = '<i class="station_icon" aria-hidden="true"></i>';
            else if (result.type === 'port' || result.type === 'place') icon = '<span class="search-port-icon" aria-hidden="true">' + ANCHOR_SVG + '</span>';
            else if (result.type === 'ship') {
                const sprite = result.shipclass != null ? spriteFor(result.shipclass) : null;
                icon = sprite ? '<span class="shipicon" aria-hidden="true" style="' + chipStyle({pos: '-' + sprite.cx + 'px -' + sprite.cy + 'px', scale: 1.3}) + '"></span>' : '<i class="directions_boat_icon" aria-hidden="true"></i>';
            }
            const kind = result.kindLabel || ({ship: 'Vessel', station: 'Station', port: 'Port', place: 'Place'}[result.type] || 'Result');
            const meta = [kind, result.code || result.id];
            if (result.country) meta.push(result.country);
            if (result.type === 'ship') {
                if (result.callsign) meta.push(result.callsign);
                if (result.imo > 0) meta.push('IMO ' + result.imo);
            }
            row.innerHTML = icon + '<div class="search-result-text"><div class="search-result-title" translate="no">' + text(result.name || result.id || 'Unknown') +
                '</div><div class="search-result-sub">' + meta.filter(v => v !== undefined && v !== null && v !== '').map(text).join(' · ') + '</div></div>';
            row.addEventListener('click', () => choose(index));
            list.appendChild(row);
        }
        list.hidden = !results.length;
        input.setAttribute('aria-expanded', String(!!results.length));
    }

    async function search() {
        cancel(); resetResults();
        const query = input.value.trim();
        clear.hidden = !input.value;
        if (!query || root.hidden) return;
        const current = request;
        controller = new AbortController();
        const finish = stopLoading = beginLoading(root, 'Searching', root.querySelector('.search-spinner'));
        try {
            const items = await opts.search(query, type, controller.signal);
            if (current === request && !root.hidden) render(items);
        } catch (error) {
            if (current === request && !root.hidden && error.name !== 'AbortError') status.textContent = opts.errorMessage?.(error) || 'Search unavailable. Please try again.';
        } finally { finish(); }
    }

    input.addEventListener('input', () => {
        cancel(); resetResults(); clear.hidden = !input.value;
        if (input.value.trim()) timer = setTimeout(search, opts.delay ?? 300);
    });
    clear.addEventListener('click', () => {
        cancel(); resetResults(); input.value = ''; clear.hidden = true; input.focus();
    });
    root.querySelectorAll('[data-search-type]').forEach(button => button.addEventListener('click', () => {
        type = button.dataset.searchType;
        try { localStorage.setItem(opts.storageKey || 'currentSearchType', type); } catch { /* Optional. */ }
        syncType(); search(); input.focus();
    }));
    root.addEventListener('keydown', event => {
        if (event.key === 'Escape') { event.preventDefault(); event.stopPropagation(); close(true); }
        if (event.target !== input || event.isComposing) return;
        if ((event.key === 'ArrowDown' || event.key === 'ArrowUp') && results.length) {
            event.preventDefault();
            select((selected + (event.key === 'ArrowDown' ? 1 : selected < 0 ? 0 : -1) + results.length) % results.length);
        } else if (event.key === 'Enter') {
            event.preventDefault();
            if (results.length) choose(selected < 0 ? 0 : selected);
        }
    });
    for (const event of ['pointerdown', 'click', 'dblclick', 'wheel']) root.addEventListener(event, e => e.stopPropagation());
    scrim.addEventListener('click', () => close(true));
    syncType();
    return { open, close, isOpen: () => !root.hidden, toggle: () => root.hidden ? open() : close(true) };
}
