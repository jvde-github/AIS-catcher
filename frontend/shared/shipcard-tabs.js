// Vessel fields and history slots on the shared map-card tabs.
// Compact mode shows the live metrics; the handle expands or collapses details.
import { decodeHTMLEntities, fieldRows } from './components.js';
import { build as buildTabs, metrics } from './card-tabs.js';
import { MATCHED_PORT_FIELDS } from './shipcard.js';
import { CLASS_A, CLASS_B } from './core/constants.js';
import { getEtaVal, getMmsiTypeVal, getShipTypeShort, getStatusVal } from './core/text.js';

export const TABS = [['summary', 'Summary'], ['vessel', 'Vessel'], ['voyage', 'Voyage'], ['ais', 'AIS'], ['history', 'History']];
export const DEFAULT_TAB = 'summary';
export const validTab = (t) => (TABS.some(([k]) => k === t) ? t : DEFAULT_TAB);

const PIN = '<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.4"><path d="M12 21s-6-5.2-6-10a6 6 0 1 1 12 0c0 4.8-6 10-6 10z"/><circle cx="12" cy="11" r="2.2"/></svg>';

function el(tag, cls, attrs) {
    const e = document.createElement(tag);
    if (cls) e.className = cls;
    for (const a in attrs || {}) e.setAttribute(a, attrs[a]);
    return e;
}

function group(panel, label, id) {
    const g = el('div', 'sc-group');
    const l = el('div', 'sc-group-label');
    l.textContent = label;
    const body = el('div', 'hist-wrap', { id });
    g.appendChild(l);
    g.appendChild(body);
    panel.appendChild(g);
    return body;
}

/* `o.slot` is the host's AIS section (moved into the AIS tab), `o.tab` the
   tab to open (null folds), `o.onTab(key)` hears a change (null on a fold),
   `o.foldable()` enables the compact disclosure handle. Returns the cells
   for shipcard.populate(), select(key), active(), update(ship, h). */
