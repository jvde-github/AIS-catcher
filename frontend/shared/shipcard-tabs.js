// Vessel fields and history slots on the shared map-card tabs.
// Compact mode shows the live metrics; the handle expands or collapses details.
import { decodeHTMLEntities, fieldRows } from './components.js';
import { build as buildTabs, metrics } from './card-tabs.js';
import { MATCHED_PORT_FIELDS, setPortLink } from './shipcard.js';
import { CLASS_A, CLASS_B } from './core/constants.js';
import { getEtaVal, getMmsiTypeVal, getShipTypeFull, getShipTypeShort, getStatusShort, getStatusVal } from './core/text.js';

// 0 under way, 8 sailing: moving; 1 anchor, 5 moored: still; 2-4, 6 not under command, restricted, constrained, aground
const statusTone = (st) => (st === 0 || st === 8 ? 'ok' : st === 1 || st === 5 ? 'warn' : st === 2 || st === 3 || st === 4 || st === 6 ? 'bad' : 'off');

// the ship type's swatch: the classes the map tells apart, the rest one colour
const swatchType = (cls) => ({ 2: 'cargo', 6: 'tanker', 4: 'passenger', 8: 'fishing', 7: 'highspeed', 5: 'special' })[cls] || 'other';

const ageToSeconds = (text) => {
    if (!text) return null;
    let n = 0, any = false;
    for (const [, v, u] of String(text).matchAll(/(\d+)\s*([dhms])/g)) { n += Number(v) * { d: 86400, h: 3600, m: 60, s: 1 }[u]; any = true; }
    return any ? n : null;
};

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

