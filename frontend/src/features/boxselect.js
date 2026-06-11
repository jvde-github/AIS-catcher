// Box select: drag a rectangle on the map to enable tracks for all vessels
// inside it (issue #253). One-shot: activates from the context menu, restores
// normal map panning after the drag or on Escape.

import DragBox from 'ol/interaction/DragBox';
import DragPan from 'ol/interaction/DragPan';
import { fromLonLat } from 'ol/proj';
import { containsCoordinate } from 'ol/extent';

// { getMap, getShipsDB, showTracks, showNotification }
let deps = null;
let box = null;

export function init(d) {
    deps = d;
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
    const map = deps.getMap();

    box = new DragBox();
    box.on('boxend', async () => {
        const extent = box.getGeometry().getExtent();
        const shipsDB = deps.getShipsDB();
        const selected = [];
        for (const mmsi in shipsDB) {
            const s = shipsDB[mmsi].raw;
            if (s.lat != null && s.lon != null &&
                containsCoordinate(extent, fromLonLat([s.lon, s.lat]))) {
                selected.push(mmsi);
            }
        }
        stop();
        const n = await deps.showTracks(selected);
        deps.showNotification(n === 0 ? "No vessels in selection" :
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
    const map = deps.getMap();
    map.removeInteraction(box);
    map.getInteractions().forEach((i) => {
        if (i instanceof DragPan) i.setActive(true);
    });
    map.getTargetElement().classList.remove('crosshair_cursor');
    document.removeEventListener('keydown', onKeyDown);
    box = null;
}
