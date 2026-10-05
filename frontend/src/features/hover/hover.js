// The pointer on the map: what it is over (a vessel, an aircraft, a map
// object, a place's border), the tooltip and the ring that follow it, the
// hovered vessel's track, and what a click opens. The entry attaches
// handlePointerMove and handleClick to the map.

import { settings } from '../../core/state.js';
import {
    ships as shipsDB, planes as planesDB, clock, planesSince, hoverMmsi as hoverMMSI, hoverType, setHover,
} from '../../core/store.js';
import { getShipName } from '../../core/names.js';
import { getSpeedVal, getSpeedUnit } from '../../core/units.js';
import { getDeltaTimeVal, getCountryName, getShipTypeShort, sanitizeString, getICAO } from '@aiscatcher/core/text.js';
import { debounce, flagHTML } from '@aiscatcher/ui/components.js';
import { validityBand } from '@aiscatcher/ui/binary.js';
import { toLonLat } from 'ol/proj';
import { getWidth } from 'ol/extent';
import { map, markers, syncCircleFeature, shapeLayer, trackLayer } from '../../map.js';
import { shapeFeatures } from '../../fleet.js';
import { closeSettings } from '../settings/settings.js';
import { closeDialog } from '../../dialog.js';
import { showTargetcard, closeTargetcard } from '../targetcard/card.js';
import { showContextMenu } from '../contextmenu/contextmenu.js';
import { showHoverTrack, hoverEnded } from '../../tracks.js';
import * as mapObjects from '../mapobjects/mapobjects.js';
import * as measure from '../measure/measure.js';
import * as range from '../range/range.js';

// { ui: the entry's map UI (chrome) }
let deps = null;

export function init(d) {
    deps = d;
}

const hover_info = document.getElementById('hover-info');
export let hover_feature = undefined;

let hoverCircleFeature = undefined;

let clickTimeout = undefined;
export const handleClick = function (pixel, target, event) {
    const feature = target.closest('.ol-control') ? undefined : map.forEachFeatureAtPixel(pixel,
        function (feature) { if ('ship' in feature || 'plane' in feature || 'link' in feature || 'binary' in feature || 'replayMmsi' in feature) { return feature; } }, { hitTolerance: 10 });

    if (clickTimeout) {
        clearTimeout(clickTimeout);
        clickTimeout = null;
        if (!feature) return;
    }

    let included = feature && 'ship' in feature && feature.ship.mmsi in shipsDB;

    if (event.originalEvent.shiftKey || measure.isActive()) {
        measure.handleMapClick(included ? feature.ship.mmsi : null, () => toLonLat(map.getCoordinateFromPixel(pixel)));
        return;
    }

    if (feature && 'link' in feature && !included) {
        window.open(feature.link, '_blank');
    } else if (feature && feature.station_mmsi && feature.station_mmsi in shipsDB) {
        closeDialog();
        closeSettings();
        showTargetcard('ship', feature.station_mmsi, pixel);
        return;
    } else if (feature && feature.binary === true && !feature.is_associated) {
        closeDialog();
        closeSettings();
        mapObjects.click(feature);
        return;
    } else if (feature && 'replayMmsi' in feature) {
        closeDialog();
        closeSettings();
            showContextMenu(event.originalEvent, feature.replayMmsi, 'ship', ["ctx-replay-ship"]);
        return;
    } else if (feature && 'ship' in feature) {
        closeDialog();
        closeSettings();
        showTargetcard('ship', feature.ship.mmsi, pixel);
    }
    else if (feature && 'plane' in feature) {
        closeDialog();
        closeSettings();
        showTargetcard('plane', feature.plane.hexident, pixel);
    }
    else {
        clickTimeout = setTimeout(function () {
            closeTargetcard();
            clickTimeout = null;
        }, 300);
    }
};

export function getTooltipContent(ship) {
    const sub = (ship.shiptype ? getShipTypeShort(ship.shiptype) + ' - ' : '') +
        'received ' + getDeltaTimeVal(clock - ship.last_signal) + ' ago';

    // the vessel is one band wearing its validity, each message card below it
    // another wearing its kind, their bars in one line
    let content = '<div class="tip-band" style="--band: ' + validityBand(ship.validated) + '"><div class="tooltip-card">' +
        flagHTML(ship.country, 'flag-tooltip', getCountryName(ship.country)) +
        '<div>' +
        (getShipName(ship) || ship.mmsi) +
        '<span class="tooltip-dim"> at </span>' + getSpeedVal(ship.speed) + ' ' + getSpeedUnit() +
        '<div class="tooltip-sub">' + sub + '</div>' +
        '</div>' +
        '</div></div>';

    content += mapObjects.shipTooltip(ship);

    return content;
}