function group(panel, label, id, expandable = false) {
    const g = el('div', 'sc-group' + (expandable ? ' sc-history-preview' : ''));
    const l = el('div', 'sc-group-label');
    l.textContent = label;
    const body = el('div', 'hist-wrap', { id });
    g.append(l, body);
    if (expandable) {
        const more = el('button', 'sc-more-pill', {type:'button', 'aria-controls':id, 'aria-expanded':'false'});
        more.innerHTML = 'Show earlier ' + label.toLowerCase().replace(/^reported /, '') + '<svg viewBox="0 0 24 24" width="12" height="12" aria-hidden="true"><path d="M6 9l6 6 6-6" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"/></svg>';
        more.hidden = g.hidden = true;
        more.onclick = () => {
            g.classList.add('is-expanded');
            more.setAttribute('aria-expanded', 'true');
            more.hidden = true;
        };
        g.appendChild(more);
        new body.ownerDocument.defaultView.MutationObserver(() => {
            const count = body.querySelectorAll('.tl-item').length;
            g.hidden = count === 0;
            more.hidden = count < 2 || g.classList.contains('is-expanded');
        }).observe(body, {childList:true, subtree:true});
    }
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
        '<div class="line"><span class="pip" aria-hidden="true"></span><span class="t" aria-hidden="true"><i></i></span><span class="pin sc-sum-pin">' + PIN + '</span></div>' +
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
    const hullBody = group(panelEls.vessel, 'Antenna position', prefix + 'hull_body');
    const legend = el('span', 'sc-legend');
    legend.textContent = 'GPS antenna';
    hullBody.previousElementSibling.appendChild(legend);

    rows(panelEls.voyage, [
        { cls: 'row-wide-first', fields: [{ key: 'destination', label: 'Destination' }, { key: 'eta', label: 'ETA' }] },
        { fields: MATCHED_PORT_FIELDS },
        { cls: 'row-wide-first', fields: [{ key: 'status', label: 'Status' }, { key: 'altitude', label: 'Altitude' }] },
        { fields: [{ key: 'speed', label: 'Speed' }, { key: 'cog', label: 'Course' }, { key: 'heading', label: 'Heading' }] },
        { fields: [{ key: 'lat', label: 'Latitude' }, { key: 'lon', label: 'Longitude' }] },
    ]);
    // the position as one line in a box, with a copy button
    const posRow = panelEls.voyage.lastElementChild;
    posRow.classList.add('sc-pos');
    const posLabel = posRow.querySelector(':scope > div > span:first-child');
    if (posLabel) posLabel.textContent = 'Position';
    const copy = el('button', 'sc-copy', { type: 'button', 'aria-label': 'Copy position', title: 'Copy position' });
    copy.innerHTML = '<svg viewBox="0 0 24 24" width="17" height="17" aria-hidden="true"><g fill="none" stroke="currentColor" stroke-width="1.8" stroke-linejoin="round"><rect x="8" y="8" width="12" height="12" rx="2"/><path d="M16 8V6a2 2 0 0 0-2-2H6a2 2 0 0 0-2 2v8a2 2 0 0 0 2 2h2"/></g></svg>';
    copy.onclick = (e) => {
        e.stopPropagation();
        const txt = [...posRow.querySelectorAll(':scope > div > span:last-child')].map(x => x.textContent.trim()).join(' ');
        navigator.clipboard?.writeText(txt).then(() => { copy.classList.add('is-done'); setTimeout(() => copy.classList.remove('is-done'), 1200); }, () => {});
    };
    posRow.appendChild(copy);
    const tiles = el('div', 'sc-tiles');
    tiles.innerHTML = '<div class="sc-tile sc-tile--seen"><span class="sc-tile__label">Last signal</span><span class="sc-tile__value"></span></div>' +
        '<div class="sc-tile"><span class="sc-tile__label">Messages</span><span class="sc-tile__value"></span></div>';
    const tileSeen = tiles.children[0], tileCount = tiles.children[1];
    cells.visits = group(panelEls.history, 'Port visits', prefix + 'visits', true);
    group(panelEls.history, 'Reported changes', prefix + 'changes_body', true);

    // AIS: the host's own section, whole
    panelEls.ais.appendChild(tiles);
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
    let displayedMmsi;
    function update(ship, h) {
        if (ship.mmsi !== displayedMmsi) {
            displayedMmsi = ship.mmsi;
            for (const group of panelEls.history.querySelectorAll('.sc-history-preview')) {
                group.classList.remove('is-expanded');
                const more = group.querySelector('.sc-more-pill');
                more.setAttribute('aria-expanded', 'false');
                more.hidden = group.querySelectorAll('.tl-item').length < 2;
            }
        }
        const u = h.units;
        const set = (e, v) => { e.textContent = v == null || v === '' ? 'N/A' : String(v); };
        const vessel = ship.mmsi_type === CLASS_A || ship.mmsi_type === CLASS_B;
        const sender = getMmsiTypeVal(ship);
        set(summary.type, ship.shiptype ? getShipTypeShort(ship.shiptype) : vessel ? null : sender);
        // the short type in the line, the full wording on hover (as the status pill does)
        summary.type.title = ship.shiptype ? getShipTypeFull(ship.shiptype) : '';
        const status = Number.isInteger(ship.status) && ship.status >= 0 && ship.status < 15 ? getStatusVal(ship) : null;
        // the pill says it in a word or two; the full wording on hover and on the Voyage tab
        summary.status.textContent = status ? getStatusShort(ship) : '';
        summary.status.title = status || '';
        summary.sep.hidden = summary.status.hidden = !status;
        // the navigational status as a pill: moving green, lying still amber, in trouble red
        summary.status.className = 'sc-sum-status sc-pill sc-pill--' + statusTone(ship.status);
        // the vessel's registry number, the MMSI when it has none
        summary.id.textContent = ship.imo != null ? 'IMO ' + ship.imo : ship.eni ? 'ENI ' + decodeHTMLEntities(ship.eni) : ship.mmsi != null ? 'MMSI ' + ship.mmsi : '';
        const hasPos = ship.lat != null && ship.lon != null;
        if (!hasPos) summary.pos.textContent = 'N/A';
        // the degree formats come back as markup (&deg;), so set it as such; the values are numbers
        else if (u.getPositionShort) summary.pos.innerHTML = u.getPositionShort(ship);
        else summary.pos.innerHTML = u.getLatValFormat(ship) + ', ' + u.getLonValFormat(ship);
        const places = ship.visits?.filter(v => v.inside) || [];
        summary.region.textContent = places?.length ? places[0].name : '';
        summary.region.hidden = !summary.region.textContent;
        // no area name: the coordinates are the main value, not a grey footnote
        summary.pos.classList.toggle('v', summary.region.hidden);
        const currentPlace = places.length ? {...places[0], runtime_id: places[0].id, place_version: ship.place_version, lat: ship.lat, lon: ship.lon} : null;
        setPortLink(summary.region, currentPlace, h.openPort, h.goTo);
        const dest = ship.destination && String(ship.destination).trim();
        const port = ship.matched_port;
        const portName = port && typeof port.name === 'string' && port.name.trim() ? port.name.trim() : null;
        summary.destLabel.textContent = portName ? 'Destination' : 'Reported destination';
        set(summary.dest, portName || decodeHTMLEntities(dest));
        summary.reported.textContent = portName && dest ? decodeHTMLEntities(dest) : '';
        summary.reported.hidden = !summary.reported.textContent;
        setPortLink(summary.dest, port, h.openPort, h.goTo);
        setPortLink(summary.pin, port, h.openPort, h.goTo);
        summary.pin.setAttribute('aria-label', portName ? 'Open ' + portName : 'Destination');
        // Vessel: a swatch in the ship type's colour, units small, absent values a faint dash
        const typeCell = panelEls.vessel.querySelector('[id$="_shiptype"]');
        if (typeCell) typeCell.title = ship.shiptype != null ? getShipTypeFull(ship.shiptype) : '';
        if (typeCell && ship.shiptype != null) {
            let sw = typeCell.querySelector('.sc-swatch');
            if (!sw) { sw = el('span', 'sc-swatch'); typeCell.prepend(sw); }
            // the host's track colour for this class; else the card's own palette
            sw.dataset.type = swatchType(ship.shipclass);
            sw.style.background = (h.typeColor && h.typeColor(ship)) || '';
        }
        for (const key of ['dimension', 'draught']) {
            const c = panelEls.vessel.querySelector(`[id$="_${key}"]`);
            const m = c && /^(.*\d)\s+([a-zA-Z]+)$/.exec(c.textContent.trim());
            if (m) {
                c.textContent = m[1].replace(/\s*x\s*/i, ' × ') + ' ';
                const unit = el('span', 'sc-unit');
                unit.textContent = m[2];
                c.appendChild(unit);
            }
        }
        // the vessel rows, and the position row, which lives on Voyage
        for (const c of [...panelEls.vessel.querySelectorAll('.mapcard-content-row > div > span:last-child, .sc-pos > div > span:last-child'),
            ...panelEls.voyage.querySelectorAll('.sc-pos > div > span:last-child')])
            c.classList.toggle('sc-empty', /^(-|N\/A)?$/.test(c.textContent.trim()));
        // Voyage: the ETA on one line, its time zone small like a unit
        const etaCell = panelEls.voyage.querySelector('[id$="_eta"]');
        const etaM = etaCell && /^(.*\d)\s+UTC$/.exec(etaCell.textContent.trim());
        if (etaM) {
            etaCell.textContent = etaM[1] + ' ';
            const z = el('span', 'sc-unit');
            z.textContent = 'UTC';
            etaCell.appendChild(z);
        }
        // Voyage: the status as the same pill as on Summary
        const voyStatus = panelEls.voyage.querySelector('[id$="status"]');
        if (voyStatus) {
            voyStatus.innerHTML = '';
            if (status) { const pill = el('span', 'sc-pill sc-pill--' + statusTone(ship.status)); pill.textContent = status; voyStatus.appendChild(pill); }
            else voyStatus.textContent = 'N/A';
        }
        const age = h.age ? h.age(ship) : null;
        summary.seen.textContent = age ? 'Last received ' + age + ' ago' : 'Last received unknown';
        // a dot that says how fresh: under 3 minutes live, under 30 stale, else lost
        // seconds from the host when it says them, else read back from the age it shows ("2d 3h", "1m 10s", "25s")
        const secs = h.ageSeconds ? h.ageSeconds(ship) : ageToSeconds(age);
        summary.seen.dataset.fresh = secs == null ? 'lost' : secs < 180 ? 'live' : secs < 1800 ? 'stale' : 'lost';
        // AIS tiles: how long ago, coloured the same way, and how many messages
        tileSeen.dataset.fresh = summary.seen.dataset.fresh;
        // "25s" reads as 25 with "s ago" small; "1m 10s" stays whole with "ago" small
        const m = age && /^(\d+)s$/.exec(age);
        tileSeen.lastChild.innerHTML = '';
        if (age) { tileSeen.lastChild.append((m ? m[1] : age) + ' '); const unit = el('span', 'sc-unit'); unit.textContent = (m ? 's ' : '') + 'ago'; tileSeen.lastChild.appendChild(unit); }
        else tileSeen.lastChild.textContent = 'N/A';
        tileCount.lastChild.innerHTML = '';
        if (ship.count != null) { tileCount.lastChild.append(String(ship.count) + ' '); const unit = el('span', 'sc-unit'); unit.textContent = 'received'; tileCount.lastChild.appendChild(unit); }
        else tileCount.lastChild.textContent = 'N/A';
        const hasEta = ship.eta_month != null && ship.eta_day != null && ship.eta_hour != null && ship.eta_minute != null;
        summary.eta.innerHTML = '';
        if (hasEta) { const b = document.createElement('b'); b.textContent = getEtaVal(ship); summary.eta.append('ETA ', b); }
        summary.eta.hidden = !hasEta;
    }

    return { ...shell, cells, update };
}
