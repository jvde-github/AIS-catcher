// A vessel's band in a map hover, the part both hosts share: flag, name and
// speed, then its type, how long ago it was heard and by which stations. The
// band wears the vessel's validity; the host adds the band of a station on
// board and the message cards, and styles the station boxes.

import { getCountryName, getDeltaTimeVal, getShipTypeShort } from '@aiscatcher/core/text.js';
import { flagHTML } from './components.js';
import { validityBand } from './binary.js';
import { stationBoxes } from './stations.js';

/* o: { name: the host's name for the vessel, as html; speed: its speed with
   unit, null when unknown; age: seconds since it was heard, null when unknown;
   label: a station's name by its id, null where the host lists no stations } */
export function vesselBand(ship, o) {
    const by = o.label ? stationBoxes(ship.stations, o.label) : '';
    const sub = (ship.shiptype ? getShipTypeShort(ship.shiptype) + ' - ' : '') +
        (o.age != null ? 'received ' + getDeltaTimeVal(o.age) + ' ago' : '') + (by ? ' by ' + by : '');
    return `<div class="tip-band" style="--band: ${validityBand(ship.validated)}"><div class="tooltip-card">` +
        flagHTML(ship.country, 'flag-tooltip', getCountryName(ship.country)) +
        `<div><span class="tooltip-name" translate="no">${o.name}</span>` +
        (o.speed ? `<span class="tooltip-dim"> at </span>${o.speed}` : '') +
        (sub ? `<div class="tooltip-sub">${sub}</div>` : '') + '</div></div></div>';
}
