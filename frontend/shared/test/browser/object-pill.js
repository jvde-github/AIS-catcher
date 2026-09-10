// Browser regression harness; bundle with esbuild and the host's ol dependency.
import Map from 'ol/Map.js';
import View from 'ol/View.js';
import { fromLonLat } from 'ol/proj.js';
import { create } from '../../mapobjects.js';

const assert = (value, message) => { if (!value) throw new Error(message); };
const opened = [], fetched = [];
const host = {
    options: () => ({ display: 'badge', colorClass: true }),
    ship: () => null, shipLabel: String, shipLink: () => '',
    fetchJSON: async url => { fetched.push(url); return { ships: [] }; },
    objectUrl: id => id, shipMessagesUrl: String, eventsUrl: String,
    openStation: id => opened.push(id), openVessel: () => {},
    isHovered: () => false, rehover: () => {}, map: () => map,
};
const objects = create(host);
const map = new Map({ target: 'map', layers: [objects.layer], controls: [],
    view: new View({ center: fromLonLat([4.26667, 52.1]), zoom: 16, rotation: .35 }) });
const rows = [
    { id: 'pNLSCE', kind: 9, label: 'Scheveningen', code: 'NLSCE', lat: 52.1, lon: 4.26667 },
    { id: 's1', kind: 8, label: 'Station one', online: true, lat: 52.1, lon: 4.26667 },
    { id: 's2', kind: 8, label: 'Station two', online: false, lat: 52.1, lon: 4.26667 },
];
window.checkPills = async () => {
    objects.setViewZoom(16);
    objects.applyDelta({ time: 100, objects: rows }); objects.redraw(); map.renderSync();
    await new Promise(requestAnimationFrame);
    const features = objects.vector.getFeatures();
    assert(features.length === 3, 'One hit-test feature per object');
    const centre = map.getPixelFromCoordinate(fromLonLat([4.26667, 52.1]));
    for (let i = 0; i < 3; ++i) {
        const hit = map.forEachFeatureAtPixel([centre[0] + (i - 1) * 20, centre[1]], f => f, { hitTolerance: 10 });
        assert(hit?.binary_object.id === rows[i].id, `Independent hit target ${i}`);
        objects.click(hit);
    }
    assert(opened.join(',') === '1,2', 'Both stations open independently of the port');
    assert(fetched.some(url => url.includes('NLSCE')), 'Port opens its own dialog');
    // Same count, different online status: cached segments must remain distinct.
    const styles = objects.layer.getStyleFunction();
    const online = styles(features.find(f => f.binary_object.id === 's1'))[0].getImage().getImage(1);
    const offline = styles(features.find(f => f.binary_object.id === 's2'))[0].getImage().getImage(1);
    assert(online !== offline, 'Station status has distinct cached rendering');
    // Invisible corners must not cover adjacent objects; the capsule has no badge.
    const first = styles(features.find(f => f.binary_object.id === 'pNLSCE'))[0].getImage().getImage(1);
    assert(first.getContext('2d').getImageData(0, 0, 1, 1).data[3] === 0, 'Rounded corner transparent');
    assert(features.every(f => styles(f).length === 1), 'No numeric badge style');
    // Zoom separates nearby positions instead of preserving a stale pill.
    objects.applyDelta({ objects: [{ ...rows[2], lon: rows[2].lon + .00004 }] });
    objects.setViewZoom(23); objects.redraw();
    assert(objects.vector.getFeatureById('mo-s2').pill_count === 1, 'Zoom releases separated member');
    // A single marker continues to use its original standalone style.
    assert(styles(objects.vector.getFeatureById('mo-s2')).length === 1, 'Standalone station style retained');
    return 'PASS: independent port/station hits, rotated view, cached status colours, rounded corners, no badges, zoom separation';
};
