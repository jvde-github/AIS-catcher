// Station coverage overlays drawn around the receiver: the polar range ring
// derived from the radar histogram (long timeframe + 1h short ring) and the
// fixed distance circles. Both live on one shared vector layer.

import OlFeature from 'ol/Feature';
import VectorLayer from 'ol/layer/Vector';
import VectorSource from 'ol/source/Vector';
import Polygon from 'ol/geom/Polygon';
import Style from 'ol/style/Style';
import Stroke from 'ol/style/Stroke';
import { fromLonLat } from 'ol/proj';

import { settings } from '../core/state.js';
import { calcOffset1M } from '../../shared/core/geo.js';
import { rangeGeometry } from '../../shared/markers.js';
import { getDistanceUnit } from '../core/units.js';

// { getConfig, getStation, getActiveReceiver, getHoverFeature, showDialog, saveSettings, redrawMap }
let deps = null;

export function init(d) { deps = d; }

const rangeVector = new VectorSource({ features: [] });

const rangeStyleFunction = function (feature) {
    const clr = feature.short
        ? (settings.dark_mode ? settings.range_color_dark_short : settings.range_color_short)
        : (settings.dark_mode ? settings.range_color_dark : settings.range_color);

    return new Style({
        stroke: new Stroke({
            color: clr,
            width: feature === deps.getHoverFeature() ? 4 : 2
        })
    });
};

export const rangeLayer = new VectorLayer({
    source: rangeVector,
    style: rangeStyleFunction
});

// ─── polar range ring ────────────────────────────────────────────────────────

let range_outline = undefined;
let range_outline_short = undefined;
let range_update_time = null;
let rangeFeature = undefined;
let rangeShortFeature = undefined;

export function resetUpdateTime() { range_update_time = null; }

export function stationHasLocation() {
    const station = deps.getStation();
    return station && Object.hasOwn(station, "lat") && Object.hasOwn(station, "lon") &&
        !(station.lat == 0 && station.lon == 0);
}

export function setRangeSwitch(b) {
    if (b != settings.show_range) toggleRange();
}

export function setRangeColor(v, f) {
    settings[f] = v;
    deps.redrawMap();
}

export function setRangeTimePeriod(v) {
    settings.range_timeframe = v;
    deps.saveSettings();
    fetchRange(true).then(() => drawRange());
}

export async function toggleRange() {
    if (!deps.getConfig().features.share_location || !stationHasLocation()) {
        deps.showDialog("Error", "Unable to show range as station location not available");
        settings.show_range = false;
    } else settings.show_range = !settings.show_range;

    deps.saveSettings();

    await fetchRange();
    drawRange();
}

const REFRESH_MINUTES = 15;

// nautical miles per km, used to scale the radar bins into degrees
const NM_PER_KM = 0.5399568;

export async function fetchRange(forcefetch = false) {
    if (!stationHasLocation() || !settings.show_range) {
        settings.show_range = false;
        range_update_time = undefined;
        return;
    }

    const now = new Date();

    if (!forcefetch && range_update_time &&
        Math.floor((now - range_update_time) / 1000 / 60) < REFRESH_MINUTES) return;

    range_update_time = now;

    let h;
    try {
        const response = await fetch("api/history_full.json?receiver=" + deps.getActiveReceiver());
        h = await response.json();
    } catch (error) {
        settings.show_range = false;
        return;
    }

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

    const station = deps.getStation();
    const deltaNorth = calcOffset1M([station.lon, station.lat], 0)[0];
    const deltaEast = calcOffset1M([station.lon, station.lat], 90)[1];

    const radialPoint = (r, k) => [
        station.lon + ((r * deltaEast * 1000) / NM_PER_KM) * Math.sin(((k * 2) / N) * Math.PI),
        station.lat + ((r * deltaNorth * 1000) / NM_PER_KM) * Math.cos(((k * 2) / N) * Math.PI),
    ];

    const outline = [];
    const outlineShort = [];
    for (let i = 0; i < N; i++) {
        outline.push(radialPoint(range[i], i), radialPoint(range[i], i + 1));
        outlineShort.push(radialPoint(range_short[i], i), radialPoint(range_short[i], i + 1));
    }

    range_outline = outline.map(point => fromLonLat(point));
    range_outline_short = outlineShort.map(point => fromLonLat(point));
}

export function drawRange() {
    for (const f of [rangeFeature, rangeShortFeature]) {
        if (f) rangeVector.removeFeature(f);
    }
    rangeFeature = undefined;
    rangeShortFeature = undefined;

    if (!settings.show_range) return;

    if (range_outline) {
        rangeFeature = new OlFeature({ geometry: new Polygon([range_outline]) });
        rangeFeature.short = false;
        rangeFeature.rangering = true;
        rangeFeature.tooltip = "Station Range " + settings.range_timeframe;
        rangeVector.addFeature(rangeFeature);
    }

    if (range_outline_short) {
        rangeShortFeature = new OlFeature({ geometry: new Polygon([range_outline_short]) });
        rangeShortFeature.short = true;
        rangeShortFeature.rangering = true;
        rangeShortFeature.tooltip = "Station Range 1h";
        rangeVector.addFeature(rangeShortFeature);
    }
}

// ─── distance circles ────────────────────────────────────────────────────────

let distanceFeatures = undefined;
let distanceLat = undefined;
let distanceLon = undefined;
let distanceMetric = undefined;

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
            width: feature === deps.getHoverFeature() ? 5 : 1
        })
    });
}

export function updateDistanceCircles() {
    const { lat, lon } = deps.getStation();

    if (!settings.distance_circles) {
        removeDistanceCircles();
        return;
    }

    if (!lat || !lon) return;

    // rebuild only when the station moved or the unit system changed
    if (distanceFeatures && distanceLat == lat && distanceLon == lon &&
        distanceMetric == settings.metric) return;

    removeDistanceCircles();

    distanceLat = lat;
    distanceLon = lon;
    distanceMetric = settings.metric;
    distanceFeatures = [];

    const conv = settings.metric === "DEFAULT" ? 1.852 : settings.metric === "SI" ? 1 : 1.609344;

    for (const radius of CIRCLE_RADII) {
        const distanceCircle = new OlFeature({
            geometry: rangeGeometry(lat, lon, radius * conv)
        });

        distanceCircle.tooltip = radius / 1000 + " " + getDistanceUnit().toUpperCase();
        distanceCircle.distancecircle = true;
        distanceCircle.setStyle(distanceCircleStyleFunction);
        rangeVector.addFeature(distanceCircle);
        distanceFeatures.push(distanceCircle);
    }
}
