import {test} from 'node:test';
import assert from 'node:assert/strict';
import * as filter from '../../src/core/filter.js';
test('obsolete saved place selection does not hide ships', () => {
    filter.reset(); filter.set('place', 'old-place');
    assert(!filter.isActive());
    assert(filter.shipPasses({}));
    filter.set('speed_min', 3);
    assert(!filter.shipPasses({speed:1}));
    filter.reset();
});
