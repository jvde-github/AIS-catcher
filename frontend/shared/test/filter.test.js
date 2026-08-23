import { test } from "node:test";
import assert from "node:assert/strict";
import * as filter from "../core/filter.js";
import { ShippingClass } from "../core/constants.js";

test("filter: buckets by class and speed", () => {
    assert.equal(filter.bucketFor(ShippingClass.B, 5), "bm");
    assert.equal(filter.bucketFor(ShippingClass.B, 0), "bs");
    assert.equal(filter.bucketFor(ShippingClass.ATON, null), "aton");
    assert.equal(filter.bucketFor(ShippingClass.HELICOPTER, 100), "air");
});

test("filter: inactive filter passes everything", () => {
    const f = filter.create();
    assert.equal(f.isActive(), false);
    assert.equal(f.shipPasses({ shipclass: ShippingClass.B, speed: 0 }), true);
});

test("filter: state lives in the host object", () => {
    const state = {};
    const f = filter.create({ state: () => state });
    f.toggle("bucket", "bs");
    assert.deepEqual(state.hidden_buckets, ["bs"]);
    assert.equal(f.isHidden("bucket", "bs"), true);
    assert.equal(f.shipPasses({ shipclass: ShippingClass.B, speed: 0 }), false);
    assert.equal(f.shipPasses({ shipclass: ShippingClass.B, speed: 3 }), true);
    f.reset();
    assert.equal(f.isActive(), false);
    assert.deepEqual(state.hidden_buckets, []);
});

test("filter: two instances do not share state", () => {
    const sa = {}, sb = {};
    const a = filter.create({ state: () => sa }), b = filter.create({ state: () => sb });
    a.setAll("class", false);
    assert.equal(a.isActive(), true);
    assert.equal(b.isActive(), false);
});

test("filter: seen uses the host clock", () => {
    const state = {};
    let now = 1000;
    const f = filter.create({ state: () => state, now: () => now });
    f.set("seen", 1);
    assert.equal(f.shipPasses({ last_signal: 970 }), true);
    now = 2000;
    assert.equal(f.shipPasses({ last_signal: 970 }), false);
});

test("filter: describe", () => {
    const state = {};
    const f = filter.create({ state: () => state });
    f.set("speed_min", 5);
    f.toggle("status", 1);
    assert.deepEqual(f.describe(), ["1 status hidden", "speed 5-∞ kn"]);
});
