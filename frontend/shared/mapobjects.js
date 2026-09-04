// Map objects on either host: the rows a transport delivers, the viewer's
// since-feed or the site's tiles, kept by id, aged, drawn, hovered and opened
// through one kind table. Message markers, areas and receiving stations are
// kinds; a station whose vessel is on the map rides that vessel as a badge,
// as do the message badges the ship rows carry. The host supplies transport,
// vessel lookup, hover bookkeeping and what a click opens.

import Feature from 'ol/Feature.js';
import Point from 'ol/geom/Point.js';
import Polygon from 'ol/geom/Polygon.js';
import Fill from 'ol/style/Fill.js';
import Stroke from 'ol/style/Stroke.js';
import Style from 'ol/style/Style.js';
import Icon from 'ol/style/Icon.js';
import CircleStyle from 'ol/style/Circle.js';
import VectorLayer from 'ol/layer/Vector.js';
import VectorSource from 'ol/source/Vector.js';
import { fromLonLat } from 'ol/proj.js';

import { hexToRgb } from './color.js';
import { hasValidCoords } from './core/geo.js';
import * as render from './binary.js';
import { KIND_CAT, CAT_COLORS, AGE_FADE, LAYER_ALPHA, decodeBadge, decorate, tooltipSections, getBinaryMessageList, getBinaryMessageTabs,
    cardOpen, badgeCanvas, discCanvas, pillCanvas, hatchCanvas, areaRings, ageBucket, isDangerArea, isGroupArea, markerHead, markerCaption,
    messageDialog, kindsOf, glyphsHTML, KIND_LABEL } from './binary.js';
import { stationCanvas, stationBadgeCanvas, stationBand, withCount } from './stations.js';

const HOVER_DWELL_MS = 500;

const catOf = (o) => KIND_CAT[o.kind] || 'data';
const statusOf = (o) => (o.online === false ? 'offline' : 'online');
const stationId = (o) => Number(String(o.id).slice(1));
const stationInfo = (o) => ({ name: o.label, id: stationId(o), country: o.country, mmsi: o.mmsi, status: statusOf(o) });
const bandOf = (o) => stationBand(stationInfo(o));

/* host: {
     fetchJSON(url)                       the transport behind hydrate calls
     objectUrl(key), shipMessagesUrl(mmsi), eventsUrl(sinceSeq)
     ship(mmsi) -> {mmsi, lat, lon, name, binary} | null
     shipLabel(mmsi), shipLink(mmsi)      names in cards
     options() -> {display, colorClass, idLabels, groupAreas, hidden(cat), focus: row | null}
     isHovered(feature), rehover(feature), isHoveringShip(mmsi), rehoverShip(mmsi)
     openVessel(mmsi)
     openStation(id)?                    optional: without it a station click flies to it
     pickStation(feature, html)?         optional: offer the bands of stations sharing one marker, true when handled
     map()?                              the OL map, for flying to a place
   } */
