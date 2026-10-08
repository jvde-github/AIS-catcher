import { test } from "node:test";
import assert from "node:assert/strict";
import * as markersLib from "@aiscatcher/map/markers.js";

test("a solid track is drawn over a dark line a little wider than it, a dashed stretch on its own", () => {
    const settings = { track_weight: 1, track_opacity: 1, track_class_colors: { 3: "#2fb457" } };
    const markers = markersLib.create({ settings: () => settings });
    const solid = markers.track({ mmsi: 1, shipclass: 3 });
    assert.equal(solid.length, 2);
    assert.equal(solid[0].getStroke().getWidth(), solid[1].getStroke().getWidth() + 2);
    assert.match(solid[0].getStroke().getColor(), /^rgba\(0, 0, 0, /);
    assert.equal(solid[1].getStroke().getColor(), "#2fb457");
    assert.ok(!Array.isArray(markers.track({ mmsi: 1, shipclass: 3, isDashed: true })));
});
