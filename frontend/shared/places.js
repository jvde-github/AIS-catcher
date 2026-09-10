// One summary format serves both the map feed and the managed place list.
export function placeFeature(row) {
    const properties = {schema_version:1, revision:row.revision, name:row.label,
        place_type:row.place_type, size:row.size};
    if (row.place_type === 'port') {
        properties.unlocode = row.code;
        if (row.parent_unlocode) properties.parent_unlocode = row.parent_unlocode;
    } else if (row.code) properties.port_unlocode = row.code;
    if (row.category) properties.category = row.category;
    return {type:'Feature', id:row.uuid, runtime_id:row.runtime_id,
        has_geometry:row.has_geometry, properties,
        geometry:{type:'Point', coordinates:[row.lon,row.lat]}};
}
