import * as strip from '@aiscatcher/ui/ticker.js';
import * as mapObjects from '../mapobjects/mapobjects.js';
import { ships } from '../../core/store.js';
import { fetchShips } from '../../data.js';
import { showTargetcard } from '../targetcard/card.js';

let bar = null;

export function init(d) {
    bar = strip.create({
        mount: document.getElementById("ticker"),
        buckets: d.buckets,
        bucketHidden: d.bucketHidden,
        selection: {
            noteSeen: mapObjects.eventSeen,
            resolveVessel: async (m) => {
                if (!ships[m]) await fetchShips(false);
                return ships[m]?.raw || null;
            },
            openVessel: (m) => showTargetcard('ship', m),
            navigate: d.navigate,
        },
        // the receiver's events: safety texts, destinations, status and draught notices
        poll: () => mapObjects.pollEvents((events) => bar.push(events)),
    });
}

export function setEnabled(on) { bar.setEnabled(on); }
export function setCounts(c) { bar.setCounts(c); }
