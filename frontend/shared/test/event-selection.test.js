import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createEventSelection } from '../core/event-selection.js';

function host(resolveVessel) {
    const calls = [];
    return { calls, noteSeen: m => calls.push(['seen', m]), resolveVessel,
        openVessel: (m, vessel) => calls.push(['open', m, vessel]),
        navigate: (position, zoom) => calls.push(['navigate', position, zoom]) };
}

test('ticker selection: both local and remote resolution use the same sequence', async () => {
    const vessel = { lat: 52, lon: 4, name: 'EXAMPLE' };
    for (const resolve of [() => vessel, async () => vessel]) {
        const h = host(resolve);
        await createEventSelection(h)({ id: '244123456', lat: 51, lon: 5 });
        assert.deepEqual(h.calls, [['seen', 244123456], ['open', 244123456, vessel], ['navigate', { lat: 52, lon: 4 }, 14]]);
    }
});

test('ticker selection: missing or failed lookup falls back to event coordinates', async () => {
    for (const resolve of [() => null, async () => { throw new Error('offline'); }]) {
        const h = host(resolve);
        await createEventSelection(h)({ id: 244123456, lat: 51, lon: 5 });
        assert.deepEqual(h.calls, [['seen', 244123456], ['navigate', { lat: 51, lon: 5 }, 14]]);
    }
});

test('ticker selection: a known vessel without position opens and uses event position', async () => {
    const vessel = { name: 'EXAMPLE' }, h = host(() => vessel);
    await createEventSelection(h)({ id: 42, lat: 51, lon: 5 });
    assert.deepEqual(h.calls, [['seen', 42], ['open', 42, vessel], ['navigate', { lat: 51, lon: 5 }, 14]]);
});

test('ticker selection: unknown position does not move the map', async () => {
    const h = host(() => null);
    await createEventSelection(h)({ id: 42 });
    assert.deepEqual(h.calls, [['seen', 42]]);
});

test('ticker selection: an older lookup cannot override a newer click', async () => {
    let finish;
    const h = host(m => m === 1 ? new Promise(resolve => { finish = resolve; }) : { lat: 52, lon: 4 });
    const select = createEventSelection(h);
    const first = select({ id: 1 });
    await select({ id: 2 });
    finish({ lat: 40, lon: 10 });
    await first;
    assert.deepEqual(h.calls.map(c => c.slice(0, 2)), [['seen', 1], ['seen', 2], ['open', 2], ['navigate', { lat: 52, lon: 4 }]]);
});
