// One shape out of a GeoJSON text: a bare geometry, a Feature, or a
// FeatureCollection holding exactly one. Anything else is a question the
// caller would have to answer, so it is an error here.
const SHAPES = ['Point', 'Polygon', 'MultiPolygon'];

export function shapeFromGeoJSON(text) {
    let doc;
    try { doc = JSON.parse(text); } catch { throw Error('Not valid JSON.'); }
    if (doc?.type === 'FeatureCollection') {
        const features = Array.isArray(doc.features) ? doc.features : [];
        if (features.length !== 1) throw Error(`Import one shape; this collection holds ${features.length}.`);
        doc = features[0];
    }
    const geometry = doc?.type === 'Feature' ? doc.geometry : doc;
    if (!geometry || !SHAPES.includes(geometry.type) || !Array.isArray(geometry.coordinates))
        throw Error('Import a Point, Polygon or MultiPolygon.');
    return {type: geometry.type, coordinates: geometry.coordinates};
}
