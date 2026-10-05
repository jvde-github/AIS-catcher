// The fleet on the map: each poll of the map tab fetches the vessels, tracks,
// aircraft, map objects and range, and redrawMap builds the markers, hulls,
// labels and track legs from the store; the entry hands redrawMap to map.js
// as its renderer.

import { fromLonLat } from 'ol/proj';
import OlFeature from 'ol/Feature';
import Point from 'ol/geom/Point';
import LineString from 'ol/geom/LineString';
import { settings } from './core/state.js';
import * as filter from './core/filter.js';
import {
    ships as shipsDB, planes as planesDB, paths, station, cardType as card_type, markerTracks as marker_tracks,
} from './core/store.js';
import { hasValidCoords } from '@aiscatcher/core/geo.js';
import { palette as validPalette, bucketColor, speedBucket } from '@aiscatcher/core/palette.js';
import * as markersLib from '@aiscatcher/map/markers.js';
import {
    map, markerVector, planeVector, shapeVector, labelVector, trackVector, planeLayer, labelLayer,
} from './map.js';
import { fetchShips, fetchPlanes, fetchTracks, trackWindowStart, onRefresh } from './data.js';
import { saveSettings } from './features/settings/settings.js';
import { targetcardVisible, updateFocusMarker } from './features/targetcard/card.js';
import * as targetcard from './features/targetcard/targetcard.js';
import * as planecard from './features/planecard/planecard.js';
import { updateMarkerCount } from './features/counts/counts.js';
import { updateTablecard } from './features/tablecard/tablecard.js';
import { drawStation, applyFixedCenter } from './features/follow/follow.js';
import { replaycardVisible } from './panels.js';
import * as replay from './features/replay/replay.js';
import * as mapObjects from './features/mapobjects/mapobjects.js';
import * as measure from './features/measure/measure.js';
import * as range from './features/range/range.js';
import { updateHoverMarker } from './features/hover/hover.js';

// legs whose points never reported a speed, in speed-colored mode
const TRACK_SPEED_UNKNOWN_COLOR = '#9aa0a6';

export let shapeFeatures = {};
let markerFeatures = {};

const getSprite = markersLib.applySprite;
const getPlaneSprite = markersLib.applyPlaneSprite;

async function updateMap() {
    // Opening the replay bar hands the map over: from that point the live
    // layers are on their way out and polling for ships, tracks and planes
    // fetches data the user is no longer looking at. Closing it resumes.
    if (replaycardVisible() || replay.isActive()) return;

    const ok = await fetchShips();
    if (!ok) return;

    await Promise.all([
        fetchTracks(),
        planeLayer.getVisible() ? fetchPlanes() : Promise.resolve(true),
        (mapObjects.objectLayer.isVisible() && mapObjects.objectsAnyShown()) ? mapObjects.fetchObjects() : Promise.resolve(true),
        range.fetchRange(),
    ]);

    if (settings.setcoord == "true" || settings.setcoord == true) {
        if (station != null && Object.hasOwn(station, "lat") && Object.hasOwn(station, "lon")) {
            settings.setcoord = false;
            let view = map.getView();
            view.setCenter(fromLonLat([station.lon, station.lat]));
            saveSettings();
        }
    }

    if (targetcardVisible()) {
        if (card_type == "ship")
            targetcard.populate();
        else if (card_type == "plane")
            planecard.populate();
    }

    updateMarkerCount();
    redrawMap();
}

