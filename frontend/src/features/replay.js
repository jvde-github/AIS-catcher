// Track replay: draws the fleet as it stood at a chosen instant, advanced by a
// rAF clock or set directly by the scrubber. Owns its layer and ship store and
// never reads shipsDB, so vessels long gone from the live view still draw with
// their real colour, name and size.

import OlFeature from 'ol/Feature';
import VectorLayer from 'ol/layer/Vector';
import VectorSource from 'ol/source/Vector';
import Point from 'ol/geom/Point';
import Style from 'ol/style/Style';
import Icon from 'ol/style/Icon';
import { fromLonLat } from 'ol/proj';

import { settings } from '../core/state.js';
import { calculateBearing } from '../core/geo.js';

// { getReceiver, spriteFor, iconScale, fadeOpacity, spriteSheet,
//   setLiveLayers, showNotification, onStateChange }
let deps = null;

const markerSource = new VectorSource({ features: [] });
export const markerLayer = new VectorLayer({ source: markerSource });

// mmsi -> { pts, cls, name, len }; pts is oldest-first [lat, lon, t, e]
let fleet = {};
// mmsi -> { marker, shown, style, last }
let features = {};
let bounds = { now: 0, oldest: 0, newest: 0 };
let blocks = new Set();
let manifest = {};
let filling = false;
let instant = 0;
let active = false;
let loading = false;

// How long a vessel lingers at its last known position once it stops reporting,
// matching how the live map holds and fades a ship that has gone quiet.
const STALE_DROP = 1800;

// Longest silence still worth interpolating across. Must stay above the
// server's GRANULARITY (300 s), below which history is thinned by design.
const MAX_INTERP = 600;

// Blocks are addressed by index so a stretch of time is always the same URL and
// a closed one can sit in the HTTP cache; a range derived from "now" would mint
// a new URL every second. The server owns the size and publishes it.
let BLOCK = 600;
const BLOCK_LOOKBACK = STALE_DROP;

const blockStart = (t) => Math.floor(t / BLOCK) * BLOCK;

const SPEEDS = [10, 30, 60, 120, 300];
let speed = 60;
let playing = false;
let raf = null;
let lastFrame = 0;

export function init(d) {
    deps = d;
    markerLayer.setVisible(false);
}

export function isActive() { return active; }
export function isLoading() { return loading; }
export function isPlaying() { return playing; }
export function getSpeed() { return speed; }

export function cycleSpeed() {
    speed = SPEEDS[(SPEEDS.indexOf(speed) + 1) % SPEEDS.length];
    deps.onStateChange?.();
}
// the replayable timeline, known from `bounds` before any block is fetched
export function getTimeline() {
    return { start: bounds.oldest, end: bounds.newest || bounds.now };
}
export function getInstant() { return instant; }

function api(path) {
    const sep = path.includes('?') ? '&' : '?';
    return fetch(`${path}${sep}receiver=${deps.getReceiver()}`).then((r) => {
        if (!r.ok) throw new Error(`${path} -> ${r.status}`);
        return r.json();
    });
}

export async function refreshBounds() {
    try {
        const info = await api('api/replay_info.json');
        bounds = { now: info.now || 0, oldest: info.oldest || 0, newest: info.newest || 0 };
        if (info.block > 0) BLOCK = info.block;
    } catch (e) {
        bounds = { now: 0, oldest: 0, newest: 0 };
    }
    return bounds;
}

// Blocks overlap by design, so the same point arrives more than once. A point
// is a duplicate only when the whole tuple matches, never on time alone: a
// vessel can hold several points sharing a timestamp when it clears the
// deadband twice in one second. Returns null if nothing was new.
function mergePoints(existing, incoming) {
    const merged = [];
    let i = 0, j = incoming.length - 1, added = false;

    while (i < existing.length || j >= 0) {
        const a = i < existing.length ? existing[i] : null;
        const b = j >= 0 ? incoming[j] : null;
        const cmp = !a ? 1 : !b ? -1 : a[2] - b[2] || a[3] - b[3];

        if (cmp < 0) { merged.push(a); i++; }
        else if (cmp > 0) { merged.push(b); j--; added = true; }
        else if (a[0] === b[0] && a[1] === b[1]) { merged.push(a); i++; j--; }
        else { merged.push(a); i++; }
    }
    return added ? merged : null;
}

// Idempotent, so prefetch and scrub can both ask freely.
async function loadBlock(start) {
    if (blocks.has(start)) return false;

    const data = await api(
        `api/replay.json?block=${start / BLOCK}&lookback=${BLOCK_LOOKBACK}`);
    blocks.add(start);

    for (const mmsi in data) {
        const pts = data[mmsi];
        if (!pts || pts.length === 0) continue;

        let s = fleet[mmsi];
        if (!s) {
            const m = manifest[mmsi] || {};
            s = fleet[mmsi] = {
                pts: [],
                cls: m.c ?? 1,
                name: m.n || '',
                // to_bow/to_stern are omitted when undefined; icon scaling wants 0
                len: (m.b || 0) + (m.s || 0),
            };
        }
        const merged = mergePoints(s.pts, pts);
        if (merged) s.pts = merged;
    }
    return true;
}

