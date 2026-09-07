import { test } from 'node:test';
import assert from 'node:assert/strict';
import { setupDom } from './dom.js';
setupDom('');
const { createPortDialog } = await import('../port-dialog.js');
const tick = () => new Promise(resolve => setTimeout(resolve, 0));
const port = { code: 'NLRTM', label: 'Rotterdam' };
const ships = Array.from({ length: 12 }, (_, i) => ({ mmsi: 244000001 + i, shipname: i ? `Ship ${i}` : '<img src=x onerror=alert(1)>', country: 'NL', shipclass: 0, cog: 90, speed: i ? 0 : null, timestamp: 0 }));

test('port dialog caps the list, escapes names, preserves zero speed and opens a ship', async () => {
    const opened = [], urls = [];
    const open = createPortDialog({ fetchJSON: async url => { urls.push(url); return { ships, total: 12 }; }, openVessel: id => opened.push(id) });
    await open([port]);
    assert.deepEqual(urls, ['ships_port.json?code=NLRTM']);
    assert.equal(document.querySelectorAll('#port-ships tbody tr').length, 10);
    assert.equal(document.querySelector('#port-ships img'), null);
    assert.match(document.querySelector('#port-ships tbody').textContent, /<img src=x onerror=alert\(1\)>/);
    assert.match(document.querySelector('#port-ships tbody').textContent, /0\.0/);
    assert.equal(document.querySelector('.port-ship-count').textContent, '10 out of 12');
    assert.equal(document.querySelectorAll('#port-ships .fi-nl').length, 10);
    assert.equal(document.querySelectorAll('#port-ships .table-shiptype-icon').length, 10);
    document.querySelector('.port-ship-link').click();
    assert.deepEqual(opened, [244000001]);
    assert.ok(document.getElementById('port-ships').classList.contains('hidden'));
});

test('switching an overlapping port ignores a late response from the previous port', async () => {
    let first;
    const open = createPortDialog({ fetchJSON: url => url.includes('NLRTM') ? new Promise(resolve => { first = resolve; }) : Promise.resolve({ ships: [], total: 0 }), openVessel() {} });
    const pending = open([port, { code: 'BEANR', label: 'Antwerp' }]);
    assert.match(document.querySelector('.port-results').textContent, /Loading/);
    const select = document.querySelector('.port-choice select');
    select.value = '1';
    select.dispatchEvent(new Event('change', { bubbles: true }));
    await tick();
    assert.equal(document.querySelector('#port-ships .dialog-title').textContent, 'Destination: Antwerp');
    assert.equal(document.querySelectorAll('.port-ship-count').length, 1);
    assert.equal(document.querySelector('.port-ship-count').textContent, '0 out of 0');
    first({ ships, total: 12 });
    await pending;
    assert.match(document.querySelector('.port-results').textContent, /No ships currently/);
    assert.equal(document.querySelectorAll('#port-ships tbody tr').length, 0);
});

test('failed requests can be retried and a response does not reopen a closed dialog', async () => {
    let attempts = 0, finish;
    const open = createPortDialog({ fetchJSON: async () => { if (!attempts++) throw new Error('offline'); return { ships: [], total: 0 }; }, openVessel() {} });
    await open([port]);
    assert.match(document.querySelector('.port-results').textContent, /Could not load/);
    document.querySelector('.port-retry').click();
    await tick();
    assert.match(document.querySelector('.port-results').textContent, /No ships currently/);
    const slow = createPortDialog({ fetchJSON: () => new Promise(resolve => { finish = resolve; }), openVessel() {} });
    const pending = slow([port]);
    document.querySelector('#port-ships .close_icon').click();
    finish({ ships, total: 12 });
    await pending;
    assert.ok(document.getElementById('port-ships').classList.contains('hidden'));
});