export function redrawMap() {
    shapeFeatures = {};
    markerFeatures = {};

    markerVector.clear();
    planeVector.clear();
    shapeVector.clear();
    labelVector.clear();
    trackVector.clear();

    labelLayer.declutter_ = settings.labels_declutter;

    const zoom = map.getView().getZoom();
    const showShapeOutlines = zoom > 11.5;
    const includeLabels = (settings.show_labels === "dynamic" && showShapeOutlines) || settings.show_labels === "always";

    for (let [mmsi, entry] of Object.entries(shipsDB)) {
        let ship = entry.raw;
        if (!filter.visible(entry)) continue;
        if (hasValidCoords(ship.lat, ship.lon)) {
            getSprite(ship)

            const lon = ship.lon
            const lat = ship.lat

            const point = new Point(fromLonLat([lon, lat]))
            let feature = new OlFeature({
                geometry: point
            })

            feature.ship = ship;

            markerFeatures[ship.mmsi] = feature
            markerVector.addFeature(feature)

            if (includeLabels)
                labelVector.addFeature(feature)

            const outline = showShapeOutlines ? markersLib.shipOutlineGeometry(ship) : null;
            if (outline) {
                const shapeFeature = new OlFeature({ geometry: outline })
                shapeFeature.ship = ship
                shapeFeatures[ship.mmsi] = shapeFeature

                shapeVector.addFeature(shapeFeature)
            }
        }
    }
    measure.refreshMeasures();

    mapObjects.redrawBinaryMessages();

    if (planeLayer.getVisible()) {

        for (let [hexident, entry] of Object.entries(planesDB)) {
            let plane = entry.raw;
            if (hasValidCoords(plane.lat, plane.lon)) {
                getPlaneSprite(plane, settings.plane_palette)

                const lon = plane.lon
                const lat = plane.lat

                const point = new Point(fromLonLat([lon, lat]))
                const feature = new OlFeature({
                    geometry: point
                })

                feature.plane = plane;

                markerFeatures[plane.hexident] = feature
                planeVector.addFeature(feature)

                if (includeLabels)
                    labelVector.addFeature(feature)
            }
        }
    }

    const cutoff = trackWindowStart();
    const speedMode = settings.track_color_mode === "speed";
    const speedPalette = validPalette(settings.track_speed_palette);
    const speedTop = Number(settings.track_speed_max) || 20;

    // A leg is drawn at the mean of the speeds its two points report, in the
    // tenths of a knot the path carries; a leg with no speed at either end
    // gets a neutral color rather than the bottom of the ramp, which would
    // read as "stopped".
    const legBucket = (newer, older) => {
        if (!speedMode) return 0;
        const a = newer[4], b = older[4];
        const sog = a != null && b != null ? (a + b) / 2 : (a != null ? a : b);
        return sog == null ? -1 : speedBucket(sog / 10, speedTop);
    };

    for (let [mmsi, entry] of Object.entries(paths)) {

        if (shipsDB[mmsi] && !filter.visible(shipsDB[mmsi])) continue;

        if (marker_tracks.has(Number(mmsi)) || settings.show_all_tracks) {
            const path = paths[mmsi];
            const ship = shipsDB[mmsi]?.raw;
            const shipclass = ship?.shipclass;

            // Path: [lat, lon, start_time, end_time, sog, cog, hdg]
            if (path.length > 0 && path[0].length >= 4) {
                const emitSegment = (coords, dashed, bucket) => {
                    if (coords.length < 2) return;
                    const feature = new OlFeature(new LineString(coords));
                    feature.mmsi = mmsi;
                    feature.isDashed = dashed;
                    feature.shipclass = shipclass;
                    if (speedMode)
                        feature.speedColor = bucket < 0 ? TRACK_SPEED_UNKNOWN_COLOR : bucketColor(speedPalette, bucket);
                    trackVector.addFeature(feature);
                };

                let currentSegment = [];
                let currentDashed = false;
                let currentBucket = 0;

                for (let i = 0; i < path.length; i++) {
                    const point = path[i];
                    if (cutoff && point[3] < cutoff)
                        break;
                    const coord = fromLonLat([point[1], point[0]]);

                    let isDashed = false;
                    let bucket = 0;
                    if (i > 0) {
                        const timeBetweenPoints = path[i - 1][2] - point[3]; // newer start_time - older end_time
                        isDashed = timeBetweenPoints > settings.track_trash_threshold;
                        bucket = legBucket(path[i - 1], point);
                    }

                    if (currentSegment.length === 0) {
                        // First point
                        currentSegment.push(coord);
                        currentDashed = false; // First segment is always solid
                    } else if (currentSegment.length === 1 || (currentDashed === isDashed && currentBucket === bucket)) {
                        // Continue current segment
                        currentSegment.push(coord);
                        currentDashed = isDashed;
                        currentBucket = bucket;
                    } else {
                        emitSegment(currentSegment, currentDashed, currentBucket);
                        // Start new segment
                        currentSegment = [currentSegment[currentSegment.length - 1], coord];
                        currentDashed = isDashed;
                        currentBucket = bucket;
                    }
                }

                emitSegment(currentSegment, currentDashed, currentBucket);
            }
        }
    }

    range.drawRange();
    updateFocusMarker();
    updateHoverMarker();

    updateMarkerCount();
    updateTablecard();

    drawStation();
    applyFixedCenter();
    range.updateDistanceCircles();

}

onRefresh("map", updateMap);
