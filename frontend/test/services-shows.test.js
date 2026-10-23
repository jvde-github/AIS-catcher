import { test } from 'node:test';
import assert from 'node:assert/strict';
import { setupDom } from './dom.js';

setupDom('<div id="host"></div>');
const { renderShows } = await import('../control/js/services-editor.js');

const stations = [{ key: 3, label: 'Harwich' }, { key: 5, label: 'Orford' }];
const draw = (data, opts) => {
    const host = document.createElement('div');
    renderShows(host, data, { stations, touched: () => {}, rerender: () => draw(data, opts), ...opts });
    return host;
};
const click = (host, text) => [...host.querySelectorAll('button')].find(b => b.textContent === text).click();

test("a server's chosen stations are its station_id, so its live streams keep to them too", () => {
    const data = { port: 8100, view: { stations: [3], seen: 30 } };
    let host = draw(data, { byStation: true });
    // a view's stations from before show as chosen
    assert.equal(host.querySelector('[aria-checked="true"]').textContent, 'Chosen stations');
    click(host, 'Orford');
    assert.deepEqual(data.station_id, [3, 5]);
    assert.deepEqual(data.view, { seen: 30 });
    host = draw(data, { byStation: true });
    click(host, 'All stations');
    assert.equal(data.station_id, undefined);
});

test("a webhook's chosen stations stay its view's", () => {
    const data = { url: 'https://example.org/h' };
    const host = draw(data, {});
    click(host, 'Chosen stations');
    assert.deepEqual(data.view, { stations: [3, 5] });
    assert.equal(data.station_id, undefined);
});
