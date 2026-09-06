import { test } from "node:test";
import assert from "node:assert/strict";
import { create, decimalToDMS, decimalToDDM } from "../core/units.js";

test("units: nautical by default", () => {
    const u = create();
    assert.equal(u.getDistanceVal(10), "10.0");
    assert.equal(u.getDistanceUnit(), "nmi");
    assert.equal(u.getSpeedUnit(), "kts");
    assert.equal(u.getDimUnit(), "m");
});

test("units: SI and imperial conversions", () => {
    const si = create({ system: "SI" });
    assert.equal(si.getDistanceVal(1), "1.9");
    assert.equal(si.getSpeedUnit(), "km/h");
    const imp = create({ system: "IMPERIAL" });
    assert.equal(imp.getDimVal(10), "33");
    assert.equal(imp.getDimUnit(), "ft");
});

test("units: system can be a live callable", () => {
    let sys = "DEFAULT";
    const u = create({ system: () => sys });
    assert.equal(u.getSpeedUnit(), "kts");
    sys = "SI";
    assert.equal(u.getSpeedUnit(), "km/h");
});

test("units: coordinates by style", () => {
    const ship = { lat: 51.5, lon: -0.25, approx: false };
    assert.equal(create().getLatValFormat(ship), "51.50000");
    assert.match(create({ coordinates: "dms" }).getLonValFormat(ship), /^0&deg;15'0\.0"W$/);
    assert.equal(decimalToDDM(51.5, true), "51&deg;30.00'N");
    assert.equal(decimalToDMS(-0.25, false), "0&deg;15'0.0\"W");
});

test("units: approximate positions are marked", () => {
    assert.equal(create().getLatValFormat({ lat: 1, approx: true }), "<i>1.00000</i>");
});

test("units: ship dimension needs all four", () => {
    const u = create();
    assert.equal(u.getShipDimension({ to_bow: 100, to_stern: 20, to_port: 10, to_starboard: 10 }), "120 × 20 m");
    assert.equal(u.getShipDimension({ to_bow: 100 }), null);
});

test("units: change values", () => {
    const u = create();
    assert.equal(u.getChangeVal({ f: 1, to: 75 }), "7.5 m");
    assert.equal(u.getChangeVal({ f: 2, to: 1 }), "At anchor");
    assert.equal(u.getChangeVal({ f: 5, to: "ROTTERDAM" }), "ROTTERDAM");
});
