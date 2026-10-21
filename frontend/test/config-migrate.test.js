import { test } from 'node:test';
import assert from 'node:assert/strict';
import { migrateConfig } from '../control/js/config-migrate.js';

test('a file from before: station keys become station 0, the viewer section the main server and the tracking', () => {
    const cfg = migrateConfig({
        stations: [{ id: 7, name: 'Mast' }],
        services: [{ port: 8110, serve: ['api'] }],
        control: { wizard: false, viewer: { station: 'work', lat: 51.9, lon: 4.3, share_loc: true, use_gps: false,
            realtime: true, decoder: true, log: 'off', serve: ['api', 'mcp'], file: '/tmp/x.stats', backup: 10 } }
    });
    assert.deepEqual(cfg.stations, [{ id: 0, name: 'work', lat: 51.9, lon: 4.3, share_loc: true }, { id: 7, name: 'Mast' }]);
    assert.deepEqual(cfg.services, [{ main: true, realtime: true, decoder: true, log: 'off', serve: ['api', 'mcp'] }, { port: 8110, serve: ['api'] }]);
    assert.deepEqual(cfg.tracking, { file: '/tmp/x.stats', backup: 10 });
    assert.deepEqual(cfg.control, { wizard: false });
});

test('a file of today is left as it is; a main server not first is put first', () => {
    const today = { stations: [{ id: 0, name: 'x', share_loc: false }], services: [{ port: 8110, serve: ['api'] }, { main: true, realtime: true }], tracking: { file: 'a' }, control: { wizard: false } };
    const out = migrateConfig(JSON.parse(JSON.stringify(today)));
    assert.deepEqual(out.services, [{ main: true, realtime: true }, { port: 8110, serve: ['api'] }]);
    assert.deepEqual(out.stations, today.stations);
    assert.deepEqual(out.tracking, today.tracking);
});

test('the viewer section without station keys: GPS as the engine took it, 0,0 is no position, a listed station 0 stays', () => {
    const cfg = migrateConfig({ stations: [{ id: 0, name: 'kept' }], control: { viewer: { lat: 0, lon: 0, station: 'ignored' } } });
    assert.deepEqual(cfg.stations, [{ id: 0, name: 'kept' }]);
    const gps = migrateConfig({ control: { viewer: { station: 'boat', own_mmsi: 244000001 } } });
    assert.deepEqual(gps.stations, [{ id: 0, name: 'boat', mmsi: 244000001, gps: true, share_loc: false }]);
    assert.deepEqual(gps.services, [{ main: true }]);
    assert.equal(gps.tracking, undefined);
});

test('the server section stays as it is: its web viewers are outputs, not servers of services', () => {
    const cfg = migrateConfig({ server: [{ port: 8100, geojson: true, zone: ['north'] }], services: [{ main: true }] });
    assert.deepEqual(cfg.server, [{ port: 8100, geojson: true, zone: ['north'] }]);
    assert.deepEqual(cfg.services, [{ main: true }]);
});

test('a viewer section of a long-used station: keys a server of the site has no use for are left behind', () => {
    const cfg = migrateConfig({ control: { viewer: {
        station: 'Home', lat: 52, lon: 4.3, share_loc: true, use_gps: false, realtime: true, decoder: true, log: true,
        msg: true, replay: true, geojson: true, prome: true, context: 'settings', webcontrol_http: 'http://pi:8118',
        file: '/var/lib/ais.stats', backup: 10, history: 3600, track_time: 7200, track_memory: 2048, expire: true,
        places: '/var/lib/places', server_mode: false, replay_time: 3600,
        port: 8100, ip_bind: '0.0.0.0', split: true, zone: ['north'], groups_in: 3, cdn: '/cdn', msgs: true, zlib: true,
        reuse_port: true, stats_on_close: true } } });
    assert.deepEqual(cfg.services, [{ main: true, realtime: true, decoder: true, log: true, msg: true, replay: true,
        geojson: true, prome: true, context: 'settings', webcontrol_http: 'http://pi:8118' }]);
    assert.deepEqual(cfg.tracking, { file: '/var/lib/ais.stats', backup: 10, history: 3600, track_time: 7200,
        track_memory: 2048, expire: true, places: '/var/lib/places', server_mode: false, replay_time: 3600 });
    assert.deepEqual(cfg.stations, [{ id: 0, name: 'Home', lat: 52, lon: 4.3, share_loc: true }]);
});
