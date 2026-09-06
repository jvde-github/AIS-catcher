import { test } from "node:test";
import assert from "node:assert/strict";
import { setupDom } from "./dom.js";

setupDom(`<div class="settings-window" id="s"><div class="settings-header"><div class="hdr-slot"></div></div>
<div class="settings-main">
  <header>Map</header><section><div><span>Labels</span><input type="checkbox" data-setting="labels"></div></section>
  <header>Tracks</header><section><div><span>Weight</span><input type="range" data-setting="weight"></div></section>
  <header>Other</header><section><div><span>Name</span><select data-setting="name"><option value="a">a</option><option value="b">b</option></select></div></section>
</div></div>`);
const panel = await import("../panel.js");

test("panel: builds groups and pages, tags classes", () => {
    const p = panel.create({ window: document.getElementById("s"), groups: [{ title: "Map", subs: [["Map", "Map"], ["Tracks", "Tracks"]] }] });
    p.build();
    const tabs = [...document.querySelectorAll(".settings-tabs:not(.settings-subtabs) .settings-tab")].map((t) => t.textContent);
    assert.deepEqual(tabs, ["Map", "Other"]);
    assert.equal(document.querySelectorAll(".settings-subtabs .settings-tab").length, 2);
    const sections = [...document.querySelectorAll(".settings-main section")];
    assert.deepEqual(sections.map((x) => x.style.display), ["", "none", "none"]);
    assert.ok(sections[0].classList.contains("fieldset"));
    assert.ok(document.querySelector("select").classList.contains("select"));
    p.selectGroup(1);
    assert.deepEqual(sections.map((x) => x.style.display), ["none", "none", ""]);
});

test("panel: bind keeps controls and store in step with type coercion", () => {
    const state = { labels: true, weight: 3, name: "b" };
    const store = { get: (k) => state[k], set: (k, v) => { state[k] = v; } };
    const changes = [];
    const p = panel.create({ window: document.getElementById("s"), groups: [] });
    p.bind(store, { onChange: (k, v) => changes.push([k, v]) });
    const cb = document.querySelector('[data-setting="labels"]');
    assert.equal(cb.checked, true);
    assert.equal(document.querySelector('[data-setting="name"]').value, "b");

    cb.checked = false;
    cb.dispatchEvent(new Event("change", { bubbles: true }));
    const r = document.querySelector('[data-setting="weight"]');
    r.value = "5";
    r.dispatchEvent(new Event("change", { bubbles: true }));
    assert.deepEqual(state, { labels: false, weight: 5, name: "b" });
    assert.deepEqual(changes, [["labels", false], ["weight", 5]]);
});
