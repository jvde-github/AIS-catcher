/* ============================================================================
   shipcard-tabs.js — the ship card as five remembered tabs
   Summary · Vessel · Voyage · AIS · History, one view at a time in place of
   the fold of sections. Summary is a fixed composition: a head with the type
   and reported status and the class, speed, course and heading, then the
   current location against the reported destination, when the vessel was
   last heard and its ETA. The other tabs hold the same rows the classic card
   shows, with the reported changes beside the voyage they belong to; AIS is
   the host's own section, handed in as one element. The selected tab is the
   host's to remember. No tab at all is the folded card: the tab bar and the
   Summary head show, and a host that folds (a cramped pane) lets a tap on
   the open tab fold it again.
   Nothing here selects rows; a host using this layout keeps the card
   maximised and hides its chevron. What the cells say comes from
   shipcard.js; Summary's own cells are filled by update().
   ========================================================================= */
import { fieldRows } from './components.js';
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
   `o.foldable()` says whether a tap on the open tab folds. Returns the cells
   for shipcard.populate(), select(key), active(), update(ship, h). */
export function build(mount, prefix, o) {
    o = o || {};
    mount.innerHTML = '';
    mount.classList.add('sc-host');
    const list = el('div', 'sc-tabs', { role: 'tablist', 'aria-label': 'Vessel details' });
    const panels = el('div', 'sc-panels');
    const tabEls = {}, panelEls = {};
    for (const [key, label] of TABS) {
        const b = el('button', 'sc-tab', { type: 'button', role: 'tab', id: prefix + 'tab_' + key, 'aria-controls': prefix + 'panel_' + key, 'aria-selected': 'false', tabindex: '-1' });
        b.textContent = label;
        b.dataset.tab = key;
        const p = el('div', 'sc-panel', { role: 'tabpanel', id: prefix + 'panel_' + key, 'aria-labelledby': prefix + 'tab_' + key });
        p.hidden = true;
        list.appendChild(b);
        panels.appendChild(p);
        tabEls[key] = b;
        panelEls[key] = p;
    }
    const head = el('div', 'sc-head');
    mount.appendChild(list);
    mount.appendChild(head);
    mount.appendChild(panels);

    const cells = {};
    const rows = (panel, spec) => {
        const c = fieldRows(panel, spec, { rowClass: 'mapcard-content-row card-row sc-row', idPrefix: prefix });
        for (const k in c) cells[k] = c[k];
    };

    // Summary: a fixed composition, filled by update(); the head stays up
    // while the card is folded
    head.innerHTML =
        '<div class="sc-sum-line"><span class="sc-sum-text"><span class="sc-sum-type"></span><span class="sc-sum-sep">·</span><span class="sc-sum-status"></span></span>' +
        '<span class="sc-sum-id"></span></div>' +
        '<div class="sc-metrics"><div><span>Class</span><b class="sc-sum-class"></b></div><div><span>Speed</span><b class="sc-sum-speed"></b></div>' +
        '<div><span>Course</span><b class="sc-sum-cog"></b></div><div><span>Heading</span><b class="sc-sum-heading"></b></div></div>';
    panelEls.summary.innerHTML =
        '<div class="sc-strip">' +
        '<div class="ends"><div><div class="l">Current location</div><div class="v sc-sum-region"></div><div class="sc-sum-pos"></div></div>' +
        '<div><div class="l">Reported destination</div><div class="v sc-sum-dest"></div></div></div>' +
        '<div class="line" aria-hidden="true"><span class="pip"></span><span class="t"><i></i></span><span class="pin">' + PIN + '</span></div>' +
        '<div class="under"><span class="sc-sum-seen"></span><span class="sc-sum-eta"></span></div>' +
        '</div>';
    const q = (c) => mount.querySelector('.' + c);
    const summary = { type: q('sc-sum-type'), sep: q('sc-sum-sep'), status: q('sc-sum-status'), id: q('sc-sum-id'), pos: q('sc-sum-pos'), region: q('sc-sum-region'),
                      dest: q('sc-sum-dest'), seen: q('sc-sum-seen'), eta: q('sc-sum-eta'),
                      cls: q('sc-sum-class'), speed: q('sc-sum-speed'), cog: q('sc-sum-cog'), heading: q('sc-sum-heading') };

    // Vessel
    rows(panelEls.vessel, [
        { fields: [{ key: 'mmsi', label: 'MMSI' }, { key: 'callsign', label: 'Callsign' }, { key: 'imo', label: 'IMO' }] },
        { fields: [{ key: 'country', label: 'Country' }, { key: 'type', label: 'Sender' }, { key: 'shiptype', label: 'Ship type' }] },
        { fields: [{ key: 'dimension', label: 'Dimension' }, { key: 'draught', label: 'Draught' }, { key: 'bluesign', label: 'Blue sign' }] },
    ]);
    group(panelEls.vessel, 'Antenna position', prefix + 'hull_body');

    // Voyage, with what the vessel reported changing
    rows(panelEls.voyage, [
        { cls: 'row-wide-first', fields: [{ key: 'status', label: 'Status' }, { key: 'altitude', label: 'Altitude' }] },
        { fields: [{ key: 'speed', label: 'Speed' }, { key: 'cog', label: 'Course' }, { key: 'heading', label: 'Heading' }] },
        { cls: 'row-wide-first', fields: [{ key: 'destination', label: 'Destination' }, { key: 'eta', label: 'ETA' }] },
        { fields: [{ key: 'lat', label: 'Latitude' }, { key: 'lon', label: 'Longitude' }, { key: 'region', label: 'Region' }] },
    ]);
    group(panelEls.voyage, 'Reported changes', prefix + 'changes_body');

    // AIS: the host's own section, whole
    if (o.slot) panelEls.ais.appendChild(o.slot);
    else panelEls.ais.innerHTML = '<span class="dim-note">No reception data</span>';

    // History
    group(panelEls.history, 'Speed', prefix + 'speed_body');
    group(panelEls.history, 'Draught', prefix + 'draught_body');

    let active;   // undefined until the first select, so a folded start still applies
    function select(key, notify) {
        key = key == null ? null : validTab(key);
        if (key === active) return;
        active = key;
        for (const [k] of TABS) {
            tabEls[k].setAttribute('aria-selected', String(k === key));
            tabEls[k].tabIndex = k === key || (!key && k === DEFAULT_TAB) ? 0 : -1;
            panelEls[k].hidden = k !== key;
        }
        mount.classList.toggle('sc-folded', !key);
        head.hidden = key != null && key !== 'summary';
        panels.scrollTop = 0;
        if (notify !== false && o.onTab) o.onTab(key);
    }
    list.addEventListener('click', (e) => {
        const b = e.target.closest('.sc-tab');
        if (!b) return;
        select(b.dataset.tab === active && o.foldable && o.foldable() ? null : b.dataset.tab);
    });
    list.addEventListener('keydown', (e) => {
        const keys = TABS.map(([k]) => k);
        let i = keys.indexOf(active);
        if (e.key === 'ArrowRight') i = i < 0 ? 0 : (i + 1) % keys.length;
        else if (e.key === 'ArrowLeft') i = i < 0 ? keys.length - 1 : (i - 1 + keys.length) % keys.length;
        else if (e.key === 'Home') i = 0;
        else if (e.key === 'End') i = keys.length - 1;
        else return;
        e.preventDefault();
        select(keys[i]);
        tabEls[keys[i]].focus();
    });
    select(o.tab, false);

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
        summary.id.textContent = ship.imo != null ? 'IMO ' + ship.imo : ship.eni ? 'ENI ' + ship.eni : ship.mmsi != null ? 'MMSI ' + ship.mmsi : '';
        const hasPos = ship.lat != null && ship.lon != null;
        summary.pos.innerHTML = hasPos ? u.getLatValFormat(ship) + ', ' + u.getLonValFormat(ship) : 'N/A';
        const region = hasPos && ship.region != null ? (h.regionName ? h.regionName(ship.region) : ship.region) : null;
        summary.region.textContent = region && region !== '-' ? region : '';
        summary.region.hidden = !summary.region.textContent;
        const dest = ship.destination && String(ship.destination).trim();
        set(summary.dest, dest);
        const age = h.age ? h.age(ship) : null;
        summary.seen.textContent = age ? 'Last received ' + age + ' ago' : 'Last received unknown';
        const hasEta = ship.eta_month != null && ship.eta_day != null && ship.eta_hour != null && ship.eta_minute != null;
        summary.eta.textContent = hasEta ? 'ETA ' + getEtaVal(ship) : '';
        summary.eta.hidden = !hasEta;
        set(summary.cls, vessel ? sender.replace(/^Class /, '') : sender);
        set(summary.speed, ship.speed != null ? u.getSpeedVal(ship.speed) + ' ' + u.getSpeedUnit() : null);
        set(summary.cog, ship.cog != null ? Number(ship.cog).toFixed(1) + '°' : null);
        set(summary.heading, ship.heading != null ? Number(ship.heading).toFixed(0) + '°' : null);
    }

    return { cells, select, active: () => active, update, scrollTop: () => { panels.scrollTop = 0; } };
}
