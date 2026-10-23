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

test('the server section: each web viewer is a server of services, with ships of its own behind a filter or zones', () => {
    const cfg = migrateConfig({ server: [
        { port: 8100, geojson: true, prome: true, station: 'Home', lat: 52, lon: 4.3, share_loc: true, realtime: true, split: true, ip_bind: '0.0.0.0', file: '/var/lib/ais.bin', backup: 10 },
        { port: 8200, filter: true, allow_type: '1,2,3', unique: true, file: '/var/lib/a.bin', history: 3600 },
        { port: 8300, zone: ['north'], groups_in: 3 },
        { port: 8400, active: false }], services: [{ main: true }] });
    assert.equal(cfg.server, undefined);
    // the unfiltered viewer's tracking is the site's, its station is station 0, its title stays
    assert.deepEqual(cfg.tracking, { file: '/var/lib/ais.bin', backup: 10 });
    assert.deepEqual(cfg.stations, [{ id: 0, name: 'Home', lat: 52, lon: 4.3, gps: true, share_loc: true }]);
    assert.deepEqual(cfg.services, [{ main: true },
        { port: 8100, geojson: true, metrics: true, station: 'Home', realtime: true, bind: '0.0.0.0' },
        { port: 8200, bind: '0.0.0.0', tracking: { allow_type: '1,2,3', unique: true, history: 3600 } },
        { port: 8300, bind: '0.0.0.0', tracking: { zone: ['north'], groups_in: 3 } },
        { port: 8400, bind: '0.0.0.0', active: false }]);
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

test("a server's or a webhook's filter object is its view", () => {
    const cfg = migrateConfig({ services: [{ main: true }, { port: 8100, filter: { stations: [3] } }, { port: 8101, filter: true, allow_type: '1' }],
        webhooks: [{ url: 'https://example.org/h', filter: { mmsi_allow: [244000101] } }] });
    assert.deepEqual(cfg.services[1], { port: 8100, view: { stations: [3] } });
    assert.deepEqual(cfg.services[2], { port: 8101, filter: true, allow_type: '1' });
    assert.deepEqual(cfg.webhooks[0], { url: 'https://example.org/h', view: { mmsi_allow: [244000101] } });
});

test('a web viewer with filter switched on but no term reads the site, and a viewer on 127.0.0.1 stays local', () => {
    const cfg = migrateConfig({ server: [{ port: 8100, filter: true, file: '/var/lib/ais.bin', backup: 10 }, { port: 8101, ip_bind: '127.0.0.1' }], services: [{ main: true }] });
    assert.deepEqual(cfg.services, [{ main: true }, { port: 8100, bind: '0.0.0.0' }, { port: 8101 }]);
    assert.deepEqual(cfg.tracking, { file: '/var/lib/ais.bin', backup: 10 });
});

test('a web viewer switched off is a server switched off, keeping its settings and leaving the site as it is', () => {
    const cfg = migrateConfig({ server: [{ port: 8100, active: false, station: 'Pier', lat: 52, lon: 4, file: '/var/lib/pier.bin', realtime: true }], services: [{ main: true }] });
    assert.deepEqual(cfg.services[1], { port: 8100, active: false, station: 'Pier', lat: 52, lon: 4, file: '/var/lib/pier.bin', realtime: true, bind: '0.0.0.0' });
    assert.equal(cfg.tracking, undefined);
    assert.equal(cfg.stations, undefined);
});
