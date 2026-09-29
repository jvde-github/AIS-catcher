import test from 'node:test';
import assert from 'node:assert/strict';
import {importPlace} from '../places.js';

const PORT = '11111111-1111-4111-8111-111111111111', TERMINAL = '22222222-2222-4222-8222-222222222222';

test('a schema 3 place comes through as it is', () => {
    const p = {schema_version: 3, revision: 4, name: 'Kade 1', place_type: 'berth', part_of: PORT, category: 'quay',
        rank: 1, attributes: {depth: 12}, references: [{title: 'x'}]};
    const {properties, notes} = importPlace(p);
    assert.deepEqual(properties, p);
    assert.deepEqual(notes, []);
});

test('the first layout: code, port by code, details, custom', () => {
    const port = importPlace({schema_version: 1, revision: 2, name: 'Portville', place_type: 'port', unlocode: 'NLPVL', size: 2}).properties;
    assert.deepEqual(port, {schema_version: 3, revision: 2, name: 'Portville', place_type: 'port', codes: {unlocode: ['NLPVL']}, rank: 2});

    const berth = importPlace({schema_version: 1, revision: 1, name: 'Kade', place_type: 'berth', port_unlocode: 'NLPVL',
        details: {source: 'osm', depth: 12, code: 'K1'}}, {NLPVL: PORT});
    assert.deepEqual(berth.properties, {schema_version: 3, revision: 1, name: 'Kade', place_type: 'berth', part_of: PORT,
        codes: {own: ['local:K1']}, attributes: {depth: 12}, geometry_source: {source: 'osm'}});
    assert.deepEqual(berth.notes, []);

    const lost = importPlace({schema_version: 1, name: 'Kade', place_type: 'berth', port_unlocode: 'NLXXX'});
    assert.equal(lost.properties.part_of, undefined);
    assert.match(lost.notes[0], /NLXXX is not in this catalogue/);

    const inTerminal = importPlace({schema_version: 1, name: 'Kade', place_type: 'berth', port_unlocode: 'NLPVL',
        details: {terminal: TERMINAL}}, {NLPVL: PORT}).properties;
    assert.equal(inTerminal.part_of, TERMINAL);
    assert.equal(inTerminal.attributes, undefined);

    const custom = importPlace({schema_version: 1, name: 'Vaarweg', place_type: 'custom', category: 'Canal'}).properties;
    assert.deepEqual(custom, {schema_version: 3, name: 'Vaarweg', place_type: 'guardzone', category: 'canal'});

    // a code the register does not know is kept, but not as a UN/LOCODE
    const invented = importPlace({schema_version: 1, name: 'Haven', place_type: 'port', unlocode: 'haven-1'}).properties;
    assert.deepEqual(invented.codes, {own: ['local:haven-1']});
});

test('the second layout: area, subtype, size', () => {
    const {properties} = importPlace({schema_version: 2, revision: 3, name: 'Strait', place_type: 'area', size: 1,
        attributes: {area_subtype: 'waterway', notes: 'x'}, no: 7});
    assert.deepEqual(properties, {schema_version: 3, revision: 3, name: 'Strait', place_type: 'guardzone', rank: 1,
        attributes: {notes: 'x'}, no: 7, category: 'waterway'});
    // an explicit "no parent" is kept, a broken parent is not
    assert.equal(importPlace({name: 'A', place_type: 'berth', part_of: null}).properties.part_of, null);
    assert.equal('part_of' in importPlace({name: 'A', place_type: 'berth', part_of: 'Rotterdam'}).properties, false);
});

test('a Feature from elsewhere becomes a guard zone with what it has', () => {
    const foreign = importPlace({name: 'Windpark', kind: 'restricted', fill: '#f00'});
    assert.deepEqual(foreign.properties, {name: 'Windpark', kind: 'restricted', fill: '#f00', place_type: 'guardzone', schema_version: 3});
    assert.deepEqual(foreign.notes, []);

    const unknown = importPlace({name: 'Pier', place_type: 'jetty', rank: 9, codes: {unlocode: ['NLRTM']}, redirect_to: 'x'});
    assert.equal(unknown.properties.place_type, 'guardzone');
    assert.equal('rank' in unknown.properties, false);
    assert.equal('codes' in unknown.properties, false);
    assert.equal('redirect_to' in unknown.properties, false);
    assert.equal(unknown.notes.length, 2);

    assert.deepEqual(importPlace(undefined).properties, {name: 'Imported place', place_type: 'guardzone', schema_version: 3});
});
