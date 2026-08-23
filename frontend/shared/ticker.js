/* ============================================================================
   ticker.js — a strip of recent events along an edge of the map (layer 2)

   Shows a few events at a time in slots that slide and fade, keeps a row
   of class-count chips and a total, and pans a line that does not fit.
   What an event says is the host's: it pushes events already described.

       const t = AISTicker.create({
           mount: document.getElementById("ticker"),     // .ticker with .ticker-feed etc. inside
           buckets: BUCKETS,                              // chip spec (core/filter.js BUCKETS)
           bucketHidden: (id) => filter.isHidden("bucket", id),
           onSelect: (id) => openVessel(id),
       });
       t.setEnabled(true);
       t.push([{ key, at, id, text, html, fresh }]);     // at: ms timestamp; fresh: marks a first sighting
       // or { key, at, id, fresh, describe: () => ({ text, html }) } to render with current names/units
       t.setCounts({ total, shown, filtered, buckets: { am: 3, ... } });

   Older events surface less often; the newest few hold their slot. The
   host decides when to poll and what to push; the strip does the rest.
   ========================================================================= */

import { bucketChip, debounce } from "./components.js";
import { compactCount } from "./core/text.js";

const SLIDE_MS = 8000;
const FADE_MS = 1000;
const MAX_AGE_MS = 30 * 60 * 1000;
const MAX_EVENTS = 40;

const COUNTS_EVERY = 4;
// not 0: PERIODS uses 4 and 8, whose slides a phase-0 counts slide would swallow
const COUNTS_PHASE = 1;

// older events surface less often: shown only when the slide counter lands on
// their period
const PERIODS = [
    { under: 2 * 60 * 1000, period: 1 },
    { under: 5 * 60 * 1000, period: 2 },
    { under: 15 * 60 * 1000, period: 4 },
];
const OLDEST_PERIOD = 8;

const MAX_SLOTS = 3;
const SLOT_WIDTH = 520;
const PAN_PX_PER_SEC = 40;

