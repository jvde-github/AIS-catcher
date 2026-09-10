import { test } from 'node:test';
import assert from 'node:assert/strict';
import { setupDom } from './dom.js';
import { beginLoading } from '../loading.js';

test('loading stays quiet for quick requests and clears after slow ones', t => {
    setupDom('<div id="surface"><header></header></div>');
    t.mock.timers.enable({ apis: ['setTimeout'] });
    const target = document.getElementById('surface');
    const mount = target.querySelector('header');
    const quick = beginLoading(target, 'Loading vessel', mount);
    assert.equal(target.getAttribute('aria-busy'), 'true');
    t.mock.timers.tick(399);
    assert.equal(target.querySelector('.spinner'), null);
    quick();
    t.mock.timers.tick(1);
    assert.equal(target.querySelector('.spinner'), null);
    const slow = beginLoading(target, 'Loading vessel', mount);
    t.mock.timers.tick(400);
    assert.equal(mount.querySelector('.spinner').getAttribute('aria-label'), 'Loading vessel');
    slow();
    assert.equal(target.querySelector('.spinner'), null);
    assert.equal(target.getAttribute('aria-busy'), 'false');
});

test('a superseded request cannot hide the current spinner', t => {
    setupDom('<div id="surface"></div>');
    t.mock.timers.enable({ apis: ['setTimeout'] });
    const target = document.getElementById('surface');
    const old = beginLoading(target);
    t.mock.timers.tick(400);
    const current = beginLoading(target);
    t.mock.timers.tick(400);
    old();
    assert.equal(target.querySelectorAll('.spinner').length, 1);
    assert.equal(target.getAttribute('aria-busy'), 'true');
    current();
    t.mock.timers.tick(1000);
    assert.equal(target.querySelector('.spinner'), null);
});
