import { test } from 'node:test';
import assert from 'node:assert/strict';
import { setupDom } from './dom.js';
import { build } from '../card-tabs.js';
import { build as stationTabs } from '../stationcard-tabs.js';

test('shared tabs navigate compact cards and keep entity handles independent', () => {
    const win = setupDom('<aside class="mapcard"><div id="ship" data-context-type="ship"></div><div id="plane" data-context-type="plane"></div><footer class="mapcard-footer"></footer></aside>');
    const options = { tabs: [['summary', 'Summary'], ['details', 'Details']], tab: null, foldable: () => true };
    const ship = build(document.getElementById('ship'), 's_', options);
    const plane = build(document.getElementById('plane'), 'p_', options);
    assert.equal(document.querySelectorAll('.sc-drag-handle').length, 2);
    assert.equal(ship.head.hidden, false);
    assert.equal(ship.panels.summary.hidden, true);
    document.getElementById('s_tab_summary').click();
    assert.equal(ship.panels.summary.hidden, false);
    document.getElementById('s_tab_summary').click();
    assert.equal(ship.active(), 'summary');
    document.getElementById('s_tab_summary').dispatchEvent(new win.KeyboardEvent('keydown', { key: 'ArrowRight', bubbles: true }));
    assert.equal(ship.active(), 'details');
    document.getElementById('s_tab_details').click();
    assert.equal(ship.active(), 'details');
    assert.equal(ship.head.hidden, true);
    assert.equal(document.activeElement.id, 's_tab_details');
    document.querySelector('[aria-controls="s_panels"]').click();
    assert.equal(ship.active(), null);
    assert.equal(plane.active(), null);
    ship.destroy();
    assert.equal(document.querySelectorAll('.sc-drag-handle').length, 1);
    plane.destroy();
});

test('station tabs preserve live fields and charts when restoring classic', () => {
    setupDom('<aside class="mapcard"><div id="station"><div class="card-section" data-section="stats"></div><div class="mapcard-content-row card-row" data-section="stats"><div><span>ID</span><span id="id">2820</span></div></div><div data-section="stats" id="stats"></div><div data-section="activity" id="chart"></div><div data-section="msgrate" id="rate"></div></div><footer class="mapcard-footer"></footer></aside>');
    const mount = document.getElementById('station');
    const originals = [...mount.children];
    const value = document.getElementById('id');
    const chart = document.getElementById('chart');
    const shell = stationTabs(mount, 'station_', { tab: null, foldable: () => true });
    assert.equal(shell.head.contains(value), true);
    assert.equal(shell.panels.activity.contains(chart), true);
    value.textContent = '2821';
    chart.textContent = 'updated chart';
    shell.destroy();
    assert.deepEqual([...mount.children], originals);
    assert.equal(document.getElementById('id'), value);
    assert.equal(value.textContent, '2821');
    assert.equal(chart.textContent, 'updated chart');
    assert.equal(document.querySelectorAll('.sc-drag-handle').length, 0);
});

test('a tab choice supersedes a pending drag animation', async () => {
    setupDom('<aside class="mapcard"><div id="body"></div><footer class="mapcard-footer"></footer></aside>');
    const shell = build(document.getElementById('body'), 'drag_', {
        tabs: [['summary', 'Summary'], ['details', 'Details']], tab: null, foldable: () => true,
    });
    const handle = document.querySelector('.sc-drag-handle');
    const panels = document.getElementById('drag_panels');
    panels.getBoundingClientRect = () => ({ height: 100 });
    handle.setPointerCapture = () => {};
    handle.hasPointerCapture = () => false;
    const pointer = (type, y) => {
        const event = new Event(type);
        Object.assign(event, { pointerId: 1, isPrimary: true, button: 0, clientY: y });
        handle.dispatchEvent(event);
    };
    pointer('pointerdown', 10);
    pointer('pointermove', 100);
    pointer('pointerup', 100);
    document.getElementById('drag_tab_details').click();
    await new Promise(resolve => setTimeout(resolve, 200));
    assert.equal(shell.active(), 'details');
    assert.equal(panels.style.height, '');
    assert.equal(panels.style.overflow, '');
    shell.destroy();
    assert.equal(document.getElementById('body').classList.contains('sc-folded'), false);
});

test('Summary and detail metrics share missing and zero-value formatting', async () => {
    setupDom('<aside class="mapcard"><div id="vessel"></div><footer class="mapcard-footer"></footer></aside>');
    const { build: shipTabs } = await import('../shipcard-tabs.js');
    const { populate } = await import('../shipcard.js');
    const shell = shipTabs(document.getElementById('vessel'), 'test_', { tab: 'summary' });
    const helpers = { units: { getShipDimension: () => '', getSpeedVal: String, getSpeedUnit: () => 'kts' } };
    populate(shell.cells, {}, helpers);
    for (const key of ['speed', 'cog', 'heading']) {
        assert.deepEqual(shell.cells[key].map(cell => cell.textContent), ['-', '-']);
    }
    assert.equal(shell.cells.class.textContent, '-');
    populate(shell.cells, { speed: 0, cog: 0, heading: 0 }, helpers);
    for (const [key, expected] of [['speed', '0 kts'], ['cog', '0.0°'], ['heading', '0°']]) {
        assert.deepEqual(shell.cells[key].map(cell => cell.textContent), [expected, expected]);
    }
    shell.destroy();
});

test('content touch dragging leaves scrolling and controls alone', async () => {
    setupDom('<aside class="mapcard"><div id="content"></div><footer class="mapcard-footer"></footer></aside>');
    const shell = build(document.getElementById('content'), 'content_', {
        tabs: [['summary', 'Summary'], ['details', 'Details']], tab: null, foldable: () => true,
    });
    shell.head.textContent = 'Metrics';
    const panels = document.getElementById('content_panels');
    panels.getBoundingClientRect = () => ({ height: 100 });
    Object.defineProperties(panels, { clientHeight: { value: 100 }, scrollHeight: { value: 300 } });
    const touch = (target, type, y) => {
        const event = new Event(type, { bubbles: true, cancelable: true });
        event.touches = type === 'touchend' ? [] : [{ identifier: 1, clientX: 10, clientY: y }];
        target.dispatchEvent(event);
        return event;
    };
    touch(shell.head, 'touchstart', 10);
    assert.equal(touch(shell.head, 'touchmove', 90).defaultPrevented, true);
    touch(shell.head, 'touchend', 90);
    await new Promise(resolve => setTimeout(resolve, 200));
    assert.equal(shell.active(), 'summary');
    touch(panels, 'touchstart', 90);
    assert.equal(touch(panels, 'touchmove', 10).defaultPrevented, false);
    touch(panels, 'touchend', 10);
    assert.equal(shell.active(), 'summary');
    const link = document.createElement('a');
    link.href = '#example';
    shell.panels.summary.appendChild(link);
    panels.scrollTop = 200;
    touch(link, 'touchstart', 90);
    assert.equal(touch(link, 'touchmove', 10).defaultPrevented, false);
    touch(link, 'touchend', 10);
    touch(panels, 'touchstart', 90);
    assert.equal(touch(panels, 'touchmove', 10).defaultPrevented, true);
    touch(panels, 'touchend', 10);
    await new Promise(resolve => setTimeout(resolve, 200));
    assert.equal(shell.active(), null);
    shell.destroy();
});
