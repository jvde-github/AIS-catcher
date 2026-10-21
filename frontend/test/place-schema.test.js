import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { KINDS, importPlace } from '@aiscatcher/core/places.js';

const read = (path) => readFileSync(new URL(path, import.meta.url), 'utf8');
const schema = JSON.parse(read('../packages/core/place.schema.json'));

// The part of JSON Schema the place schema uses, and nothing more.
function errors(value, s, path = '') {
    if (s.$ref) return errors(value, s.$ref.split('/').slice(1).reduce((o, k) => o[k], schema), path);
    const out = [];
    const typeOf = (v) => v === null ? 'null' : Array.isArray(v) ? 'array' : Number.isInteger(v) ? 'integer' : typeof v;
    if ('const' in s && value !== s.const) out.push(`${path}: not ${JSON.stringify(s.const)}`);
    if (s.enum && !s.enum.includes(value)) out.push(`${path}: not one of ${s.enum.join(', ')}`);
    if (s.type && !(typeOf(value) === s.type || (s.type === 'number' && typeOf(value) === 'integer'))) out.push(`${path}: not ${s.type}`);
    if (typeof value === 'string') {
        if (s.minLength != null && value.length < s.minLength) out.push(`${path}: too short`);
        if (s.maxLength != null && value.length > s.maxLength) out.push(`${path}: too long`);
        if (s.pattern && !new RegExp(s.pattern).test(value)) out.push(`${path}: does not match ${s.pattern}`);
    }
    if (typeof value === 'number') {
        if (s.minimum != null && value < s.minimum) out.push(`${path}: below ${s.minimum}`);
        if (s.maximum != null && value > s.maximum) out.push(`${path}: above ${s.maximum}`);
    }
    if (Array.isArray(value)) {
        if (s.maxItems != null && value.length > s.maxItems) out.push(`${path}: more than ${s.maxItems}`);
        if (s.uniqueItems && new Set(value.map((v) => JSON.stringify(v))).size !== value.length) out.push(`${path}: repeats`);
        if (s.items) value.forEach((v, i) => out.push(...errors(v, s.items, `${path}/${i}`)));
    }
    if (typeOf(value) === 'object') {
        for (const key of s.required || []) if (!(key in value)) out.push(`${path}: lacks ${key}`);
        for (const [key, v] of Object.entries(value)) {
            if (s.properties && key in s.properties) out.push(...errors(v, s.properties[key], `${path}/${key}`));
            else if (s.additionalProperties === false) out.push(`${path}: ${key} not allowed`);
            else if (typeof s.additionalProperties === 'object') out.push(...errors(v, s.additionalProperties, `${path}/${key}`));
        }
    }
    if (s.oneOf && s.oneOf.filter((o) => !errors(value, o, path).length).length !== 1) out.push(`${path}: not exactly one of the choices`);
    if (s.anyOf && !s.anyOf.some((o) => !errors(value, o, path).length)) out.push(`${path}: none of the choices`);
    if (s.not && !errors(value, s.not, path).length) out.push(`${path}: ${s.not.description || 'matches what it must not'}`);
    return out;
}

const kindsInSchema = schema.properties.properties.properties.place_type.enum;
const PORT = '11111111-1111-4111-8111-111111111111', BERTH = '22222222-2222-4222-8222-222222222222';
const square = { type: 'Polygon', coordinates: [[[4, 52], [4.1, 52], [4.1, 52.1], [4, 52.1], [4, 52]]] };
const feature = (properties, id = BERTH) => ({ type: 'Feature', id, properties, geometry: square });

test('the schema, the loader, the import and the editor know the same place types', () => {
    const loader = read('../../Source/Tracking/PlaceCatalogue.h').match(/names\[\]\s*=\s*\{([^}]*)\}/)[1].match(/"([a-z]+)"/g).map((s) => s.slice(1, -1));
    const editor = read('../control/js/places-editor.js').match(/data-field="place_type">(.*?)<\/select>/)[1].match(/value="([a-z]+)"/g).map((s) => s.slice(7, -1));
    for (const kinds of [KINDS, loader, editor]) assert.deepEqual([...kinds].sort(), [...kindsInSchema].sort());
});

test('a complete schema 3 place is valid, and each rule is checked', () => {
    const p = { schema_version: 3, revision: 4, name: 'Kade 1', place_type: 'berth', part_of: PORT, no: 12, category: 'quay', rank: 1,
        codes: { own: ['local:K1'] }, country: 'NL', aliases: ['Quay one'], attributes: { depth: 12 }, geometry_source: { source: 'osm' } };
    assert.deepEqual(errors(feature(p), schema), []);
    assert.ok(errors(feature({ ...p, rank: 4 }), schema).some((e) => e.includes('/rank')));
    assert.ok(errors(feature({ ...p, place_type: 'area' }), schema).some((e) => e.includes('/place_type')));
    assert.ok(errors(feature({ ...p, part_of: 'Rotterdam' }), schema).some((e) => e.includes('/part_of')));
    assert.ok(errors(feature({ ...p, codes: { unlocode: ['nlrtm'] } }), schema).some((e) => e.includes('/codes/unlocode/0')));
    assert.ok(errors(feature(p, 'NOT-A-UUID'), schema).some((e) => e.includes('/id')));
});

test('older spellings are refused by the schema and carried over by the import', () => {
    const schema2 = { schema_version: 2, revision: 3, name: 'Strait of Hormuz', place_type: 'area', size: 3,
        attributes: { area_subtype: 'waterway' }, codes: { own: ['local:SOH'] } };
    const schema1 = { schema_version: 1, revision: 2, name: 'Old berth', place_type: 'berth', port_unlocode: 'NLHVH',
        details: { code: 'B12', source: 'osm', depth: 12.5 } };
    for (const p of [schema2, schema1]) {
        assert.ok(errors(feature(p), schema).length, `${p.name} as written is not schema 3`);
        const { properties } = importPlace(p, { NLHVH: PORT });
        assert.deepEqual(errors(feature(properties), schema), [], `${p.name} once imported`);
    }
    assert.equal(importPlace(schema2).properties.place_type, 'guardzone');
    assert.equal(importPlace(schema1, { NLHVH: PORT }).properties.part_of, PORT);
});
