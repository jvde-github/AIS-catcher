import { test } from 'node:test';
import assert from 'node:assert/strict';
import { setupDom } from './dom.js';
import { renderStationActivity } from '@aiscatcher/ui/station-activity.js';

setupDom('<div id="host"></div>');
const slots = (host) => [...host.querySelectorAll('.activity-slots i')];

test("the last day is one strip of the slots that started in the last 24 hours, the viewer's an hour each", () => {
    const host = document.getElementById('host');
    const now = Date.parse('2026-10-23T10:30:00Z');
    const hourly = (fill) => Array.from({ length: 24 }, (_, h) => fill(h));
    renderStationActivity(host, { '2026-10-22': hourly(() => 'no_data'), '2026-10-23': hourly(h => (h < 9 ? 'no_data' : h === 9 ? 'connected' : 'active')) },
        { lastDay: true, now, states: ['active', 'connected', 'not_active', 'no_data'] });
    const s = slots(host);
    assert.equal(s.length, 24);
    assert.equal(s.at(-1).getAttribute('aria-label'), '2026-10-23 10:00 UTC · Receiving');
    assert.equal(s.at(-2).className, 'quiet');
    assert.deepEqual([...host.querySelectorAll('.activity-legend span')].map(x => x.textContent), ['Receiving', 'Connected', 'Not active', 'No data']);
});

test("the hub's twelve slots a day still draw, the week as seven days", () => {
    const host = document.getElementById('host');
    const now = Date.parse('2026-10-23T10:30:00Z');
    renderStationActivity(host, { '2026-10-23': Array(12).fill('active') }, { fillWeek: true, now });
    assert.equal(host.querySelectorAll('.activity-day').length, 7);
    assert.equal(slots(host).length, 7 * 12);
});
