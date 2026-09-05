/* ============================================================================
   events.js — the receiver's events, as ticker items
   What a receiver words as news about a vessel: a safety message, a change
   of destination, status or draught. Asked for after the last sequence seen,
   kept to what lies in view, and laid out the same on every host. The host
   supplies the feed's address, the fetch, the map and a vessel's label.
   ========================================================================= */
import { fromLonLat } from 'ol/proj.js';
import { hasValidCoords } from './core/geo.js';

export function create(host) {
    /* ---- the event strip ---------------------------------------------------- */

    // The receiver's events after the last sequence number seen, as ticker events.
    // Only the view has a say here: anything urgent stays, the rest when it lies
    // within the view with a margin, or when the view is the world. An event is as
    // old as when it began: a repeat counts on it, it does not make it news.
    let eventsSince = 0, eventsEpoch = 0;
    async function pollEvents(push) {
        try {
            const data = await host.fetchJSON(host.eventsUrl(eventsSince));
            const seen = seenList();
            const events = (data.events || []).filter(relevant).map((e) => tickerEvent(e, seen));
            if (events.length) push(events);
            if (data.seq) eventsSince = Math.max(eventsSince, data.seq);
        } catch { /* next poll */ }
    }

    // another receiver has its own sequence, and its keys must not meet the old ones
    function resetEvents() { eventsSince = 0; eventsEpoch++; }

    const EVENT_URGENT = 2, WORLD_ZOOM = 6, VIEW_MARGIN = 0.5, WORLD_M = 2 * 20037508.342789244;
    function relevant(e) {
        if ((e.level || 0) >= EVENT_URGENT || !hasValidCoords(e.lat, e.lon)) return true;
        const map = host.map && host.map();
        if (!map || (map.getView().getZoom() || 0) < WORLD_ZOOM) return true;
        // in the projected plane the extent stays one range across the antimeridian
        const ext = map.getView().calculateExtent(map.getSize());
        const w = ext[2] - ext[0], h = ext[3] - ext[1];
        const [x, y] = fromLonLat([e.lon, e.lat]);
        const inX = (v) => v >= ext[0] - w * VIEW_MARGIN && v <= ext[2] + w * VIEW_MARGIN;
        return y >= ext[1] - h * VIEW_MARGIN && y <= ext[3] + h * VIEW_MARGIN && (inX(x) || inX(x + WORLD_M) || inX(x - WORLD_M));
    }

    // A vessel the user has opened from the strip is seen: for an hour its events
    // surface less often. Kept in the browser, one entry per vessel.
    const SEEN_KEY = 'events_seen', SEEN_MS = 60 * 60 * 1000;
    function seenList() {
        try { const v = JSON.parse(localStorage.getItem(SEEN_KEY) || '{}'); return v && typeof v === 'object' ? v : {}; } catch { return {}; }
    }
    function noteSeen(mmsi) {
        if (!mmsi) return;
        const now = Date.now(), list = seenList();
        for (const k in list) if (list[k] < now) delete list[k];
        list[mmsi] = now + SEEN_MS;
        try { localStorage.setItem(SEEN_KEY, JSON.stringify(list)); } catch { /* no storage */ }
    }
    const isSeen = (mmsi, list) => !!mmsi && (list[mmsi] || 0) > Date.now();

    const isDistressDevice = (mmsi) => /^97[024]/.test(String(mmsi));
    const isBaseStation = (mmsi) => mmsi >= 2000000 && mmsi <= 9999999;

    // a receiver too old to word its changes still says what kind they are
    const KIND_FIELD = { 2: 'destination', 3: 'status', 4: 'draught' };

    // The receiver words its events; the strip only lays them out. A change
    // names its field and the value on either side, a safety message is the
    // message and who it was for. The arrow stands even where the old value is
    // unknown, so a change never reads as a first report.
    function tickerEvent(e, seen) {
        const label = (m) => String(host.shipLabel(m));
        const who = isDistressDevice(e.from) ? (String(e.from).startsWith('972') ? 'MOB device' : String(e.from).startsWith('974') ? 'EPIRB' : 'AIS-SART')
            : isBaseStation(e.from) ? `VTS ${label(e.from).replace(/^MMSI /, '')}` : label(e.from);
        const to = e.to ? label(e.to) : '';
        const parts = [];
        let text;
        const field = e.label || KIND_FIELD[e.kind] || '';
        if (field) {
            parts.push(`<span class="tk-name">${escapeHtml(who)}</span>`, '<span class="tk-sep">·</span>', `<span class="tk-label">${escapeHtml(field)}</span>`);
            if (e.was) parts.push(`<span class="tk-was">${escapeHtml(e.was)}</span>`);
            parts.push('<span class="tk-arrow">&rarr;</span>', `<span class="tk-to">${escapeHtml(e.text)}</span>`);
            text = `${who} · ${field} ${e.was ? e.was + ' ' : ''}→ ${e.text}`;
        } else {
            const alert = e.level >= 1;
            const body = e.text + (e.count > 1 ? ` (×${e.count})` : '');
            parts.push(`<span class="${alert ? 'tk-alert' : 'tk-name'}">${alert ? '&#9888; ' : ''}${escapeHtml(who)}` +
                (to ? '<span class="tk-arrow">&rarr;</span>' + escapeHtml(to) : '') + '</span>',
                '<span class="tk-sep">·</span>', `<span class="tk-to">${escapeHtml(body)}</span>`);
            text = `${alert ? '⚠ ' : ''}${who}${to ? ` → ${to}` : ''} · ${body}`;
        }
        return { key: `ev-${eventsEpoch}-${e.seq}`, at: (e.first || e.t || 0) * 1000, id: e.from, fresh: true, level: e.level || 0, demoted: isSeen(e.from, seen) || isSeen(e.to, seen), text, html: parts.join('') };
    }
    const escapeHtml = (v) => String(v).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));

    return { pollEvents, resetEvents, noteSeen };
}
