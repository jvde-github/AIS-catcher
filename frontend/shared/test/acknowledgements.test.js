import { test } from 'node:test';
import assert from 'node:assert/strict';
import { acknowledgedMessages } from '../acknowledgements.js';
import { setupDom } from './dom.js';
setupDom('');
const binary = await import('../binary.js');
const sent = (extra = {}) => ({ type: 12, sender: 111, anchor: 222, timestamp: 100, first: 100, count: 1, message: { mmsi: 111, dest_mmsi: 222, seqno: 0, text: 'TEST, PLS ACK' }, ...extra });
const ack = (extra = {}) => ({ type: 13, sender: 222, anchor: 111, timestamp: 105, count: 1, message: { mmsi: 222, dest_mmsi: 111, ack_seqno: 0 }, ...extra });
test('ack matches reversed MMSIs and type including sequence zero', () => {
    for (const [type, ackType] of [[12, 13], [6, 7]]) {
        const m = sent({ type });
        assert.deepEqual([...acknowledgedMessages([m, ack({ type: ackType })])], [m]);
    }
});
test('ack rejects wrong party, type, sequence, time and missing fields', () => {
    for (const a of [ack({ type: 7 }), ack({ sender: 333 }), ack({ anchor: 444 }), ack({ timestamp: 99 }), ack({ timestamp: 221 }), ack({ timestamp: NaN }), ack({ message: { ack_seqno: 1 } }), ack({ message: {} })])
        assert.equal(acknowledgedMessages([sent(), a]).size, 0);
    assert.equal(acknowledgedMessages([sent({ message: {} }), ack()]).size, 0);
});
test('reused sequences and aggregated sends remain unconfirmed', () => {
    assert.equal(acknowledgedMessages([sent(), sent({ timestamp: 102 }), ack()]).size, 0);
    assert.equal(acknowledgedMessages([sent({ count: 2 }), ack()]).size, 0);
    const m = sent();
    assert.equal(acknowledgedMessages([m, ack(), ack({ timestamp: 106 })]).size, 1);
});
test('matches across Sent/Received tabs with accessible reception check', () => {
    const messages = [sent(), ack()].map(binary.decorate);
    document.body.innerHTML = binary.getBinaryMessageTabs(messages, 111);
    const pane = document.querySelector('[data-pane="sent"]');
    assert.equal(pane.querySelectorAll('.msg-acknowledged').length, 1);
    assert.match(pane.querySelector('.msg-acknowledged').title, /does not indicate that a person read it/);
    assert.match(document.querySelector('[data-pane="received"]').textContent, /Safety message acknowledgement/);
    document.body.innerHTML = binary.getBinaryMessageTabs(messages, 222);
    assert.equal(document.querySelector('[data-pane="received"] .msg-acknowledged').textContent, '✓');
});

test('binary acknowledgments keep their own detail category without contributing ship glyphs', () => {
    const receipt = binary.decorate(ack({ type: 7 }));
    const message = binary.decorate(sent({ type: 6 }));
    assert.equal(receipt.cat, 'ack');
    assert.equal(binary.KIND_LABEL.ack, 'Acknowledgments');
    assert.deepEqual(binary.kindsOf([receipt]), []);
    assert.deepEqual(binary.kindsOf([message, receipt]), ['text']);
    assert.equal(binary.BINARY_CATEGORIES.includes('ack'), false);
    assert.equal(binary.categoryOf(ack()), 'safety');
    document.body.innerHTML = binary.getBinaryMessageTabs([message, receipt], 111);
    assert.match(document.querySelector('[data-pane="received"]').textContent, /Binary message acknowledgement/);
    assert.equal(document.querySelector('[data-pane="sent"] .msg-acknowledged').textContent, '✓');
});

test('binary acknowledgments remain in cached ship hover previews without adding glyphs', async () => {
    const { create } = await import('../mapobjects.js');
    let fetched = 0;
    const messages = [sent({ type: 6 }), ack({ type: 7 })];
    const objects = create({
        options: () => ({}), shipLabel: id => String(id),
        shipMessagesUrl: id => 'messages/' + id,
        fetchJSON: async () => { fetched++; return { messages }; },
    });
    const ship = {mmsi:111, binary:1};
    await objects.hydrateShip(ship.mmsi, ship.binary);
    const html = objects.shipTooltip(ship);
    assert.match(html, /Binary message acknowledgement/);
    assert.deepEqual(await objects.shipKinds(ship), ['text']);
    assert.equal(fetched, 1);
});

test('opening message details refreshes acknowledgments even when the badge stays unchanged', async () => {
    const { create } = await import('../mapobjects.js');
    let fetched = 0, messages = [sent({ type: 6 })];
    const objects = create({
        options: () => ({}), shipLabel: id => String(id), ship: () => null,
        shipMessagesUrl: id => 'messages/' + id,
        fetchJSON: async () => { fetched++; return { messages }; },
    });
    await objects.hydrateShip(111, 1);
    messages = [...messages, ack({ type: 7 })];
    await objects.hydrateShip(111, 1); // ordinary hover still reuses the cache
    assert.equal(fetched, 1);
    objects.showVesselMessages(111, 1);
    await new Promise(resolve => setTimeout(resolve, 0));
    assert.equal(fetched, 2);
    const dialog = document.getElementById('binary-messages');
    assert.match(dialog.textContent, /Binary message acknowledgement/);
    assert.equal(dialog.querySelector('[data-pane="sent"] .msg-acknowledged').textContent, '✓');
});
