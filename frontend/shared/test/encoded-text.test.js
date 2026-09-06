import { test } from 'node:test';
import assert from 'node:assert/strict';
import { setupDom } from './dom.js';
import { decodeHTMLEntities } from '../components.js';
import { build } from '../shipcard-tabs.js';
import { populate } from '../shipcard.js';

test('encoded names decode once and remain text, including HTML-like input', () => {
    setupDom();
    const cell = document.createElement('span');
    for (const [input, expected] of [
        ['KX1T&nbsp;Trenton&nbsp;East-', 'KX1T\u00a0Trenton\u00a0East-'],
        ['BE ANR&gt;CI ABJ', 'BE ANR>CI ABJ'],
        ['A &amp; B &#039; &#96;', "A & B ' `"],
        ['&amp;gt;', '&gt;'],
        ['&lt;img src=x onerror=alert(1)&gt;', '<img src=x onerror=alert(1)>'],
        ['</textarea><img src=x onerror=alert(1)>', '</textarea><img src=x onerror=alert(1)>'],
        [null, ''],
    ]) {
        cell.textContent = decodeHTMLEntities(input);
        assert.equal(cell.textContent, expected);
        assert.equal(cell.children.length, 0);
    }
});

test('summary and voyage destinations display the same decoded text', () => {
    setupDom('<aside class="mapcard"><div id="vessel"></div></aside>');
    const mount = document.getElementById('vessel');
    const shell = build(mount, 'encoded_', { tab: 'summary' });
    const helpers = { units: { getShipDimension: () => '' } };
    for (const [destination, expected] of [
        ['BE ANR&gt;CI ABJ', 'BE ANR>CI ABJ'],
        ['&lt;img src=x onerror=alert(1)&gt;', '<img src=x onerror=alert(1)>'],
    ]) {
        const ship = { destination, callsign: 'A&amp;B' };
        populate(shell.cells, ship, helpers);
        shell.update(ship, helpers);
        assert.equal(shell.cells.destination.textContent, expected);
        assert.equal(mount.querySelector('.sc-sum-dest').textContent, expected);
        assert.equal(shell.cells.callsign.textContent, 'A&B');
        assert.equal(mount.querySelector('img'), null);
    }
    shell.destroy();
});
