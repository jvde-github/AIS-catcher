import { test } from "node:test";
import assert from "node:assert/strict";
import * as geo from "../core/geo.js";

test("geo: bearing and validity", () => {
    assert.equal(geo.hasValidCoords(51.9, 4.4), true);
    assert.equal(geo.hasValidCoords(91, 0), false);
    assert.equal(Math.round(geo.calculateBearing([0, 0], [0, 1])), 0);
    assert.equal(Math.round(geo.calculateBearing([0, 0], [1, 0])), 90);
});

test("geo: ship outline is a closed ring of five points", () => {
    const ring = geo.shipOutlineLocal(100, 20, 10, 10);
    assert.equal(ring.length, 5);
});
