
import Polygon from 'ol/geom/Polygon';
import { fromLonLat } from 'ol/proj';
import { calcOffset1M, calcMove, shipOutlineLocal } from '../../shared/core/geo.js';

export * from '../../shared/core/geo.js';

export function createShipOutlineGeometry(ship) {
    if (!ship) return null;
    const coordinate = [ship.lon, ship.lat];

    let heading = ship.heading;
    let { to_bow, to_stern, to_port, to_starboard } = ship;

    if (to_bow == null || to_stern == null || to_port == null || to_starboard == null) return null;

    if (heading == null) {
        if (ship.cog == null || !(ship.speed > 1)) return null;
        heading = ship.cog;
    }

    const deltaBow = calcOffset1M(coordinate, heading % 360);
    const deltaStarboard = calcOffset1M(coordinate, (heading + 90) % 360);

    const ring = shipOutlineLocal(to_bow, to_stern, to_port, to_starboard)
        .map(([f, s]) => calcMove(calcMove(coordinate, deltaBow, f), deltaStarboard, s));
    ring.push(ring[0]);

    return new Polygon([ring.map(coord => fromLonLat(coord))]);
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