// `at` is where the playhead should sit. The replay always covers the whole
// timeline and the worker pulls in only what is near the playhead, so scrubbing
// never needs a reload.
export async function load(at) {
    if (loading) return false;
    loading = true;
    deps.onStateChange?.();

    try {
        if (!bounds.now) await refreshBounds();
        if (!bounds.now) {
            deps.showNotification('Replay: server returned no time bounds');
            return false;
        }

        fleet = {};
        blocks.clear();
        const tl = getTimeline();
        instant = at > 0 ? Math.max(tl.start, Math.min(tl.end, at)) : tl.start;

        // one manifest for the whole window; repeating it per block would undo
        // the saving that blocking is for
        manifest = await api(
            `api/replay_ships.json?since=${tl.start}&lookback=${BLOCK_LOOKBACK}`);

        // the block under the playhead is enough to draw; the worker does the rest
        await loadBlock(blockStart(instant));

        if (Object.keys(fleet).length === 0) {
            deps.showNotification('Replay: no tracks in that window');
            return false;
        }

        active = true;
        markerSource.clear();
        buildFeatures();
        markerLayer.setVisible(true);
        deps.setLiveLayers(false);
        draw();

        pump();
        return true;
    } catch (err) {
        console.log('Replay load failed: ' + err);
        deps.showNotification('Replay: failed to load history');
        return false;
    } finally {
        loading = false;
        deps.onStateChange?.();
    }
}

// The block under the playhead, then a short runway ahead and one behind.
// Chosen fresh each call rather than as a fixed list: the playhead keeps moving
// while a request is in flight, so the best next block moves with it.
function nextWanted() {
    const tl = getTimeline();
    const first = blockStart(tl.start), last = blockStart(tl.end);
    const cur = Math.min(Math.max(blockStart(instant), first), last);

    for (const d of [0, 1, 2, 3, -1]) {
        const b = cur + d * BLOCK;
        if (b >= first && b <= last && !blocks.has(b)) return b;
    }
    return null;
}

// Re-entrant calls are no-ops, so seek and the play clock can poke it freely.
async function pump() {
    if (filling) return;
    filling = true;

    try {
        let b;
        while (active && (b = nextWanted()) !== null) {
            await loadBlock(b);
            buildFeatures();
            if (!playing) draw();
            deps.onStateChange?.();
        }
    } catch (err) {
        console.log('Replay block fetch failed: ' + err);
    } finally {
        filling = false;
    }
}

export function play() {
    const tl = getTimeline();
    if (!active || playing || tl.end <= tl.start) return;
    // restarting from the very end would show a single frame and stop
    if (instant >= tl.end) instant = tl.start;

    playing = true;
    lastFrame = performance.now();

    const tick = (now) => {
        if (!playing) return;
        const dt = (now - lastFrame) / 1000;
        lastFrame = now;

        const wasBlock = blockStart(instant);
        instant += dt * speed;
        if (blockStart(instant) !== wasBlock) pump();

        if (instant >= tl.end) {
            instant = tl.end;
            playing = false;
            raf = null;
            draw();
            deps.onStateChange?.();
            return;
        }
        draw();
        deps.onStateChange?.();
        raf = requestAnimationFrame(tick);
    };
    raf = requestAnimationFrame(tick);
    deps.onStateChange?.();
}

export function pause() {
    playing = false;
    if (raf !== null) {
        cancelAnimationFrame(raf);
        raf = null;
    }
    deps.onStateChange?.();
}

export function stop() {
    pause();
    active = false;
    fleet = {};
    features = {};
    blocks.clear();
    manifest = {};
    markerSource.clear();
    markerLayer.setVisible(false);
    deps.setLiveLayers(true);
    deps.onStateChange?.();
}

export function seek(time) {
    if (!active) return;
    const tl = getTimeline();
    instant = Math.max(tl.start, Math.min(tl.end, time));
    draw();
    deps.onStateChange?.();
    pump();
}

// Course and speed made good from a to b. Bearing comes from the segment being
// travelled, so the icon always points where the vessel is visibly going.
function leg(s, a, b) {
    if (a < 0 || b >= s.pts.length || a === b) return { brg: null, knots: 0 };

    const [latA, lonA] = s.pts[a], [latB, lonB] = s.pts[b];
    const dLat = latB - latA;
    const dLon = (lonB - lonA) * Math.cos(latA * Math.PI / 180);
    const metres = Math.hypot(dLat, dLon) * 111320;
    const secs = Math.max(1, s.pts[b][2] - s.pts[a][3]);

    return {
        brg: metres > 1 ? calculateBearing([lonA, latA], [lonB, latB]) : null,
        knots: (metres / secs) / 0.514444,
    };
}

