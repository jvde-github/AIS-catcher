// The viewer's configuration from the server, one object for the session: the
// updater (features/server-config) writes a newer revision into it in place.
import { initialServerConfig } from '../features/server-config/server-config.js';

export const config = initialServerConfig();

export function severalStations() {
    return (config.stations || []).length >= 2;
}

export function stationLabel(id) {
    const r = (config.stations || []).find((x) => x.id === id);
    return r && r.name ? r.name : "Station " + id;
}

export function focusLabel(id) {
    return id >= 0 && severalStations() ? stationLabel(id) : "";
}

export function stationNames(ids) {
    return (ids || []).map((id) => stationLabel(id)).join(", ");
}
