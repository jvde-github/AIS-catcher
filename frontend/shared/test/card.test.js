import { test } from "node:test";
import assert from "node:assert/strict";
import { setupDom } from "./dom.js";

setupDom('<aside id="c" class="mapcard card-max"><header class="mapcard-header"></header><div class="mapcard-content"></div><footer class="mapcard-footer"></footer></aside>');
const cardLib = await import("../card.js");

test("card: rows build sections, toggle, events", () => {
    const c = cardLib.create({ mount: document.getElementById("c") });
    const cells = c.rows(null, [
        { section: { key: "voyage", label: "Voyage", open: true }, fields: [{ key: "dest", label: "Destination" }, { key: "eta", label: "ETA" }] },
        { section: { key: "source", label: "Source", open: false }, fields: [{ key: "stations", label: "Stations" }] },
    ], { group: "ship" });
    cells.dest.textContent = "ROTTERDAM";
    assert.equal(document.querySelectorAll("#c .card-section").length, 2);
    assert.equal(document.querySelector('#c [data-section="voyage"]:not(.card-section)').classList.contains("card-collapsed"), false);
    assert.equal(document.querySelector('#c [data-section="source"]:not(.card-section)').classList.contains("card-collapsed"), true);

    const seen = [];
    c.el.addEventListener("card:section", (e) => seen.push(e.detail));
    c.section.toggle("source");
    assert.deepEqual(seen, [{ key: "source", open: true }]);
    assert.equal(document.querySelector('#c [data-section="source"]:not(.card-section)').classList.contains("card-collapsed"), false);
    c.section.available("source", false);
    assert.equal(document.querySelector('#c [data-section="source"].card-section').classList.contains("card-empty"), true);
    assert.equal(document.querySelector('#c [data-section="source"]:not(.card-section)').classList.contains("card-collapsed"), true);
    c.section.available("source", true);
    assert.equal(document.querySelector('#c [data-section="source"].card-section').classList.contains("card-empty"), false);
    assert.equal(document.querySelector('#c [data-section="source"]:not(.card-section)').classList.contains("card-collapsed"), false);
});

test("card: footer pages only when more than fit", () => {
    document.querySelector("#c .mapcard-footer").innerHTML = "";
    const c = cardLib.create({ mount: document.getElementById("c"), iconMax: 2 });
    c.footer.add({ icon: "more_horiz", label: "More", action: "more", group: "ship", more: true });
    ["a", "b"].forEach((a) => c.footer.add({ icon: a, label: a, action: a, group: "ship" }));
    c.footer.show("ship");
    const vis = () => [...document.querySelectorAll("#c .mapcard-footer > div")].filter((d) => d.style.display === "flex").map((d) => d.dataset.action);
    assert.deepEqual(vis(), ["a", "b"]);                    // fits: no More
    c.footer.add({ icon: "c", label: "c", action: "c", group: "ship" });
    c.footer.show("ship");
    assert.deepEqual(vis(), ["more", "a", "b"]);
    c.footer.rotate("ship");
    assert.deepEqual(vis(), ["more", "c"]);
});

test("card: min/max toggles the max-only elements", () => {
    document.querySelector("#c .mapcard-content").innerHTML = '<div class="card-max-only">x</div><div class="card-min-only">y</div>';
    const c = cardLib.create({ mount: document.getElementById("c") });
    assert.equal(c.isMax(), true);
    c.setMax(false);
    assert.equal(document.querySelector("#c .card-max-only").classList.contains("hidden"), true);
    assert.equal(document.querySelector("#c .card-min-only").classList.contains("visible"), true);
    c.setMax(true);
    assert.equal(document.querySelector("#c .card-max-only").classList.contains("hidden"), false);
});
