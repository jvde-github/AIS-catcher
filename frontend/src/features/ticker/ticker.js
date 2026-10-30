import * as strip from '@aiscatcher/ui/ticker.js';
import * as mapObjects from '../mapobjects/mapobjects.js';
import { ships } from '../../core/store.js';
import { fetchShips } from '../../data.js';
import * as filter from '../../core/filter.js';
import { ui } from '../../ui.js';
import { bucketChips } from '../counts/counts.js';
import { showTargetcard } from '../targetcard/card.js';

let bar = null;

export function init() {
    bar = strip.create({
        mount: document.getElementById("ticker"),
        buckets: bucketChips(),
        bucketHidden: (b) => filter.isActive() && filter.isHidden("bucket", b),
        selection: {
            noteSeen: mapObjects.eventSeen,
            resolveVessel: async (m) => {
                if (!ships[m]) await fetchShips(false);
                return ships[m]?.raw || null;
            },
            openVessel: (m) => showTargetcard('ship', m),
            navigate: ({ lat, lon }, minZoom) => {
                ui.reveal([lon, lat], undefined, { minZoom, center: true });
            },
        },
        // the receiver's events: safety texts, destinations, status and draught notices
        poll: () => mapObjects.pollEvents((events) => bar.push(events)),
    });
}

export function setEnabled(on) { bar.setEnabled(on); }
export function setCounts(c) { bar.setCounts(c); }
