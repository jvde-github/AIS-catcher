// Station coverage overlays drawn around each station: the polar range ring
// derived from its radar histogram (long timeframe + 1h short ring) and the
// fixed distance circles. All live on one shared vector layer.

import OlFeature from 'ol/Feature';
import VectorLayer from 'ol/layer/Vector';
import VectorSource from 'ol/source/Vector';
import Polygon from 'ol/geom/Polygon';
import Style from 'ol/style/Style';
import Stroke from 'ol/style/Stroke';
import { fromLonLat } from 'ol/proj';

import { settings } from '../../core/state.js';
import { calcOffset1M } from '@aiscatcher/core/geo.js';
import { rangeGeometry } from '@aiscatcher/map/markers.js';
import { getDistanceUnit } from '../../core/units.js';
import { redraw } from '../../map.js';
import { receiver } from '../../data.js';
import { station } from '../../core/store.js';
import { config } from '../../core/config.js';
import { saveSettings } from '../settings/settings.js';
import { showDialog } from '../../dialog.js';
import { register } from '../../actions.js';
import { hover_feature } from '../hover/hover.js';


// the stations on the map, handed in at startup: importing them here would
// close a loop through the settings
let stationsOnMap = () => [];

export function init(stations) {
    if (stations) stationsOnMap = stations;
    register({
        setRangeSwitch: (e, ds, el) => setRangeSwitch(el.checked),
        toggleRange: () => toggleRange(),
    });
}

const rangeVector = new VectorSource({ features: [] });

const rangeStyleFunction = function (feature) {
    const clr = feature.short
        ? (settings.dark_mode ? settings.range_color_dark_short : settings.range_color_short)
        : (settings.dark_mode ? settings.range_color_dark : settings.range_color);

    return new Style({
        stroke: new Stroke({
            color: clr,
            width: feature === hover_feature ? 4 : 2
        })
    });
};

export const rangeLayer = new VectorLayer({
    source: rangeVector,
    style: rangeStyleFunction
});

// ─── where the overlays go ───────────────────────────────────────────────────

// The stations to draw around. A server of several stations names them in its
// picker, each by its number, and puts those that share where they are on the
// map: all of them, or the one in focus. Otherwise the one station the ship
// feed gives, as the legacy viewer has it.
function centres() {
    const named = new Set((config.receivers || []).map((r) => r.idx).filter((idx) => idx >= 0));
    if (named.size) {
        const shown = stationsOnMap().filter((s) => named.has(s.id) && (receiver < 0 || s.id === receiver));
        if (shown.length) return shown;
    }
    return stationHasLocation() ? [{ id: receiver, lat: station.lat, lon: station.lon, label: '' }] : [];
}

// ─── polar range ring ────────────────────────────────────────────────────────

let range_update_time = null;
let range_key = undefined;  // the stations the rings were fetched for
let outlines = [];          // per station: { label, long, short } in map coordinates
let rangeFeatures = [];

export function resetUpdateTime() { range_update_time = null; }

export function stationHasLocation() {
    return station && Object.hasOwn(station, "lat") && Object.hasOwn(station, "lon") &&
        !(station.lat == 0 && station.lon == 0);
}

export function setRangeSwitch(b) {
    if (b != settings.show_range) toggleRange();
}

export function setRangeColor(v, f) {
    settings[f] = v;
    redraw();
}

// another timeframe: the ring is fetched again
export function reload() {
    fetchRange(true).then(() => drawRange());
}

export async function toggleRange() {
    if (!config.features.share_location || !centres().length) {
        showDialog("Error", "Unable to show range as station location not available");
        settings.show_range = false;
    } else settings.show_range = !settings.show_range;

    saveSettings();

    await fetchRange();
    drawRange();
}

const REFRESH_MINUTES = 15;

// nautical miles per km, used to scale the radar bins into degrees
const NM_PER_KM = 0.5399568;

