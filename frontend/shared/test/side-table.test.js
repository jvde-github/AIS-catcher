import { test, afterEach } from 'node:test';
import assert from 'node:assert/strict';
import { setupDom } from './dom.js';
import { createPanelSwitch } from '../side-table.js';
import { createNearbyDialog } from '../nearby-dialog.js';
import { createPortDialog } from '../port-dialog.js';

let layout;
afterEach(() => {
    layout?.destroy();
    document.querySelectorAll('.side-table-context').forEach(root => root._dispose?.());
});

function setup(width) {
    setupDom('<main id="workspace"><div id="card"></div><aside id="tableside" class="tableside_window"><button id="vessel">Vessel in view</button></aside></main>');
    const workspace = document.getElementById('workspace');
    const card = document.getElementById('card');
    const table = document.getElementById('tableside');
    Object.defineProperty(workspace, 'clientWidth', { get: () => width });
    let resized;
    globalThis.ResizeObserver = class {
        constructor(callback) { this.callback = callback; }
        observe(element) { if (element === workspace) resized = this.callback; }
        disconnect() {}
    };
    const setTableOpen = on => {
        if (on) layout.opening('table');
        table.classList.toggle('active', on);
        document.body.classList.toggle('table-open', on);
    };
    const openVessel = () => {
        layout.opening('card');
        card.classList.add('visible');
    };
    layout = createPanelSwitch({
        workspace,
        card: { isOpen: () => card.classList.contains('visible'), close: () => card.classList.remove('visible') },
        table: { isOpen: () => table.classList.contains('active'), close: () => setTableOpen(false) },
    });
    document.getElementById('vessel').onclick = openVessel;
    return {
        card, table, openVessel, setTableOpen,
        resize: next => { width = next; resized(); },
        host: {
            setTableOpen, openVessel,
            fetchJSON: async () => ({ ships: [{mmsi:123456789, shipname:'Test vessel'}], total:1, stations:[], places:[] }),
        },
    };
}

test('a full-width port or Nearby table replaces the card, and selecting a vessel closes the table', async () => {
    const ui = setup(750);
    const openers = [
        () => createPortDialog(ui.host)([{code:'NLRTM', label:'Rotterdam'}]),
        () => createNearbyDialog(ui.host)({lat:52, lon:4, title:'Nearby'}),
    ];
    for (const open of openers) {
        ui.openVessel();
        await open();
        assert.equal(ui.card.classList.contains('visible'), false);
        assert.equal(ui.table.classList.contains('active'), true);
        const context = ui.table.dataset.context;
        ui.table.querySelector('.side-table-context:not(.hidden) .vessel-link').click();
        assert.equal(ui.card.classList.contains('visible'), true);
        assert.equal(ui.table.classList.contains('active'), false);
        assert.equal(document.body.classList.contains('table-open'), false);
        assert.equal(ui.table.dataset.context, context);
    }
    // The overview uses the same transition and retains its existing DOM.
    ui.table.querySelector('#nearby .place-overview').click();
    assert.equal(ui.card.classList.contains('visible'), false);
    document.getElementById('vessel').click();
    assert.equal(ui.card.classList.contains('visible'), true);
    assert.equal(ui.table.classList.contains('active'), false);
});

test('a workspace with room for both keeps the card and each side table open', async () => {
    const ui = setup(751);
    ui.openVessel();
    await createPortDialog(ui.host)([{code:'NLRTM'}]);
    assert.equal(ui.card.classList.contains('visible'), true);
    await createNearbyDialog(ui.host)({lat:52, lon:4});
    ui.table.querySelector('#nearby .vessel-link').click();
    assert.equal(ui.card.classList.contains('visible'), true);
    assert.equal(ui.table.classList.contains('active'), true);
});

test('shrinking the workspace keeps the last opened surface, and growing it does not reopen either', () => {
    const ui = setup(1200);
    ui.openVessel();
    ui.setTableOpen(true);
    ui.resize(700); // The window stays wide; the workspace is what counts.
    assert.equal(ui.card.classList.contains('visible'), false);
    assert.equal(ui.table.classList.contains('active'), true);
    ui.resize(1200);
    assert.equal(ui.card.classList.contains('visible'), false);
    ui.openVessel();
    ui.resize(700);
    assert.equal(ui.card.classList.contains('visible'), true);
    assert.equal(ui.table.classList.contains('active'), false);
    ui.resize(1200);
    assert.equal(ui.table.classList.contains('active'), false);
});
