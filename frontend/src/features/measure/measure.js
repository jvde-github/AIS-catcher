// Distance/bearing measuring tool: shift-click sets a start point/ship, the
// next click the end. The shared module owns the measures, the layer's
// content and the card rows; this binds it to the viewer's ships and card.

import VectorLayer from 'ol/layer/Vector';
import VectorSource from 'ol/source/Vector';

import { getDistanceVal, getDistanceUnit } from '../../core/units.js';
import * as measureLib from '@aiscatcher/map/measure.js';
import { ships } from '../../core/store.js';
import { register } from '../../actions.js';
import { measurecardVisible, toggleMeasurecard, updateMeasureIndicator } from '../../panels.js';
import { showNotification } from '../../dialog.js';

const measureSource = new VectorSource();
let measure = null;

export const measureVector = new VectorLayer({
    source: measureSource,
    style: (feature) => measure.style(feature),
});

export function init() {
    measure = measureLib.create({
        source: measureSource,
        rows: document.getElementById('measurecardInner'),
        vessel: (mmsi) => {
            const s = ships[mmsi];
            return s ? { lon: s.raw.lon, lat: s.raw.lat, name: s.raw.shipname || s.raw.mmsi } : null;
        },
        notify: showNotification,
        ensureCard: () => { if (!measurecardVisible()) toggleMeasurecard(); },
        onChange: () => updateMeasureIndicator(),
        distance: { value: getDistanceVal, unit: getDistanceUnit },
    });
    measure.refresh();

    register({
        setMeasureMode: () => { setMeasureMode(); showNotification('Shift+click on start point/object'); },
    });
}

export function refreshMeasures() { measure.refresh(); }
export function setMeasureMode() { measure.arm(); }
export function cancel() { measure.cancel(); }
export function isActive() { return measure.isActive(); }
export function count() { return measure.count(); }
export function handleMapClick(shipMmsi, getLonLat) { measure.handleMapClick(shipMmsi, getLonLat); }
export function updateMeasureEnd(shipMmsi, getLonLat) { measure.updateEnd(shipMmsi, getLonLat); }
