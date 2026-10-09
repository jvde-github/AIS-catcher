// Box select: drag a rectangle on the map to enable tracks for all vessels
// inside it (issue #253). One-shot: activates from the context menu, restores
// normal map panning after the drag or on Escape.

import DragBox from 'ol/interaction/DragBox';
import DragPan from 'ol/interaction/DragPan';
import { fromLonLat } from 'ol/proj';
import { containsCoordinate } from 'ol/extent';
import { map } from '../../map.js';
import { ships as shipsDB } from '../../core/store.js';
import { showNotification } from '../../dialog.js';
import { register } from '../../actions.js';
import { showTracksForMMSIs } from '../../tracks.js';

let box = null;

export function init() {
    register({
        startBoxSelect: () => { start(); showNotification('Drag a rectangle to enable tracks (Esc to cancel)'); },
    });
}

function onKeyDown(e) {
    if (e.key === 'Escape') stop();
}

export function isActive() {
    return box != null;
}

export function start() {
    if (box) {
        stop();
        return;
    }

    box = new DragBox();
    box.on('boxend', async () => {
        const extent = box.getGeometry().getExtent();
        const selected = [];
        for (const mmsi in shipsDB) {
            const s = shipsDB[mmsi].raw;
            if (s.lat != null && s.lon != null &&
                containsCoordinate(extent, fromLonLat([s.lon, s.lat]))) {
                selected.push(mmsi);
            }
        }
        stop();
        const n = await showTracksForMMSIs(selected);
        showNotification(n === 0 ? "No vessels in selection" :
            `Tracks enabled for ${n} vessel${n === 1 ? '' : 's'}`);
    });

    map.addInteraction(box);
    map.getInteractions().forEach((i) => {
        if (i instanceof DragPan) i.setActive(false);
    });
    map.getTargetElement().classList.add('crosshair_cursor');
    document.addEventListener('keydown', onKeyDown);
}

export function stop() {
    if (!box) return;
    map.removeInteraction(box);
    map.getInteractions().forEach((i) => {
        if (i instanceof DragPan) i.setActive(true);
    });
    map.getTargetElement().classList.remove('crosshair_cursor');
    document.removeEventListener('keydown', onKeyDown);
    box = null;
}
