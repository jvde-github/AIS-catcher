import { test } from "node:test";
import assert from "node:assert/strict";
import { setupDom } from "./dom.js";

setupDom('<div id="t" class="ticker"><div class="ticker-count"><span class="ticker-count-value">0</span></div><div class="ticker-feed"></div><div class="ticker-buckets"></div></div>');
const ticker = await import("../ticker.js");

test("ticker: slots, chips, counts and push", async (tc) => {
    const picked = [];
    const selectedEvents = [];
    const t = ticker.create({
        mount: document.getElementById("t"),
        buckets: [{ id: "am", label: "A moving", icon: "shipicon", pos: "0 0" }, { id: "bs", label: "B still", icon: "shipicon", pos: "0 0" }],
        bucketHidden: (id) => id === "bs",
        onSelect: (id, event) => { picked.push(id); selectedEvents.push(event); },
    });
    tc.after(() => t.destroy());
    assert.equal(document.querySelectorAll("#t .ticker-item").length, 1);
    assert.equal(document.querySelectorAll("#t .ticker-buckets .stat-item").length, 2);

    t.setEnabled(true);
    t.setCounts({ total: 1234, shown: 12, filtered: true, buckets: { am: 7 } });
    assert.equal(document.querySelector(".ticker-count-value").textContent, "12 / 1.2K");
    assert.equal(document.querySelector('[data-bucket="am"] span:last-child').textContent, "7");
    assert.ok(document.querySelector('[data-bucket="bs"]').classList.contains("stat-off"));

    t.push([{ key: "k1", at: Date.now(), id: 42, lat: 51.9, lon: 4.5, text: "SHIP · destination X", html: "<b>SHIP</b>", fresh: false }]);
    await new Promise((r) => setTimeout(r, 1100));          // the fade before a new slide shows
    const shown = [...document.querySelectorAll("#t .ticker-item")].filter((i) => i.style.display !== "none");
    assert.equal(shown.length, 1);
    assert.equal(shown[0].title, "SHIP · destination X");
    shown[0].click();
    assert.deepEqual(picked, ["42"]);
    assert.equal(selectedEvents[0].lat, 51.9);
    assert.equal(selectedEvents[0].lon, 4.5);
    await new Promise((r) => setTimeout(r, 1100));
    assert.equal(shown[0].style.display, "none", "the last dismissed event must fade out");
    assert.ok(document.getElementById("t").classList.contains("ticker-idle"));
});

test("ticker: an urgent event is pinned until dismissed", async (tc) => {
    document.querySelector("#t .ticker-feed").innerHTML = "";     // the slots of the test before
    const t = ticker.create({ mount: document.getElementById("t"), buckets: [], onSelect: () => {} });
    tc.after(() => t.destroy());
    t.setEnabled(true);
    t.push([
        { key: "u", at: Date.now(), id: 1, text: "urgent", html: "urgent", level: 2 },
        { key: "r", at: Date.now() + 1, id: 2, text: "routine", html: "routine", level: 0 },
    ]);
    await new Promise((r) => setTimeout(r, 1100));
    const up = () => [...document.querySelectorAll("#t .ticker-item")].filter((i) => i.style.display !== "none").map((i) => i.dataset.key);
    assert.deepEqual(up(), ["u"]);
    t.advance();
    await new Promise((r) => setTimeout(r, 1100));
    assert.deepEqual(up(), ["u"]);
    document.querySelector('#t .ticker-item[data-key="u"]').click();
    await new Promise((r) => setTimeout(r, 1100));
    assert.deepEqual(up(), ["r"]);
});
