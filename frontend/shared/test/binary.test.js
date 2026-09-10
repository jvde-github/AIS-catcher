import { test } from "node:test";
import assert from "node:assert/strict";
import { setupDom } from "./dom.js";

setupDom("");
const binary = await import("../binary.js");

// records as the store's message list carries them
const record = (sender, anchor, ts, text) => binary.decorate({
    key: ts, seq: ts, type: anchor ? 6 : 8, dac: 1, fi: 0, timestamp: ts, first: ts, count: 1, ttl: 2700,
    sender, anchor, message: { mmsi: sender, dac: 1, fi: 0, text },
});

test("binary: a vessel's dialog splits received from sent and switches tabs", () => {
    const list = [record(111, 0, 100, "a"), record(111, 222, 90, "b"), record(333, 111, 80, "c")];
    document.body.innerHTML = binary.getBinaryMessageTabs(list, 111);

    const tabs = [...document.querySelectorAll(".msg-tabs .tab")];
    assert.deepEqual(tabs.map((t) => t.textContent.replace(/\s+/g, " ").trim()), ["Received 1", "Sent 2"]);
    assert.ok(tabs[0].classList.contains("active"));
    const pane = (id) => document.querySelector(`.msg-pane[data-pane="${id}"]`);
    assert.equal(pane("received").querySelectorAll(".meteo-tooltip").length, 1);
    assert.equal(pane("sent").querySelectorAll(".meteo-tooltip").length, 2);
    assert.equal(pane("received").style.display, "");
    assert.equal(pane("sent").style.display, "none");

    tabs[1].click();
    assert.ok(tabs[1].classList.contains("active") && !tabs[0].classList.contains("active"));
    assert.equal(pane("sent").style.display, "");
    assert.equal(pane("received").style.display, "none");
});

test("binary: one direction only still shows both tabs, the full one in front", () => {
    const sentOnly = [record(111, 0, 100, "a"), record(111, 222, 90, "b")];
    document.body.innerHTML = binary.getBinaryMessageTabs(sentOnly, 111);
    const tabs = [...document.querySelectorAll(".msg-tabs .tab")];
    assert.deepEqual(tabs.map((t) => t.textContent.replace(/\s+/g, " ").trim()), ["Received 0", "Sent 2"]);
    assert.ok(tabs[1].classList.contains("active"));
    const pane = (id) => document.querySelector(`.msg-pane[data-pane="${id}"]`);
    assert.equal(pane("sent").style.display, "");
    assert.equal(pane("sent").querySelectorAll(".meteo-tooltip").length, 2);
    assert.equal(pane("received").style.display, "none");
    assert.equal(pane("received").textContent.trim(), "");

    document.body.innerHTML = binary.getBinaryMessageTabs([], 111);
    const none = [...document.querySelectorAll(".msg-tabs .tab")];
    assert.deepEqual(none.map((t) => t.textContent.replace(/\s+/g, " ").trim()), ["Received 0", "Sent 0"]);
    assert.ok(none[0].classList.contains("active"));
    assert.equal(document.querySelector(".msg-pane[data-pane='received']").textContent.trim(), "");
});

test("binary: a message no kind claims is captioned by its id, or by its type when the payload has none", () => {
    const empty = binary.decorate({ key: 5, seq: 5, type: 6, dac: 0, fi: 0, timestamp: 5, first: 5, count: 1, ttl: 2700, sender: 111, anchor: 222, message: { mmsi: 111, dest_mmsi: 222 } });
    const html = binary.getBinaryMessageList([empty], { mmsi: 222 });
    assert.ok(html.includes("Binary Message (addressed), no application data"), html);
    assert.ok(!html.includes("undefined"), html);
    const app = binary.decorate({ key: 6, seq: 6, type: 8, dac: 235, fi: 10, timestamp: 6, first: 6, count: 1, ttl: 2700, sender: 111, anchor: 0, message: { mmsi: 111 } });
    assert.ok(binary.getBinaryMessageList([app], {}).includes("DAC 235, FI 10"));
});