export function build(mount, prefix, o) {
    o = o || {};
    const shell = buildTabs(mount, prefix, { ...o, tabs: TABS, label: 'Vessel details' });
    const { head, panels: panelEls } = shell;

    const cells = {};
    const rows = (panel, spec) => {
        const c = fieldRows(panel, spec, { rowClass: 'mapcard-content-row card-row sc-row', idPrefix: prefix });
        for (const k in c) cells[k] = c[k];
    };

    // Only the live metrics remain visible while the mobile card is folded.
    const summaryMetrics = metrics(head, [
        { key: 'class', label: 'Class' }, { key: 'speed', label: 'Speed' },
        { key: 'cog', label: 'Course' }, { key: 'heading', label: 'Heading' },
    ], prefix + 'summary_');
    panelEls.summary.innerHTML =
        '<div class="sc-sum-line"><span class="sc-sum-text"><span class="sc-sum-type"></span><span class="sc-sum-sep">·</span><span class="sc-sum-status"></span></span>' +
        '<span class="sc-sum-id"></span></div>' +
        '<div class="sc-strip">' +
        '<div class="ends"><div><div class="l">Current location</div><div class="v sc-sum-region"></div><div class="sc-sum-pos"></div></div>' +
        '<div><div class="l sc-sum-dest-label">Reported destination</div><div class="v sc-sum-dest"></div><div class="sc-sum-reported"></div></div></div>' +
        '<div class="line" aria-hidden="true"><span class="pip"></span><span class="t"><i></i></span><span class="pin sc-sum-pin">' + PIN + '</span></div>' +
        '<div class="under"><span class="sc-sum-seen"></span><span class="sc-sum-eta"></span></div>' +
        '</div>';
    const q = (c) => mount.querySelector('.' + c);
    const summary = { type: q('sc-sum-type'), sep: q('sc-sum-sep'), status: q('sc-sum-status'), id: q('sc-sum-id'), pos: q('sc-sum-pos'), region: q('sc-sum-region'),
                      dest: q('sc-sum-dest'), destLabel: q('sc-sum-dest-label'), reported: q('sc-sum-reported'), pin: q('sc-sum-pin'),
                      seen: q('sc-sum-seen'), eta: q('sc-sum-eta'),
                      };

    rows(panelEls.vessel, [
        { fields: [{ key: 'mmsi', label: 'MMSI' }, { key: 'callsign', label: 'Callsign' }, { key: 'imo', label: 'IMO' }] },
        { fields: [{ key: 'country', label: 'Country' }, { key: 'type', label: 'Sender' }, { key: 'shiptype', label: 'Ship type' }] },
        { fields: [{ key: 'dimension', label: 'Dimension' }, { key: 'draught', label: 'Draught' }, { key: 'bluesign', label: 'Blue sign' }] },
    ]);
    group(panelEls.vessel, 'Antenna position', prefix + 'hull_body');

    rows(panelEls.voyage, [
        { cls: 'row-wide-first', fields: [{ key: 'destination', label: 'Destination' }, { key: 'eta', label: 'ETA' }] },
        { fields: MATCHED_PORT_FIELDS },
        { cls: 'row-wide-first', fields: [{ key: 'status', label: 'Status' }, { key: 'altitude', label: 'Altitude' }] },
        { fields: [{ key: 'speed', label: 'Speed' }, { key: 'cog', label: 'Course' }, { key: 'heading', label: 'Heading' }] },
        { fields: [{ key: 'lat', label: 'Latitude' }, { key: 'lon', label: 'Longitude' }, { key: 'region', label: 'Region' }] },
    ]);
    group(panelEls.voyage, 'Reported changes', prefix + 'changes_body');

    // AIS: the host's own section, whole
    if (o.slot) panelEls.ais.appendChild(o.slot);
    else panelEls.ais.innerHTML = '<span class="dim-note">No reception data</span>';

    group(panelEls.history, 'Speed', prefix + 'speed_body');
    group(panelEls.history, 'Draught', prefix + 'draught_body');

    // Populate repeated metrics through the same formatter as the detail rows.
    for (const [key, cell] of Object.entries(summaryMetrics)) {
        cells[key] = cells[key] ? [cells[key], cell] : cell;
    }


    /* Summary from the record: nothing inferred, an absence is N/A; only a
       missing ETA drops its line */
    function update(ship, h) {
        const u = h.units;
        const set = (e, v) => { e.textContent = v == null || v === '' ? 'N/A' : String(v); };
        const vessel = ship.mmsi_type === CLASS_A || ship.mmsi_type === CLASS_B;
        const sender = getMmsiTypeVal(ship);
        set(summary.type, ship.shiptype ? getShipTypeShort(ship.shiptype) : vessel ? null : sender);
        const status = Number.isInteger(ship.status) && ship.status >= 0 && ship.status < 15 ? getStatusVal(ship) : null;
        summary.status.textContent = status || '';
        summary.sep.hidden = summary.status.hidden = !status;
        // the vessel's registry number, the MMSI when it has none
        summary.id.textContent = ship.imo != null ? 'IMO ' + ship.imo : ship.eni ? 'ENI ' + decodeHTMLEntities(ship.eni) : ship.mmsi != null ? 'MMSI ' + ship.mmsi : '';
        const hasPos = ship.lat != null && ship.lon != null;
        summary.pos.innerHTML = hasPos ? u.getLatValFormat(ship) + ', ' + u.getLonValFormat(ship) : 'N/A';
        const region = hasPos && ship.region != null ? (h.regionName ? h.regionName(ship.region) : ship.region) : null;
        summary.region.textContent = region && region !== '-' ? region : '';
        summary.region.hidden = !summary.region.textContent;
        const dest = ship.destination && String(ship.destination).trim();
        const port = ship.matched_port;
        const portName = port && typeof port.name === 'string' && port.name.trim() ? port.name.trim() : null;
        summary.destLabel.textContent = portName ? 'Destination' : 'Reported destination';
        set(summary.dest, portName || decodeHTMLEntities(dest));
        summary.reported.textContent = portName && dest ? decodeHTMLEntities(dest) : '';
        summary.reported.hidden = !summary.reported.textContent;
        const goTo = portName && h.goTo && typeof port.lat === 'number' && typeof port.lon === 'number' ? () => h.goTo(port.lat, port.lon) : null;
        for (const e of [summary.dest, summary.pin]) {
            e.classList.toggle('sc-sum-link', !!goTo);
            e.onclick = goTo;
        }
        summary.dest.onkeydown = goTo ? (e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); goTo(); } } : null;
        if (goTo) { summary.dest.setAttribute('role', 'button'); summary.dest.tabIndex = 0; }
        else { summary.dest.removeAttribute('role'); summary.dest.removeAttribute('tabindex'); }
        const age = h.age ? h.age(ship) : null;
        summary.seen.textContent = age ? 'Last received ' + age + ' ago' : 'Last received unknown';
        const hasEta = ship.eta_month != null && ship.eta_day != null && ship.eta_hour != null && ship.eta_minute != null;
        summary.eta.textContent = hasEta ? 'ETA ' + getEtaVal(ship) : '';
        summary.eta.hidden = !hasEta;
    }

    return { ...shell, cells, update };
}
