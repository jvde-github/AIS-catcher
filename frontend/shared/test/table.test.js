import { test } from "node:test";
import assert from "node:assert/strict";
import { setupDom } from "./dom.js";

setupDom(
    '<div id="tableside">' +
    '<div class="table-pager" id="pager"><button>&#8249;</button><span></span><button>&#8250;</button></div>' +
    '<table><thead><tr>' +
    '<th data-column="shipname" class="sort-icon ascending">Name</th>' +
    '<th data-column="speed" class="sort-icon">Spd</th>' +
    '</tr></thead></table>' +
    '</div>');
const table = await import("../table.js");

test("table: comparators keep blanks last in both directions", () => {
    assert.ok(table.compareNumber(3, 7, "ascending") < 0);
    assert.ok(table.compareNumber(null, 7, "ascending") > 0);      // blank after 7
    assert.ok(table.compareNumber(null, 7, "descending") < 0);     // the sign flip puts it after again
    assert.ok(table.compareNumber(3, null, "descending") > 0);

    assert.ok(table.compareString("ANNA", "BERT") < 0);
    assert.equal(table.compareString(null, null), 0);
    assert.ok(table.compareString(null, "BERT") > 0);
    assert.ok(table.compareString("ANNA", null) < 0);
});

test("table: nextOrder flips the sorted column, starts a new one ascending", () => {
    const ths = document.querySelectorAll("th[data-column]");
    assert.equal(table.nextOrder(ths[0]), "descending");           // was ascending
    assert.equal(table.nextOrder(ths[1]), "ascending");            // was unsorted
});

test("table: markSort moves the marker", () => {
    const root = document.getElementById("tableside");
    table.markSort(root, "speed", "descending");
    const ths = root.querySelectorAll("th[data-column]");
    assert.equal(ths[0].classList.contains("ascending"), false);
    assert.equal(ths[1].classList.contains("descending"), true);

    table.markSort(root, "shipname", "ascending");
    assert.equal(ths[0].classList.contains("ascending"), true);
    assert.equal(ths[1].classList.contains("descending"), false);
});

test("table: renderPager label, disabling and hiding", () => {
    const pager = document.getElementById("pager");
    table.renderPager(pager, 2, 12, 224);
    assert.equal(pager.hidden, false);
    assert.equal(pager.querySelector("span").textContent, "3 / 12 · 224");
    const btns = pager.querySelectorAll("button");
    assert.equal(btns[0].disabled, false);
    assert.equal(btns[1].disabled, false);

    table.renderPager(pager, 0, 2, 30);
    assert.equal(btns[0].disabled, true);
    table.renderPager(pager, 1, 2, 30);
    assert.equal(btns[1].disabled, true);

    table.renderPager(pager, 0, 1, 8);                             // one page: no pager
    assert.equal(pager.hidden, true);
});
