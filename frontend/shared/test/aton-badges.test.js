import { test } from 'node:test';
import assert from 'node:assert/strict';
import { setupDom } from './dom.js';
setupDom('');
const binary = await import('../binary.js');
const { create } = await import('../mapobjects.js');
const status = () => ({type:6, sender:111, timestamp:200, count:1,
    message:{mmsi:111, dac:235, fid:10, asm_light_status:1}});
const text = () => ({type:8, sender:111, timestamp:100, count:1,
    message:{mmsi:111, text:'Report'}});

test('AtoN status stays in message details and the map category without contributing ship glyphs', () => {
    const aton = binary.decorate(status());
    assert.equal(aton.cat, 'aton');
    assert.deepEqual(binary.kindsOf([aton]), []);
    assert.deepEqual(binary.kindsOf([binary.decorate(text()), aton]), ['text']);
    assert.ok(binary.BINARY_CATEGORIES.includes('aton'));
    assert.match(binary.getBinaryMessageList([aton]), /AtoN Status/);
    assert.match(binary.tooltipSections([aton]), /AtoN Status/);
});

test('AtoN status appears in a cached ship hover without contributing a badge', async () => {
    let fetched = 0;
    const objects = create({options:()=>({}), shipLabel:id=>String(id),
        shipMessagesUrl:id=>'messages/'+id,
        fetchJSON:async()=>{fetched++; return {messages:[text(),status()]};}});
    await objects.hydrateShip(111, 1);
    const html = objects.shipTooltip({mmsi:111, binary:1});
    assert.match(html, /AtoN Status/);
    assert.deepEqual(await objects.shipKinds({mmsi:111, binary:1}), ['text']);
    assert.equal(fetched, 1);
});
