// Geodesic math and geometry builders; no app state.

import Polygon from 'ol/geom/Polygon';
import CircleGeom from 'ol/geom/Circle';
import { fromLonLat } from 'ol/proj';

export const toRadians = function (degrees) {
    return degrees * Math.PI / 180;
};

export const toDegrees = function (radians) {
    return radians * 180 / Math.PI;
};

export const calculateBearing = function (start, end) {
    const startLat = toRadians(start[1]);
    const startLon = toRadians(start[0]);
    const endLat = toRadians(end[1]);
    const endLon = toRadians(end[0]);

    const dLon = endLon - startLon;
    const y = Math.sin(dLon) * Math.cos(endLat);
    const x = Math.cos(startLat) * Math.sin(endLat) - Math.sin(startLat) * Math.cos(endLat) * Math.cos(dLon);

    const bearing = toDegrees(Math.atan2(y, x));
    return (bearing + 360) % 360; // Normalize to 0-360
};

const cos100R = 0.9999999998770914; // cos(100m / R);
const sin100R = 1.567855942823164e-5; // sin(100m / R)
const rad = Math.PI / 180;
const radInv = 180 / Math.PI;

// Per-meter lat/lon delta when moving from `coordinate` along `heading`.
export function calcOffset1M(coordinate, heading) {
    const lat = coordinate[1] * rad;
    const rheading = ((heading + 360) % 360) * rad;
    const sinLat = Math.sin(lat);
    const cosLat = Math.cos(lat);

    let sinLat2 = sinLat * cos100R + cosLat * sin100R * Math.cos(rheading);
    let lat2 = Math.asin(sinLat2);
    let deltaLon = Math.atan2(Math.sin(rheading) * sin100R * cosLat, cos100R - sinLat * sinLat2);

    return [(lat2 * radInv - coordinate[1]) / 100, (deltaLon * radInv) / 100];
}

export function calcMove(coordinate, delta, distance) {
    return [coordinate[0] + delta[1] * distance, coordinate[1] + delta[0] * distance];
}

export function createShipOutlineGeometry(ship) {
    if (!ship) return null;
    const coordinate = [ship.lon, ship.lat];

    let heading = ship.heading;
    let { to_bow, to_stern, to_port, to_starboard } = ship;

    if (to_bow == null || to_stern == null || to_port == null || to_starboard == null) return null;

    if (heading == null) {
        if (ship.cog != null && ship.speed > 1) heading = ship.cog;
        else return new CircleGeom(fromLonLat(coordinate), Math.max(to_bow, to_stern));
    }

    const deltaBow = calcOffset1M(coordinate, heading % 360);
    const deltaStarboard = calcOffset1M(coordinate, (heading + 90) % 360);

    const stern = calcMove(coordinate, deltaBow, -to_stern);

    const A = calcMove(stern, deltaStarboard, to_starboard);
    const B = calcMove(stern, deltaStarboard, -to_port);
    const C = calcMove(B, deltaBow, 0.8 * (to_bow + to_stern));
    const Dmid = calcMove(C, deltaStarboard, 0.5 * (to_starboard + to_port));
    const D = calcMove(Dmid, deltaBow, 0.2 * (to_bow + to_stern));
    const E = calcMove(C, deltaStarboard, to_starboard + to_port);

    let shipOutlineCoords = [A, B, C, D, E, A].map(coord => fromLonLat(coord));
    return new Polygon([shipOutlineCoords]);
}

export function createDistanceGeometry(lat, lon, radius) {
    const deltaNorth = calcOffset1M([lon, lat], 0)[0];
    const deltaEast = calcOffset1M([lon, lat], 90)[1];
    const N = 50;

    let outline = [];
    for (let i = 0; i < N; i++) {
        outline.push([
            lon + ((radius * deltaEast)) * Math.sin(((i * 2) / N) * Math.PI),
            lat + ((radius * deltaNorth)) * Math.cos(((i * 2) / N) * Math.PI),
        ]);
    }

    outline = outline.map(point => fromLonLat(point));
    return new Polygon([outline]);
}
