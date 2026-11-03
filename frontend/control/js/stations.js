// Station 0 is the default station, where an input stands that names none: always there, and first.

export const num = v => (v === '' || v === null || v === undefined ? NaN : Number(v));
export const validPos = (lat, lon) => Number.isFinite(lat) && Number.isFinite(lon) && Math.abs(lat) <= 90 && Math.abs(lon) <= 180;

export const isDefaultStation = s => Number(s.id) === 0;

export const stationName = s => String(s.name || '').trim() || (isDefaultStation(s) ? 'Default station' : `Station ${s.id}`);

export const stationLabel = (s, several) => stationName(s) + (several && isDefaultStation(s) ? ' (default)' : '');

// the default station first, there even when the file has none
export function stationsOf(cfg) {
    const listed = Array.isArray(cfg?.stations) ? cfg.stations : [];
    return [listed.find(isDefaultStation) || { id: 0 }, ...listed.filter(s => !isDefaultStation(s))];
}
