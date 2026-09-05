/* ============================================================================
   shipcard.js — what the ship card says, whichever shape it takes
   The values every host shows about a vessel and the renderers for its hull,
   its reported changes and its draught history. A layout - the classic fold
   of sections in shipcard-classic.js, the remembered tabs in shipcard-tabs.js -
   builds the cells and hands them here to be filled. A cell key may map to
   several elements when a layout shows a value more than once.
   ========================================================================= */
import { CHANGE, getCountryName, getEtaVal, getMmsiTypeVal, getShipTypeShort, getStatusVal, sanitizeString } from './core/text.js';
import { getChangeListHTML, getDraughtChartSVG, getShipDimensionSVG, getSpeedHistorySVG } from './core/spark.js';

export const CHANGE_LIST_FIELDS = [CHANGE.DESTINATION, CHANGE.ETA, CHANGE.SHIPNAME, CHANGE.CALLSIGN, CHANGE.STATUS];

/* Fills the common cells. `h` carries the host's units, `regionName(region)`,
   `infoIcon(kind)` for the sender and ship-type details, and optionally
   `callsign(ship)` where a host words an absent callsign itself. */
export function populate(cells, ship, h) {
    const u = h.units;
    const each = (k) => (Array.isArray(cells[k]) ? cells[k] : cells[k] ? [cells[k]] : []);
    const text = (k, v) => each(k).forEach((el) => { el.textContent = v == null || v === '' ? '-' : String(v); });
    const html = (k, v) => each(k).forEach((el) => { el.innerHTML = v == null || v === '' ? '-' : v; });
    const icon = (k) => (h.infoIcon ? h.infoIcon(k) : '');

    text('mmsi', ship.mmsi != null ? ship.mmsi : ship.ship_id);
    text('callsign', h.callsign ? h.callsign(ship) : ship.callsign);
    each('imo').forEach((el) => { el.previousElementSibling.textContent = ship.imo != null || !ship.eni ? 'IMO' : 'ENI'; });
    text('imo', ship.imo != null ? ship.imo : ship.eni);
    text('country', getCountryName(ship.country));
    html('type', sanitizeString(getMmsiTypeVal(ship)) + icon('tech'));
    const shiptype = ship.shiptype != null ? ship.shiptype : null;
    html('shiptype', sanitizeString(getShipTypeShort(shiptype)) + (shiptype != null ? icon('shiptype') : ''));
    text('dimension', u.getShipDimension(ship));
    text('draught', ship.draught ? u.getDraughtVal(ship.draught) + ' ' + u.getDimUnit() : null);
    text('bluesign', ship.maneuver === 2 ? 'Set' : ship.maneuver === 1 ? 'Not set' : null);

    text('status', getStatusVal(ship));
    text('altitude', ship.altitude != null ? Number(ship.altitude).toFixed(0) + ' m' : null);
    text('speed', ship.speed != null ? u.getSpeedVal(ship.speed) + ' ' + u.getSpeedUnit() : null);
    text('cog', ship.cog != null ? Number(ship.cog).toFixed(1) + '°' : null);
    text('heading', ship.heading != null ? Number(ship.heading).toFixed(0) + '°' : null);
    text('destination', ship.destination);
    text('eta', ship.eta_month != null && ship.eta_day != null && ship.eta_hour != null && ship.eta_minute != null ? getEtaVal(ship) : null);
    html('lat', ship.lat != null ? u.getLatValFormat(ship) : null);
    html('lon', ship.lon != null ? u.getLonValFormat(ship) : null);
    text('region', h.regionName ? h.regionName(ship.region) : ship.region);
}

export const hull = (ship, u) => getShipDimensionSVG(ship, u);
export const changeList = (changes, u) => getChangeListHTML(changes, CHANGE_LIST_FIELDS, u);
export const draughtChart = (changes, draught, u) => getDraughtChartSVG(changes, draught, u);
export const speedChart = (pts, u) => getSpeedHistorySVG(pts, 10, u);
export const hasChanges = (changes) => (changes || []).some((c) => CHANGE_LIST_FIELDS.indexOf(c.f) !== -1);
export const hasDraughtChange = (changes) => (changes || []).some((c) => c.f === CHANGE.DRAUGHT && !c.i);
