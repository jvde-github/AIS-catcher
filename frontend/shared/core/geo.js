// Geodesic math shared by every frontend: no app state, no map library.


export const hasValidCoords = function (lat, lon) {
    return lat != null && lon != null && !(lat == 0 && lon == 0) && lat >= -90 && lat < 90 && lon >= -180 && lon < 180;
};

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

// The hull as five points in ship coordinates: metres forward of the reported
// position (the antenna) and metres to starboard of it. Fixed for the life of
// the vessel, so the animation can precompute it once and only rotate.
export function shipOutlineLocal(to_bow, to_stern, to_port, to_starboard) {
    const L = to_bow + to_stern;
    return [
        [-to_stern, to_starboard],                       // A  stern, starboard quarter
        [-to_stern, -to_port],                           // B  stern, port quarter
        [0.8 * L - to_stern, -to_port],                  // C  shoulder, port
        [to_bow, 0.5 * (to_starboard - to_port)],        // D  bow
        [0.8 * L - to_stern, to_starboard],              // E  shoulder, starboard
    ];
}
