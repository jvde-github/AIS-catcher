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