function getTooltipContentPlane(plane) {
    const altitude = plane.airborne == 1 ? (plane.altitude ? Math.round(plane.altitude) + ' ft' : '-') : 'ground';
    const speed = plane.speed ? Math.round(plane.speed) : '-';
    return '<div class="tooltip-card">' +
        flagHTML(plane.country, 'flag-tooltip', getCountryName(plane.country)) +
        '<div>' +
        sanitizeString(plane.callsign || getICAO(plane)) +
        '<span class="tooltip-dim"> at </span>' + altitude + '/' + speed + ' kn' +
        '<div class="tooltip-sub">received ' + getDeltaTimeVal(planesSince - plane.last_signal) + ' ago</div>' +
        '</div>' +
        '</div>';
}

function mapFreeBox() {
    return deps.ui.chrome.freeBox(map.getTargetElement());
}

const showTooltipShip = (tooltip, mmsi, pixel, distance, angle = 0) => {

    tooltip.innerHTML = mmsi;
    tooltip.classList.toggle('tooltip-bands', typeof mmsi === 'string' && mmsi.includes('tip-band'));

    if (pixel) {
        const { offsetWidth: tw, offsetHeight: th } = tooltip;
        const at = deps.ui.chrome.alongCourse(mapFreeBox(), tw, th, pixel, distance, angle);

        Object.assign(tooltip.style, {
            left: `${at.left}px`,
            top: `${at.top}px`,
            visibility: 'visible'
        });
    }
};

export const stopHover = function () {

    if (!hoverMMSI) return;

    debounceShowHoverTrack.cancel();

    hover_info.style.visibility = 'hidden';
    hover_info.style.left = '0px';
    hover_info.style.top = '0px';

    hoverEnded(hoverMMSI);

    const dc = hover_feature && ('distancecircle' in hover_feature || 'rangering' in hover_feature);
    const sf = hoverType == 'ship' && hoverMMSI in shapeFeatures;

    setHover(undefined, undefined);
    hover_feature = undefined;

    if (dc) range.rangeLayer.changed();

    if (sf)
        shapeLayer.changed();

    updateHoverMarker();
    trackLayer.changed();
}

let lastHoverPixel = null;

export const startHover = function (type, mmsi, pixel, feature) {

    if (type != 'ship' && type != 'tooltip' && type != 'plane') return;
    lastHoverPixel = pixel;

    if (mmsi !== hoverMMSI || hoverType !== type) {
        stopHover();

        setHover(mmsi, type);
        hover_feature = feature;

        const shipRaw = type == 'ship' ? shipsDB[mmsi]?.raw : null;
        const planeRaw = type == 'plane' ? planesDB[mmsi]?.raw : null;
        if (shipRaw && shipRaw.lon && shipRaw.lat) {
            showTooltipShip(hover_info, getTooltipContent(shipRaw), pixel, 15, shipRaw.cog);
            if (settings.show_track_on_hover && pixel) {
                debounceShowHoverTrack(mmsi);
            }
            if (mmsi in shapeFeatures) {
                shapeFeatures[mmsi].changed();
            }
            trackLayer.changed();
        } else if (planeRaw && planeRaw.lon && planeRaw.lat) {
            showTooltipShip(hover_info, getTooltipContentPlane(planeRaw), pixel, 15, planeRaw.heading);
        }
        else {
            showTooltipShip(hover_info, hoverMMSI, pixel, 0);

            if (hover_feature && ('distancecircle' in hover_feature || 'rangering' in hover_feature))
                range.rangeLayer.changed();
        }

        updateHoverMarker();
    }
}

// the ring sits on the hovered vessel or plane, or on a hovered map object's marker
function hoverObjectRaw() {
    const g = hoverType == 'tooltip' && hover_feature && hover_feature.getGeometry ? hover_feature.getGeometry() : null;
    if (!g || g.getType() !== 'Point') return null;
    const [lon, lat] = toLonLat(g.getCoordinates());
    return { lon, lat };
}

export function updateHoverMarker() {
    const raw = hoverType == 'ship' ? shipsDB[hoverMMSI]?.raw : hoverType == 'plane' ? planesDB[hoverMMSI]?.raw : hoverObjectRaw();
    const had = hoverCircleFeature != undefined;
    hoverCircleFeature = syncCircleFeature(hoverCircleFeature, raw, hoverMMSI, markers.hoverRing);
    if (had && !hoverCircleFeature) {
        stopHover();
    }
}

