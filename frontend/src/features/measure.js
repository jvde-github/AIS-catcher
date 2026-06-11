// Distance/bearing measuring tool: shift-click sets a start point/ship, the
// next click the end. Owns the measures list, map layer and measurecard table.

import OlFeature from 'ol/Feature';
import VectorLayer from 'ol/layer/Vector';
import VectorSource from 'ol/source/Vector';
import Point from 'ol/geom/Point';
import LineString from 'ol/geom/LineString';
import Style from 'ol/style/Style';
import Stroke from 'ol/style/Stroke';
import Fill from 'ol/style/Fill';
import Text from 'ol/style/Text';
import { fromLonLat, toLonLat } from 'ol/proj';
import { getLength } from 'ol/sphere';

import { getDistanceVal, getDistanceUnit } from '../core/format.js';
import { calculateBearing } from '../core/geo.js';

// { getShipsDB, showNotification, ensureMeasurecardVisible }
let deps = null;
let measures = [];
let isMeasuring = false;
let measureMode = false;

const measureSource = new VectorSource();

const measureStyle = new Style({
    stroke: new Stroke({
        color: 'green',
        lineDash: [20, 20],
        width: 2,
    })
});

const measureStyleWhite = new Style({
    stroke: new Stroke({
        color: 'white',
        lineDash: [20, 20],
        lineDashOffset: 20,
        width: 2,
    })
});

// label for the measure line
const measureLabelStyle = new Style({
    text: new Text({
        font: '14px Calibri,sans-serif',
        fill: new Fill({
            color: 'rgba(255, 255, 255, 1)',
        }),
        backgroundFill: new Fill({
            color: 'green',
        }),
        padding: [3, 3, 3, 3],
        textBaseline: 'bottom',
        offsetY: -15,
    }),
});

export const measureVector = new VectorLayer({
    source: measureSource,
    style: function (feature) {
        return measureStyleFunction(feature);
    },
});

function measureStyleFunction(feature) {
    const styles = [];
    const geometry = feature.getGeometry();
    const type = geometry.getType();
    let point, label;
    if (type === 'LineString') {
        point = new Point(geometry.getLastCoordinate());
        label = `${feature.measureDistance} ${getDistanceUnit()}, ${feature.measureBearing} degrees`;
    }
    styles.push(measureStyle);
    styles.push(measureStyleWhite);
    if (label) {
        measureLabelStyle.setGeometry(point);
        measureLabelStyle.getText().setText(label);
        styles.push(measureLabelStyle);
    }
    return styles;
}

export function init(d) {
    deps = d;

    document.addEventListener('DOMContentLoaded', () => {
        const mapcardContent = document.getElementById('measurecardInner');

        mapcardContent.addEventListener('click', (event) => {
            const row = event.target.closest('tr');
            if (row) {
                const measureIndex = row.getAttribute('data-index');
                if (event.target.classList.contains('visibility_icon')) {
                    measures[measureIndex].visible = !measures[measureIndex].visible;
                    refreshMeasures();
                } else if (event.target.classList.contains('delete_icon')) {
                    measures.splice(measureIndex, 1);
                    refreshMeasures();
                }
            }
        });

        refreshMeasures();
    });
}

