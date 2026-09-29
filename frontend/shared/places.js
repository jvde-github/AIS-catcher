// A summary supports selection; fetch the complete Feature before editing.
export function placeFeature(row) {
    const properties = {schema_version:3, revision:row.revision, name:row.label,
        place_type:row.place_type, rank:row.rank};
    if (row.code && row.place_type === 'port') properties.codes={unlocode:[row.code]};
    if (row.part_of) properties.part_of=row.part_of;
    if (row.no) properties.no=row.no;
    if (row.redirect_to) properties.redirect_to=row.redirect_to;
    if (row.country) properties.country=row.country;
    if (row.category) properties.category=row.category;
    return {type:'Feature',id:row.uuid,runtime_id:row.runtime_id,has_geometry:row.has_geometry,
        properties,geometry:{type:'Point',coordinates:[row.lon,row.lat]}};
}

const KINDS = ['port', 'section', 'terminal', 'berth', 'anchorage', 'mooring', 'marina', 'guardzone', 'sector', 'water'];
const UUID = /^[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}$/;
const LOCODE = /^[A-Z]{2}[A-Z0-9]{3}$/;
const plain = v => v && typeof v === 'object' && !Array.isArray(v);

// The properties of a Feature from anywhere, read as a place: what is recognised
// is carried over under its current name, older spellings included, and the rest
// travels along untouched. `ports` gives the id of the port designating a
// UN/LOCODE, for a file that names its port by code. Returns the properties and
// what could not be carried over.
export function importPlace(properties, ports = {}) {
    const p = plain(properties) ? {...properties} : {}, notes = [];
    const old = p.schema_version === 1;
    const details = plain(p.details) ? {...p.details} : {};
    const attributes = plain(p.attributes) ? {...p.attributes} : {};
    const source = plain(p.geometry_source) ? {...p.geometry_source} : {};
    const codes = plain(p.codes) ? {...p.codes} : {};

    if (p.place_type === 'custom' || p.place_type === 'area') p.place_type = 'guardzone';
    if (!KINDS.includes(p.place_type)) {
        if (p.place_type) notes.push(`Type "${p.place_type}" is not known: imported as a guard zone.`);
        p.place_type = 'guardzone';
    }
    if (typeof p.unlocode === 'string' && p.unlocode) {
        if (p.place_type === 'port' && LOCODE.test(p.unlocode)) codes.unlocode = [p.unlocode];
        else codes.own = [...(codes.own || []), 'local:' + p.unlocode];
    }
    if (typeof details.code === 'string' && details.code) codes.own = [...(codes.own || []), 'local:' + details.code];
    if (p.place_type !== 'port' && codes.unlocode) {
        notes.push('Only a port designates a UN/LOCODE: the code was left out.');
        delete codes.unlocode;
    }

    const named = p.parent_unlocode || p.port_unlocode;
    if (UUID.test(details.terminal || '')) p.part_of = details.terminal;
    if (p.part_of !== null && !UUID.test(p.part_of || '')) {
        delete p.part_of;
        if (named && ports[named]) p.part_of = ports[named];
        else if (named) notes.push(`Port ${named} is not in this catalogue: no parent was set.`);
    }

    if (!p.category && attributes.area_subtype) p.category = attributes.area_subtype;
    if (typeof p.category !== 'string' || !p.category) delete p.category;
    else if (old) p.category = p.category.toLowerCase();

    if (p.rank === undefined) p.rank = p.size;
    if (!(Number.isInteger(p.rank) && p.rank >= 0 && p.rank <= 3)) delete p.rank;
    if (!UUID.test(p.redirect_to || '')) delete p.redirect_to;
    if (typeof p.name !== 'string' || !p.name.trim()) p.name = 'Imported place';

    for (const key of ['source', 'url', 'osm', 'confidence'])
        if (key in details) source[key] = details[key];
    for (const key of ['source', 'url', 'osm', 'confidence', 'code', 'terminal']) delete details[key];
    delete attributes.area_subtype;
    Object.assign(attributes, details);
    for (const [key, value] of [['codes', codes], ['attributes', attributes], ['geometry_source', source]]) {
        if (Object.keys(value).length) p[key] = value;
        else delete p[key];
    }
    for (const key of ['unlocode', 'port_unlocode', 'parent_unlocode', 'size', 'details']) delete p[key];
    p.schema_version = 3;
    return {properties: p, notes};
}
