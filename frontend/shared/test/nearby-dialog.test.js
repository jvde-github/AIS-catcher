import { test, afterEach } from 'node:test';
import assert from 'node:assert/strict';
import { setupDom } from './dom.js';
setupDom('');
const { createNearbyDialog } = await import('../nearby-dialog.js');
const { createPortDialog } = await import('../port-dialog.js');
const tick = () => new Promise(resolve => setTimeout(resolve, 0));
const anchor = {lat:38.03, lon:23.52, mmsi:237990001, title:'AEGEAN TIFFANY', country:'GR', shiptype:70, status:5};
const ships = Array.from({length:12}, (_,i) => ({mmsi:237990002+i, shipname:'Ship '+i, country:'GR', speed:0, shipclass:0, range:i/10, bearing:90}));
const data = {time:1000, ships, stations:[{id:3, name:'Station 3', online:true, last:940, range:1, bearing:180}],
    places:[{runtime_id:4, code:'GREEU', label:'Eleusina', has_geometry:true}], place_version:'v1'};
afterEach(() => {
    document.querySelectorAll('.side-table-context').forEach(root => root._dispose?.());
    document.body.innerHTML = '';
});

test('Nearby uses a vessel header and shared paged rows, keeping the list when a vessel opens', async () => {
    const opened = [], urls = [], hover = [];
    const open = createNearbyDialog({fetchJSON:async url => {urls.push(url); return data;},
        openVessel:id => opened.push(id), hoverVessel:id => hover.push(['enter',id]), unhoverVessel:id => hover.push(['leave',id])});
    await open(anchor);
    assert.equal(document.querySelector('.modal-overlay'), null);
    assert.equal(document.getElementById('tableside').dataset.context, 'nearby');
    assert.equal(document.getElementById('nearby-title').textContent, anchor.title);
    assert.equal(document.querySelector('.place-panel-kind').textContent, 'Cargo · Moored');
    assert.ok(document.querySelector('.place-panel-heading .fi-gr'));
    assert.equal(urls.length, 1);
    assert.match(urls[0], /mmsi=237990001/);
    assert.equal(document.querySelectorAll('#nearby tbody tr').length, 10);
    assert.equal(document.querySelector('.table-count').textContent, '1–10 of 12');
    assert.equal(document.querySelector('.col-spd:not(th)').textContent, '0.0');
    assert.equal(document.querySelector('.nearby-bearing').getAttribute('aria-label'), 'E');
    const name = document.querySelector('.vessel-link');
    name.dispatchEvent(new MouseEvent('mouseover', {bubbles:true}));
    name.click();
    assert.deepEqual(opened, [ships[0].mmsi]);
    assert.equal(document.getElementById('tableside').dataset.context, 'nearby');
    assert.ok(document.querySelector('tr.selected'));
    document.querySelector('.nearby-next').click();
    assert.deepEqual(hover, [['enter',ships[0].mmsi],['leave',ships[0].mmsi]]);
    assert.equal(document.querySelector('.table-count').textContent, '11–12 of 12');
    assert.equal(document.querySelector('.nearby-next').disabled, true);
    assert.equal(urls.length, 1); // tab and page changes reuse the one nearby answer
    document.querySelector('[data-nearby-tab=stations]').click();
    assert.equal(document.querySelector('.table-count').textContent, '1–1 of 1');
});

test('station details keep Nearby, and a port replaces it with the port sidebar', async () => {
    const opened = [], places = [];
    const host = {fetchJSON:async () => data, openStation:id => opened.push(id)};
    const openPlace = createPortDialog({fetchJSON:async () => ({ships:[], total:0}), openVessel(){}});
    await createNearbyDialog({...host, openPlace:place => {places.push(place); return openPlace([place]);}})(anchor);
    document.querySelector('[data-nearby-tab=stations]').click();
    document.querySelector('tr[data-station] button').click();
    assert.deepEqual(opened, [3]);
    assert.equal(document.getElementById('tableside').dataset.context, 'nearby');
    document.querySelector('[data-nearby-tab=places]').click();
    document.querySelector('tr[data-place] button').click();
    await tick();
    assert.equal(places[0].place_version, 'v1');
    assert.equal(document.getElementById('tableside').dataset.context, 'port-ships');
    assert.ok(document.getElementById('nearby').classList.contains('hidden'));
    assert.equal(document.getElementById('nearby')._dispose, null);
    assert.ok(!document.getElementById('port-ships').classList.contains('hidden'));
    document.querySelector('#nearby .place-close').click(); // a stale shell cannot close the new one
    assert.equal(document.getElementById('tableside').dataset.context, 'port-ships');
});

