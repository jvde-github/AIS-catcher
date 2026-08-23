import { test } from "node:test";
import assert from "node:assert/strict";
import { setupDom } from "./dom.js";

setupDom('<div id="t" class="ticker"><div class="ticker-count"><span class="ticker-count-value">0</span></div><div class="ticker-feed"></div><div class="ticker-buckets"></div></div>');
const ticker = await import("../ticker.js");

test("ticker: slots, chips, counts and push", async (tc) => {
    const picked = [];
    const t = ticker.create({
        mount: document.getElementById("t"),
        buckets: [{ id: "am", label: "A moving", icon: "shipicon", pos: "0 0" }, { id: "bs", label: "B still", icon: "shipicon", pos: "0 0" }],
        bucketHidden: (id) => id === "bs",
        onSelect: (id) => picked.push(id),
    });
    tc.after(() => t.destroy());
    assert.equal(document.querySelectorAll("#t .ticker-item").length, 3);
    assert.equal(document.querySelectorAll("#t .ticker-buckets .stat-item").length, 2);

    t.setEnabled(true);
    t.setCounts({ total: 1234, shown: 12, filtered: true, buckets: { am: 7 } });
    assert.equal(document.querySelector(".ticker-count-value").textContent, "12 / 1.2K");
    assert.equal(document.querySelector('[data-bucket="am"] span:last-child').textContent, "7");
    assert.ok(document.querySelector('[data-bucket="bs"]').classList.contains("stat-off"));

    t.push([{ key: "k1", at: Date.now(), id: 42, text: "SHIP · destination X", html: "<b>SHIP</b>", fresh: false }]);
    await new Promise((r) => setTimeout(r, 1100));          // the fade before a new slide shows
    const shown = [...document.querySelectorAll("#t .ticker-item")].filter((i) => i.style.display !== "none");
    assert.equal(shown.length, 1);
    assert.equal(shown[0].title, "SHIP · destination X");
    shown[0].click();
    assert.deepEqual(picked, ["42"]);
});
