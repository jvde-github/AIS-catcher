import { test } from "node:test";
import assert from "node:assert/strict";
import * as text from "../core/text.js";

test("text: status strings clamp to undefined", () => {
    assert.equal(text.getStatusVal({ status: 0 }), "Under way using engine");
    assert.equal(text.getStatusVal({ status: 99 }), "Undefined");
    assert.equal(text.getStatusVal({}), "Undefined");
});

test("text: compact counts", () => {
    assert.equal(text.compactCount(999), "999");
    assert.equal(text.compactCount(1234), "1.2K");
    assert.equal(text.compactCount(34000), "34K");
    assert.equal(text.compactCount(2500000), "2.5M");
});

test("text: ETA handles not-available markers", () => {
    assert.equal(text.getEtaVal({ eta_month: 3, eta_day: 4, eta_hour: 7, eta_minute: 5 }), "4 Mar 07:05Z");
    assert.equal(text.getEtaVal({ eta_month: 0, eta_day: 0, eta_hour: 24, eta_minute: 60 }), "-");
});

test("text: delta time", () => {
    assert.equal(text.getDeltaTimeVal(0), "0s");
    assert.equal(text.getDeltaTimeVal(61), "1m 1s");
    assert.equal(text.getDeltaTimeVal(90061), "1d 1h 1m 1s");
    assert.equal(text.getDeltaTimeVal(-5), "0s");
});

test("text: ship types", () => {
    assert.equal(text.getShipTypeFull(70), text.getShipTypeFull(70));
    assert.match(text.getShipTypeFull(9999), /^Unknown/);
});

test("text: sanitize and url guard", () => {
    assert.equal(text.sanitizeString('<b a="1">'), "&lt;b a=&quot;1&quot;&gt;");
    assert.equal(text.isHttpUrl("https://x"), true);
    assert.equal(text.isHttpUrl("javascript:alert(1)"), false);
});

test("text: ICAO hex", () => {
    assert.equal(text.getICAOFromHexIdent(0xabc), "000ABC");
});
