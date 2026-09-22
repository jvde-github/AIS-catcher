// A summary supports selection; fetch the complete Feature before editing.
export function placeFeature(row) {
    const properties = {schema_version:2, revision:row.revision, name:row.label,
        place_type:row.place_type, size:row.size};
    if (row.code && row.place_type === 'port') properties.codes={unlocode:[row.code]};
    if (row.part_of) properties.part_of=row.part_of;
    if (row.no) properties.no=row.no;
    if (row.redirect_to) properties.redirect_to=row.redirect_to;
    if (row.country) properties.country=row.country;
    if (row.category) properties.attributes={area_subtype:row.category};
    return {type:'Feature',id:row.uuid,runtime_id:row.runtime_id,has_geometry:row.has_geometry,
        properties,geometry:{type:'Point',coordinates:[row.lon,row.lat]}};
}
