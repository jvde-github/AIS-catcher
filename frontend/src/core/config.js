// The viewer's configuration from the server, one object for the session: the
// updater (features/server-config) writes a newer revision into it in place.
import { initialServerConfig } from '../features/server-config/server-config.js';

export const config = initialServerConfig();

// the page has stations to tell apart: its picker lists them
export function severalStations() {
    return (config.stations || []).length >= 2;
}

// a station's name as the page's station picker lists it
export function stationLabel(id) {
    const r = (config.stations || []).find((x) => x.id === id);
    return r && r.name ? r.name : "Station " + id;
}

// the station a page looks through, by name; empty when it looks through all
export function focusLabel(id) {
    return id >= 0 && severalStations() ? stationLabel(id) : "";
}

// the stations that heard a ship, by name, the latest first
export function stationNames(ids) {
    return (ids || []).map((id) => stationLabel(id)).join(", ");
}
