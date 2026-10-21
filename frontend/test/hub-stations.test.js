import { test } from 'node:test';
import assert from 'node:assert/strict';
import { stationsOf, stationName, stationLabel } from '../control/js/stations.js';

test('the hub names a station by its name, else its number; the default station says so among others', () => {
    assert.equal(stationName({ id: 0 }), 'Default station');
    assert.equal(stationName({ id: 7, name: '  ' }), 'Station 7');
    assert.equal(stationName({ id: 7, name: 'Mast' }), 'Mast');
    assert.equal(stationLabel({ id: 0, name: 'Home' }, true), 'Home (default)');
    assert.equal(stationLabel({ id: 0, name: 'Home' }, false), 'Home');
    assert.equal(stationLabel({ id: 7 }, true), 'Station 7');
});

test('the stations of a configuration: the default station first, there when the file has none', () => {
    assert.deepEqual(stationsOf({ stations: [{ id: 7 }, { id: 0, name: 'Home' }] }), [{ id: 0, name: 'Home' }, { id: 7 }]);
    assert.deepEqual(stationsOf({}), [{ id: 0 }]);
    assert.deepEqual(stationsOf(null), [{ id: 0 }]);
});