export function create(opts) {
    const bar = opts.mount;
    const countsEl = bar.querySelector(".ticker-count-value");
    const bucketsEl = bar.querySelector(".ticker-buckets");
    const feedEl = bar.querySelector(".ticker-feed");
    const onSelect = opts.onSelect || function () {};
    const bucketHidden = opts.bucketHidden || function () { return false; };

    let events = [];
    let cursor = -1;
    let slide = 0;
    let slideTimer = null;
    let fadeTimer = null;
    let counts = { shown: 0, total: 0, filtered: false };
    let slots = [];
    let slotsWanted = 1;
    let bucketSpans = {};
    let wide = false;
    let bucketsWidth = 0;
    let shownKey = null;
    let shownAny = false;   // the counts slide waits until something has been up

    bar.style.setProperty("--ticker-fade", FADE_MS + "ms");

    function buildSlots() {
        if (!feedEl) return;
        for (let i = 0; i < MAX_SLOTS; i++) {
            const item = document.createElement("div");
            item.className = "ticker-item";
            item.innerHTML = '<span class="ticker-dot"></span>'
                + '<span class="ticker-text"><span class="ticker-scroller"></span></span>'
                + '<span class="ticker-age"></span>';
            item.addEventListener("click", () => { if (item.dataset.id) onSelect(item.dataset.id); });
            item.style.display = "none";
            feedEl.appendChild(item);
            slots.push({
                item,
                dot: item.querySelector(".ticker-dot"),
                text: item.querySelector(".ticker-text"),
                scroller: item.querySelector(".ticker-scroller"),
                age: item.querySelector(".ticker-age"),
                shown: false,
            });
        }
    }

    // off the bar, not the feed: the feed is display:none while the counts are up
    function measureSlots() {
        if (!bar || !feedEl) return;
    
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
    
        const beside = width - bucketsWidth - gap;
        wide = beside >= SLOT_WIDTH;
        bar.classList.toggle("ticker-wide", wide);
    
        slotsWanted = Math.max(1, Math.min(MAX_SLOTS, Math.floor((wide ? beside : width) / SLOT_WIDTH) || 0));
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
        measureSlots();
        renderCounts();
        renderBuckets();
        resume();
    }

    function resume() {
        if (!isEnabled() || document.hidden) return;
        if (!slideTimer) slideTimer = setInterval(advance, SLIDE_MS);
        if (events.length && !shownKey) advance();
    }

    function suspend() {
        clearInterval(slideTimer);
        clearTimeout(fadeTimer);
        slideTimer = fadeTimer = null;
        shownKey = null;
        bar.classList.remove("ticker-fading");
    }

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

    /* newest first; an event already held (by key) is ignored */
    function push(incoming) {
        const held = new Set(events.map((e) => e.key));
        const fresh = incoming.filter((e) => !held.has(e.key)).sort((a, b) => b.at - a.at);
        if (!fresh.length) return;
        const wasEmpty = !events.length;
        events = fresh.concat(events).slice(0, MAX_EVENTS);
        cursor = -1;
        if (wasEmpty && isEnabled()) advance();
    }

    function periodFor(age) {
        for (const p of PERIODS) if (age < p.under) return p.period;
        return OLDEST_PERIOD;
    }
    
    function ageLabel(age) {
        const mins = Math.floor(age / 60000);
        return mins < 1 ? "just now" : mins + "m ago";
    }
    
    function advance() {
        const now = Date.now();
        events = events.filter((e) => now - e.at < MAX_AGE_MS);
    
        fadeTo(pick(now));
    }
    
    function pick(now) {
        if (!events.length) return [];
    
        slide++;
    
        if (!wide && shownAny && slide % COUNTS_EVERY === COUNTS_PHASE) return [];
    
        // skip what is already up, or a period-1 event holds its slot every lap
        const holding = events.length > slotsWanted && shownKey ? new Set(shownKey.split("|")) : null;
    
        const picked = [];
        const sweep = (due) => {
            for (let i = 0; i < events.length && picked.length < slotsWanted; i++) {
                cursor = (cursor + 1) % events.length;
                const event = events[cursor];
    
                if (picked.includes(event)) continue;
                if (holding && holding.has(event.key)) continue;
                if (due && slide % periodFor(now - event.at) !== 0) continue;
    
                picked.push(event);
            }
        };
    
        sweep(true);
        sweep(false);
    
        return picked;
    }
    
    function fadeTo(picked) {
        const key = picked.map((e) => e.key).join("|");
    
        if (key === shownKey) {
            picked.forEach((event, i) => {
                if (slots[i]) slots[i].age.textContent = ageLabel(Date.now() - event.at);
            });
            return;
        }
        shownKey = key;
    
        bar.classList.add("ticker-fading");
        clearTimeout(fadeTimer);
        fadeTimer = setTimeout(() => {
            show(picked);
            // two frames: the incoming region is display:none until show() runs
            requestAnimationFrame(() => requestAnimationFrame(() => bar.classList.remove("ticker-fading")));
        }, FADE_MS);
    }
    
    const PAN_PX_PER_SEC = 40;
    
    function panIfClipped(slot) {
        slot.text.classList.remove("panning");
    
        // rects, not scrollWidth: engines disagree on what an inline-block that
        // overflows its clipped parent reports there
        const overflow = slot.scroller.getBoundingClientRect().width - slot.text.getBoundingClientRect().width;
        if (overflow <= 1) return;
    
        slot.scroller.style.setProperty("--pan-distance", -Math.ceil(overflow) + "px");
        const wanted = Math.max(4, overflow / PAN_PX_PER_SEC + 3);
        slot.scroller.style.setProperty("--pan-duration", Math.min(wanted, SLIDE_MS / 1000) + "s");
        slot.text.classList.add("panning");
    }

    function show(picked) {
        if (!slots.length) return;
        bar.classList.toggle("ticker-idle", picked.length === 0);

        slots.forEach((slot, i) => {
            const event = picked[i];
            if (slot.shown !== !!event) {
                slot.item.style.display = event ? "flex" : "none";
                slot.shown = !!event;
            }
            if (!event) return;
            shownAny = true;

            const d = event.describe ? event.describe() : event;
            const changed = slot.item.title !== d.text;
            slot.scroller.innerHTML = d.html;
            if (changed) panIfClipped(slot);
            slot.age.textContent = ageLabel(Date.now() - event.at);
            slot.dot.className = "ticker-dot" + (event.fresh ? " ticker-new" : "");
            slot.item.dataset.id = event.id != null ? event.id : "";
            slot.item.title = d.text;
        });
    }

    buildSlots();
    buildBuckets();
    show([]);

    const onVisibility = () => (document.hidden ? suspend() : resume());
    document.addEventListener("visibilitychange", onVisibility);

    const observer = new ResizeObserver(debounce(() => {
        measureSlots();
        slots.forEach((slot, i) => {
            if (i >= slotsWanted && slot.shown) {
                slot.item.style.display = "none";
                slot.shown = false;
            }
        });
    }, 200));
    observer.observe(bar);

    function destroy() {
        suspend();
        observer.disconnect();
        document.removeEventListener("visibilitychange", onVisibility);
    }

    return { el: bar, push, setCounts, setEnabled, isEnabled, destroy };
}