const normalizePixel = (coord) => {
    const view = map.getView(),
        projection = view.getProjection(),
        centerX = view.getCenter()[0],
        worldWidth = getWidth(projection.getExtent());

    coord[0] -= Math.floor((coord[0] - centerX) / worldWidth + 0.5) * worldWidth;

    const [x, y] = map.getPixelFromCoordinate(coord),
        [width, height] = map.getSize();

    return [
        Math.max(0, Math.min(width - 1, x)),
        Math.max(0, Math.min(height - 1, y))
    ];
};

export function getFeature(pixel, target) {
    if (target.closest('.ol-control')) return undefined;
    let boundary;
    const coordinate = map.getCoordinateFromPixel(pixel);
    const tolerance = 6 * map.getView().getResolution();
    const feature = map.forEachFeatureAtPixel(pixel, feature => {
        if (feature.placeDefinition) {
            const closest = feature.getGeometry().getClosestPoint(coordinate);
            if (Math.hypot(closest[0] - coordinate[0], closest[1] - coordinate[1]) <= tolerance)
                boundary = boundary || feature;
            return undefined;
        }
        if ('ship' in feature || 'plane' in feature || 'tooltip' in feature || 'binary' in feature)
            return feature;
    }, { hitTolerance: 10 });
    return feature || boundary;
}

export const handlePointerMove = function (pixel, target) {
    const feature = getFeature(pixel, target);
    const mapElement = map.getTargetElement();
    const cursor = feature?.binary && !measure.isActive() && !map.getView().getInteracting() && mapElement.matches(':hover') ? 'pointer' : '';
    if (mapElement.style.cursor !== cursor) mapElement.style.cursor = cursor;

    if (feature) {
        const geometry = feature.getGeometry();
        const geometryType = geometry.getType();
        if (geometryType === 'Point') {
            const coordinate = geometry.getCoordinates();
            pixel = normalizePixel(coordinate);
        }
    }

    if (feature && 'ship' in feature && feature.ship.mmsi in shipsDB) {
        const mmsi = feature.ship.mmsi;
        startHover('ship', mmsi, pixel, feature);
    }
    else if (feature && 'plane' in feature && feature.plane.hexident in planesDB) {
        const hexident = feature.plane.hexident;
        startHover('plane', hexident, pixel, feature);
    }
    else if (feature && feature.station_mmsi && feature.station_mmsi in shipsDB) {
        const ship = shipsDB[feature.station_mmsi].raw;
        startHover('tooltip', mapObjects.stationTooltip(feature, getTooltipContent(ship)), pixel, feature);
    }
    else if (feature && feature.binary === true) {
        if (feature.is_associated && feature.binary_mmsi && feature.binary_mmsi in shipsDB) {
            startHover('ship', feature.binary_mmsi, pixel, feature);
            return;
        } else if (feature.binary_object) {
            startHover('tooltip', mapObjects.markerTooltip(feature), pixel, feature);
        } else {
            startHover('tooltip', "Binary Message", pixel, feature);
        }
    }
    else if (feature && 'tooltip' in feature) {
        startHover('tooltip', feature.tooltip, pixel, feature);
    } else if (hoverMMSI || hoverType) {
        stopHover();
    }

    measure.updateMeasureEnd(
        feature && 'ship' in feature ? feature.ship.mmsi : null,
        () => toLonLat(map.getCoordinateFromPixel(pixel)));
};

const debounceShowHoverTrack = debounce(showHoverTrack, 250);

// ─── for map objects: a vessel named in a list, a tooltip that fills in late ──

export function hoverVessel(mmsi) {
    if (shipsDB?.[mmsi]) startHover('ship', mmsi);
}

export function unhoverVessel(mmsi) {
    if (hoverType === 'ship' && hoverMMSI === mmsi) stopHover();
}

// a marker's tooltip re-renders once its members arrive, if still hovered
export function isHovered(feature) {
    return hover_feature === feature;
}

export function isHoveringShip(mmsi) {
    return hoverType == 'ship' && hoverMMSI == mmsi;
}

export function rehoverShip(mmsi) {
    const raw = shipsDB[mmsi]?.raw;
    if (raw && hoverType == 'ship' && hoverMMSI == mmsi) {
        showTooltipShip(hover_info, getTooltipContent(raw), lastHoverPixel, 15, raw.cog);
    }
}

export function rehover(feature) {
    if (hover_feature === feature && hoverType == 'tooltip') {
        startHover('tooltip', mapObjects.markerTooltip(feature), lastHoverPixel, feature);
    }
}
