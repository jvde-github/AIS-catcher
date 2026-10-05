import { test } from "node:test";
import assert from "node:assert/strict";
import { getShipDimensionSVG } from "@aiscatcher/core/spark.js";
import { create } from "@aiscatcher/core/units.js";

test("dimension SVG: on a very short hull the beam and keel lines stay within the hull", () => {
    // 2 m long, 10 m wide: the beam sets the scale, so the hull is a few pixels long
    const svg = getShipDimensionSVG({ to_bow: 1, to_stern: 1, to_port: 5, to_starboard: 5 }, create());
    const xs = /<polygon points="([^"]+)"/.exec(svg)[1].split(" ").map((p) => Number(p.split(",")[0]));
    const hullL = Math.min(...xs), hullR = Math.max(...xs);
    assert.ok(hullR - hullL < 30, "the hull is shorter than the beam lines' inset");

    const lines = [...svg.matchAll(/<line x1="([^"]+)" y1="[^"]+"\s+x2="([^"]+)" y2="[^"]+"\s+class="(dim-ext|dim-keel)"/g)];
    // the two beam extension lines are the ones that end right of the hull without starting at the reference point
    const beam = lines.filter((m) => m[3] === "dim-ext" && Number(m[2]) > hullR).slice(-2);
    assert.equal(beam.length, 2);
    for (const m of beam) {
        assert.match(m[1], /^\d+\.\d$/);
        assert.ok(Number(m[1]) >= hullL && Number(m[1]) <= hullR, "beam line starts at " + m[1]);
    }
    const keel = lines.find((m) => m[3] === "dim-keel");
    assert.ok(Number(keel[2]) >= hullL && Number(keel[2]) <= hullR, "keel line ends at " + keel[2]);
});
