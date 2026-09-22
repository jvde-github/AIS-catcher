/* ============================================================================
   events-history.js — the receiver's past events, in the side table
   The ticker shows what is current; the rings hold a great deal more than its
   horizon reaches. This reads them a page at a time, keyed on the sequence
   number so events arriving mid-read never shift a page. None of the strip's
   filters apply here: an event out of view, or from a vessel just opened, is
   exactly what a reader came looking for.
   ========================================================================= */
import { createSideTable } from './side-table.js';
import { renderEventText } from './core/event-text.js';
import { pageSize, rowsFitting, renderPager } from './table.js';
import { hasValidCoords } from './core/geo.js';
import { sanitizeString } from './core/text.js';

// a floor, so a very short panel still shows something
const MIN_PAGE = 4;
const ANIM_MS = 1250;
const LEVELS = [
    { level: 0, label: 'All' },
    { level: 1, label: 'Notice' },
    { level: 2, label: 'Urgent' },
];

const KINDS = {
    safety: 'Safety', destination: 'Destination', status: 'Status',
    draught: 'Draught', place_enter: 'Place', place_exit: 'Place',
};

const WARN = '<svg viewBox="0 -960 960 960" width="15" height="15" fill="currentColor" aria-hidden="true">' +
    '<path d="m40-120 440-760 440 760H40Zm104-60h672L480-760 144-180Zm336-26q13 0 21.5-8.5T510-236q0-13-8.5-21.5T480-266q-13 0-21.5 8.5T450-236q0 13 8.5 21.5T480-206Zm-30-104h60v-210h-60v210Z"/></svg>';

const LEVEL_NAME = ['Routine', 'Notice', 'Urgent'];

function ageText(seconds) {
    if (!(seconds > 0)) return 'now';
    const m = Math.round(seconds / 60);
    if (m < 60) return m + 'm';
    const h = Math.floor(m / 60);
    return h < 24 ? `${h}h` : `${Math.floor(h / 24)}d`;
}

