// Track replay: draws the fleet as it stood at a chosen instant, advanced by a
// rAF clock or set directly by the scrubber. Owns its layer and ship store and
// never reads shipsDB, so vessels long gone from the live view still draw with
// their real colour, name and size.

import OlFeature from 'ol/Feature';
import VectorLayer from 'ol/layer/Vector';
import VectorSource from 'ol/source/Vector';
import Point from 'ol/geom/Point';
import Polygon from 'ol/geom/Polygon';
import Style from 'ol/style/Style';
import Icon from 'ol/style/Icon';
import { fromLonLat } from 'ol/proj';

import { settings } from '../core/state.js';
import { decodeHTMLEntities } from '../core/util.js';
import { shipOutlineLocal } from '../../shared/core/geo.js';
import { getSpeedVal, getSpeedUnit } from '../core/units.js';
import { getDeltaTimeVal, getShipTypeShort, sanitizeString, getCountryName } from '../../shared/core/text.js';
import { flagHTML } from '../../shared/components.js';

// { getReceiver, spriteFor, iconScale, fadeOpacity, labelText, spriteSheet,
//   getResolution, hullStyle, setLiveLayers, showNotification, onStateChange }
let deps = null;

const markerSource = new VectorSource({ features: [] });
export const markerLayer = new VectorLayer({ source: markerSource });

const hullSource = new VectorSource({ features: [] });
export const hullLayer = new VectorLayer({ source: hullSource });

const HULL_ON_PX = 12;
const HULL_OFF_PX = 10;
// Ceiling on the OL objects the animation holds, independent of fleet size:
// the pixel gate means only vessels large on screen ever ask for one.
const HULL_POOL_MAX = 256;

const DEG = Math.PI / 180;
const hullPool = [];        // idle { feature, geom, ring } triples
const hulls = {};           // mmsi -> triple currently on screen

// mmsi -> { pts, cls, name, len, dim }; dim is [bow, stern, port, starboard]
// relative to the reported position; pts is oldest-first [lat, lon, t, e, sog, cog, hdg]
let fleet = {};
// mmsi -> { marker, shown, style, last }
let features = {};
let bounds = { now: 0, oldest: 0, newest: 0 };
// Point encoding the server publishes: 1 is sog in half knots and no angles,
// 2 adds cog/hdg and moves sog to tenths. Blocks are cached per format.
let pointFormat = 1;
let sogDiv = 2;
let cogDiv = 1;      // 0.1 degree units in format 2
let underWayUnits = 2;   // 1 knot in the current sog units
let blocks = new Set();
let manifest = {};
let filling = false;
let instant = 0;
let active = false;
let loading = false;
// bumped on teardown; an in-flight load drops its work if it no longer matches
let generation = 0;

// How long a vessel lingers at its last known position once it stops reporting,
// matching how the live map holds and fades a ship that has gone quiet.
const STALE_DROP = 1800;

// Longest silence worth interpolating across. Must clear the gaps thinned
// history leaves — the server's thinning spacing plus the raw gap that survives
// it, both published in replay_info — or an old track stutters where the same
// track interpolated smoothly when fresh.
let MAX_INTERP = 1200;

// Blocks are addressed by index so a stretch of time is always the same URL and
// a closed one can sit in the HTTP cache; a range derived from "now" would mint
// a new URL every second. The server owns the size and publishes it.
let BLOCK = 600;
const BLOCK_LOOKBACK = STALE_DROP;

const blockStart = (t) => Math.floor(t / BLOCK) * BLOCK;

const SPEEDS = [10, 30, 60, 120, 300, 600, 900, 1800, 3600];
let speed = 300;
let playing = false;
let raf = null;
let lastFrame = 0;

export function init(d) {
    deps = d;
    markerLayer.setVisible(false);
    hullLayer.setVisible(false);
    if (d.hullStyle) hullLayer.setStyle(d.hullStyle);
}

export function isActive() { return active; }

// the interpolated position being drawn this frame, not the vessel's live fix
export function fixFor(mmsi) {
    const fix = features[mmsi]?.fix;
    return fix ? { lat: fix.lat, lon: fix.lon } : null;
}
export function refresh() { if (active) draw(); }
export function isLoading() { return loading; }
export function isPlaying() { return playing; }
export function getSpeed() { return speed; }