// one station's rings from its history, around where it is
function ringsOf(h, at) {
    const N = h.day.stat[0].radar_a.length;

    // Per bearing bin: the 1h ring is the max over the minute buckets, the long
    // ring extends that with the hour buckets plus N days of daily buckets.
    const range = [];
    const range_short = [];

    const additionalDays = settings.range_timeframe == "7d" ? 7 :
        settings.range_timeframe == "30d" ? 30 : 0;

    const maxOver = (buckets, i, m) => {
        for (let j = 0; j < buckets.length; j++) {
            m = Math.max(m, buckets[j].radar_a[i], buckets[j].radar_b[i]);
        }
        return m;
    };

    for (let i = 0; i < N; i++) {
        const short = maxOver(h.minute.stat, i, 0);
        range_short.push(short);

        let m = maxOver(h.hour.stat, i, short);
        m = maxOver(h.day.stat.slice(0, additionalDays), i, m);
        range.push(m);
    }

    const deltaNorth = calcOffset1M([at.lon, at.lat], 0)[0];
    const deltaEast = calcOffset1M([at.lon, at.lat], 90)[1];

    const radialPoint = (r, k) => [
        at.lon + ((r * deltaEast * 1000) / NM_PER_KM) * Math.sin(((k * 2) / N) * Math.PI),
        at.lat + ((r * deltaNorth * 1000) / NM_PER_KM) * Math.cos(((k * 2) / N) * Math.PI),
    ];

    const outline = [];
    const outlineShort = [];
    for (let i = 0; i < N; i++) {
        outline.push(radialPoint(range[i], i), radialPoint(range[i], i + 1));
        outlineShort.push(radialPoint(range_short[i], i), radialPoint(range_short[i], i + 1));
    }

    return { label: at.label, long: outline.map(point => fromLonLat(point)), short: outlineShort.map(point => fromLonLat(point)) };
}

export async function fetchRange(forcefetch = false) {
    // the stations arrive beside the ships: none yet leaves the setting as it is
    const at = settings.show_range ? centres() : [];
    if (!at.length) {
        outlines = [];
        range_key = undefined;
        range_update_time = undefined;
        return;
    }

    const now = new Date();
    // a station that came, went or moved is fetched for at once
    const key = JSON.stringify(at.map((c) => [c.id, c.lat, c.lon]));

    if (!forcefetch && range_key === key && range_update_time &&
        Math.floor((now - range_update_time) / 1000 / 60) < REFRESH_MINUTES) return;

    range_update_time = now;
    range_key = key;

    // each station from its own figures: its history, as the picker would look through it
    const next = [];
    for (const c of at) {
        try {
            const response = await fetch("api/history_full.json?receiver=" + c.id);
            const h = await response.json();
            if (h && h.day && h.day.stat && h.day.stat.length) next.push(ringsOf(h, c));
        } catch {
            // a station without figures yet draws no ring
        }
    }
    outlines = next;
}

export function drawRange() {
    for (const f of rangeFeatures) rangeVector.removeFeature(f);
    rangeFeatures = [];

    if (!settings.show_range) return;

    for (const o of outlines) {
        const name = o.label ? " · " + o.label : "";
        for (const [ring, short] of [[o.long, false], [o.short, true]]) {
            const f = new OlFeature({ geometry: new Polygon([ring]) });
            f.short = short;
            f.rangering = true;
            f.tooltip = "Station Range " + (short ? "1h" : settings.range_timeframe) + name;
            rangeVector.addFeature(f);
            rangeFeatures.push(f);
        }
    }
}

// ─── distance circles ────────────────────────────────────────────────────────

let distanceFeatures = undefined;
let distanceKey = undefined; // the centres and unit the circles were drawn for

const CIRCLE_RADII = [5000, 10000, 25000, 50000, 100000];

export function removeDistanceCircles() {
    if (distanceFeatures) {
        for (const f of distanceFeatures) rangeVector.removeFeature(f);
    }
    distanceFeatures = undefined;
}

function distanceCircleStyleFunction(feature) {
    return new Style({
        stroke: new Stroke({
            color: settings.distance_circle_color,
            width: feature === hover_feature ? 5 : 1
        })
    });
}

export function updateDistanceCircles() {
    if (!settings.distance_circles) {
        removeDistanceCircles();
        return;
    }

    const at = centres();
    if (!at.length) {
        removeDistanceCircles();
        return;
    }

    // rebuild only when a station moved, came or went, or the unit system changed
    const key = JSON.stringify([at.map((c) => [c.lat, c.lon]), settings.metric]);
    if (distanceFeatures && distanceKey === key) return;

    removeDistanceCircles();
    distanceKey = key;
    distanceFeatures = [];

    const conv = settings.metric === "DEFAULT" ? 1.852 : settings.metric === "SI" ? 1 : 1.609344;

    for (const c of at)
        for (const radius of CIRCLE_RADII) {
            const distanceCircle = new OlFeature({
                geometry: rangeGeometry(c.lat, c.lon, radius * conv)
            });

            distanceCircle.tooltip = radius / 1000 + " " + getDistanceUnit().toUpperCase() + (c.label ? " · " + c.label : "");
            distanceCircle.distancecircle = true;
            distanceCircle.setStyle(distanceCircleStyleFunction);
            rangeVector.addFeature(distanceCircle);
            distanceFeatures.push(distanceCircle);
        }
}
