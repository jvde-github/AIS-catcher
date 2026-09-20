// Search received vessels and the receiver's full station/place metadata.
// Neither the current map bounds nor its display filters restrict these results.
import { decodeHTMLEntities } from '../../shared/components.js';

const normalize = value => String(value ?? '').normalize('NFKD').replace(/\p{M}/gu, '').toLowerCase().trim();
// The viewer stores escaped AIS strings; the shared result renderer takes text.
const plain = value => String(value ?? '').includes('&') ? decodeHTMLEntities(value) : value;

export function createLocalSearch(host) {
    let kept = null;
    return async (query, type, signal) => {
        const receiver = host.receiver();
        const rows = [];
        if (type === 'all' || type === 'ships') {
            for (const entry of Object.values(host.ships() || {})) {
                const ship = entry.raw;
                rows.push({type: 'ship', id: ship.mmsi, name: plain(host.shipName(ship)), callsign: plain(host.callSign(ship)),
                    imo: ship.imo, shipclass: ship.shipclass, lat: ship.lat, lon: ship.lon});
            }
        }
        if (type !== 'ships') {
            let data = kept && kept.receiver === receiver && Date.now() - kept.at < 60000 ? kept.data : null;
            if (!data) {
                const response = await fetch('api/mapobjects.json?since=0&receiver=' + receiver, {signal});
                if (!response.ok) throw new Error('Could not load stations and places');
                data = await response.json();
                kept = {receiver, at: Date.now(), data};
            }
            for (const object of data.objects || []) {
                if ((type === 'all' || type === 'stations') && object.kind === 8) {
                    rows.push({type: 'station', id: String(object.id).replace(/^s/, ''), name: object.label,
                        country: object.country, mmsi: object.mmsi, lat: object.lat, lon: object.lon, object});
                } else if ((type === 'all' || type === 'ports') && (object.kind === 9 || object.kind === 10)) {
                    const kind = object.place_type || 'port';
                    rows.push({type: kind === 'port' ? 'port' : 'place', id: object.runtime_id ?? object.code,
                        code: object.code, name: object.label, lat: object.lat, lon: object.lon,
                        kindLabel: ({port: 'Port', berth: 'Berth', anchorage: 'Anchorage', custom: 'Region'})[kind] || 'Place',
                        object: {...object, place_version: data.place_version}});
                }
            }
        }
        if (receiver !== host.receiver()) return [];
        const needle = normalize(query), terms = needle.split(/\s+/);
        return rows.map(row => {
            const fields = [row.name, row.id, row.code, row.callsign, row.imo > 0 ? row.imo : '', row.mmsi, row.country].map(normalize);
            const content = fields.join(' ');
            return {row, matches: terms.every(term => content.includes(term)),
                rank: fields.includes(needle) ? 0 : fields.some(field => field.startsWith(needle)) ? 1 : 2};
        }).filter(item => item.matches).sort((a, b) => a.rank - b.rank || String(a.row.name || a.row.id).localeCompare(String(b.row.name || b.row.id)))
            .map(item => item.row);
    };
}
