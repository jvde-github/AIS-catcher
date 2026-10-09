import { test } from "node:test";
import assert from "node:assert/strict";
import * as geo from "@aiscatcher/core/geo.js";

test("geo: bearing and validity", () => {
    assert.equal(geo.hasValidCoords(51.9, 4.4), true);
    assert.equal(geo.hasValidCoords(91, 0), false);
    assert.equal(Math.round(geo.calculateBearing([0, 0], [0, 1])), 0);
    assert.equal(Math.round(geo.calculateBearing([0, 0], [1, 0])), 90);
});

test("geo: ship outline has a square stern, straight sides and a bow curving to a pointed stem", () => {
    const ring = geo.shipOutlineLocal(100, 20, 10, 10);
    assert.deepEqual(ring[0], [-20, 10]);                          // stern, starboard
    assert.deepEqual(ring[1], [-20, -10]);                         // stern, port
    assert.ok(Math.abs(ring[2][0] - 60.4) < 1e-9 && ring[2][1] === -10);          // port shoulder, a third of the length from the bow
    assert.ok(Math.abs(ring[ring.length - 1][0] - 60.4) < 1e-9 && ring[ring.length - 1][1] === 10);
    assert.ok(ring.some(([f, s]) => Math.abs(f - 100) < 1e-9 && Math.abs(s) < 1e-9), "the stem lies on the centreline at the bow");
    assert.ok(ring.every(([f, s]) => f >= -20 && f <= 100 + 1e-9 && Math.abs(s) <= 10), "nothing outside the hull's box");
    // the beam only narrows towards the bow
    const port = ring.slice(2, 13).map(([, s]) => s);
    assert.ok(port.every((s, i) => i === 0 || s >= port[i - 1]), "the port bow curves steadily inward");
});
