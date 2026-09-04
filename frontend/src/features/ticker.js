import * as strip from '../../shared/ticker.js';
import { getChangeVal } from '../core/units.js';
import { CHANGE, CHANGE_LABEL, sanitizeString } from '../../shared/core/text.js';
import { decodeHTMLEntities } from '../core/util.js';
import { ships } from '../core/store.js';
import { getShipName } from '../core/names.js';
import * as binary from './binary.js';

const POLL_MS = 10000;

// the newest few on a cold open, asked for rather than filtered out afterwards;
// fetched wider than kept because most baselines are dropped below
const PRIME_MAX = 5;
const PRIME_FETCH = 15;

let bar = null;
let since = 0;
let pollTimer = null;

export function init(d) {
    bar = strip.create({
        mount: document.getElementById("ticker"),
        buckets: d.buckets,
        bucketHidden: d.bucketHidden,
        onSelect: (id) => { binary.eventSeen(Number(id)); d.openVessel(Number(id)); },
    });
    document.addEventListener("visibilitychange", () => document.hidden ? stopPolling() : startPolling());
}

export function setEnabled(on) {
    bar.setEnabled(on);
    if (on) startPolling(); else stopPolling();
}

export function setCounts(c) { bar.setCounts(c); }

function startPolling() {
    if (!bar.isEnabled() || document.hidden || pollTimer) return;
    poll();
    pollTimer = setInterval(poll, POLL_MS);
}

function stopPolling() {
    clearInterval(pollTimer);
    pollTimer = null;
}

// a first-set name is not news, but a first draught is a reading, and the only
// baseline the draught chart has to start from; what the receiver words as an
// event, a destination or a status, comes on the strip that way instead
function worthShowing(change) {
    return (!change.i || change.f === CHANGE.DRAUGHT) && change.f !== CHANGE.DESTINATION && change.f !== CHANGE.STATUS;
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

        const now = Date.now();
        bar.push(changes.map((c) => ({
            key: c.t + ":" + c.mmsi + ":" + c.f,
            at: serverNow ? now - Math.max(0, serverNow - c.t) * 1000 : now,
            id: c.mmsi,
            fresh: !!c.i,
            describe: () => describe(c),
        })));
    } catch (error) {
        console.log("Failed loading ticker changes:", error);
    }
    await pollEvents();
}

// the receiver's events: safety texts, destinations, status notices
const pollEvents = () => binary.pollEvents((events) => bar.push(events));
