import { test } from "node:test";
import assert from "node:assert/strict";
import { setupDom } from "./dom.js";

setupDom('<ul class="floating-menu" id="m"></ul>');
const menu = await import("../menu.js");

test("menu: items, tags, checks and separators", () => {
    let labels = true;
    const m = menu.create({ mount: document.getElementById("m"), checks: { toggleLabels: () => labels } });
    m.add([
        { action: "copy", label: "Copy", group: "text", tags: ["ship"] },
        { action: "track", label: "Track", group: "ship", tags: ["ship"], icon: "timeline" },
        { action: "toggleLabels", label: "Labels", group: "map", tags: ["map"], check: true },
        { action: "settings", label: "Settings", group: "map" },
    ]);
    const lis = [...document.querySelectorAll("#m li")];
    assert.equal(lis.length, 4);
    assert.equal(lis[1].querySelector("i").className, "timeline_icon");

    m.open({ x: 10, y: 10, tags: ["ship"] });
    const shown = lis.filter((l) => l.style.display !== "none").map((l) => l.dataset.action);
    assert.deepEqual(shown, ["copy", "track", "settings"]);        // untagged always shows
    assert.equal(m.isOpen(), true);

    m.close();
    m.open({ x: 0, y: 0, tags: ["map"] });
    assert.deepEqual(lis.filter((l) => l.style.display !== "none").map((l) => l.dataset.action), ["toggleLabels", "settings"]);
    assert.equal(lis[2].classList.contains("checked"), true);
    labels = false;
    m.close(); m.open({ x: 0, y: 0, tags: ["map"] });
    assert.equal(lis[2].classList.contains("checked"), false);
});

test("menu: show() overrides the tag rule; setLabel", () => {
    const m = menu.create({ mount: document.getElementById("m") });
    m.add([{ action: "a", label: "A", tags: ["x"] }, { action: "b", label: "B", tags: ["x"] }]);
    m.open({ x: 0, y: 0, tags: ["x"], show: (li, byTag) => byTag && li.dataset.action !== "b" });
    const vis = [...document.querySelectorAll("#m li")].filter((l) => l.style.display !== "none").map((l) => l.dataset.action);
    assert.ok(vis.includes("a") && !vis.includes("b"));
    m.setLabel("a", "Aa");
    assert.equal(document.querySelector('#m li[data-action="a"] span').textContent, "Aa");
});
