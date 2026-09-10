import { test } from "node:test";
import assert from "node:assert/strict";
import { setupDom } from "./dom.js";
import { stationBand } from "../stations.js";

setupDom();

test("station band: entities in a configured name are decoded once, markup is still escaped", () => {
    const band = stationBand({ name: "KX1T&nbsp;Trenton&nbsp;East-Boston", id: 1225, country: "us", status: "online" });
    assert.ok(band.includes("KX1T\u00a0Trenton\u00a0East-Boston"), band);
    assert.ok(!band.includes("&nbsp;"), band);
    const hostile = stationBand({ name: "<img src=x onerror=alert(1)>", id: 7 });
    assert.ok(!hostile.includes("<img"), hostile);
});

test("station band: no name falls back to the generic label", () => {
    assert.ok(stationBand({ id: 3 }).includes("Receiving station"));
});
