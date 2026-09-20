import { test } from "node:test";
import assert from "node:assert/strict";
import { setupDom } from "./dom.js";

setupDom(
    '<div id="tableside">' +
    '<footer class="table-footer"><span class="table-count"></span><div class="table-pager" id="pager"><button>&#8249;</button><span></span><button>&#8250;</button></div></footer>' +
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

test("table: bottom pager shows ranges and disables unavailable pages", () => {
    const pager = document.getElementById("pager");
    const count = document.querySelector('.table-count');
    const btns = pager.querySelectorAll("button");
    table.renderPager(pager, {page: 2, perPage: 20, total: 224});
    assert.equal(pager.hidden, false);
    assert.equal(pager.querySelector("span").textContent, "3 / 12");
    assert.equal(count.textContent, '41–60 of 224');
    assert.equal(btns[0].disabled, false);
    assert.equal(btns[1].disabled, false);

    table.renderPager(pager, {page: 0, perPage: 20, total: 30});
    assert.equal(btns[0].disabled, true);
    table.renderPager(pager, {page: 1, perPage: 20, total: 30});
    assert.equal(btns[1].disabled, true);
    assert.equal(count.textContent, '21–30 of 30');

    table.renderPager(pager, {page: 0, perPage: 20, total: 8});
    assert.equal(pager.hidden, false);
    assert.equal(count.textContent, '1–8 of 8');
    assert.equal(btns[0].disabled, true);
    assert.equal(btns[1].disabled, true);
    table.renderPager(pager, {page: 0, perPage: 20, total: 0});
    assert.equal(count.textContent, '0 of 0');
    assert.equal(pager.querySelector("span").textContent, '1 / 1');
    table.renderPager(pager, {page: 1, perPage: 20, total: 100, loading: true});
    assert.equal(btns[0].disabled, true);
    assert.equal(btns[1].disabled, true);
});

test('table: shared rows preserve identifiers and escape feed text', () => {
    const body = document.createElement('tbody');
    const ship = {id: 'abc"<img>', mmsi: 244000001, name: '<img src=x onerror=alert(1)>', country: 'NL', speed: '0.0', distance: '3.2', last: '2s', selected: true, stale: true, relay: true};
    body.innerHTML = table.renderVesselRows([ship], {idAttribute: 'data-id', distance: true});
    assert.equal(body.querySelector('tr').dataset.id, ship.id);
    assert.equal(body.querySelector('img'), null);
    assert.ok(body.querySelector('tr.selected.stale'));
    assert.ok(body.querySelector('.fi-nl'));
    assert.ok(body.querySelector('.row-relay'));
    assert.equal(body.querySelector('.vessel-link').firstChild.textContent, ship.name);
    assert.equal(body.querySelector('.col-spd').textContent, '0.0');
    assert.equal(body.querySelectorAll('td').length, 5);
    body.innerHTML = table.renderVesselRows([{...ship, id: undefined, speed: null}]);
    assert.equal(body.querySelector('tr').dataset.mmsi, String(ship.mmsi));
    assert.equal(body.querySelectorAll('td').length, 4);
    assert.equal(body.querySelector('.col-spd').textContent, '—');
});