test('switching context cancels the old hover and ignores a late Nearby response', async () => {
    let finish;
    const openNearby = createNearbyDialog({fetchJSON:() => new Promise(resolve => {finish = resolve;}), openVessel(){}});
    const pending = openNearby(anchor);
    await createPortDialog({fetchJSON:async () => ({ships:[], total:0}), openVessel(){}})([{code:'GREEU'}]);
    finish(data); await pending;
    assert.equal(document.getElementById('tableside').dataset.context, 'port-ships');
    assert.equal(document.querySelectorAll('#nearby tbody tr').length, 0);
    const hover = [];
    await createNearbyDialog({fetchJSON:async () => data, openVessel(){}, hoverVessel:id => hover.push(id), unhoverVessel:id => hover.push(-id)})(anchor);
    document.querySelector('#nearby .vessel-link').dispatchEvent(new MouseEvent('mouseover', {bubbles:true}));
    await createPortDialog({fetchJSON:async () => ({ships:[], total:0}), openVessel(){}})([{code:'GREEU'}]);
    assert.deepEqual(hover, [ships[0].mmsi, -ships[0].mmsi]);
});

test('In view preserves the overview and Nearby closes using the common sidebar action', async () => {
    document.body.innerHTML = '<aside id="tableside" class="tableside_window"><input id="overview" value="kept"></aside>';
    const overview = document.getElementById('overview');
    const open = createNearbyDialog({fetchJSON:async () => data, openVessel(){}});
    await open(anchor);
    document.querySelector('#nearby .place-overview').click();
    assert.equal(document.getElementById('overview'), overview);
    assert.equal(overview.value, 'kept');
    assert.ok(document.getElementById('tableside').classList.contains('active'));
    assert.equal(document.getElementById('tableside').dataset.context, undefined);
    await open(anchor);
    document.querySelector('#nearby .place-close').click();
    assert.ok(!document.getElementById('tableside').classList.contains('active'));
});

test('Nearby escapes names, handles errors and missing vessel metadata, and retries', async () => {
    let calls = 0;
    const open = createNearbyDialog({fetchJSON:async () => {
        if (!calls++) return {error:'<img src=x>'};
        return {...data, ships:[{...ships[0], shipname:'<img src=x>', speed:null}]};
    }, openVessel(){}});
    assert.equal(open({...anchor, lat:NaN}), undefined);
    assert.equal(calls, 0);
    await open({...anchor, title:'<img src=x>', shiptype:null, status:15});
    assert.equal(document.querySelector('.place-panel-kind').textContent, 'Vessel');
    assert.equal(document.querySelector('#nearby img'), null);
    assert.match(document.querySelector('.port-status').textContent, /<img src=x>/);
    document.querySelector('.nearby-retry').click(); await tick();
    assert.equal(document.querySelector('.col-spd:not(th)').textContent, '—');
    assert.equal(document.querySelector('#nearby .vessel-link').textContent, '<img src=x>');
    assert.equal(document.querySelector('#nearby img'), null);
});

test('a failed look keeps saying so, with its retry, whichever tab is chosen', async () => {
    const open = createNearbyDialog({fetchJSON: async () => { throw new Error('offline'); }, openVessel(){}});
    await open(anchor);
    assert.match(document.querySelector('.port-status').textContent, /Could not look around here\. offline/);
    document.querySelector('[data-nearby-tab=stations]').click();
    assert.match(document.querySelector('.port-status').textContent, /Could not look around here\. offline/);
    assert.doesNotMatch(document.querySelector('.port-status').textContent, /within 99 nm/);
    assert.ok(document.querySelector('.nearby-retry'));
});

test('an answer that comes after the table was closed is shown when it is opened again', async () => {
    let finish;
    const open = createNearbyDialog({fetchJSON: () => new Promise(resolve => { finish = resolve; }), openVessel(){}});
    const opening = open(anchor);
    await tick();
    const panel = document.querySelector('#nearby').parentElement;
    panel.classList.remove('active');
    finish(data); await opening; await tick();
    panel.classList.add('active');
    assert.equal(document.querySelector('#nearby .spinner'), null);
    assert.equal(document.querySelectorAll('#nearby tbody tr').length > 0, true);
});

test('a station row opens the station through the host', async () => {
    const stations = [];
    const open = createNearbyDialog({fetchJSON: async () => data, openVessel(){}, openStation: id => stations.push(id)});
    await open(anchor);
    document.querySelector('[data-nearby-tab=stations]').click();
    document.querySelector('#nearby tr[data-station] button, #nearby tr[data-station]').click();
    assert.deepEqual(stations, [3]);
});

test('a station anchor uses station metadata and opens the first populated category', async () => {
    await createNearbyDialog({fetchJSON:async () => ({...data, ships:[]}), openVessel(){}})({lat:1,lon:2,station:3,title:'Receiver',online:false});
    assert.equal(document.querySelector('.place-panel-kind').textContent, 'Station · Offline');
    assert.equal(document.querySelector('[aria-selected=true]').dataset.nearbyTab, 'stations');
    assert.match(document.querySelector('[data-nearby-tab=ships]').textContent, /Ships 0/);
});
