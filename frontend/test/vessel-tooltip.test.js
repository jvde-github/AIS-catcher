import { test } from 'node:test';
import assert from 'node:assert/strict';
import { vesselBand } from '@aiscatcher/ui/vessel-tooltip.js';

const names = { 3: 'Harwich', 5: 'Felixstowe', 7: 'Norfolk', 9: '<b>Dover</b>' };
const label = (id) => names[id];
const boxes = (html) => [...html.matchAll(/<span class="station-box[^"]*"[^>]*>([^<]*)<\/span>/g)].map((m) => m[1]);

test('the band names the vessel, its speed, type and age, and the stations that heard it, latest first', () => {
    const html = vesselBand({ country: 'NL', validated: 1, shiptype: 70, stations: [5, 3] },
        { name: 'MAERSK', speed: '12.3 kn', age: 75, label });
    assert.match(html, /<span class="tooltip-name" translate="no">MAERSK<\/span><span class="tooltip-dim"> at <\/span>12\.3 kn/);
    assert.match(html, /<div class="tooltip-sub">Cargo - received 1m 15s ago by <span class="station-box station-last"/);
    assert.deepEqual(boxes(html), ['Felixstowe', 'Harwich']);
    assert.match(html, /<span class="station-box station-last" title="Heard it last">Felixstowe<\/span>/);   // the latest is marked
});

test('more stations than fit end in a dotted box, and their names are escaped', () => {
    const html = vesselBand({ stations: [9, 7, 5, 3] }, { name: 'X', speed: null, age: 0, label });
    assert.deepEqual(boxes(html), ['&lt;b&gt;Dover&lt;/b&gt;', 'Norfolk', 'Felixstowe', '···']);
});

test('without a station label, a speed or an age the band leaves those parts out', () => {
    const html = vesselBand({ stations: [5, 3] }, { name: '244000001', speed: null, age: null, label: null });
    assert.ok(!html.includes('station-box'));
    assert.ok(!html.includes(' at '));
    assert.ok(!html.includes('tooltip-sub'));
});
