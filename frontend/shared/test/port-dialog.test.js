import { test } from 'node:test';
import assert from 'node:assert/strict';
import { setupDom } from './dom.js';
setupDom('');
const { createPortDialog } = await import('../port-dialog.js');
const tick = () => new Promise(resolve => setTimeout(resolve, 0));
const port = { code: 'NLRTM', label: 'Rotterdam' };
const ships = Array.from({ length: 12 }, (_, i) => ({ mmsi: 244000001 + i, shipname: i ? `Ship ${i}` : '<img src=x onerror=alert(1)>', country: 'NL', shipclass: 0, cog: 90, speed: i ? 0 : null, timestamp: 0 }));

test('place panel displays a server page and keeps the list while opening a vessel', async () => {
    const opened = [], urls = [];
    const open = createPortDialog({ fetchJSON: async url => { urls.push(url); return { ships: ships.slice(0, 10), total: 12, offset: 0, limit: 10 }; }, openVessel: id => opened.push(id) });
    await open([port]);
    assert.deepEqual(urls, ['ships_place.json?tab=closest&hours=0&offset=0&limit=10&code=NLRTM']);
    assert.equal(document.querySelectorAll('#port-ships tbody tr').length, 10);
    assert.equal(document.querySelector('#port-ships img'), null);
    assert.match(document.querySelector('#port-ships tbody').textContent, /<img src=x onerror=alert\(1\)>/);
    assert.match(document.querySelector('#port-ships tbody').textContent, /0\.0/);
    assert.equal(document.querySelector('.port-ship-count').textContent, '1–10 of 12');
    assert.equal(document.querySelectorAll('#port-ships tbody .fi-nl').length, 10);
    assert.equal(document.querySelectorAll('#port-ships .table-shiptype-icon').length, 10);
    document.querySelector('.port-ship-link').click();
    assert.deepEqual(opened, [244000001]);
    assert.ok(!document.getElementById('port-ships').classList.contains('hidden'));
    assert.equal(document.querySelector('.modal-overlay'), null);
    assert.ok(document.querySelector('#port-ships tr.selected'));
});

test('a place with no outline offers only what it can answer', async () => {
    const open = createPortDialog({ fetchJSON: async () => ({ time: 1, has_geometry: false, total: 0,
        counts: {inside:0,left:0,arrived:0,visits:0,closest:2,expected:0}, ships: [] }), openVessel() {} });
    await open([{ code: 'NLVLI', label: 'Vlissingen', has_geometry: false }]);
    assert.deepEqual([...document.querySelectorAll('#port-ships [role=tab]')].map(b => b.dataset.placeTab),
                     ['closest', 'expected']);
    assert.equal(document.querySelector('#port-ships .sc-tab[aria-selected=true]')?.dataset.placeTab, 'closest');
});

test('opening another port while one loads ignores the late response of the first', async () => {
    let first;
    const open = createPortDialog({ fetchJSON: url => url.includes('NLRTM') ? new Promise(resolve => { first = resolve; }) : Promise.resolve({ ships: [], total: 0 }), openVessel() {} });
    const pending = open([port]);
    assert.equal(document.querySelector('.port-results').getAttribute('aria-busy'), 'true');
    assert.equal(document.querySelectorAll('#port-ships thead th').length, 4);
    assert.ok(document.querySelector('.port-loading .spinner'));
    assert.doesNotMatch(document.querySelector('.port-results').textContent, /Loading/);
    assert.equal(document.querySelectorAll('#port-ships [role=tab]').length, 6); // + Closest
    open([{ code: 'BEANR', label: 'Antwerp' }]);
    await tick();
    assert.equal(document.querySelector('#port-ships-title').textContent, 'Antwerp');
    assert.equal(document.querySelector('#port-ships .side-table-label').textContent, 'Port');
    assert.equal(document.querySelector('#port-ships .place-panel-kind').textContent, 'Belgium · BEANR');
    assert.equal(document.querySelectorAll('.port-ship-count').length, 1);
    assert.equal(document.querySelector('.port-ship-count').textContent, '0 of 0');
    assert.equal(document.querySelector('.port-results').getAttribute('aria-busy'), 'false');
    assert.equal(document.querySelector('.port-loading'), null);
    first({ ships, total: 12 });
    await pending;
    assert.match(document.querySelector('.port-results').textContent, /No ships in this view/);
    assert.equal(document.querySelectorAll('#port-ships tbody tr').length, 0);
});

