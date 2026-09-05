/* ============================================================================
   ticker.js — a strip of recent events along an edge of the map (layer 2)

   Shows one event at a time over the strip's width, sliding and fading,
   keeps a total and, on a very wide strip, a row of class-count chips
   beside it, and pans a line that does not fit. What an event says is the
   host's: it pushes events already described, with a level and whether the
   user has seen the vessel lately.

       const t = AISTicker.create({
           mount: document.getElementById("ticker"),     // .ticker with .ticker-feed etc. inside
           buckets: BUCKETS,                              // chip spec (core/filter.js BUCKETS)
           bucketHidden: (id) => filter.isHidden("bucket", id),
           selection: { noteSeen, resolveVessel, openVessel, navigate },
       });
       t.setEnabled(true);
       t.push([{ key, at, id, text, html, fresh, level, demoted }]);   // at: ms; level 0-2 holds longer; demoted surfaces rarely
       // or { key, at, id, fresh, describe: () => ({ text, html }) } to render with current names/units
       t.setCounts({ total, shown, filtered, buckets: { am: 3, ... } });

   Older events surface less often, urgent ones stay fresh longer and hold
   the strip longer, a demoted one surfaces rarely. An urgent event is pinned:
   it keeps the strip until tapped away, or until it ages out after ninety
   minutes. On touch a horizontal drag scrolls a long line under the finger
   and a vertical flick dismisses.
   ========================================================================= */

import { bucketChip, debounce } from "./components.js";
import { compactCount } from "./core/text.js";
import { createEventSelection } from "./core/event-selection.js";

const SLIDE_MS = 8000;
const FADE_MS = 1000;
const MAX_AGE_MS = 30 * 60 * 1000;
const MAX_EVENTS = 40;

// older events surface less often: shown only when the slide counter lands on
// their period; an urgent event's clock runs slower, a demoted event has none
const PERIODS = [
    { under: 2 * 60 * 1000, period: 1 },
    { under: 5 * 60 * 1000, period: 2 },
    { under: 15 * 60 * 1000, period: 4 },
];
const OLDEST_PERIOD = 8;
const LEVEL_SLOWER = [1, 2, 3];      // how much longer an event of each level counts as recent
const LEVEL_DWELL = [1, 1.5, 2];     // how much longer it holds the strip
const levelOf = (e) => Math.min(e.level || 0, 2);

// the count chips only on a strip this wide, with this much room left for the line
const CHIPS_MIN_WIDTH = 1400;
const LINE_MIN_WIDTH = 520;

const POLL_MS = 20000;

