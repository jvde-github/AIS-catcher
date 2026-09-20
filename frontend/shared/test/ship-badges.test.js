import { test } from 'node:test';
import assert from 'node:assert/strict';
import { setupDom } from './dom.js';
setupDom('');
const binary = await import('../binary.js');
const { create } = await import('../mapobjects.js');

test('badge suppression preserves the existing count and age layout', () => {
    for (let kind = 0; kind < 8; kind++) {
        for (let age = 0; age < 3; age++) {
            const word = 15 | (kind << 4) | (age << 7);
            assert.deepEqual(binary.decodeBadge(word), {count:15, kind, age});
            assert.equal(binary.showsShipBadge(word), kind !== 3);
            assert.deepEqual(binary.decodeBadge(word | 512), {count:15, kind:9, age});
            assert.equal(binary.showsShipBadge(word | 512), false);
        }
    }
    assert.equal(binary.showsShipBadge(0), false);
    assert.equal(binary.KIND_CAT[9], 'ack');
});

for (const [word, message, label] of [
    [2 | (3 << 4), {type:6, sender:111, timestamp:100, count:1,
        message:{mmsi:111, dac:235, fid:10, asm_light_status:1}}, /AtoN Status/],
    [2 | 512, {type:7, sender:111, anchor:222, timestamp:100, count:1,
        message:{mmsi:111, dest_mmsi:222, ack_seqno:0}}, /Binary message acknowledgement/],
]) {
    test(`summary ${word} keeps hover details without map or title badges`, async () => {
        let fetched = 0, rehovered = 0;
        const objects = create({options:()=>({}), shipLabel:String,
            shipMessagesUrl:id=>'messages/'+id,
            isHoveringShip:()=>true, rehoverShip:()=>{rehovered++;},
            fetchJSON:async()=>{fetched++; return {messages:[message]};}});
        const ship = {mmsi:111, binary:word, lat:52, lon:4};
        objects.shipBadge(ship);
        assert.equal(objects.vector.getFeatures().length, 0);
        assert.deepEqual(await objects.shipKinds(ship), []);
        let title;
        objects.titleGlyphs(ship, '', html=>{title=html;});
        assert.equal(title, '');
        assert.equal(fetched, 0);
        assert.ok(objects.shipTooltip(ship));
        assert.ok(objects.shipTooltip(ship));
        assert.equal(fetched, 0); // the existing dwell still precedes the lookup
        await new Promise(resolve=>setTimeout(resolve, 550));
        assert.equal(fetched, 1); // repeated hovers share the existing request/cache
        assert.ok(rehovered);
        assert.match(objects.shipTooltip(ship), label);
        assert.equal(objects.shipTooltip({mmsi:222, binary:0}), '');
        assert.equal(fetched, 1);
        objects.shipBadge({...ship, binary:1});
        assert.equal(objects.vector.getFeatures().length, 1);
    });
}