test('failed requests can be retried and a response does not reopen a closed panel', async () => {
    let attempts = 0, finish;
    const open = createPortDialog({ fetchJSON: async () => { if (!attempts++) throw new Error('offline'); return { ships: [], total: 0 }; }, openVessel() {} });
    await open([port]);
    assert.match(document.querySelector('.port-results').textContent, /Could not load/);
    document.querySelector('.port-retry').click();
    await tick();
    assert.match(document.querySelector('.port-results').textContent, /No ships in this view/);
    const slow = createPortDialog({ fetchJSON: () => new Promise(resolve => { finish = resolve; }), openVessel() {} });
    const pending = slow([port]);
    document.querySelector('#port-ships .place-close').click();
    finish({ ships, total: 12 });
    await pending;
    assert.ok(document.getElementById('port-ships').classList.contains('hidden'));
});

 test('place tabs exclude Expected for a berth and render paired visit times', async () => {
    const urls=[];
    const open=createPortDialog({fetchJSON:async url=>{urls.push(url);return {time:200,has_geometry:true,total:1,counts:{inside:1,left:0,arrived:1,visits:1},ships:[{mmsi:123,shipname:'Berth ship',entered:100,exited:null,inside:true}]};},openVessel(){}});
    await open([{runtime_id:0,place_version:'v1',place_type:'berth',label:'Quay'}]);
    assert.equal(document.querySelector('#port-ships .side-table-label').textContent, 'Berth');
    assert.equal(document.querySelector('#port-ships .place-panel-kind').hidden, true);
    assert.equal(document.querySelectorAll('#port-ships [role=tab]').length,5);
    assert.equal(document.querySelector('[data-place-tab=expected]'), null);
    // a quiet hour is not a reason to take a tab away: a drawn place keeps them
    assert.equal(document.querySelector('[data-place-tab=left]').hidden, false);
    assert.equal(document.querySelector('[data-place-tab=inside]').hidden, false);
    assert.equal(document.querySelector('[data-place-tab=closest]').hidden, false);
    assert.match(urls[0], /tab=inside.*id=0&version=v1/);
    document.querySelector('[data-place-tab=visits]').click(); await tick();
    assert.match(urls.at(-1), /tab=visits/);
    assert.match(document.querySelector('#port-ships thead').textContent,/EntryExitDuration/);
    assert.match(urls.at(-1), /hours=0/);
    assert.equal(document.querySelector('#port-ships select'), null);
    assert.doesNotMatch(document.querySelector('#port-ships').textContent, /retained|Period/);
});


test('pagination asks the server for the next page and tab changes reset it', async () => {
    const urls = [], opened = [];
    const open = createPortDialog({fetchJSON: async url => {
        urls.push(url);
        const query = new URL(url, 'http://localhost').searchParams;
        const offset = Number(query.get('offset')), limit = Number(query.get('limit'));
        return {ships: ships.slice(offset, offset + limit), total: ships.length, offset, limit};
    }, openVessel: mmsi => opened.push(mmsi)});
    await open([port]);
    assert.equal(document.querySelector('.place-prev').disabled, true);
    document.querySelector('.place-next').click(); await tick();
    assert.match(urls.at(-1), /offset=10&limit=10/);
    assert.equal(document.querySelector('.port-ship-count').textContent, '11–12 of 12');
    assert.equal(document.querySelector('.place-next').disabled, true);
    document.querySelector('.port-ship-link').click();
    assert.deepEqual(opened, [ships[10].mmsi]);
    assert.equal(document.querySelector('.place-page').textContent, '2 / 2');
    document.querySelector('[data-place-tab=expected]').click(); await tick();
    assert.match(urls.at(-1), /tab=expected.*offset=0/);
    assert.equal(document.querySelector('#port-ships input'), null);
    document.querySelector('.place-close').click();
});