export function create(opts) {
    const bar = opts.mount;
    const countsEl = bar.querySelector(".ticker-count-value");
    const bucketsEl = bar.querySelector(".ticker-buckets");
    const feedEl = bar.querySelector(".ticker-feed");
    const selectEvent = opts.selection ? createEventSelection(opts.selection) : null;
    const onSelect = opts.onSelect || function () {};
    const bucketHidden = opts.bucketHidden || function () { return false; };

    let events = [];
    let cursor = -1;
    let slide = 0;
    let slideTimer = null;
    // the host's feed, asked while the strip is on screen and enabled
    const poll = opts.poll || null;
    const pollMs = opts.pollMs || POLL_MS;
    let pollTimer = null;
    let fadeTimer = null;
    let dwell = SLIDE_MS;
    let counts = { shown: 0, total: 0, filtered: false };
    let slot = null;
    let bucketSpans = {};
    let wide = false;
    let bucketsWidth = 0;
    let shownKey = null;
    // a clicked event has been seen: it leaves the strip and a repeat of it stays away
    const dismissed = new Set();
    const DISMISSED_MAX = 200;

    bar.style.setProperty("--ticker-fade", FADE_MS + "ms");


    function buildSlot() {
        if (!feedEl) return;
        const item = document.createElement("div");
        item.className = "ticker-item";
        item.innerHTML = '<span class="ticker-dot"></span>'
            + '<span class="ticker-text"><span class="ticker-scroller"></span></span>'
            + '<span class="ticker-age"></span>';
        slot = {
            item,
            dot: item.querySelector(".ticker-dot"),
            text: item.querySelector(".ticker-text"),
            scroller: item.querySelector(".ticker-scroller"),
            age: item.querySelector(".ticker-age"),
            shown: false,
            overflow: 0,
            dragged: false,
        };
        item.addEventListener("click", () => {
            if (slot.dragged) { slot.dragged = false; return; }
            const event = slot.event;
            if (item.dataset.key) dismiss(item.dataset.key);
            if (event && selectEvent) {
                selectEvent(event).catch((error) => console.warn('Ticker selection failed', error));
            } else if (event) onSelect(event.id == null ? '' : String(event.id), event);
        });
        bindTouch();
        item.style.display = "none";
        feedEl.appendChild(item);
    }

    // a plain tap still falls through to the click; a drag does not
    const FLICK_PX = 40, DRAG_PX = 8;
    function bindTouch() {
        let x0 = 0, y0 = 0, base = 0, moved = false;
        slot.item.addEventListener("touchstart", (e) => {
            if (e.touches.length !== 1) return;
            x0 = e.touches[0].clientX;
            y0 = e.touches[0].clientY;
            moved = false;
            base = currentOffset();
            hold(base);
            clearTimeout(slideTimer);
            slideTimer = null;
        }, { passive: true });
        slot.item.addEventListener("touchmove", (e) => {
            if (e.touches.length !== 1) return;
            const dx = e.touches[0].clientX - x0, dy = e.touches[0].clientY - y0;
            if (!moved && Math.abs(dx) < DRAG_PX && Math.abs(dy) < DRAG_PX) return;
            moved = true;
            if (Math.abs(dx) < Math.abs(dy)) return;
            if (e.cancelable) e.preventDefault();
            slot.scroller.style.setProperty("--pan-offset", Math.min(0, Math.max(-slot.overflow, base + dx)) + "px");
        }, { passive: false });
        const end = (e) => {
            const t = e.changedTouches[0];
            const dx = t.clientX - x0, dy = t.clientY - y0;
            if (moved && Math.abs(dy) > FLICK_PX && Math.abs(dy) > Math.abs(dx)) {
                slot.dragged = true;
                if (slot.item.dataset.key) dismiss(slot.item.dataset.key);
                return;
            }
            slot.dragged = moved;
            schedule(dwell);
        };
        slot.item.addEventListener("touchend", end, { passive: true });
        slot.item.addEventListener("touchcancel", end, { passive: true });
    }

    function currentOffset() {
        const m = getComputedStyle(slot.scroller).transform;
        const match = m && m !== "none" ? m.match(/matrix\(([^)]+)\)/) : null;
        return match ? parseFloat(match[1].split(",")[4]) || 0 : 0;
    }

    function hold(offset) {
        slot.scroller.style.setProperty("--pan-offset", offset + "px");
        slot.text.classList.add("held");
    }

    function release() {
        slot.text.classList.remove("held");
        slot.scroller.style.removeProperty("--pan-offset");
    }

    // off the bar, not the feed: the feed is display:none while the counts are up
    function measure() {
        if (!feedEl) return;

        const style = getComputedStyle(bar);
        const gap = parseFloat(style.columnGap) || 0;
        let taken = (parseFloat(style.paddingLeft) || 0) + (parseFloat(style.paddingRight) || 0);
        let fixed = 0;

        for (const child of bar.children) {
            if (child === feedEl || child === bucketsEl) continue;
            if (getComputedStyle(child).display === "none") continue;
            taken += child.getBoundingClientRect().width;
            fixed++;
        }

        const width = bar.clientWidth - taken - gap * fixed;

        // summed from the chips: the container is flex:1, so its own box is the whole bar
        if (bucketsEl) {
            const chips = [...bucketsEl.children].filter((c) => getComputedStyle(c).display !== "none");
            const bucketsGap = parseFloat(getComputedStyle(bucketsEl).columnGap) || 0;
            const content = chips.reduce((a, c) => a + c.getBoundingClientRect().width, 0)
                + bucketsGap * Math.max(0, chips.length - 1);
            if (content) bucketsWidth = content;
        }

        wide = bar.clientWidth >= CHIPS_MIN_WIDTH && width - bucketsWidth - gap >= LINE_MIN_WIDTH;
        bar.classList.toggle("ticker-wide", wide);
        bar.classList.toggle("ticker-plain", !wide);
    }

    function buildBuckets() {
        if (!bucketsEl || !opts.buckets) return;
        for (const b of opts.buckets) {
            bucketSpans[b.id] = bucketChip(b);
            bucketsEl.appendChild(bucketSpans[b.id].item);
        }
    }

    function isEnabled() {
        return bar.classList.contains("visible");
    }

    function setEnabled(on) {
        if (isEnabled() === on) return;
        bar.classList.toggle("visible", on);
        if (!on) return suspend();
        measure();
        renderCounts();
        renderBuckets();
        resume();
    }

    function startPolling() {
        if (!poll || !isEnabled() || document.hidden || pollTimer) return;
        poll();
        pollTimer = setInterval(poll, pollMs);
    }
    function stopPolling() {
        clearInterval(pollTimer);
        pollTimer = null;
    }
    function resume() {
        startPolling();
        if (!isEnabled() || document.hidden || slideTimer) return;
        advance();
    }

    function suspend() {
        stopPolling();
        clearTimeout(slideTimer);
        clearTimeout(fadeTimer);
        slideTimer = fadeTimer = null;
        shownKey = null;
        bar.classList.remove("ticker-fading");
    }

    function schedule(ms) {
        clearTimeout(slideTimer);
        slideTimer = setTimeout(() => { slideTimer = null; advance(); }, ms);
    }

    const dwellFor = (event) => SLIDE_MS * LEVEL_DWELL[levelOf(event)];

    function setCounts(c) {
        counts = c;
        if (!isEnabled()) return;
        renderCounts();
        renderBuckets();
    }

    function renderBuckets() {
        for (const [bucket, el] of Object.entries(bucketSpans)) {
            const v = (counts.buckets || {})[bucket] || 0;
            if (el.count) el.count.textContent = compactCount(v);
            el.item.dataset.zero = v === 0 ? "true" : "false";
            el.item.classList.toggle("stat-off", !!bucketHidden(bucket));
        }
    }

    function renderCounts() {
        if (!countsEl) return;
        countsEl.textContent = counts.filtered
            ? compactCount(counts.shown) + " / " + compactCount(counts.total)
            : compactCount(counts.total);
        countsEl.parentElement.title = counts.filtered
            ? counts.shown + " of " + counts.total + " vessels shown"
            : counts.total + " vessels";
    }

    function dismiss(key) {
        dismissed.add(key);
        if (dismissed.size > DISMISSED_MAX) dismissed.delete(dismissed.values().next().value);
        const before = events.length;
        events = events.filter((e) => e.key !== key);
        if (events.length === before) return;
        cursor = -1;
        fadeTo(pick(Date.now()));
    }

    const pinned = (e) => (e.level || 0) >= 2 && !e.demoted;

    // newest first; the cap never evicts a pinned event; an urgent arrival takes the strip at once
    function push(incoming) {
        const held = new Set(events.map((e) => e.key));
        const fresh = incoming.filter((e) => !held.has(e.key) && !dismissed.has(e.key)).sort((a, b) => b.at - a.at);
        if (!fresh.length) return;
        const hadPinned = events.some(pinned);
        const all = fresh.concat(events);
        const keep = all.filter(pinned);
        const rest = all.filter((e) => !pinned(e)).slice(0, Math.max(0, MAX_EVENTS - keep.length));
        events = all.filter((e) => keep.includes(e) || rest.includes(e));
        cursor = -1;
        if (!hadPinned && fresh.some(pinned) && isEnabled() && !document.hidden) advance();
        else resume();
    }

    function periodFor(event, age) {
        if (event.demoted) return OLDEST_PERIOD;
        const slower = LEVEL_SLOWER[levelOf(event)];
        for (const p of PERIODS) if (age < p.under * slower) return p.period;
        return OLDEST_PERIOD;
    }

    function ageLabel(age) {
        const mins = Math.floor(age / 60000);
        return mins < 1 ? "just now" : mins + "m ago";
    }

    const showAge = (event) => { slot.age.textContent = ageLabel(Date.now() - event.at); };

    function advance() {
        const now = Date.now();
        events = events.filter((e) => now - e.at < MAX_AGE_MS * LEVEL_SLOWER[levelOf(e)]);
        fadeTo(pick(now));
    }

    // the next event: pinned ones take turns while any is held; otherwise the
    // next one due by its period, and failing that the next one at all
    function pick(now) {
        if (!events.length) return null;
        slide++;
        const pool = events.some(pinned) ? events.filter(pinned) : events;
        const skip = pool.length > 1 ? shownKey : null;
        const next = (due) => {
            for (let i = 0; i < pool.length; i++) {
                cursor = (cursor + 1) % pool.length;
                const e = pool[cursor];
                if (e.key !== skip && !(due && slide % periodFor(e, now - e.at))) return e;
            }
            return null;
        };
        return next(true) || next(false);
    }

    function fadeTo(event) {
        clearTimeout(slideTimer);
        slideTimer = null;
        const key = event ? event.key : null;
        if (key === shownKey) {
            if (event) { showAge(event); schedule(dwell); }
            return;
        }
        shownKey = key;

        bar.classList.add("ticker-fading");
        clearTimeout(fadeTimer);
        fadeTimer = setTimeout(() => {
            fadeTimer = null;
            const pan = show(event);
            if (event) {
                // never shorter than a panning line needs to be read
                dwell = Math.max(dwellFor(event), pan * 1000 + 2000);
                schedule(dwell);
            }
            // two frames: the incoming region is display:none until show() runs
            requestAnimationFrame(() => requestAnimationFrame(() => bar.classList.remove("ticker-fading")));
        }, FADE_MS);
    }

    // a narrow strip pans slower: a phone is read at arm's length
    const panSpeed = () => (wide ? 40 : 28);

    function panIfClipped() {
        slot.text.classList.remove("panning");

        // rects, not scrollWidth: engines disagree on what an inline-block that
        // overflows its clipped parent reports there
        const overflow = slot.scroller.getBoundingClientRect().width - slot.text.getBoundingClientRect().width;
        slot.overflow = Math.max(0, overflow);
        if (overflow <= 1) return 0;

        slot.scroller.style.setProperty("--pan-distance", -Math.ceil(overflow) + "px");
        const seconds = Math.max(4, overflow / panSpeed() + 3);
        slot.scroller.style.setProperty("--pan-duration", seconds + "s");
        slot.text.classList.add("panning");
        return seconds;
    }

    // the seconds the line pans, 0 when it fits
    function show(event) {
        if (!slot) return 0;
        slot.event = event;
        bar.classList.toggle("ticker-idle", !event);
        if (slot.shown !== !!event) {
            slot.item.style.display = event ? "flex" : "none";
            slot.shown = !!event;
        }
        if (!event) return 0;

        const d = event.describe ? event.describe() : event;
        const changed = slot.item.title !== d.text;
        slot.scroller.innerHTML = d.html;
        let pan = 0;
        if (changed) { release(); pan = panIfClipped(); }
        showAge(event);
        slot.dot.className = "ticker-dot" + (event.fresh ? " ticker-new" : "");
        slot.item.dataset.id = event.id != null ? event.id : "";
        slot.item.dataset.key = event.key;
        slot.item.title = d.text;
        return pan;
    }

    buildSlot();
    buildBuckets();
    show(null);

    const onVisibility = () => { if (document.hidden) suspend(); else resume(); };
    document.addEventListener("visibilitychange", onVisibility);

    const observer = new ResizeObserver(debounce(measure, 200));
    observer.observe(bar);

    function destroy() {
        suspend();
        observer.disconnect();
        document.removeEventListener("visibilitychange", onVisibility);
    }

    return { push, setCounts, setEnabled, isEnabled, advance, destroy, pollNow: () => { if (poll) poll(); } };
}
