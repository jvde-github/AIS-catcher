// Following: the map kept centred on a vessel or on the station, the
// station's own marker, and where a vessel is - in the replayed frame while
// replay runs, else live.

import OlFeature from 'ol/Feature';
import Point from 'ol/geom/Point';
import Style from 'ol/style/Style';
import Icon from 'ol/style/Icon';
import { fromLonLat } from 'ol/proj';
import { settings } from '../../core/state.js';
import { config } from '../../core/config.js';
import { ships as shipsDB, station, counts as shipCounts, cardMmsi } from '../../core/store.js';
import { getShipName } from '../../core/names.js';
import * as stations from '@aiscatcher/ui/stations.js';
import { map } from '../../map.js';
import { refreshIntervalMs } from '../../data.js';
import { saveSettings } from '../settings/settings.js';
import { showNotification } from '../../dialog.js';
import { register } from '../../actions.js';
import { hasValidCoords } from '@aiscatcher/core/geo.js';
import { fetchShips } from '../../data.js';
import { selectMapTab } from '../../tabs/index.js';
import * as replay from '../replay/replay.js';
import * as mapObjects from '../mapobjects/mapobjects.js';
import * as targetcard from '../targetcard/targetcard.js';
import { targetcardMinIfMaxonMobile } from '../targetcard/card.js';

// { ui: the entry's map UI (reveal) }
let deps = null;

export function init(d) {
    deps = d;
}

// vessel `m` on the map tab, its card open, the map zoomed in on it
export async function openFocus(m, z) {
    await fetchShips(false);

    selectMapTab(m);

    let ship = shipsDB[m] && shipsDB[m].raw;
    if (ship && hasValidCoords(ship.lat, ship.lon)) {
        let shipCoords = fromLonLat([ship.lon, ship.lat]);
        let view = map.getView();
        view.setCenter(shipCoords);
    }

    if (z) mapResetView(z);
    else mapResetView(14);

    if (ship && hasValidCoords(ship.lat, ship.lon)) deps.ui.reveal([ship.lon, ship.lat], undefined, { center: true });
}

function mapResetView(z) {

    let view = map.getView();
    view.setZoom(Math.min(view.getMaxZoom(), Math.max(z, view.getZoom() + 1)));
    targetcardMinIfMaxonMobile();
}

export function vesselPosition(m) {
    if (m == null) return null;

    if (replay.isActive()) {
        const fix = replay.fixFor(m);
        if (fix) return fix;
    }
    if (m in shipsDB) {
        const ship = shipsDB[m].raw;
        if (ship.lat != null && ship.lon != null) return { lat: ship.lat, lon: ship.lon };
    }
    return null;
}

export function mapResetViewZoom(z, m) {
    const pos = vesselPosition(m);
    if (pos) {
        let view = map.getView();
        view.setCenter(fromLonLat([pos.lon, pos.lat]));
        view.setZoom(Math.min(view.getMaxZoom(), Math.max(z, view.getZoom() + 1)));
    }

    targetcardMinIfMaxonMobile();
}

export function vesselLabel(m) {
    if (m == null) return "";
    if (String(m).toUpperCase() == "STATION") return "the station";

    const ship = m in shipsDB ? shipsDB[m].raw : null;
    return (ship && getShipName(ship)) || String(m);
}

export function pinVessel(m) {
    settings.center_point = m;
    settings.fix_center = true;
    saveSettings();
    drawStation();
    applyFixedCenter();
    targetcard.updateFollowOption();
    showNotification("Following " + vesselLabel(m));
}

export function unpinCenter() {
    const was = settings.center_point;
    settings.fix_center = false;
    saveSettings();
    drawStation();
    targetcard.updateFollowOption();
    showNotification("No longer following " + vesselLabel(was));
}

export function isFollowing(m) {
    return settings.fix_center && m != null &&
        String(settings.center_point).toUpperCase() === String(m).toUpperCase();
}

export function toggleFollow(m) {
    if (m == null) return;

    if (isFollowing(m)) unpinCenter();
    else pinVessel(m);
}

let stationDrawn = '';
let stationFeature = undefined;

export function drawStation() {
    const onVessel = !!mapObjects.ownVessel();
    const hidden = settings.show_station == false || stationCoords() == null || onVessel;
    const key = hidden ? '' : `${station.lat},${station.lon},${station.gps}`;
    if (key === stationDrawn) return;
    stationDrawn = key;

    if (stationFeature) {
        mapObjects.setReceiverMarker(null);
        stationFeature = undefined;
    }
    if (hidden) return;

    const { canvas, size } = stations.stationCanvas(station.gps);
    stationFeature = new OlFeature({ geometry: new Point(fromLonLat([station.lon, station.lat])) });
    stationFeature.setStyle(new Style({ image: new Icon({ img: canvas, width: size, height: size }) }));
    stationFeature.tooltip = stations.stationBand({ name: config.station, gps: station.gps, mmsi: station.mmsi });
    stationFeature.station = true;
    mapObjects.setReceiverMarker(stationFeature);
}

function followTargetIsStation() {
    return String(settings.center_point).toUpperCase() == "STATION";
}

function stationCoords() {
    return station != null && Object.hasOwn(station, "lat") && Object.hasOwn(station, "lon") ? station : null;
}

let followAnimating = false;

function panTo(lon, lat, smooth) {
    const view = map.getView();

    if (view.getInteracting()) return;
    if (view.getAnimating() && !followAnimating) return;

    const coord = fromLonLat([lon, lat]);
    const from = view.getCenter();
    const size = map.getSize();

    if (from != null) {
        const px = Math.hypot(coord[0] - from[0], coord[1] - from[1]) / view.getResolution();
        if (px < 0.5) return;
        // a jump too big to read as motion is not worth easing
        smooth = smooth && size != null && px <= 2 * Math.max(size[0], size[1]);
    }

    if (!smooth) {
        if (followAnimating) view.cancelAnimations();
        view.setCenter(coord);
        return;
    }

    view.animate({ center: coord, duration: refreshIntervalMs }, () => { followAnimating = false; });
    followAnimating = true;
}

export function applyFixedCenter() {
    if (!settings.fix_center) return;

    const replaying = replay.isActive();

    let target = null;
    if (followTargetIsStation()) {
        target = stationCoords();
    } else if (replaying || settings.center_point in shipsDB) {
        // prefers the frame being drawn over where the vessel is right now
        target = vesselPosition(settings.center_point);
    } else if (shipCounts.total > 0) {
        // the followed vessel aged out or left range; replay owns its own
        // fleet, so a gap in the live set says nothing while it is running
        const dropped = settings.center_point;
        settings.center_point = "STATION";
        settings.fix_center = false;
        targetcard.updateFollowOption();
        showNotification("No longer following " + vesselLabel(dropped) + ": out of range");
        return;
    }

    if (target == null) return;

    panTo(target.lon, target.lat, !replaying);

    settings.lat = target.lat;
    settings.lon = target.lon;
}

register({
    toggleFollowStationCtx: () => toggleFollow("STATION"),
    unpinCenter: () => unpinCenter(),
    toggleFollowCard: () => toggleFollow(cardMmsi),
});
