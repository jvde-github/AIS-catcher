import { CHANGE, CHANGE_LABEL, getChangeVal, sanitizeString, compactCount } from '../core/format.js';
import { debounce, decodeHTMLEntities } from '../core/util.js';
import { ships } from '../core/store.js';
import { getShipName } from '../core/format.js';

// Top-of-map ticker: the vessel count plus a rotating view of what vessels have
// just reported changing. Pulled, not streamed - one poll every 10s carrying
// only what is new, so an idle viewer costs a request and an empty array.

const POLL_MS = 10000;
const SLIDE_MS = 8000;
const FADE_MS = 600;
const MAX_AGE_MS = 10 * 60 * 1000;
const MAX_EVENTS = 40;

// slides between turns for the fleet breakdown, so the counts stay visible even
// when there is news to tell
const COUNTS_EVERY = 4;

// what a first poll may show, so opening the map does not replay the whole ring
const PRIME_MAX = 5;
const PRIME_AGE = 60;

// Old news earns the screen less often. A slide only shows an event when the
// slide counter lands on its period, so fresh events appear every pass and a
// nine-minute-old one appears every fourth. Anything older is already gone.
const PERIODS = [
    { under: 2 * 60 * 1000, period: 1 },
    { under: 5 * 60 * 1000, period: 2 },
];
const OLDEST_PERIOD = 4;

// A wide screen can carry more than one headline, so the bar fills the space
// it has rather than leaving it blank.
const MAX_SLOTS = 3;
const SLOT_WIDTH = 520;

let deps = null;
let events = [];
let since = 0;
let cursor = -1;
let slide = 0;
let pollTimer = null;
let slideTimer = null;
let counts = { shown: 0, total: 0, filtered: false };

let bar, countsEl, bucketsEl, feedEl;
let slots = [];
let slotsWanted = 1;
let bucketSpans = {};

export function init(d) {
    deps = d;
    bar = document.getElementById("ticker");
    countsEl = document.getElementById("ticker_counts");
    bucketsEl = document.getElementById("ticker_buckets");
    feedEl = document.getElementById("ticker_feed");

    buildSlots();
    buildBuckets();
    show([]);

    document.addEventListener("visibilitychange", () => document.hidden ? suspend() : resume());
    window.addEventListener("resize", debounce(() => {
        if (!isEnabled()) return;
        measureSlots();
        advance();
    }, 200));
}

function buildSlots() {
    if (!feedEl) return;

    for (let i = 0; i < MAX_SLOTS; i++) {
        const item = document.createElement("div");
        item.className = "ticker-item";
        item.innerHTML = '<span class="ticker-dot"></span><span class="ticker-text"></span><span class="ticker-age"></span>';

        // the headline names a vessel, so it should take you to it
        item.addEventListener("click", () => {
            if (item.dataset.mmsi) deps.openVessel(Number(item.dataset.mmsi));
        });

        // Created hidden, and `shown` must say so. The slide loop only writes
        // display when it disagrees with `shown`, so a slot that claims to be
        // hidden while the stylesheet still shows it is never corrected - which
        // left empty slots rendering as a bare dot beside the one real headline.
        item.style.display = "none";

        feedEl.appendChild(item);
        slots.push({
            item,
            dot: item.querySelector(".ticker-dot"),
            text: item.querySelector(".ticker-text"),
            age: item.querySelector(".ticker-age"),
            shown: false,
        });
    }
}

// One headline per full slot's worth of width. Only a resize can change the
// answer, so it is not worth a layout read on every slide.
function measureSlots() {
    const width = feedEl ? feedEl.clientWidth : 0;
    slotsWanted = Math.max(1, Math.min(MAX_SLOTS, Math.floor(width / SLOT_WIDTH)));
}

// Cloned from the counters card rather than written out again: the sprite
// offsets, titles and bucket keys stay defined in exactly one place.
function buildBuckets() {
    const source = document.querySelector("#statcard .statcard_inner");
    if (!source || !bucketsEl) return;

    for (const item of source.children) {
        const clone = item.cloneNode(true);
        const count = clone.querySelector("[id]");
        if (count) count.removeAttribute("id");

        bucketSpans[clone.dataset.bucket] = { item: clone, count };
        bucketsEl.appendChild(clone);
    }
}

// The bar owns whether it is drawn and polling; body.ticker-open, which every
// ticker-aware offset hangs off, is applyPanels's to set. Two writers for one
// fact is what let kiosk hide the bar while the cards kept its inset.
export function setEnabled(on) {
    // applyPanels runs on every panel change, a vessel click included, and hands
    // the same answer down each time. Measuring and repainting the bar for a
    // state it is already in is the sort of waste that only shows on a Pi.
    if (!bar || isEnabled() === on) return;

    bar.classList.toggle("visible", on);
    if (!on) return suspend();

    measureSlots();
    renderCounts();
    renderBuckets();
    resume();
}

export function isEnabled() {
    return !!bar?.classList.contains("visible");
}

function resume() {
    if (!isEnabled() || document.hidden) return;

    if (!pollTimer) {
        poll();
        pollTimer = setInterval(poll, POLL_MS);
    }
    if (!slideTimer) slideTimer = setInterval(advance, SLIDE_MS);
}

function suspend() {
    clearInterval(pollTimer);
    clearInterval(slideTimer);
    pollTimer = slideTimer = null;
}

export function setCounts(c) {
    counts = c;

    // the live map refreshes these several times a minute; painting a bar that
    // is switched off, replaced by replay or hidden by kiosk is pure waste
    if (!isEnabled()) return;

    renderCounts();
    renderBuckets();
}

