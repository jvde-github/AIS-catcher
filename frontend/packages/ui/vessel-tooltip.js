// A vessel's band in a map hover, the part both hosts share; the host adds the band
// of a station on board and the message cards, and styles the station boxes.

import { getCountryName, getDeltaTimeVal, getShipTypeShort, sanitizeString } from '@aiscatcher/core/text.js';
import { ShippingClass } from '@aiscatcher/core/constants.js';
import { flagHTML } from './components.js';

// How fresh the last reception is. A vessel under way reports every few seconds and one at anchor
// every three minutes, so six minutes without is two reports missed; after a quarter of an hour
// it is stale. The marker fades over the same half hour.
export const reception = (age) => (age == null ? 'unknown' : age < 360 ? 'fresh' : age < 900 ? 'aging' : 'stale');

/* The head of every tracked object's hover card, in two rows. The first is who it is, with one
   thing about it at the end: its speed when it has one, else what it is, as Base station. The
   second is the reception: how fresh, and the stations that heard it.
   o: { flag, name, value: html; age: seconds since it was heard, null when unknown;
        kind: its type in words; by: the stations that heard it, as html } */
export function hoverRows(o) {
    const end = o.value ? `<span class="tip-value">${o.value}</span>` : o.kind ? `<span class="tip-kind">${o.kind}</span>` : '';
    return `<div class="tip-top">${o.flag}<strong class="tip-name tooltip-name" translate="no">${o.name}</strong>${end}</div>` +
        `<div class="tip-meta"><span class="tip-rx" data-state="${reception(o.age)}"><i></i>${o.age != null ? getDeltaTimeVal(o.age) + ' ago' : 'not heard'}</span>` +
        (o.by ? `<span class="tip-stations"><i class="station_icon" aria-hidden="true"></i>${o.by}</span>` : '') + '</div>';
}

// the stations that heard it: two by name, the one that heard it last first and marked, then how many more
const heardBy = (ids, label) => (ids && ids.length
    ? ids.slice(0, 2).map((id, i) => (i ? sanitizeString(String(label(id))) : `<span class="tip-last" title="Heard it last">${sanitizeString(String(label(id)))}</span>`)).join(' &middot; ') + (ids.length > 2 ? ` +${ids.length - 2}` : '')
    : '');

// what a target is when it tells no ship type
const KIND = {
    [ShippingClass.B]: 'Class B', [ShippingClass.PLANE]: 'SAR aircraft', [ShippingClass.HELICOPTER]: 'SAR helicopter',
    [ShippingClass.STATION]: 'Base station', [ShippingClass.ATON]: 'Aid to navigation', [ShippingClass.SARTEPIRB]: 'SART / EPIRB',
};

/* o: { name: the host's name for the vessel, as html; speed: its speed with
   unit, as html, null when unknown; age: seconds since it was heard, null when
   unknown; label: a station's name by its id, null where the host lists no stations }
   The dot says how fresh the reception is. A position that failed validation is the one thing
   the band's edge still says, in red: one that passed, or was not checked, shows nothing. */
export function vesselBand(ship, o) {
    return (ship.validated === -1 ? '<div class="tip-band tip-barred" style="--band: #ef4444" title="This position failed validation">' : '<div class="tip-band">') + hoverRows({
        flag: flagHTML(ship.country, 'flag-tooltip', getCountryName(ship.country)), name: o.name, value: o.speed, age: o.age,
        kind: ship.shiptype ? getShipTypeShort(ship.shiptype) : KIND[ship.shipclass] || '', by: o.label ? heardBy(ship.stations, o.label) : '',
    }) + '</div>';
}