export function refreshMeasures() {
    measureSource.clear();

    const shipsDB = deps.getShipsDB();
    const mapcardContent = document.getElementById('measurecardInner');
    mapcardContent.innerHTML = '';

    let content = '';

    measures = measures.filter(measure => {

        if ((measure.start_type == 'ship' && !(measure.start_value in shipsDB))) {
            deps.showNotification('Ship out of range for measurement.');
            return false;
        }

        let sc = undefined, ss = undefined, from = undefined;
        if (measure.start_type == 'point') {
            sc = fromLonLat(measure.start_value);
            from = "point";
        } else {
            ss = shipsDB[measure.start_value].raw;
            sc = fromLonLat([ss.lon, ss.lat]);
            from = ss.shipname || ss.mmsi;
        }

        let ec = undefined, es = undefined, to = "";
        if ('end_type' in measure) {
            if (measure.end_type == 'point') {
                ec = fromLonLat(measure.end_value);
                to = "point";
            } else {
                es = shipsDB[measure.end_value].raw;
                ec = fromLonLat([es.lon, es.lat]);
                to = es.shipname || es.mmsi;
            }
        }

        let distance = 0, bearing = 0;

        if (sc && ec) {
            const geometry = new LineString([sc, ec]);

            const length = getLength(geometry);
            distance = getDistanceVal(length / 1852);
            const coordinates = geometry.getCoordinates();
            const start = toLonLat(coordinates[0]);
            const end = toLonLat(coordinates[coordinates.length - 1]);
            bearing = calculateBearing(start, end).toFixed(0);

            if (measure.visible) {
                const feature = new OlFeature(geometry);
                measureSource.addFeature(feature);
                feature.measureDistance = distance;
                feature.measureBearing = bearing;
            }
        }
        let icon = measure.visible ? 'visibility' : 'visibility_off';

        content += `<tr data-index="${measures.indexOf(measure)}"><td style="padding: 2px;"><i style="padding-left:2px; font-size: 15px;" class="${icon}_icon visibility_icon"></i></td><td style="padding: 0px;"><i style="font-size: 15px;" class="delete_icon"></i></td><td>${from}</td><td>${to}</td><td title="${distance} ${getDistanceUnit()}">${distance}</td><td title="${bearing} degrees">${bearing}</td></tr>`;

        return true;
    });

    mapcardContent.innerHTML = content;
}

function startMeasurementAtPoint(t, v) {
    isMeasuring = true;
    measures.push({ start_value: v, start_type: t, visible: true });
    deps.ensureMeasurecardVisible();
    deps.showNotification('Select end point or object');
    refreshMeasures();
    startMeasureMode();
}

function endMeasurement(t, v) {
    if (isMeasuring) {

        const lastMeasureIndex = measures.length - 1;
        measures[lastMeasureIndex] = {
            ...measures[lastMeasureIndex],
            end_value: v,
            end_type: t
        };

        isMeasuring = false;

        deps.showNotification('Measurement added.');
        refreshMeasures();
        clearMeasureMode();
    }
}

function startMeasureMode() {
    measureMode = true;
    document.getElementById('map').classList.add('crosshair_cursor');
}

function clearMeasureMode() {
    measureMode = false;
    document.getElementById('map').classList.remove('crosshair_cursor');
}

export function setMeasureMode() {
    measureMode = true;
    document.getElementById('map').classList.add('crosshair_cursor');
}

export function isActive() {
    return measureMode || isMeasuring;
}

// `shipMmsi` is null for a bare map point; `getLonLat` is lazy because the
// pixel-to-coordinate conversion is only needed in the point case.
export function handleMapClick(shipMmsi, getLonLat) {
    measureMode = false;

    if (isMeasuring) {
        if (shipMmsi != null) endMeasurement("ship", shipMmsi);
        else endMeasurement("point", getLonLat());
        return;
    }

    if (shipMmsi != null) startMeasurementAtPoint("ship", shipMmsi);
    else startMeasurementAtPoint("point", getLonLat());
}

export function updateMeasureEnd(shipMmsi, getLonLat) {
    if (!isMeasuring) return;

    const lastMeasureIndex = measures.length - 1;

    if (shipMmsi != null) {
        measures[lastMeasureIndex] = {
            ...measures[lastMeasureIndex],
            end_value: shipMmsi,
            end_type: "ship"
        };
    }
    else {
        measures[lastMeasureIndex] = {
            ...measures[lastMeasureIndex],
            end_value: getLonLat(),
            end_type: "point"
        };
    }

    refreshMeasures();
}
