// Built-in server tile sources are configuration data, not executable plugins.
// Retain unchanged OpenLayers instances and leave user/plugin layers alone.
export function createServerMaps({ol, basemaps, overlays, getMap, refresh}) {
    const installed = new Map();
    return function apply(sources = []) {
        const wanted = new Map(sources.map(s => [s.id, s]));
        for (const [id, entry] of installed) {
            if (JSON.stringify(wanted.get(id)) === JSON.stringify(entry.source)) continue;
            const registry = entry.source.overlay ? overlays() : basemaps();
            if (registry[entry.source.name] === entry.layer) {
                delete registry[entry.source.name];
                if (entry.previous) registry[entry.source.name] = entry.previous;
            }
            getMap()?.removeLayer(entry.layer);
            if (entry.source.overlay) document.getElementById(entry.source.name)?.closest('.overlay-row')?.remove();
            installed.delete(id);
        }
        for (const source of sources) {
            if (installed.has(source.id)) continue;
            const options = {url: source.url, attributions: source.attribution,
                minZoom: source.minZoom, maxZoom: source.maxZoom};
            if (source.resolutions?.length) options.tileGrid = new ol.tilegrid.TileGrid({
                extent: ol.proj.get('EPSG:3857').getExtent(),
                origin: ol.extent.getTopLeft(ol.proj.get('EPSG:3857').getExtent()),
                minZoom: source.minZoom, resolutions: source.resolutions, tileSize: [256, 256],
            });
            const layer = new ol.layer.Tile({source: new ol.source.XYZ(options), visible: false});
            const registry = source.overlay ? overlays() : basemaps();
            installed.set(source.id, {source, layer, previous: registry[source.name]});
            registry[source.name] = layer;
            // Overlays sit above base maps but below vessels and annotations.
            const layers = getMap()?.getLayers();
            if (layers) {
                let index = 0;
                if (source.overlay) {
                    const background = [...Object.values(basemaps()), ...Object.values(overlays())];
                    for (const existing of background)
                        index = Math.max(index, layers.getArray().indexOf(existing) + 1);
                }
                layers.insertAt(index, layer);
            }
        }
        if (getMap()) refresh();
    };
}