test('In view restores the existing overview without replacing its state', async () => {
    const table = document.getElementById('tableside');
    const overview = document.createElement('div');
    overview.id = 'overview-kept';
    overview.innerHTML = '<input value="remember me">';
    table.prepend(overview);
    let returned = 0;
    table.addEventListener('table:overview', () => returned++, {once:true});
    const open = createPortDialog({fetchJSON: async () => ({ships: [], total:0}), openVessel(){}});
    await open([port]);
    assert.equal(table.dataset.context, 'port-ships');
    document.querySelector('.place-overview').click();
    assert.equal(table.dataset.context, undefined);
    assert.equal(returned, 1);
    assert.ok(table.classList.contains('active'));
    assert.equal(document.getElementById('overview-kept'), overview);
    assert.equal(overview.querySelector('input').value, 'remember me');
    overview.remove();
});

test('a server that does not page is shown as one page, not the same rows under every page number', async () => {
    const open = createPortDialog({fetchJSON: async () => ({ships: ships.slice(0, 10), total: 3000, counts: {closest: 3000}}), openVessel(){}});
    await open([port]);
    assert.equal(document.querySelectorAll('#port-ships tbody tr').length, 10);
    assert.equal(document.querySelector('.port-ship-count').textContent, '1–10 of 10');
    assert.equal(document.querySelector('.place-next').disabled, true);
    document.querySelector('.place-close').click();
});

test('a shrinking result set returns to the last available page', async () => {
    let total = 12;
    const open = createPortDialog({fetchJSON: async url => {
        const offset = Number(new URL(url, 'http://localhost').searchParams.get('offset'));
        return {ships: ships.slice(offset, Math.min(offset + 10, total)), total, offset, limit: 10};
    }, openVessel(){}});
    await open([port]);
    total = 3;
    document.querySelector('.place-next').click(); await tick();
    assert.equal(document.querySelector('.port-ship-count').textContent, '1–3 of 3');
    assert.equal(document.querySelector('.place-page').textContent, '1 / 1');
    document.querySelector('.place-close').click();
});


test('Visits rows hover by MMSI without flicker and clear on paging and close', async () => {
    const events = [];
    const open = createPortDialog({fetchJSON: async url => {
        const offset = Number(new URL(url, 'http://localhost').searchParams.get('offset'));
        return {ships: ships.slice(offset, offset + 10), total: ships.length, offset, limit: 10};
    }, openVessel() {}, hoverVessel: id => events.push(['enter', id]), unhoverVessel: id => events.push(['leave', id])});
    await open([port]);
    document.querySelector('[data-place-tab=visits]').click(); await tick();
    const row = document.querySelector('#port-ships tbody tr');
    const name = row.querySelector('.vessel-link'), flag = row.querySelector('.flag');
    name.dispatchEvent(new MouseEvent('mouseover', {bubbles:true}));
    assert.deepEqual(events, [['enter', ships[0].mmsi]]);
    name.dispatchEvent(new MouseEvent('mouseout', {bubbles:true, relatedTarget:flag}));
    flag.dispatchEvent(new MouseEvent('mouseover', {bubbles:true, relatedTarget:name}));
    assert.equal(events.length, 1);
    flag.dispatchEvent(new MouseEvent('mouseout', {bubbles:true, relatedTarget:document.body}));
    assert.deepEqual(events.at(-1), ['leave', ships[0].mmsi]);
    name.dispatchEvent(new MouseEvent('mouseover', {bubbles:true}));
    document.querySelector('.place-next').click(); await tick();
    assert.deepEqual(events.at(-1), ['leave', ships[0].mmsi]);
    document.querySelector('#port-ships .vessel-link').dispatchEvent(new MouseEvent('mouseover', {bubbles:true}));
    assert.deepEqual(events.at(-1), ['enter', ships[10].mmsi]);
    document.querySelector('.place-close').click();
    assert.deepEqual(events.at(-1), ['leave', ships[10].mmsi]);
});

