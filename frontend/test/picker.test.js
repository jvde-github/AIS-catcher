import { test } from "node:test";
import assert from "node:assert/strict";
import { setupDom } from "./dom.js";

setupDom("<div id='host'></div>");
const { createPicker } = await import("@aiscatcher/ui/picker.js");

const stations = (n) => [{ value: -1, name: "All stations", group: "all" },
    ...Array.from({ length: n }, (_, i) => ({ value: i + 1, badge: i + 1, name: "Station " + (i + 1), sub: i ? "" : "Receiving", group: "one" }))];

function mount(o) {
    const host = document.getElementById("host");
    host.replaceChildren();
    const p = createPicker(o);
    host.append(p.el);
    return p;
}
const names = (p) => [...p.el.querySelectorAll(".pick__opt .pick__name")].map((n) => n.textContent);
const panel = (p) => p.el.querySelector(".pick__panel");
const search = (p) => p.el.querySelector(".pick__search");

test("picker: the button shows the chosen one, the list opens under it with a line between groups", () => {
    const p = mount({ options: () => stations(2), value: 2 });
    assert.equal(p.button.querySelector(".pick__name").textContent, "Station 2");
    assert.equal(p.button.querySelector(".pick__num").textContent, "2");
    assert.equal(panel(p).hidden, true);
    p.button.click();
    assert.equal(panel(p).hidden, false);
    assert.equal(p.button.getAttribute("aria-expanded"), "true");
    assert.deepEqual(names(p), ["All stations", "Station 1", "Station 2"]);
    assert.equal(p.el.querySelectorAll(".pick__list > .pick__sep").length, 1);
    assert.equal(p.el.querySelector('[aria-selected="true"] .pick__name').textContent, "Station 2");
    assert.equal(search(p).hidden, true);          // a short list needs no search
});

test("picker: a pick closes the list and says so once; the same value again says nothing", () => {
    const picks = [];
    const p = mount({ options: () => stations(3), value: -1, onPick: (v) => picks.push(v) });
    p.button.click();
    p.el.querySelectorAll(".pick__opt")[2].click();
    assert.deepEqual(picks, [2]);
    assert.equal(panel(p).hidden, true);
    assert.equal(p.button.querySelector(".pick__name").textContent, "Station 2");
    p.button.click();
    p.el.querySelectorAll(".pick__opt")[2].click();
    assert.deepEqual(picks, [2]);
    p.set(3);                                       // from elsewhere: no onPick
    assert.deepEqual(picks, [2]);
    assert.equal(p.button.querySelector(".pick__name").textContent, "Station 3");
});

test("picker: a long list searches by name, line and number; Enter takes the first found", () => {
    const picks = [];
    const p = mount({ options: () => stations(30), value: -1, onPick: (v) => picks.push(v), empty: "No station by that name" });
    p.button.click();
    assert.equal(search(p).hidden, false);
    search(p).value = "station 2";
    search(p).dispatchEvent(new Event("input"));
    assert.deepEqual(names(p), ["Station 2", ...Array.from({ length: 10 }, (_, i) => "Station " + (20 + i))]);
    assert.equal(p.el.querySelectorAll(".pick__list > .pick__sep").length, 0);   // no groups while searching
    search(p).value = "receiving";
    search(p).dispatchEvent(new Event("input"));
    assert.deepEqual(names(p), ["Station 1"]);
    search(p).value = "zzz";
    search(p).dispatchEvent(new Event("input"));
    assert.equal(p.el.querySelector(".pick__empty").textContent, "No station by that name");
    search(p).value = "17";
    search(p).dispatchEvent(new Event("input"));
    search(p).dispatchEvent(new window.KeyboardEvent("keydown", { key: "Enter", bubbles: true }));
    assert.deepEqual(picks, [17]);
    assert.equal(panel(p).hidden, true);
});

test("picker: a value no option has shows as the host says, marked; Escape closes; the footer ends the list", () => {
    const p = mount({ options: () => stations(1), value: 9, shown: (v) => ({ badge: v, name: "Station " + v, sub: "Not listed", warn: true }),
        footer: () => Object.assign(document.createElement("div"), { className: "extra", textContent: "New…" }) });
    assert.equal(p.button.querySelector(".pick__sub").textContent, "Not listed");
    assert.equal(p.button.classList.contains("pick__btn--warn"), true);
    p.button.click();
    assert.equal(p.el.querySelector(".pick__list").lastElementChild.className, "extra");
    window.dispatchEvent(new window.KeyboardEvent("keydown", { key: "Escape" }));
    assert.equal(panel(p).hidden, true);
});
