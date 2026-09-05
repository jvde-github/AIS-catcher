/* ============================================================================
   shipcard-classic.js — the ship card as a fold of sections
   Vessel, antenna position, voyage, reported changes, the speed and draught
   charts, each a section that opens and closes; rows can be kept for the
   compact card and the card itself folds to its header. The host adds its own
   sections after the body. What the cells say comes from shipcard.js.
   ========================================================================= */
// how the common sections start out; a host adds its own to the same object
export const SECTIONS = { vessel: true, hull: true, voyage: true, changes: false, speed: false, draught: false };

// sections whose row is one body the host draws into, not label/value cells
export const BODIES = ['hull', 'changes', 'speed', 'draught'];

const MAX = 'card-max-only';

export function spec() {
    return [
        { section: { key: 'vessel', label: 'Vessel', open: true }, cls: MAX,
          fields: [{ key: 'mmsi', label: 'MMSI' }, { key: 'callsign', label: 'Callsign' }, { key: 'imo', label: 'IMO' }] },
        { cls: MAX, fields: [{ key: 'country', label: 'Country' }, { key: 'type', label: 'Sender' }, { key: 'shiptype', label: 'Ship type' }] },
        { cls: MAX, fields: [{ key: 'dimension', label: 'Dimension' }, { key: 'draught', label: 'Draught' }, { key: 'bluesign', label: 'Blue sign' }] },
        { section: { key: 'hull', label: 'AIS antenna position', open: true }, cls: MAX, fields: [] },
        { section: { key: 'voyage', label: 'Voyage', open: true }, cls: MAX + ' row-wide-first',
          fields: [{ key: 'status', label: 'Status' }, { key: 'altitude', label: 'Altitude' }] },
        { fields: [{ key: 'speed', label: 'Speed' }, { key: 'cog', label: 'Course' }, { key: 'heading', label: 'Heading' }] },
        { cls: MAX + ' row-wide-first', fields: [{ key: 'destination', label: 'Destination' }, { key: 'eta', label: 'ETA' }] },
        { cls: MAX, fields: [{ key: 'lat', label: 'Latitude' }, { key: 'lon', label: 'Longitude' }, { key: 'region', label: 'Region' }] },
        { section: { key: 'changes', label: 'Reported changes', open: false }, cls: MAX, fields: [] },
        { section: { key: 'speed', label: 'Speed chart', open: false }, cls: MAX, fields: [] },
        { section: { key: 'draught', label: 'Draught chart', open: false }, cls: MAX, fields: [] },
    ];
}

/* Builds the body into `mount` with the host's id prefix, so `<prefix>mmsi`,
   `<prefix>hull_body` and `<prefix>changes_section` exist for the host's own
   code. `rowAttrs` go on every row; `headAttr(head, key)` lets a host wire
   its section heads its own way. Returns the value cells by key. */
export function build(card, mount, prefix, o) {
    o = o || {};
    const cells = card.rows(mount, spec(), { group: 'ship', idPrefix: prefix, attrs: o.rowAttrs, headId: (k) => prefix + k + '_head' });
    BODIES.forEach((k) => {
        const row = mount.querySelector('[data-section="' + k + '"]:not(.card-section)');
        row.id = prefix + k + '_section';
        const body = document.createElement('div');
        body.id = prefix + k + '_body';
        body.className = 'hist-wrap';
        row.appendChild(body);
    });
    if (o.headAttr) mount.querySelectorAll('.card-section').forEach((h) => o.headAttr(h, h.dataset.section));
    return cells;
}