export function create(host) {
    render.init({ color: (cat) => colorOf(cat), shipLabel: host.shipLabel, shipLink: host.shipLink });

    const objectsDB = new Map();   // id -> row
    const hydrated = new Map();    // id -> { seq, payload }
    const hydrating = new Map();   // id -> in-flight promise
    const hydratedShips = new Map(); // mmsi -> { badge, messages, promise }
    let serverTime = 0, ttl = 2700, viewZoom = 0;

    const vector = new VectorSource({ features: [] });
    const layer = new VectorLayer({ source: vector, style: styleOf, opacity: LAYER_ALPHA, properties: { interactive: true } });

    /* ---- colours -------------------------------------------------------- */

    // one colour for every message kind unless the host colours by kind; stations keep theirs
    const kindRgb = (cat) => hexToRgb(cat === 'station' ? CAT_COLORS.station : (host.options().colorClass !== false && CAT_COLORS[cat]) || CAT_COLORS.data);
    function colorOf(cat, alpha = 0.9) { const [r, g, b] = kindRgb(cat); return `rgba(${r}, ${g}, ${b}, ${alpha})`; }

    /* ---- rows ------------------------------------------------------------- */

    function applyDelta(data, reset) {
        if (reset) { objectsDB.clear(); hydrated.clear(); }
        if (data.time) serverTime = data.time;
        for (const o of data.objects || []) objectsDB.set(o.id, o);
        for (const id of data.removed || []) { objectsDB.delete(id); hydrated.delete(id); }
        prune();
    }

    // a tile is a snapshot: what it listed before and lists no more is gone, unless
    // a tile at another zoom still lists it
    const tileIds = new Map();   // tile key -> Set(id)
    const listedBy = new Map();  // id -> Set(tile key)
    function applyTile(key, data) {
        if (data.time) serverTime = data.time;
        if (data.ttl) ttl = data.ttl;
        const now = new Set();
        for (const o of data.objects || []) {
            if (!hasValidCoords(o.lat, o.lon)) continue;
            o.seen = serverTime;
            objectsDB.set(o.id, o);
            now.add(o.id);
            if (!listedBy.has(o.id)) listedBy.set(o.id, new Set());
            listedBy.get(o.id).add(key);
        }
        for (const id of tileIds.get(key) || []) {
            if (now.has(id)) continue;
            const tiles = listedBy.get(id);
            if (tiles) tiles.delete(key);
            if (!tiles || !tiles.size) { listedBy.delete(id); objectsDB.delete(id); hydrated.delete(id); }
        }
        tileIds.set(key, now);
    }

    // a row ages from its last message or, for one without (a station never heard
    // from), from the tile fetch that carried it: a tile in view refreshes its rows
    function prune() {
        for (const [id, o] of objectsDB) {
            const t = o.t || o.seen;
            if (t && serverTime - t > (o.ttl || ttl)) { objectsDB.delete(id); hydrated.delete(id); listedBy.delete(id); }
        }
    }

    function clear() { objectsDB.clear(); hydrated.clear(); tileIds.clear(); listedBy.clear(); }

    /* ---- features --------------------------------------------------------- */

    // the host's pointer code tells objects apart by the OL `type` property
    function add(feature, id, props) {
        Object.assign(feature, props);
        feature.set('type', props.is_area ? 'area' : 'mapobject');
        feature.setId(id);
        vector.addFeature(feature);
    }

    const point = (lat, lon) => new Feature({ geometry: new Point(fromLonLat([lon, lat])) });

    function badge(id, ship, props) {
        add(point(ship.lat, ship.lon), id, { binary: true, is_associated: true, binary_mmsi: ship.mmsi, ...props });
    }

    const markerProps = (o) => ({ binary_cat: catOf(o), binary_label: o.label || '', binary_age: ageBucket(o.t, serverTime), binary_status: '' });
    const stationProps = (status) => ({ binary_cat: 'station', binary_label: '', binary_age: 0, binary_status: status });

    // a station whose vessel is on the map rides it as a badge; true when it does
    function placeStationBadge(o) {
        const ship = o.mmsi ? host.ship(o.mmsi) : null;
        if (!ship || !hasValidCoords(ship.lat, ship.lon)) return false;
        badge(`mo-station-${o.id}`, ship, { station_mmsi: o.mmsi, station_info: stationInfo(o), ...stationProps(statusOf(o)) });
        return true;
    }

    // every object as features; the host says which kinds are hidden and whether the
    // view is deep enough for a row. A host showing one object's card wants that
    // object alone, at any zoom; the row it hands over stands in when the tiles do
    // not carry it at this zoom.
    function redraw() {
        vector.clear();
        const opt = host.options();
        if (opt.display === 'off') return;
        const rows = opt.focus ? [objectsDB.get(opt.focus.id) || opt.focus] : objectsDB.values();
        const standing = [];
        for (const o of rows) {
            const cat = catOf(o);
            if (!opt.focus && ((opt.hidden && opt.hidden(cat)) || (o.z != null && o.z > viewZoom))) continue;
            if (cat === 'station') {
                if (!placeStationBadge(o) && hasValidCoords(o.lat, o.lon)) standing.push(o);
                continue;
            }
            // areas are drawn, not hovered: the pointer answers to the glyph at the centre
            if (o.shapes)
                areaRings(o.shapes).forEach((ring, i) =>
                    add(new Feature({ geometry: new Polygon([ring.map((ll) => fromLonLat(ll))]) }), `mo-area-${o.id}-${i}`,
                        { is_area: true, is_danger: isDangerArea(o), is_group: isGroupArea(o), binary_object: o }));
            if (hasValidCoords(o.lat, o.lon)) standing.push(o);
        }
        // points standing on their own: those within an icon of each other share one marker
        for (const stack of stackObjects(standing)) {
            const o = stack[0];
            const props = catOf(o) === 'station'
                ? stationProps(stack.some((m) => catOf(m) === 'station' && statusOf(m) === 'online') ? 'online' : 'offline')
                : markerProps(o);
            add(point(o.lat, o.lon), `mo-${o.id}`, { binary: true, is_associated: false, binary_object: o, ...props, object_stack: stack.length > 1 ? stack : null });
        }
    }

    // two objects closer than an icon at the view's zoom draw on top of each other:
    // they share one marker, anchored on the lowest id so it stays put as members
    // come and go. Anchors sit in cells one reach wide (the reach at the equator,
    // in degrees, which no latitude exceeds), so an object only measures against
    // the nine cells around it.
    const STACK_PX = 14;
    function stackObjects(rows) {
        const stacks = [];
        rows.sort((a, b) => (String(a.id) < String(b.id) ? -1 : String(a.id) > String(b.id) ? 1 : 0));
        const metres = STACK_PX * 156543.03 / Math.pow(2, viewZoom), cell = metres / 111320;
        const cells = new Map();
        for (const o of rows) {
            const reach = metres * Math.cos(o.lat * Math.PI / 180);
            const ci = Math.floor(o.lat / cell), cj = Math.floor(o.lon / cell);
            let home = -1;
            for (let i = ci - 1; i <= ci + 1; i++)
                for (let j = cj - 1; j <= cj + 1; j++)
                    for (const k of cells.get(i + ',' + j) || []) {
                        if (home >= 0 && k > home) continue;
                        const a = stacks[k][0];
                        const dy = (o.lat - a.lat) * 111320, dx = (o.lon - a.lon) * 111320 * Math.cos(a.lat * Math.PI / 180);
                        if (dx * dx + dy * dy <= reach * reach) home = k;
                    }
            if (home >= 0) { stacks[home].push(o); continue; }
            const key = ci + ',' + cj;
            if (!cells.has(key)) cells.set(key, []);
            cells.get(key).push(stacks.length);
            stacks.push([o]);
        }
        return stacks;
    }

    function shipBadge(ship) {
        if (!ship.binary || !hasValidCoords(ship.lat, ship.lon)) return;
        const b = decodeBadge(ship.binary);
        const cat = KIND_CAT[b.kind] || 'data';
        const opt = host.options();
        if (opt.focus || (opt.hidden && opt.hidden(cat))) return;
        badge(`mo-ship-${ship.mmsi}`, ship, { ship, binary_cat: cat, binary_age: b.age });
    }

    // the host's own station on the vessel it rides
    function stationBadge(ship, info) {
        if (host.options().focus) return;
        badge('mo-station-own', ship, { station_mmsi: ship.mmsi, station_info: info, ...stationProps(info.status || 'online') });
    }

    /* ---- styles ----------------------------------------------------------- */

    const styleCache = new Map();
    const areaCache = new Map();
    const iconOf = ({ canvas, size, width, height }) => new Icon({ img: canvas, width: width || size, height: height || size });

    const highlightRing = new Style({
        image: new CircleStyle({ radius: 12, fill: new Fill({ color: 'rgba(255, 255, 255, 0)' }), stroke: new Stroke({ color: 'rgba(255, 255, 255, 0.5)', width: 2 }) }),
        zIndex: 200
    });

    function areaStyle(danger) {
        let cached = areaCache.get(danger);
        if (cached) return cached;
        const c = hexToRgb(danger ? '#e11d48' : '#0857b1');
        const ctx = document.createElement('canvas').getContext('2d');
        cached = [new Style({
            fill: new Fill({ color: ctx.createPattern(hatchCanvas(c), 'repeat') }),
            stroke: new Stroke({ color: `rgba(${c[0]}, ${c[1]}, ${c[2]}, 0.8)`, width: 1.5 }),
            zIndex: 1
        })];
        areaCache.set(danger, cached);
        return cached;
    }

    // a group assignment's rectangle shows while its glyph is under the pointer
    let hoveredArea = null;
    let hoverWatch = null;
    function revealArea(feature, o) {
        if (hoveredArea === o.id) return;
        hoveredArea = o.id;
        vector.changed();
        clearInterval(hoverWatch);
        hoverWatch = setInterval(() => {
            if (host.isHovered(feature)) return;
            clearInterval(hoverWatch);
            hoveredArea = null;
            vector.changed();
        }, 250);
    }

    function styleOf(feature) {
        const opt = host.options();
        if (feature.is_area) {
            if (feature.is_group && !opt.groupAreas && hoveredArea !== feature.binary_object.id) return null;
            return areaStyle(!!feature.is_danger);
        }
        const isBadge = !!feature.is_associated;
        const cat = feature.binary_cat === 'station' || opt.colorClass !== false ? feature.binary_cat : 'data';
        const label = (!isBadge && cat !== 'station' && opt.idLabels && feature.binary_label) || '';
        const age = feature.binary_age || 0, fade = AGE_FADE[age];
        const highlight = isBadge && opt.display === 'highlight';
        const stacked = feature.object_stack ? feature.object_stack.length : 1;
        const key = `${cat}:${isBadge ? 'b' : 'm'}:${highlight}:${label}:${age}:${feature.binary_status || ''}:${stacked}`;
        let cached = styleCache.get(key);
        if (cached) return cached;
        if (isBadge) {
            const canvas = cat === 'station' ? stationBadgeCanvas(feature.binary_status) : badgeCanvas(cat, kindRgb(cat), fade);
            const mark = new Style({ image: iconOf(canvas), zIndex: 201 });
            cached = highlight ? [highlightRing, mark] : [mark];
        } else if (cat === 'station') {
            cached = [new Style({ image: iconOf(withCount(stationCanvas(false, feature.binary_status), stacked)), zIndex: 100 })];
        } else if (label) {
            const icon = iconOf(withCount(pillCanvas(label, colorOf(cat)), stacked));
            icon.setOpacity(fade);
            cached = [new Style({ image: icon, zIndex: 100 })];
        } else {
            cached = [new Style({ image: iconOf(withCount(discCanvas(cat, kindRgb(cat), fade), stacked)), zIndex: 100 })];
        }
        styleCache.set(key, cached);
        return cached;
    }

    function restyle() { styleCache.clear(); vector.changed(); }

    /* ---- hydrate ---------------------------------------------------------- */

    const items = (data) => (data.messages || []).filter((m) => m.message && m.message.mmsi).map(decorate);

    // what stands behind an object key, fetched once per (id, seq)
    function hydrate(o) {
        const have = hydrated.get(o.id);
        if (have && have.seq === o.seq) return Promise.resolve(have.payload);
        if (hydrating.has(o.id)) return hydrating.get(o.id);
        const p = host.fetchJSON(host.objectUrl(o.id))
            .then((data) => { const payload = items(data); hydrated.set(o.id, { seq: o.seq, payload }); return payload; })
            .catch(() => [])
            .finally(() => hydrating.delete(o.id));
        hydrating.set(o.id, p);
        return p;
    }

    // a vessel's messages, fetched once per badge value, shared by hover, title and
    // dialog; the cache keeps the vessels looked at most recently
    const HYDRATED_SHIPS_MAX = 200;
    function hydrateShip(mmsi, badgeWord) {
        const have = hydratedShips.get(mmsi);
        if (have && have.badge === badgeWord) return have.promise;
        const entry = { badge: badgeWord, messages: null, promise: null };
        entry.promise = host.fetchJSON(host.shipMessagesUrl(mmsi)).then(items)
            .then((messages) => { entry.messages = messages; return messages; })
            .catch(() => { if (hydratedShips.get(mmsi) === entry) hydratedShips.delete(mmsi); return []; });
        hydratedShips.delete(mmsi);
        hydratedShips.set(mmsi, entry);
        if (hydratedShips.size > HYDRATED_SHIPS_MAX) hydratedShips.delete(hydratedShips.keys().next().value);
        return entry.promise;
    }

    // the message filter the host's hidden kinds make, built once per use
    const shownBy = () => { const hidden = host.options().hidden; return (m) => !(hidden && hidden(m.cat)); };

    /* ---- clicks in the dialog ------------------------------------------------ */

    // one listener on the document, no inline handlers: a band in the stacked-station
    // chooser, a pin on a message card, a vessel link
    if (typeof document !== 'undefined')
        document.addEventListener('click', (e) => {
            if (!e.target.closest) return;
            const pick = e.target.closest('#binary-messages .station-pick .tip-band[data-station]');
            if (pick) { messageDialog().close(); openStation(Number(pick.dataset.station)); return; }
            const pin = e.target.closest('#binary-messages .msg-pin');
            if (pin) { messageDialog().close(); flyTo(Number(pin.dataset.lat), Number(pin.dataset.lon)); return; }
            if (e.target.closest('#binary-messages a')) messageDialog().close();
        });

    // the list cards can say where a message belongs: its sender's marker when that vessel is on the map
    const listCtx = (extra) => ({ ...extra, locate: (mmsi) => { const s = host.ship(mmsi); return s && hasValidCoords(s.lat, s.lon) ? [s.lat, s.lon] : null; } });

    function openStation(id) {
        if (host.openStation) return host.openStation(id);
        const o = objectsDB.get('s' + id);
        if (o) flyTo(o.lat, o.lon);
    }

    /* ---- going there ------------------------------------------------------- */

    // bring a place into the part of the map the open dialog leaves free, and pulse it there
    function flyTo(lat, lon) {
        const map = host.map && host.map();
        if (!map || !hasValidCoords(lat, lon)) return;
        const view = map.getView(), size = map.getSize();
        const zoom = Math.max(view.getZoom() || 0, 11);
        const res = view.getResolutionForZoom(zoom);
        const c = fromLonLat([lon, lat]);
        const at = freeSpot(map, size);
        view.animate({ center: [c[0] - (at[0] - size[0] / 2) * res, c[1] + (at[1] - size[1] / 2) * res], zoom, duration: 600 });
        setTimeout(() => pulse(c), 600);
    }

    // the pixel to centre on: halfway between the dialog's lower edge and the map's
    function freeSpot(map, size) {
        const r = map.getTargetElement().getBoundingClientRect();
        const dlg = document.getElementById('binary-messages');
        const card = dlg && !dlg.classList.contains('hidden') ? dlg.querySelector('.modal-card') : null;
        const bottom = card ? Math.max(0, card.getBoundingClientRect().bottom - r.top) : 0;
        return [size[0] / 2, (bottom + size[1]) / 2];
    }

    function pulse(coord) {
        const old = vector.getFeatureById('mo-pulse');
        if (old) vector.removeFeature(old);
        const f = new Feature({ geometry: new Point(coord) });
        f.setId('mo-pulse');
        const ring = (k) => new Style({ image: new CircleStyle({ radius: 8 + 20 * k, stroke: new Stroke({ color: `rgba(255, 255, 255, ${0.9 * (1 - k)})`, width: 3 }) }), zIndex: 300 });
        f.setStyle(ring(0));
        vector.addFeature(f);
        const t0 = performance.now(), ms = 1200;
        const frame = (t) => {
            const k = Math.min(1, (t - t0) / ms);
            if (k >= 1 || vector.getFeatureById('mo-pulse') !== f) { if (vector.getFeatureById('mo-pulse') === f) vector.removeFeature(f); return; }
            f.setStyle(ring(k));
            requestAnimationFrame(frame);
        };
        requestAnimationFrame(frame);
    }

    /* ---- hover ------------------------------------------------------------ */

    // a thin rule with dots: more stands behind what the hover shows
    const MORE_ROW = '<div class="tip-more">···</div>';

    // the hover of a marker, or of what shares it: every station as its band, the
    // first message marker as its card, and one row of dots for the messages behind
    // that; the click shows it all
    function tooltip(feature) {
        const members = feature.object_stack || [feature.binary_object];
        const stations = members.filter((o) => catOf(o) === 'station'), markers = members.filter((o) => catOf(o) !== 'station');
        const more = markers.reduce((n, o) => n + (o.count || 1), 0) - 1;
        return stations.map(bandOf).join('') + (markers.length ? markerTip(markers[0], feature) : '') + (more > 0 ? MORE_ROW : '');
    }

    // the head at once, the members after a dwell
    function markerTip(o, feature) {
        if (isGroupArea(o)) revealArea(feature, o);
        const head = markerHead(o, host.shipLabel, colorOf(catOf(o)));
        const have = hydrated.get(o.id);
        if (have && have.seq === o.seq) return head + tooltipSections(have.payload);
        setTimeout(() => { if (host.isHovered(feature)) hydrate(o).then(() => host.rehover(feature)); }, HOVER_DWELL_MS);
        return head + markerCaption(o, serverTime);
    }

    // a ship row carries the number of the station riding it; its record comes by one fetch and stays
    const stationRecords = new Map();
    function ridingStationBand(ship) {
        const id = ship.station;
        if (!id) return '';
        const rec = stationRecords.get(id);
        if (rec) return stationBand({ name: rec.name, id, country: rec.country, mmsi: rec.mmsi, status: rec.online === false ? 'offline' : 'online' });
        if (!stationRecords.has(id)) {
            stationRecords.set(id, null);
            host.fetchJSON(host.objectUrl('s' + id))
                .then((data) => { if (data && data.station) { stationRecords.set(id, data.station); host.rehoverShip(ship.mmsi); } })
                .catch(() => stationRecords.delete(id));
        }
        return stationBand({ name: 'Receiving station', id });
    }

    // the vessel tooltip's message section: the badge word draws a line at once, the items follow after a dwell
    function shipTooltip(ship) {
        if (!ship.binary) return '';
        const b = decodeBadge(ship.binary);
        const cat = KIND_CAT[b.kind] || 'data';
        const shown = shownBy();
        if (!shown({ cat })) return '';
        const line = (note) => cardOpen(cat) +
            `<div style="font-size: 11px; color: #FFA500; padding: 4px 0 3px;">${b.count} ${KIND_LABEL[cat] || cat} message${b.count > 1 ? 's' : ''}${note}</div></div>`;
        const have = hydratedShips.get(ship.mmsi);
        if (have && have.badge === ship.binary && have.messages) {
            const list = have.messages.filter(shown).sort((x, y) => y.timestamp - x.timestamp);
            if (!list.length) return have.messages.length ? '' : line(' <span class="tooltip-dim">(no longer retained)</span>');
            return (tooltipSections([list[0]], true, { mmsi: Number(ship.mmsi) }) || line('')) + (list.length > 1 ? MORE_ROW : '');
        }
        setTimeout(() => { if (host.isHoveringShip(ship.mmsi)) hydrateShip(ship.mmsi, ship.binary).then(() => host.rehoverShip(ship.mmsi)); }, HOVER_DWELL_MS);
        return line('');
    }

    /* ---- card and dialog ------------------------------------------------- */

    // the kinds a vessel's messages are of: the badge's newest at once, the rest once fetched
    function shipKinds(ship) {
        if (!ship || !ship.binary) return Promise.resolve([]);
        const b = decodeBadge(ship.binary);
        const first = [KIND_CAT[b.kind] || 'data'];
        if (b.count < 2) return Promise.resolve(first);
        return hydrateShip(ship.mmsi, ship.binary).then((messages) => kindsOf(messages.filter(shownBy())) || first);
    }

    // the glyphs beside a card title: the message kinds, the first at once and the rest
    // when fetched, and the station the vessel carries; the attrs wire the clicks
    function titleGlyphs(ship, attrs, apply, stationAttrs = '') {
        if (!ship) return;
        const station = ship.station ? glyphsHTML(['station'], stationAttrs) : '';
        if (!ship.binary) { if (station) apply(station); return; }
        apply(glyphsHTML([KIND_CAT[decodeBadge(ship.binary).kind] || 'data'], attrs) + station);
        shipKinds(ship).then((cats) => { if (cats.length) apply(glyphsHTML(cats, attrs) + station); });
    }

    function showVesselMessages(mmsi, badgeWord) {
        const dlg = messageDialog();
        const show = (html) => { dlg.body.innerHTML = html; dlg.open(); };
        const tabs = (list) => show(getBinaryMessageTabs(list, mmsi, listCtx({ mmsi })));
        if (!badgeWord) return tabs([]);
        show('<p class="dim">Loading…</p>');
        hydrateShip(mmsi, badgeWord).then((messages) => tabs(messages.filter(shownBy())));
    }

    // a badge opens its vessel, a lone station opens; the members of a shared marker
    // go in one dialog, the stations as bands to pick, the messages as cards, unless
    // the host offers a station-only pick its own way
    function click(feature) {
        if (feature.station_mmsi) { host.openVessel(feature.station_mmsi); return true; }
        if (feature.is_associated) { if (feature.binary_mmsi) host.openVessel(feature.binary_mmsi); return true; }
        const o = feature.binary_object;
        if (!o) return false;
        const stack = feature.object_stack || [o];
        const stations = stack.filter((m) => catOf(m) === 'station'), markers = stack.filter((m) => catOf(m) !== 'station');
        if (stack.length === 1 && stations.length) { openStation(stationId(o)); return true; }
        const picks = stations.length ? '<div class="station-pick">' + stations.map(bandOf).join('') + '</div>' : '';
        if (!markers.length && host.pickStation && host.pickStation(feature, picks)) return true;
        const dlg = messageDialog();
        dlg.body.innerHTML = picks + (markers.length ? '<p class="dim">Loading…</p>' : '');
        dlg.open();
        if (markers.length)
            Promise.all(markers.map(hydrate)).then((lists) => { dlg.body.innerHTML = picks + getBinaryMessageList([].concat(...lists), listCtx({})); });
        return true;
    }

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
    const DESTINATION = 2;
    function tickerEvent(e, seen) {
        const label = (m) => String(host.shipLabel(m));
        const who = isDistressDevice(e.from) ? (String(e.from).startsWith('972') ? 'MOB device' : String(e.from).startsWith('974') ? 'EPIRB' : 'AIS-SART')
            : isBaseStation(e.from) ? `VTS ${label(e.from).replace(/^MMSI /, '')}` : label(e.from);
        const alert = e.kind !== DESTINATION && e.level >= 1;
        const to = e.to ? label(e.to) : '';
        const body = (e.kind === DESTINATION ? 'bound for ' : '') + e.text + (e.count > 1 ? ` (×${e.count})` : '');
        const text = `${alert ? '⚠ ' : ''}${who}${to ? ` → ${to}` : ''} · ${body}`;
        const html = `<span class="${alert ? 'tk-alert' : 'tk-name'}">${alert ? '&#9888; ' : ''}${escapeHtml(who)}${to ? '<span class="tk-arrow">&rarr;</span>' + escapeHtml(to) : ''}</span>` +
            `<span class="tk-sep">·</span><span class="tk-to">${escapeHtml(body)}</span>`;
        return { key: `ev-${eventsEpoch}-${e.seq}`, at: (e.first || e.t || 0) * 1000, id: e.from, fresh: true, level: e.level || 0, demoted: isSeen(e.from, seen) || isSeen(e.to, seen), text, html };
    }
    const escapeHtml = (v) => String(v).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));

    return {
        vector, layer,
        applyDelta, applyTile, prune, clear, redraw, restyle,
        setViewZoom: (z) => { viewZoom = Math.round(z); },
        shipBadge, stationBadge,
        hydrateShip, tooltip, ridingStationBand, shipTooltip, shipKinds, titleGlyphs, showVesselMessages, click,
        pollEvents, resetEvents, noteSeen,
    };
}
