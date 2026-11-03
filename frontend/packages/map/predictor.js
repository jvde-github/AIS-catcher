// AIS sends the rate of turn as 4.733 times the square root of the degrees a minute, signed,
// starboard positive; 127 or -127 is "faster than 10 a minute" from a transponder without a
// rate sensor, -128 or nothing is not known
const KN = 1852 / 3600;
const DEG = Math.PI / 180;

export function turnRate(turn) {
    if (turn == null || turn <= -127 || turn >= 127) return null;
    const r = (turn / 4.733) * (turn / 4.733);
    return turn < 0 ? -r : r;
}

export function turnSide(turn) {
    if (turn === 127) return "starboard";
    if (turn === -127) return "port";
    const rate = turnRate(turn);
    return rate >= 1 ? "starboard" : rate <= -1 ? "port" : null;
}

// an arc stops at half a turn: past that a rate of turn foretells nothing, and a wrong one
// would draw a spiral
export function predictorPath(lat, lon, speedKn, cogDeg, turn, minutes) {
    const v = speedKn * KN, total = minutes * 60;
    const rate = turnRate(turn);
    const hard = turn === 127 ? "starboard" : turn === -127 ? "port" : null;
    const omega = rate ? rate * DEG / 60 : 0;
    const mLat = 111320, mLon = Math.max(111320 * Math.cos(lat * DEG), 1);
    const at = (x, y) => [lon + x / mLon, lat + y / mLat];
    const degrees = th => ((th / DEG) % 360 + 360) % 360;
    const coords = [[lon, lat]], marks = [];
    let x = 0, y = 0, th = cogDeg * DEG;
    if (omega) {
        const step = 10, from = th;
        for (let t = step; t <= total && Math.abs(th - from) < Math.PI; t += step) {
            const mid = th + omega * step / 2;
            x += v * step * Math.sin(mid);
            y += v * step * Math.cos(mid);
            th += omega * step;
            coords.push(at(x, y));
            if (t % 60 === 0 && t < total) marks.push({ at: at(x, y), bearing: degrees(th) });
        }
    } else {
        for (let t = 60; t < total; t += 60) marks.push({ at: at(v * t * Math.sin(th), v * t * Math.cos(th)), bearing: cogDeg });
        x = v * total * Math.sin(th);
        y = v * total * Math.cos(th);
        coords.push(at(x, y));
    }
    return { coords, end: coords.length - 1, bearing: degrees(th), hard, marks };
}