export function createEventHistory(host) {
    return function openEventHistory() {
        const shell = createSideTable(host, 'eventhistory');
        const root = shell.root;
        root.setAttribute('aria-label', 'Past events');
        root.innerHTML = '<header class="place-panel-header">' +
            '<div class="place-panel-actions">' +
            '<button type="button" class="place-overview">&larr; In view</button>' +
            '<span class="side-table-label">Past events</span>' +
            '<button type="button" class="table-collapse place-close" title="Close sidebar" aria-label="Close sidebar">Close <span aria-hidden="true">&rarr;</span></button>' +
            '</div></header>' +
            '<div class="place-dialog-tabs-wrap">' +
            '<div class="sc-tabs place-dialog-tabs" role="tablist" aria-label="Minimum level">' + LEVELS.map(l =>
                `<button type="button" class="sc-tab" role="tab" aria-controls="eventhistory-results" data-ev-level="${l.level}" aria-selected="false">${l.label}</button>`).join('') + '</div></div>' +
            '<div id="eventhistory-results" class="port-results" role="tabpanel" aria-live="polite">' +
            '<div class="ev-history-scroll"><div class="ev-history"></div><div class="port-status"></div></div></div>' +
            '<footer class="table-footer"><span class="table-count" aria-live="polite"></span>' +
            '<div class="table-pager"><button type="button" class="ev-prev" aria-label="Previous page">&lsaquo;</button>' +
            '<span></span><button type="button" class="ev-next" aria-label="Next page">&rsaquo;</button></div></footer>';

        const listEl = root.querySelector('.ev-history');
        const statusEl = root.querySelector('.port-status');
        const scrollEl = root.querySelector('.ev-history-scroll');
        const pagerEl = root.querySelector('.table-pager');
        const prevBtn = root.querySelector('.ev-prev');
        const nextBtn = root.querySelector('.ev-next');

        let level = 0, page = 0, total = 0, busy = false, clock = 0;
        let perPage = 12, resizeTimer = 0, resize = null, polling = false;
        // the `before` each page was fetched with; cursors[0] is always "newest"
        let cursors = [0];
        let timer = 0;

        const LIVE_MS = 5000;
        const reduced = () => {
            try { return window.matchMedia('(prefers-reduced-motion: reduce)').matches; }
            catch { return false; }
        };

        /* One movement at a time: the rows already there slide down to where
           the new ones have put them; once settled, the new rows fade in above
           and anything pushed past the bottom edge fades out. No height is
           animated: a row measured at one height and settling at another reads
           as expanding and then collapsing. */
        function slideIn(previous, arrived, leaving) {
            const drop = () => leaving.forEach((node) => node.remove());
            if (reduced() || !listEl.animate) { drop(); return; }

            // keeps moving until it stops: a tail-heavy ease-out drifts
            // sub-pixel for its last third, and the fade waiting on it pauses
            const ease = 'cubic-bezier(.4,0,.2,1)';
            const fade = Math.max(120, Math.round(ANIM_MS * 0.5));
            arrived.forEach((node) => { node.style.opacity = '0'; });
            // a row on its way out still occupies the box it is leaving
            scrollEl.style.overflowY = 'hidden';

            let slide = null;
            previous.forEach(([node, was]) => {
                if (!node.isConnected) return;
                const dy = was - node.getBoundingClientRect().top;
                if (!dy) return;
                slide = node.animate(
                    [{ transform: `translateY(${dy}px)` }, { transform: 'none' }],
                    { duration: ANIM_MS, easing: ease });
            });

            const settled = () => {
                arrived.forEach((node) => {
                    node.style.opacity = '';
                    node.animate([{ opacity: 0 }, { opacity: 1 }], { duration: fade, easing: ease });
                    node.classList.add('ev-row-new');
                    setTimeout(() => node.classList.remove('ev-row-new'), 2000);
                });
                leaving.forEach((node) => {
                    const out = node.animate([{ opacity: 1 }, { opacity: 0 }],
                        { duration: fade, easing: ease });
                    out.addEventListener('finish', () => node.remove());
                    out.addEventListener('cancel', () => node.remove());
                });
                setTimeout(() => { scrollEl.style.overflowY = ''; }, fade + 30);
            };

            if (slide) slide.addEventListener('finish', settled);
            else settled();
        }

        const now = () => clock || Math.floor(Date.now() / 1000);

        // what the box shows without scrolling; rows wrap, so this is only the
        // estimate rowsFitting() corrects once they are in place
        const capacity = () =>
            Math.max(MIN_PAGE, pageSize(scrollEl, null, listEl.firstElementChild) || 12);

        // the rows past the bottom edge are the next page, and the cursor
        // follows the last one kept; with `keep` they stay in the DOM for the
        // caller to fade out and remove
        function trimToFit(keep) {
            const fitted = Math.max(1, rowsFitting(scrollEl, listEl.children));
            const rows = Array.prototype.slice.call(listEl.children);
            const leaving = rows.slice(fitted);
            if (!keep) leaving.forEach((node) => node.remove());
            perPage = fitted;
            const last = rows[fitted - 1];
            return { seq: last ? Number(last.dataset.seq) : 0, leaving };
        }

        function row(e) {
            const item = document.createElement('div');
            item.className = 'ev-row';
            item.dataset.seq = e.seq;
            const lvl = e.level || 0;
            const rendered = e.format === 'ticker-v1'
                ? renderEventText(e.text) : { html: sanitizeString(String(e.text ?? '')) };
            // the mark says the level, so drop the wording's own warning sign
            const body = rendered.html.replace(/^⚠️?\s*/, '');
            const meta = [KINDS[e.kind] || 'Event'];
            if (e.mmsi) meta.push('MMSI ' + e.mmsi);
            if (e.count > 1) meta.push('×' + e.count);

            item.innerHTML =
                `<span class="ev-row-mark ev-l${lvl}" title="${LEVEL_NAME[lvl] || 'Routine'}">` +
                    (lvl > 0 ? WARN : '<span class="ev-row-dot"></span>') + '</span>' +
                `<span class="ev-row-body"><span class="ev-row-text">${body}</span>` +
                `<span class="ev-row-meta">${sanitizeString(meta.join(' · '))}</span></span>` +
                `<span class="ev-row-age">${ageText(now() - (e.first || e.t || 0))}</span>`;

            if (e.mmsi || hasValidCoords(e.lat, e.lon)) {
                item.tabIndex = 0;
                item.classList.add('ev-row-active');
                const go = () => {
                    if (hasValidCoords(e.lat, e.lon)) host.navigate({ lat: e.lat, lon: e.lon });
                    if (e.mmsi) host.openVessel(e.mmsi);
                };
                item.addEventListener('click', go);
                item.addEventListener('keydown', (k) => {
                    if (k.key === 'Enter' || k.key === ' ') { k.preventDefault(); go(); }
                });
            }
            return item;
        }

        async function load(target) {
            if (busy || target < 0 || cursors[target] === undefined) return;
            busy = true;
            renderPager(pagerEl, { page: target, perPage, total, loading: true });
            statusEl.textContent = 'Loading…';
            try {
                // a few over the estimate, trimmed to fit rather than leaving a gap
                const data = await host.fetchJSON(host.historyUrl(cursors[target], level, capacity() + 3));
                clock = data.time || 0;
                const events = data.events || [];
                total = data.retained || events.length;
                page = target;

                listEl.innerHTML = '';
                events.forEach((e) => listEl.appendChild(row(e)));
                scrollEl.scrollTop = 0;

                const { seq: lastSeq } = trimToFit(false);
                const trimmed = listEl.children.length < events.length;
                if (lastSeq && (trimmed || data.more)) cursors[page + 1] = lastSeq;
                else cursors.length = page + 1;

                statusEl.textContent = events.length ? '' : 'No events retained';
                renderPager(pagerEl, { page, perPage, total, shown: listEl.children.length });
            } catch {
                statusEl.textContent = 'Could not load events';
                renderPager(pagerEl, { page, perPage, total });
            }
            busy = false;
        }

        // Only the first page takes new events: on page three the reader is in the
        // past, and shifting rows under them would move the ground they stand on.
        async function poll() {
            // a fetch slower than the tick must not let two polls prepend the same rows
            if (busy || polling || page !== 0 || !shell.isOpen()) return;
            polling = true;
            try {
                const data = await host.fetchJSON(host.historyUrl(0, level, capacity() + 3));
                clock = data.time || clock;
                total = data.retained || total;

                // newer than the newest on screen, not merely absent from it:
                // what lies below the page cut is page two's
                const first = listEl.firstElementChild;
                if (!first) { load(0); return; }
                const newest = Number(first.dataset.seq);
                const fresh = (data.events || [])
                    .filter((e) => e.seq > newest)
                    .sort((a, b) => a.seq - b.seq);
                if (!fresh.length) return;

                // where every row sits before the new ones displace it
                const previous = Array.prototype.map.call(
                    listEl.children, (node) => [node, node.getBoundingClientRect().top]);

                const added = [];
                fresh.forEach((e) => {
                    const item = row(e);
                    listEl.prepend(item);
                    added.push(item);
                });
                const { seq: lastSeq, leaving } = trimToFit(true);
                slideIn(previous, added.filter((item) => item.isConnected), leaving);
                if (lastSeq) cursors[1] = lastSeq;
                cursors.length = 2;
                statusEl.textContent = '';
                // the rows fading out are still in the DOM: count what is staying
                renderPager(pagerEl, { page: 0, perPage, total, shown: perPage });
            } catch { /* next tick */ }
            finally { polling = false; }
        }

        function restart(value) {
            level = value;
            root.querySelectorAll('[data-ev-level]').forEach((b) =>
                b.setAttribute('aria-selected', String(Number(b.dataset.evLevel) === value)));
            cursors = [0];
            total = 0;
            listEl.innerHTML = '';
            load(0);
        }

        root.querySelectorAll('[data-ev-level]').forEach((b) =>
            b.addEventListener('click', () => restart(Number(b.dataset.evLevel))));
        prevBtn.addEventListener('click', () => load(page - 1));
        nextBtn.addEventListener('click', () => load(page + 1));
        root.querySelector('.place-close').addEventListener('click', () => shell.close());
        root.querySelector('.place-overview').addEventListener('click', () => shell.overview());

        const fit = () => {
            clearTimeout(resizeTimer);
            resizeTimer = setTimeout(() => {
                if (!shell.isOpen() || busy || capacity() === perPage) return;
                load(page);
            }, 150);
        };
        resize = new ResizeObserver(fit);
        resize.observe(scrollEl);

        timer = setInterval(poll, LIVE_MS);
        root._dispose = () => {
            clearInterval(timer);
            clearTimeout(resizeTimer);
            resize.disconnect();
            root._dispose = null;
        };

        shell.open();
        restart(0);
    };
}
