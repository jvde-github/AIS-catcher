import { hasValidCoords } from './geo.js';

// The ticker dismisses the item first. Hosts supply operations, while this
// shared policy owns seen state, vessel resolution and coordinate fallback.
// resolveVessel(mmsi) -> { lat, lon, ... } | null, synchronously or asynchronously
// openVessel(mmsi, vessel), navigate({ lat, lon }, zoom), noteSeen(mmsi)
export function createEventSelection(host) {
    let revision = 0;
    return async function select(event) {
        const current = ++revision;
        const mmsi = Number(event.id);
        const identified = Number.isInteger(mmsi) && mmsi > 0;
        if (identified) host.noteSeen(mmsi);
        let vessel = null;
        if (identified) {
            try { vessel = await host.resolveVessel(mmsi); }
            catch { /* A failed lookup still permits navigation to the event. */ }
        }
        if (current !== revision) return;
        if (vessel) await host.openVessel(mmsi, vessel);
        if (current !== revision) return;
        const position = hasValidCoords(vessel?.lat, vessel?.lon) ? vessel : event;
        if (hasValidCoords(position.lat, position.lon)) {
            host.navigate({ lat: position.lat, lon: position.lon }, 14);
        }
    };
}
