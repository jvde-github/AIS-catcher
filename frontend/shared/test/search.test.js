import { test } from 'node:test';
import assert from 'node:assert/strict';
import { setupDom } from './dom.js';
import { createSearch } from '../search.js';
import { createLocalSearch } from '../../src/features/search.js';

const pause = () => new Promise(resolve => setTimeout(resolve, 10));
function fixture(search, onSelect = () => {}) {
    const win = setupDom('<button id="open">Search</button><div id="scrim"></div><div id="panel"></div>');
    const panel = createSearch({root: document.getElementById('panel'), scrim: document.getElementById('scrim'),
        triggers: [document.getElementById('open')], search, onSelect, delay: 0});
    const input = document.getElementById('map-search');
    const type = value => { input.value = value; input.dispatchEvent(new Event('input')); };
    panel.open();
    return {panel, input, type, win};
}

test('search ignores old responses after a newer query, clear, and close', async () => {
    const pending = [];
    const {panel, type} = fixture((q, category, signal) => new Promise(resolve => pending.push({q, signal, resolve})));
    type('old'); await pause();
    type('new'); await pause();
    assert.equal(pending[0].signal.aborted, true);
    pending[1].resolve([{type: 'ship', id: 2, name: 'New ship'}]); await pause();
    pending[0].resolve([{type: 'ship', id: 1, name: 'Old ship'}]); await pause();
    assert.equal(document.querySelector('.search-result-title').textContent, 'New ship');
    type('clear'); await pause(); document.getElementById('clear-search').click();
    pending[2].resolve([{type: 'ship', id: 3, name: 'Cleared ship'}]); await pause();
    assert.equal(document.querySelectorAll('.search-result').length, 0);
    type('close'); await pause(); panel.close();
    pending[3].resolve([{type: 'ship', id: 4, name: 'Closed ship'}]); await pause();
    assert.equal(panel.isOpen(), false);
    assert.equal(document.querySelectorAll('.search-result').length, 0);
});

test('search supports keyboard choice and renders result names as text', async () => {
    let chosen;
    const items = [{type: 'ship', id: 1, name: '<img src=x onerror=alert(1)>'}, {type: 'port', id: 'GREEU', name: 'Eleusina'}];
    const {panel, input, type, win} = fixture(async () => items, item => { chosen = item; });
    type('e'); await pause();
    assert.equal(document.querySelector('.search-result-title').textContent, items[0].name);
    assert.equal(document.querySelector('.search-result img'), null);
    input.dispatchEvent(new win.KeyboardEvent('keydown', {key: 'ArrowUp', bubbles: true}));
    assert.equal(input.getAttribute('aria-activedescendant'), 'search-result-1');
    input.dispatchEvent(new win.KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
    await pause();
    assert.deepEqual(chosen, items[1]);
    assert.equal(panel.isOpen(), false);
});

test('changing category replaces an in-flight search', async () => {
    const pending = [];
    const {panel, type} = fixture((query, category, signal) => new Promise(resolve => pending.push({category, signal, resolve})));
    type('demo'); await pause();
    document.getElementById('search-ports').click(); await pause();
    assert.equal(pending[0].signal.aborted, true);
    assert.equal(pending[1].category, 'ports');
    pending[1].resolve([]); await pause();
    pending[0].resolve([{type: 'ship', id: 1, name: 'Wrong category'}]); await pause();
    assert.equal(document.querySelectorAll('.search-result').length, 0);
    assert.equal(document.querySelector('.search-status').textContent, 'No results found');
    panel.close();
});

test('local search covers off-map vessels, identifiers, and configured places with runtime IDs', async () => {
    const oldFetch = globalThis.fetch;
    let receiver = 3, requests = [];
    globalThis.fetch = async url => {
        requests.push(url);
        return {ok: true, json: async () => ({place_version: 'v1', objects: [
            {kind: 10, runtime_id: 8, code: 'GREEU', label: 'Éleusina', place_type: 'port', lat: 38, lon: 23},
            {kind: 10, runtime_id: 9, label: 'East anchorage', place_type: 'anchorage', lat: 38, lon: 23},
            {kind: 8, id: 's14', label: 'Receiver north', country: 'GR', lat: 38, lon: 23},
        ]})};
    };
    try {
        const search = createLocalSearch({receiver: () => receiver, ships: () => ({
            a: {show: false, raw: {mmsi: 123456789, shipname: 'DEMO VESSEL', callsign: 'ABC123', imo: 9876543, lat: -30, lon: -70}},
            b: {raw: {mmsi: 987654321, shipname: 'A &amp; B', callsign: 'A&amp;B'}},
        }), shipName: s => s.shipname, callSign: s => s.callsign});
        for (const query of ['demo vessel', '123456789', 'ABC123', '9876543']) {
            assert.equal((await search(query, 'ships'))[0].id, 123456789);
        }
        assert.equal(requests.length, 0);
        assert.equal((await search('A & B', 'ships'))[0].name, 'A & B');
        assert.equal((await search('A&B', 'ships'))[0].callsign, 'A&B');
        const port = (await search('eleusina', 'all'))[0];
        assert.equal(port.object.runtime_id, 8);
        assert.equal(port.object.place_version, 'v1');
        assert.equal((await search('GREEU', 'ports'))[0].name, 'Éleusina');
        assert.equal((await search('anchorage', 'ports'))[0].type, 'place');
        assert.equal((await search('north', 'stations'))[0].id, '14');
        assert.deepEqual(requests, ['api/mapobjects.json?since=0&receiver=3']);
        receiver = 5;
        globalThis.fetch = async () => { receiver = 4; return {ok: true, json: async () => ({objects: []})}; };
        assert.deepEqual(await search('demo', 'all'), []);
    } finally { globalThis.fetch = oldFetch; }
});
