import * as strip from '../../shared/ticker.js';
import * as binary from './binary.js';

let bar = null;

export function init(d) {
    bar = strip.create({
        mount: document.getElementById("ticker"),
        buckets: d.buckets,
        bucketHidden: d.bucketHidden,
        onSelect: (id, event) => { binary.eventSeen(Number(id)); d.openVessel(Number(id), event); },
        // the receiver's events: safety texts, destinations, status and draught notices
        poll: () => binary.pollEvents((events) => bar.push(events)),
    });
}

export function setEnabled(on) { bar.setEnabled(on); }
export function setCounts(c) { bar.setCounts(c); }