test('visit times distinguish observed crossings from bounds and minimum durations', async () => {
    const cases = [
        {entered:null, entered_before:100, observed_until:400, inside:true},
        {entered:100, exited:400, inside:false},
        {entered:100, exited:null, exited_after:400, inside:false},
        {entered:null, entered_before:100, exited:400, inside:false},
        {entered:100, observed_until:400, inside:true},
        {entered:null, observed_until:400, inside:true},
        {entered:500, observed_until:400, inside:true},
        {entered_before:100, observed_until:400, inside:true, pending:true},
    ];
    const open = createPortDialog({fetchJSON:async () => ({time:9999, total:cases.length,
        ships:cases.map((v,i) => ({...v, mmsi:244000001+i, shipname:'Visit '+i}))}), openVessel(){}});
    await open([port]);
    document.querySelector('[data-place-tab=visits]').click(); await tick();
    const cells = [...document.querySelectorAll('#port-ships tbody tr')].map(row => [...row.children].map(td => td.textContent));
    assert.ok(cells[0][1].startsWith('< '));
    assert.equal(cells[0][2], '—');
    assert.equal(cells[0][3], '> 5m'); // uses observed_until, never the wall clock
    assert.ok(!/^[<>]/.test(cells[1][1]));
    assert.ok(!/^[<>]/.test(cells[1][2]));
    assert.equal(cells[1][3], '5m');
    assert.ok(cells[2][2].startsWith('> '));
    assert.equal(cells[2][3], '> 5m');
    assert.ok(cells[3][1].startsWith('< '));
    assert.equal(cells[3][3], '> 5m');
    assert.equal(cells[4][3], '> 5m');
    assert.equal(cells[5][1], 'Unknown');
    assert.equal(cells[5][3], 'Unknown');
    assert.equal(cells[6][3], 'Unknown');
    assert.equal(cells[7][1], 'Confirming…');
    assert.equal(cells[7][3], '—');
    document.querySelector('.place-close').click();
});

test('visit dates omit today, retain older dates across midnight, and keep full timestamp tooltips', async () => {
    const stamp = (year, month, day, hour, minute) => new Date(year, month - 1, day, hour, minute, 0).getTime() / 1000;
    const now = stamp(2026, 9, 15, 0, 10);
    const cases = [
        {entered_before:stamp(2026, 9, 15, 0, 5), inside:true, observed_until:now},
        {entered:stamp(2026, 9, 14, 23, 59), exited:stamp(2026, 9, 15, 0, 3)},
        {entered:stamp(2025, 9, 15, 0, 5), exited_after:now},
    ];
    await createPortDialog({fetchJSON:async () => ({time:now,total:cases.length,
        ships:cases.map((v,i) => ({...v,mmsi:244000001+i,shipname:'Visit '+i}))}),openVessel(){}})([port]);
    document.querySelector('[data-place-tab=visits]').click(); await tick();
    const rows = [...document.querySelectorAll('#port-ships tbody tr')];
    assert.equal(rows[0].children[1].textContent, '< 00:05');
    assert.match(rows[0].children[1].firstElementChild.title, /^Before .*2026/);
    assert.match(rows[0].children[1].firstElementChild.title, /00:05:00/);
    assert.notEqual(rows[1].children[1].textContent, '23:59');
    assert.match(rows[1].children[1].textContent, /14/);
    assert.equal(rows[1].children[2].textContent, '00:03');
    assert.notEqual(rows[2].children[1].textContent, '00:05');
    assert.match(rows[2].children[1].firstElementChild.title, /2025/);
    assert.equal(rows[2].children[2].textContent, '> 00:10');
    assert.match(rows[2].children[2].firstElementChild.title, /^After /);
    document.querySelector('.place-close').click();
});