// Hold a vessel that stopped reporting at point i, keeping the heading and
// sprite it had when last heard: one that went quiet at 12 knots is still a
// moving vessel, just an older fix.
function holdAt(s, i, T) {
    const age = T - s.pts[i][3];
    if (age > STALE_DROP) return null;

    const l = i > 0 ? leg(s, i - 1, i) : { brg: null, knots: 0 };
    return { lat: s.pts[i][0], lon: s.pts[i][1], knots: l.knots, brg: l.brg, age };
}

// index of the first point whose dwell ends at or after T
function firstAlive(pts, T) {
    let lo = 0, hi = pts.length - 1;
    while (lo < hi) {
        const mid = (lo + hi) >> 1;
        if (pts[mid][3] < T) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

// Position of one ship at time T.
//   - before its first point or after its last dwell ends: not present
//   - inside a point's [t, e] span: parked there (moored vessels hold still)
//   - between two points: linear along the segment
function sample(s, T) {
    const pts = s.pts;
    // before its first report a vessel genuinely is not there yet
    if (T < pts[0][2]) return null;
    if (T > pts[pts.length - 1][3]) return holdAt(s, pts.length - 1, T);

    const i = firstAlive(pts, T);

    if (T >= pts[i][2]) {
        // Sitting on a point. A real dwell means the vessel is genuinely
        // holding station; a zero-length one is just the instant we happen to
        // have landed on, so take heading from the leg it is travelling.
        if (pts[i][3] > pts[i][2])
            return { lat: pts[i][0], lon: pts[i][1], knots: 0, brg: null };

        const l = i > 0 ? leg(s, i - 1, i) : leg(s, i, i + 1);
        return { lat: pts[i][0], lon: pts[i][1], knots: l.knots, brg: l.brg };
    }

    // In the gap between two points. Sliding smoothly across a long silence
    // would invent a track the vessel never sailed, so past the cutoff hold it.
    const a = i - 1, b = i;
    const gap = pts[b][2] - pts[a][3];
    if (gap > MAX_INTERP) return holdAt(s, a, T);

    const f = gap > 0 ? Math.max(0, Math.min(1, (T - pts[a][3]) / gap)) : 0;
    const l = leg(s, a, b);

    return {
        lat: pts[a][0] + (pts[b][0] - pts[a][0]) * f,
        lon: pts[a][1] + (pts[b][1] - pts[a][1]) * f,
        knots: l.knots, brg: l.brg,
    };
}

// Additive: later blocks bring vessels the first did not have, and an existing
// entry must keep its feature or the source is left holding an orphan.
function buildFeatures() {
    for (const mmsi in fleet) {
        if (features[mmsi]) continue;

        const marker = new OlFeature({ geometry: new Point([0, 0]) });
        marker.set('mmsi', mmsi);
        marker.set('name', fleet[mmsi].name);

        features[mmsi] = { marker, shown: false, style: null, last: {} };
    }
}

// Styles persist and are mutated in place: rebuilding fleet-sized OL objects at
// animation rate is pure allocation churn, and a frame that changes nothing for
// a ship touches nothing.
function place(f, s, fix) {
    const sp = deps.spriteFor(s.cls, fix.knots, fix.brg);
    const scale = (settings.icon_scale ?? 1) * deps.iconScale(s.len);
    const opacity = fix.age ? deps.fadeOpacity(fix.age) : 1;
    const l = f.last;

    if (l.cx === sp.cx && l.cy === sp.cy && l.rot === sp.rot && l.scale === scale
        && l.opacity === opacity && l.lat === fix.lat && l.lon === fix.lon) return;

    // the sprite cell fixes the Icon; rotation, scale and opacity are mutable
    if (l.cx !== sp.cx || l.cy !== sp.cy) {
        f.style = new Style({
            image: new Icon({
                src: deps.spriteSheet,
                offset: [sp.cx, sp.cy],
                size: [sp.imgSize, sp.imgSize],
            }),
        });
        f.marker.setStyle(f.style);
    }
    const img = f.style.getImage();
    img.setRotation(sp.rot);
    img.setScale(scale);
    img.setOpacity(opacity);

    // setCoordinates signals the change itself; a pure style change must ask
    if (l.lat !== fix.lat || l.lon !== fix.lon)
        f.marker.getGeometry().setCoordinates(fromLonLat([fix.lon, fix.lat]));
    else
        f.marker.changed();

    f.last = { cx: sp.cx, cy: sp.cy, rot: sp.rot, scale, opacity, lat: fix.lat, lon: fix.lon };
}

function draw() {
    if (!active) return;

    for (const mmsi in fleet) {
        const f = features[mmsi];
        const fix = sample(fleet[mmsi], instant);

        if (!fix) {
            if (f.shown) {
                markerSource.removeFeature(f.marker);
                f.shown = false;
            }
            continue;
        }

        place(f, fleet[mmsi], fix);

        if (!f.shown) {
            markerSource.addFeature(f.marker);
            f.shown = true;
        }
    }
}