export function cycleSpeed() {
    speed = SPEEDS[(SPEEDS.indexOf(speed) + 1) % SPEEDS.length];
    deps.onStateChange?.();
}

export function speeds() { return SPEEDS; }

export function setSpeed(s) {
    if (!SPEEDS.includes(s)) return;
    speed = s;
    deps.onStateChange?.();
}

let labels = false;

export function getLabels() { return labels; }
export function toggleLabels() {
    labels = !labels;
    if (active) draw();
    deps.onStateChange?.();
}
export function getTimeline() {
    if (!bounds.oldest || !bounds.newest || bounds.newest <= bounds.oldest)
        return { start: 0, end: 0 };
    return { start: bounds.oldest, end: bounds.newest };
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
        if (info.granularity > 0 && info.dwell_gap > 0) MAX_INTERP = info.granularity + info.dwell_gap;
        pointFormat = info.point_format > 0 ? info.point_format : 1;
        sogDiv = pointFormat >= 2 ? 10 : 2;
        cogDiv = pointFormat >= 2 ? 10 : 1;
        underWayUnits = pointFormat >= 2 ? 10 : 2;
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

    const gen = generation;
    const data = await api(
        `api/replay.json?block=${start / BLOCK}&lookback=${BLOCK_LOOKBACK}&pf=${pointFormat}`);
    if (gen !== generation) return false;
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
                // escaped at ingest like the live store; tooltips render innerHTML
                name: sanitizeString(m.n || ''),
                country: sanitizeString(m.f || ''),
                type: m.t ?? null,
                dim: m.d || null,
                len: m.d ? m.d[0] + m.d[1] : 0,
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
    const gen = ++generation;
    deps.onStateChange?.();

    try {
        if (!bounds.now) await refreshBounds();
        if (!bounds.now) {
            deps.showNotification('Replay: server returned no time bounds');
            return false;
        }

        if (gen !== generation) return false;

        fleet = {};
        features = {};
        blocks.clear();
        const tl = getTimeline();
        instant = at > 0 ? Math.max(tl.start, Math.min(tl.end, at)) : tl.start;

        // One manifest for the whole window; repeating it per block would undo
        // the saving that blocking is for. Styling only, so losing it degrades
        // the looks, not the replay.
        manifest = await api(
            `api/replay_ships.json?since=${tl.start}&lookback=${BLOCK_LOOKBACK}`)
            .catch(() => ({}));

        await loadBlock(blockStart(instant));

        if (gen !== generation) return false;

        if (Object.keys(fleet).length === 0) {
            deps.showNotification('Replay: no tracks in that window');
            return false;
        }

        active = true;
        markerSource.clear();
        hullSource.clear();
        buildFeatures();
        markerLayer.setVisible(true);
        hullLayer.setVisible(true);
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

// Seconds of wall-clock playback the runway ahead of the playhead should cover.
// Measured in wall clock rather than blocks so the fast speeds, which burn a
// block every fraction of a second, still get a usable buffer.
const RUNWAY = 4;

// The block under the playhead, then a runway ahead and one behind. Chosen
// fresh each call rather than as a fixed list: the playhead keeps moving while
// a request is in flight, so the best next block moves with it.
function nextWanted() {
    const tl = getTimeline();
    const first = blockStart(tl.start), last = blockStart(tl.end);
    const cur = Math.min(Math.max(blockStart(instant), first), last);
    const ahead = Math.max(3, Math.ceil((speed * RUNWAY) / BLOCK));

    for (let d = 0; d <= ahead; d++) {
        const b = cur + d * BLOCK;
        if (b >= first && b <= last && !blocks.has(b)) return b;
    }

    const back = cur - BLOCK;
    if (back >= first && back <= last && !blocks.has(back)) return back;
    return null;
}

// Re-entrant calls are no-ops, so seek and the play clock can poke it freely.
async function pump() {
    if (filling) return;
    filling = true;

    const gen = generation;
    try {
        let b;
        while (active && gen === generation && (b = nextWanted()) !== null) {
            await loadBlock(b);
            if (gen !== generation) return;
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
        // rAF stops in a hidden tab; uncapped, the frame back skips to the end
        const dt = Math.min((now - lastFrame) / 1000, 0.25);
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
    generation++;
    active = false;
    fleet = {};
    features = {};
    countSig = "";
    blocks.clear();
    manifest = {};
    markerSource.clear();
    markerLayer.setVisible(false);
    for (const mmsi in hulls) releaseHull(mmsi);
    hullSource.clear();
    hullLayer.setVisible(false);
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

const NO_LEG = { knots: 0 };

const headingOf = (p) => (p[6] != null ? p[6] : null);
const courseMadeGood = (p) => (p[5] != null ? p[5] / cogDiv : null);

// Icons turn to the course, hulls to the heading — falling back to the course
// only while the vessel is moving, a stopped vessel's course being noise.
const courseOf = (p) => courseMadeGood(p) ?? headingOf(p);
const angleOf = (p) =>
    headingOf(p) ?? (p[4] != null && p[4] > underWayUnits ? courseMadeGood(p) : null);

// Shortest way round: a vessel swinging through north must not spin 359 degrees.
function lerpAngle(from, to, f) {
    let d = ((to - from + 540) % 360) - 180;
    return (from + d * f + 360) % 360;
}

// Speed made good from a to b, for vessels whose own report says nothing.
// Constant for the segment's lifetime, so this runs once per segment rather than
// per frame; the cache keys on the pts array so a block merge invalidates it.
function leg(s, a, b) {
    if (a < 0 || b >= s.pts.length || a === b) return NO_LEG;
    if (s.legPts === s.pts && s.legA === a && s.legB === b) return s.legV;

    const [latA, lonA] = s.pts[a], [latB, lonB] = s.pts[b];
    const dLat = latB - latA;
    const dLon = (lonB - lonA) * Math.cos(latA * Math.PI / 180);
    const metres = Math.hypot(dLat, dLon) * 111320;
    const secs = Math.max(1, s.pts[b][2] - s.pts[a][3]);

    s.legPts = s.pts;
    s.legA = a;
    s.legB = b;
    s.legV = { knots: (metres / secs) / 0.514444 };
    return s.legV;
}

// Hold a vessel that stopped reporting at point i, keeping the heading and
// sprite it had when last heard: one that went quiet at 12 knots is still a
// moving vessel, just an older fix.
function holdAt(s, i, T) {
    const age = T - s.pts[i][3];
    if (age > STALE_DROP) return null;

    const sog = s.pts[i][4];
    const knots = sog != null ? sog / sogDiv : (i > 0 ? leg(s, i - 1, i).knots : 0);

    return { lat: s.pts[i][0], lon: s.pts[i][1], knots, age,
             hdg: angleOf(s.pts[i]), cog: courseOf(s.pts[i]) };
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

// Position of one ship at time T: absent before its first point, held then
// dropped after its last, in between sliding linearly along its current
// segment. A dwell on an idle vessel parks instead; on a vessel under way a
// dwell is only deadband compression, so it keeps sailing — anchored at the
// dwell's start so the position is continuous where the gap takes over.
function sample(s, T) {
    const pts = s.pts;
    if (T < pts[0][2]) return null;
    if (T > pts[pts.length - 1][3]) return holdAt(s, pts.length - 1, T);

    const i = firstAlive(pts, T);
    const a = T >= pts[i][2] ? i : i - 1;
    const pa = pts[a];
    // sog units follow the server's point format, null when unknown; unknown
    // parks, matching blocks cached before the field existed
    const sog = pa[4];
    const underWay = sog != null && sog > underWayUnits;

    if (T <= pa[3] && !underWay)
        return { lat: pa[0], lon: pa[1], knots: 0, hdg: angleOf(pa), cog: courseOf(pa) };

    const b = a + 1;
    if (b < pts.length && pts[b][2] - pa[3] <= MAX_INTERP) {
        const start = underWay ? pa[2] : pa[3];
        const span = pts[b][2] - start;
        const f = span > 0 ? Math.max(0, Math.min(1, (T - start) / span)) : 0;
        const l = leg(s, a, b);
        // turning through the segment reads as a turn, not a snap at the end
        const between = (of) => {
            const x = of(pa), y = of(pts[b]);
            return x != null && y != null ? lerpAngle(x, y, f) : (x ?? y);
        };

        return {
            lat: pa[0] + (pts[b][0] - pa[0]) * f,
            lon: pa[1] + (pts[b][1] - pa[1]) * f,
            knots: underWay ? sog / sogDiv : l.knots,
            hdg: between(angleOf),
            cog: between(courseOf),
        };
    }

    // no next point within reach: hold rather than invent a track never sailed
    if (T <= pa[3]) {
        const l = a > 0 ? leg(s, a - 1, a) : NO_LEG;
        return { lat: pa[0], lon: pa[1], knots: sog / sogDiv, hdg: angleOf(pa), cog: courseOf(pa) };
    }
    return holdAt(s, a, T);
}

// built from the frame being shown, so it follows the replay clock
function tooltipHTML(mmsi, s, fix) {
    if (!fix) return '';
    let html = '<div class="tooltip-card">'
        + flagHTML(s.country, 'flag-tooltip', getCountryName(s.country)) + '<div>'
        + (s.name || 'MMSI ' + mmsi)
        + '<span class="tooltip-dim"> at </span>' + getSpeedVal(fix.knots || 0) + ' ' + getSpeedUnit();
    let sub = '';
    if (s.type != null) sub += getShipTypeShort(s.type);
    if (fix.age) sub += (sub ? ' - ' : '') + 'Silent for ' + getDeltaTimeVal(Math.round(fix.age));
    if (sub) html += '<div class="tooltip-sub">' + sub + '</div>';
    return html + '</div></div>';
}

// Additive: later blocks bring vessels the first did not have, and an existing
// entry must keep its feature or the source is left holding an orphan.
function buildFeatures() {
    for (const mmsi in fleet) {
        if (features[mmsi]) continue;

        const marker = new OlFeature({ geometry: new Point([0, 0]) });
        marker.set('mmsi', mmsi);
        marker.set('name', fleet[mmsi].name);

        const f = { marker, shown: false, style: null, last: {}, fix: null };
        // `tooltip` is what the map's pointermove looks for, `replayMmsi` what
        // its contextmenu uses to tell a replayed vessel from a live one
        Object.defineProperty(marker, 'tooltip', {
            get: () => tooltipHTML(mmsi, fleet[mmsi] || {}, f.fix),
        });
        marker.replayMmsi = Number(mmsi);
        features[mmsi] = f;
    }
}

// Styles persist and are mutated in place: rebuilding fleet-sized OL objects at
// animation rate is pure allocation churn, and a frame that changes nothing for
// a ship touches nothing.
function place(f, s, fix) {
    const sp = deps.spriteFor(s.cls, fix.knots, fix.cog);
    const scale = (settings.icon_scale ?? 1) * deps.iconScale(s.len);
    const opacity = fix.age ? deps.fadeOpacity(fix.age) : 1;
    const label = labels ? (s.name || String(f.marker.get('mmsi'))) : '';
    const l = f.last;

    if (l.cx === sp.cx && l.cy === sp.cy && l.rot === sp.rot && l.scale === scale
        && l.opacity === opacity && l.lat === fix.lat && l.lon === fix.lon
        && l.label === label) return;

    // the sprite cell fixes the Icon; rotation, scale and opacity are mutable
    const rebuilt = l.cx !== sp.cx || l.cy !== sp.cy;
    if (rebuilt) {
        f.style = new Style({
            image: new Icon({
                src: deps.spriteSheet,
                offset: [sp.cx, sp.cy],
                size: [sp.imgSize, sp.imgSize],
            }),
        });
        f.marker.setStyle(f.style);
    }
    // the label carries the fade too, so a fading held vessel refreshes its text
    if (rebuilt || l.label !== label || (label && l.opacity !== opacity))
        f.style.setText(label ? deps.labelText(decodeHTMLEntities(label), opacity, s.cls) : null);
    const img = f.style.getImage();
    img.setRotation(sp.rot);
    img.setScale(scale);
    img.setOpacity(opacity);

    // setCoordinates signals the change itself; a pure style change must ask
    if (l.lat !== fix.lat || l.lon !== fix.lon)
        f.marker.getGeometry().setCoordinates(fromLonLat([fix.lon, fix.lat]));
    else
        f.marker.changed();

    f.last = { cx: sp.cx, cy: sp.cy, rot: sp.rot, scale, opacity, lat: fix.lat, lon: fix.lon, label };
}

function takeHull() {
    const h = hullPool.pop();
    if (h) return h;
    const seed = [[0, 0], [0, 0], [0, 0], [0, 0], [0, 0], [0, 0]];
    const geom = new Polygon([seed]);
    const feature = new OlFeature({ geometry: geom });
    const hull = { feature, geom, flat: geom.getFlatCoordinates(), mmsi: 0 };
    Object.defineProperty(feature, 'replayMmsi', { get: () => Number(hull.mmsi) });
    Object.defineProperty(feature, 'tooltip', {
        get: () => tooltipHTML(hull.mmsi, fleet[hull.mmsi] || {}, features[hull.mmsi]?.fix),
    });
    return hull;
}

function releaseHull(mmsi) {
    const h = hulls[mmsi];
    if (!h) return;
    hullSource.removeFeature(h.feature);
    delete hulls[mmsi];
    if (hullPool.length < HULL_POOL_MAX) hullPool.push(h);
}

// Rotate the precomputed outline into place. No geodesy, no allocation: two
// trig calls and ten multiply-adds into a buffer that already exists.
function placeHull(mmsi, s, fix, res) {
    const local = s.local || (s.local = shipOutlineLocal(s.dim[0], s.dim[1], s.dim[2], s.dim[3]));

    let h = hulls[mmsi];
    if (!h) {
        h = hulls[mmsi] = takeHull();
        h.mmsi = mmsi;
        hullSource.addFeature(h.feature);
    }

    const t = fix.hdg * DEG, si = Math.sin(t), co = Math.cos(t);
    const cosLat = Math.cos(fix.lat * DEG);
    const k = cosLat > 1e-6 ? 1 / cosLat : 1;      // ground metres -> projected units
    const c = fromLonLat([fix.lon, fix.lat]);
    const X = c[0], Y = c[1];
    const flat = h.flat;

    for (let i = 0; i < 5; i++) {
        const f = local[i][0], sd = local[i][1];
        flat[i * 2]     = X + (f * si + sd * co) * k;
        flat[i * 2 + 1] = Y + (f * co - sd * si) * k;
    }
    flat[10] = flat[0];
    flat[11] = flat[1];

    // the array OL already owns was written in place, so it only needs telling
    h.geom.changed();
}

function passes(s, fix) {
    return deps.filterPasses ? deps.filterPasses(s.cls, fix.knots) : true;
}

// Counted in the loop that already visits every vessel; only a changed tally
// reaches the DOM.
let countSig = "";

function draw() {
    if (!active) return;

    const res = deps.getResolution?.();
    const buckets = {};
    let total = 0, shown = 0;

    for (const mmsi in fleet) {
        const f = features[mmsi];
        const fix = sample(fleet[mmsi], instant);
        f.fix = fix;

        let visible = !!fix;
        if (fix) {
            const bucket = deps.bucketFor(fleet[mmsi].cls, fix.knots);
            buckets[bucket] = (buckets[bucket] || 0) + 1;
            total++;
            visible = passes(fleet[mmsi], fix);
        }

        if (!visible) {
            if (f.shown) {
                markerSource.removeFeature(f.marker);
                f.shown = false;
            }
            releaseHull(mmsi);
            continue;
        }
        shown++;

        place(f, fleet[mmsi], fix);
        hull(mmsi, fleet[mmsi], fix, res);

        if (!f.shown) {
            markerSource.addFeature(f.marker);
            f.shown = true;
        }
    }

    const sig = total + ":" + shown + ":" + JSON.stringify(buckets);
    if (sig !== countSig) {
        countSig = sig;
        deps.onCounts?.({ total, shown, buckets });
    }

    deps.onFrame?.();
}

// Hulls are drawn by apparent size, not zoom: the fleet runs from 4 m tenders to
// 365 m tankers, so one zoom threshold would be wrong for most of it. Mercator
// inflates ground metres by 1/cos(lat), which is why latitude is in the test.
function hull(mmsi, s, fix, res) {
    if (!res || !s.dim || fix.hdg == null) return releaseHull(mmsi);

    const px = s.len / (res * Math.cos(fix.lat * DEG));
    const on = hulls[mmsi] ? px >= HULL_OFF_PX : px >= HULL_ON_PX;
    if (!on) return releaseHull(mmsi);

    placeHull(mmsi, s, fix, res);
}
