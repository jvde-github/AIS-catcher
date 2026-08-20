import { CHANGE, CHANGE_LABEL, getChangeVal, sanitizeString, compactCount } from '../core/format.js';
import { debounce, decodeHTMLEntities } from '../core/util.js';
import { ships } from '../core/store.js';
import { getShipName } from '../core/format.js';

const POLL_MS = 10000;
const SLIDE_MS = 8000;
const FADE_MS = 1000;
const MAX_AGE_MS = 30 * 60 * 1000;
const MAX_EVENTS = 40;

const COUNTS_EVERY = 4;
// not 0: PERIODS uses 4 and 8, whose slides a phase-0 counts slide would swallow
const COUNTS_PHASE = 1;

// the newest few on a cold open, asked for rather than filtered out afterwards;
// fetched wider than kept because most baselines are dropped below
const PRIME_MAX = 5;
const PRIME_FETCH = 15;

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
let fadeTimer = null;
let wide = false;
let bucketsWidth = 0;
let shownKey = null;

export function init(d) {
    deps = d;
    bar = document.getElementById("ticker");
    countsEl = document.getElementById("ticker_counts");
    bucketsEl = document.getElementById("ticker_buckets");
    feedEl = document.getElementById("ticker_feed");

    bar.style.setProperty("--ticker-fade", FADE_MS + "ms");

    buildSlots();
    buildBuckets();
    show([]);

    document.addEventListener("visibilitychange", () => document.hidden ? suspend() : resume());

    new ResizeObserver(debounce(() => {
        measureSlots();
        slots.forEach((slot, i) => {
            if (i >= slotsWanted && slot.shown) {
                slot.item.style.display = "none";
                slot.shown = false;
            }
        });
    }, 200)).observe(bar);
}

function buildSlots() {
    if (!feedEl) return;

    for (let i = 0; i < MAX_SLOTS; i++) {
        const item = document.createElement("div");
        item.className = "ticker-item";
        item.innerHTML = '<span class="ticker-dot"></span>'
            + '<span class="ticker-text"><span class="ticker-scroller"></span></span>'
            + '<span class="ticker-age"></span>';

        item.addEventListener("click", () => {
            if (item.dataset.mmsi) deps.openVessel(Number(item.dataset.mmsi));
        });

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
    let taken = parseFloat(style.paddingLeft) + parseFloat(style.paddingRight);
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

    slotsWanted = Math.max(1, Math.min(MAX_SLOTS, Math.floor((wide ? beside : width) / SLOT_WIDTH)));
}

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

export function setEnabled(on) {
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
    clearTimeout(fadeTimer);
    pollTimer = slideTimer = fadeTimer = null;
    shownKey = null;
    bar.classList.remove("ticker-fading");
}

export function setCounts(c) {
    counts = c;

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

// deliberate: a first-set name or destination is not news, but a first draught
// is a reading - and the only baseline the draught chart has to start from
function worthShowing(change) {
    return !change.i || change.f === CHANGE.DRAUGHT;
}

function describe(change) {
    const named = change.mmsi in ships ? getShipName(ships[change.mmsi].raw) : null;
    const name = change.f === CHANGE.SHIPNAME ? String(change.mmsi) : (named || String(change.mmsi));
    const to = getChangeVal(change);
    const label = (CHANGE_LABEL[change.f] || "").toLowerCase();
    const from = change.from != null && !change.i ? getChangeVal({ f: change.f, to: change.from }) : null;

    // names arrive from the store already escaped; the change values do not
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

function changeKey(c) {
    return c.t + ":" + c.mmsi + ":" + c.f;
}

async function poll() {
    try {
        const response = await fetch("api/changes_recent.json"
            + (since ? "?since=" + since : "?max=" + PRIME_FETCH));
        if (!response.ok) return;

        const data = await response.json();
        const serverNow = data.time || 0;
        const first = !since;

        let changes = (data.changes || []).filter(worthShowing).sort((a, b) => b.t - a.t);

        if (first) changes = changes.slice(0, PRIME_MAX);

        // a second behind: the ring is walked with a strict `time > since`
        if (serverNow) since = serverNow - 1;

        const held = new Set(events.map((e) => changeKey(e.change)));
        changes = changes.filter((c) => !held.has(changeKey(c)));
        if (!changes.length) return;

        const now = Date.now();
        const fresh = changes.map((c) => ({
            at: serverNow ? now - Math.max(0, serverNow - c.t) * 1000 : now,
            change: c,
        }));

        const wasEmpty = !events.length;
        events = fresh.concat(events).slice(0, MAX_EVENTS);
        cursor = -1;
        if (wasEmpty) advance();
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

    fadeTo(pick(now));
}

function pick(now) {
    if (!events.length) return [];

    slide++;

    if (!wide && slide % COUNTS_EVERY === COUNTS_PHASE) return [];

    // skip what is already up, or a period-1 event holds its slot every lap
    const holding = events.length > slotsWanted && shownKey ? new Set(shownKey.split("|")) : null;

    const picked = [];
    const sweep = (due) => {
        for (let i = 0; i < events.length && picked.length < slotsWanted; i++) {
            cursor = (cursor + 1) % events.length;
            const event = events[cursor];

            if (picked.includes(event)) continue;
            if (holding && holding.has(changeKey(event.change))) continue;
            if (due && slide % periodFor(now - event.at) !== 0) continue;

            picked.push(event);
        }
    };

    sweep(true);
    sweep(false);

    return picked;
}

function fadeTo(picked) {
    const key = picked.map((e) => changeKey(e.change)).join("|");

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
    slot.scroller.style.setProperty("--pan-duration", Math.max(4, overflow / PAN_PX_PER_SEC + 3) + "s");
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

        const { text, html } = describe(event.change);
        const changed = slot.item.title !== text;

        slot.scroller.innerHTML = html;
        if (changed) panIfClipped(slot);
        slot.age.textContent = ageLabel(Date.now() - event.at);
        slot.dot.className = "ticker-dot" + (event.change.i ? " ticker-new" : "");
        slot.item.dataset.mmsi = event.change.mmsi;
        slot.item.title = text;
    });
}