function renderBuckets() {
    for (const [bucket, el] of Object.entries(bucketSpans)) {
        const v = (counts.buckets || {})[bucket] || 0;
        if (el.count) el.count.textContent = v;
        el.item.dataset.zero = v === 0 ? "true" : "false";
        el.item.classList.toggle("stat-off", !!deps.bucketHidden(bucket));
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

// A baseline for a text field is the moment a vessel became identifiable, which
// is worth announcing. A baseline draught or status is not a change at all -
// every vessel emits one on the first message, so they would drown the rest.
function worthShowing(change) {
    if (!change.i) return true;
    return change.f === CHANGE.SHIPNAME || change.f === CHANGE.DESTINATION || change.f === CHANGE.CALLSIGN;
}

// Styled the way the shipcard's timeline styles a change: what it moved from is
// dimmed, what it moved to is the value that matters. Returns text as well, for
// the tooltip and for deciding whether the headline actually changed.
function describe(change) {
    // A name change is the one field whose new value is what shipName already
    // resolves to, so naming the vessel by it would print the same word twice
    // and read as an update with nothing in it. The MMSI is the subject there.
    const named = change.mmsi in ships ? getShipName(ships[change.mmsi].raw) : null;
    const name = change.f === CHANGE.SHIPNAME ? String(change.mmsi) : (named || String(change.mmsi));
    const to = getChangeVal(change);
    const label = (CHANGE_LABEL[change.f] || "").toLowerCase();
    const from = change.from != null && !change.i ? getChangeVal({ f: change.f, to: change.from }) : null;

    // The name arrives from shipsDB already escaped (script.js escapes it at
    // ingest), so it goes into the markup as-is and is decoded for the plain-text
    // tooltip. Escaping it again is what printed L&#039;HIPPOCAMPE on screen.
    let text = decodeHTMLEntities(name);
    let html = `<span class="tk-name">${name}</span>`;

    if (label) {
        text += " · " + label;
        html += `<span class="tk-sep">·</span><span class="tk-label">${sanitizeString(label)}</span>`;
    }
    if (from != null) {
        text += " " + from + " →";
        html += `<span class="tk-was">${sanitizeString(from)}</span><span class="tk-arrow">&rarr;</span>`;
    }
    text += " " + to;
    html += `<span class="tk-to">${sanitizeString(to)}</span>`;

    return { text, html };
}

// one field of one vessel at one second - MIN_INTERVAL on the server keeps a
// second write to the same field out of the same second
function changeKey(c) {
    return c.t + ":" + c.mmsi + ":" + c.f;
}

async function poll() {
    try {
        const response = await fetch("api/changes_recent.json" + (since ? "?since=" + since : ""));
        if (!response.ok) return;

        const data = await response.json();
        const serverNow = data.time || 0;
        const first = !since;

        // two rings arrive back to back, so time order is the client's job
        let changes = (data.changes || []).filter(worthShowing).sort((a, b) => b.t - a.t);

        // first poll: enough to fill the bar, not the whole ring
        if (first) changes = changes.filter((c) => serverNow - c.t < PRIME_AGE).slice(0, PRIME_MAX);

        // A second behind the server's clock: the ring is walked with a strict
        // `time > since`, so asking for exactly `time` steps over anything
        // written in that same second and loses it for good. The overlap is
        // deduped below rather than left to double up in the rotation.
        if (serverNow) since = serverNow - 1;

        const held = new Set(events.map((e) => changeKey(e.change)));
        changes = changes.filter((c) => !held.has(changeKey(c)));
        if (!changes.length) return;

        // change times are server time; only their distance from the server's
        // own clock survives a browser that disagrees
        const now = Date.now();
        const fresh = changes.map((c) => ({
            at: serverNow ? now - Math.max(0, serverNow - c.t) * 1000 : now,
            change: c,
        }));

        events = fresh.concat(events).slice(0, MAX_EVENTS);
        cursor = -1;
        advance();
    } catch (error) {
        console.log("Failed loading ticker changes:", error);
    }
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

    if (!events.length) return show([]);

    slide++;

    // the breakdown takes a turn of its own rather than sharing the bar: a
    // count sitting beside a headline reads as part of that headline
    if (slide % COUNTS_EVERY === 0) return show([]);

    const picked = [];

    // at most one lap, so no slot repeats an event another slot is showing
    for (let i = 0; i < events.length && picked.length < slotsWanted; i++) {
        cursor = (cursor + 1) % events.length;
        const event = events[cursor];
        if (slide % periodFor(now - event.at) === 0) picked.push(event);
    }

    show(picked.length ? picked : [events[0]]);
}

// nothing to report is not nothing to say: an empty list leaves the bar on the
// counters card's breakdown rather than blank
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

        // Described on the way to the screen, not on the way in: a change often
        // arrives before the vessel's static data does, and a name baked in at
        // poll time would stay an MMSI for the event's whole life.
        const { text, html } = describe(event.change);
        const changed = slot.item.title !== text;

        slot.text.innerHTML = html;
        slot.age.textContent = ageLabel(Date.now() - event.at);
        slot.dot.className = "ticker-dot" + (event.change.i ? " ticker-new" : "");
        slot.item.dataset.mmsi = event.change.mmsi;
        slot.item.title = text;

        // Web Animations rather than a class plus a forced reflow, and only when
        // the headline is actually a different one - a slide that re-shows the
        // same event should not blink.
        if (changed) slot.item.animate([{ opacity: 0 }, { opacity: 1 }], { duration: FADE_MS, easing: "ease-out" });
    });
}
