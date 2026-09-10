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
    assert.deepEqual(urls, ['ships_place.json?tab=expected&hours=0&code=NLRTM']);
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

test('opening another port while one loads ignores the late response of the first', async () => {
    let first;
    const open = createPortDialog({ fetchJSON: url => url.includes('NLRTM') ? new Promise(resolve => { first = resolve; }) : Promise.resolve({ ships: [], total: 0 }), openVessel() {} });
    const pending = open([port]);
    assert.equal(document.querySelector('.port-results').getAttribute('aria-busy'), 'true');
    assert.equal(document.querySelectorAll('#port-ships thead th').length, 4);
    assert.ok(document.querySelector('.port-loading .spinner'));
    assert.doesNotMatch(document.querySelector('.port-results').textContent, /Loading/);
    assert.equal(document.querySelectorAll('#port-ships [role=tab]').length, 5);
    open([{ code: 'BEANR', label: 'Antwerp' }]);
    await tick();
    assert.equal(document.querySelector('#port-ships .dialog-title').firstChild.textContent, 'Antwerp');
    assert.equal(document.querySelector('#port-ships .dialog-title .dialog-title-hint').textContent, 'BEANR');
    assert.equal(document.querySelectorAll('.port-ship-count').length, 1);
    assert.equal(document.querySelector('.port-ship-count').textContent, '0 out of 0');
    assert.equal(document.querySelector('.port-results').getAttribute('aria-busy'), 'false');
    assert.equal(document.querySelector('.port-loading'), null);
    first({ ships, total: 12 });
    await pending;
    assert.match(document.querySelector('.port-results').textContent, /No ships in this view/);
    assert.equal(document.querySelectorAll('#port-ships tbody tr').length, 0);
});

test('failed requests can be retried and a response does not reopen a closed dialog', async () => {
    let attempts = 0, finish;
    const open = createPortDialog({ fetchJSON: async () => { if (!attempts++) throw new Error('offline'); return { ships: [], total: 0 }; }, openVessel() {} });
    await open([port]);
    assert.match(document.querySelector('.port-results').textContent, /Could not load/);
    document.querySelector('.port-retry').click();
    await tick();
    assert.match(document.querySelector('.port-results').textContent, /No ships in this view/);
    const slow = createPortDialog({ fetchJSON: () => new Promise(resolve => { finish = resolve; }), openVessel() {} });
    const pending = slow([port]);
    document.querySelector('#port-ships .close_icon').click();
    finish({ ships, total: 12 });
    await pending;
    assert.ok(document.getElementById('port-ships').classList.contains('hidden'));
});

 test('place tabs exclude Expected for a berth and render paired visit times', async () => {
    const urls=[];
    const open=createPortDialog({fetchJSON:async url=>{urls.push(url);return {time:200,has_geometry:true,total:1,counts:{inside:1,left:0,arrived:1,visits:1},ships:[{mmsi:123,shipname:'Berth ship',entered:100,exited:null,inside:true}]};},openVessel(){}});
    await open([{runtime_id:0,place_version:'v1',place_type:'berth',label:'Quay'}]);
    assert.equal(document.querySelectorAll('#port-ships [role=tab]').length,4);
    assert.match(urls[0], /tab=inside.*id=0&version=v1/);
    document.querySelector('[data-place-tab=visits]').click(); await tick();
    assert.match(urls.at(-1), /tab=visits/);
    assert.match(document.querySelector('#port-ships thead').textContent,/EntryExitDuration/);
    assert.match(urls.at(-1), /hours=0/);
    assert.equal(document.querySelector('#port-ships select'), null);
    assert.doesNotMatch(document.querySelector('#port-ships').textContent, /retained|Period/);
});
