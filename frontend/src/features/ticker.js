import * as strip from '../../shared/ticker.js';
import * as mapObjects from './mapobjects.js';

let bar = null;

export function init(d) {
    bar = strip.create({
        mount: document.getElementById("ticker"),
        buckets: d.buckets,
        bucketHidden: d.bucketHidden,
        selection: {
            noteSeen: mapObjects.eventSeen,
            resolveVessel: d.resolveVessel,
            openVessel: d.openVessel,
            navigate: d.navigate,
        },
        // the receiver's events: safety texts, destinations, status and draught notices
        poll: () => mapObjects.pollEvents((events) => bar.push(events)),
    });
}

export function setEnabled(on) { bar.setEnabled(on); }
export function setCounts(c) { bar.setCounts(c); }
