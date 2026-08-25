// Distance/bearing measuring tool: shift-click sets a start point/ship, the
// next click the end. The shared module owns the measures, the layer's
// content and the card rows; this binds it to the viewer's ships and card.

import VectorLayer from 'ol/layer/Vector';
import VectorSource from 'ol/source/Vector';

import { getDistanceVal, getDistanceUnit } from '../core/units.js';
import * as measureLib from '../../shared/measure.js';

const measureSource = new VectorSource();
let measure = null;

export const measureVector = new VectorLayer({
    source: measureSource,
    style: (feature) => measure.style(feature),
});

// { getShipsDB, showNotification, ensureMeasurecardVisible, onMeasuresChanged }
export function init(d) {
    measure = measureLib.create({
        source: measureSource,
        rows: document.getElementById('measurecardInner'),
        vessel: (mmsi) => {
            const s = d.getShipsDB()[mmsi];
            return s ? { lon: s.raw.lon, lat: s.raw.lat, name: s.raw.shipname || s.raw.mmsi } : null;
        },
        notify: d.showNotification,
        ensureCard: d.ensureMeasurecardVisible,
        onChange: d.onMeasuresChanged,
        distance: { value: getDistanceVal, unit: getDistanceUnit },
    });
    measure.refresh();
}

export function refreshMeasures() { measure.refresh(); }
export function setMeasureMode() { measure.arm(); }
export function cancel() { measure.cancel(); }
export function isActive() { return measure.isActive(); }
export function count() { return measure.count(); }
export function handleMapClick(shipMmsi, getLonLat) { measure.handleMapClick(shipMmsi, getLonLat); }
export function updateMeasureEnd(shipMmsi, getLonLat) { measure.updateEnd(shipMmsi, getLonLat); }
