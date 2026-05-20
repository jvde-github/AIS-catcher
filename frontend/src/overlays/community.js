import { settings } from '../core/state.js';

let initialized = false;

function getFeedSprite(ship) {
    const { type, class_b: classB, direction: dir } = ship;
    switch (type) {
        case 2: return { cx: 0, cy: 40, imgSize: 20, rot: 0 };
        case 3: return { cx: 0, cy: 60, imgSize: 25, rot: dir != null ? dir * Math.PI / 180 : 0 };
        default: {
            const cx = classB ? 20 : 0;
            const isMoving = type === 0 && dir != null;
            return { cx, cy: isMoving ? 0 : 20, imgSize: 20, rot: isMoving ? dir * Math.PI / 180 : 0 };
        }
    }
}

function decodeCompactShips(buffer) {
    const view = new DataView(buffer);
    if (view.byteLength < 12) return [];

    let offset = 8;
    const count = view.getInt32(offset, false); offset += 4;

    const ships = [];
    for (let i = 0; i < count; i++) {
        if (offset + 11 > view.byteLength) break;

        const latRaw = view.getInt32(offset, false);  offset += 4;
        const lonRaw = view.getInt32(offset, false);  offset += 4;
        const flags  = view.getUint8(offset);         offset += 1;
        const dirRaw = view.getUint16(offset, false); offset += 2;

        const lat = (latRaw === 0x7FFFFFFF) ? null : latRaw / 600000.0;
        const lon = (lonRaw === 0x7FFFFFFF) ? null : lonRaw / 600000.0;
        if (lat === null || lon === null) continue;

        ships.push({
            lat, lon,
            type:      flags & 0x03,
            approx:   (flags >> 2) & 1,
            class_b:  (flags >> 3) & 1,
            direction: (dirRaw === 0xFFFF) ? null : dirRaw / 10.0,
        });
    }
    return ships;
}

export function init(map, addOverlayLayer, debounce, markerLayer) {
    if (initialized) return;
    initialized = true;

    const ol = window.ol;

    const button = document.getElementById('xchange');
    if (button) {
        button.classList.remove('fill-red');
        button.classList.add('fill-green');
    }

    const feedVector = new ol.source.Vector({ features: [] });

    const feedLayer = new ol.layer.Vector({
        source: feedVector,
        style: (feature) => {
            const sp = getFeedSprite(feature.ship);
            return new ol.style.Style({
                image: new ol.style.Icon({
                    src: "https://api.aiscatcher.org/community/v1/sprites.png",
                    offset: [sp.cx, sp.cy],
                    size: [sp.imgSize, sp.imgSize],
                    rotation: sp.rot,
                    scale: settings.icon_scale * 0.8,
                    opacity: 0.5,
                }),
            });
        },
    });

    let ships = [];

    async function fetchShips() {
        try {
            const extent = map.getView().calculateExtent(map.getSize());
            const ll = ol.proj.transformExtent(extent, 'EPSG:3857', 'EPSG:4326');
            const url = `https://api.aiscatcher.org/community/v1/ships?${map.getView().getZoom()},${ll[1] - 0.0045},${ll[0] - 0.0045},${ll[3] + 0.0045},${ll[2] + 0.0045}`;
            const response = await fetch(url);
            ships = decodeCompactShips(await response.arrayBuffer());
            return true;
        } catch (error) {
            console.log("Failed loading community ships: " + error);
            return false;
        }
    }

    function redraw() {
        feedVector.clear();
        for (const ship of ships) {
            const feature = new ol.Feature({
                geometry: new ol.geom.Point(ol.proj.fromLonLat([ship.lon, ship.lat])),
            });
            feature.ship = ship;
            feature.community = true;
            feature.link = `https://www.aiscatcher.org/livemap?lat=${ship.lat}&lon=${ship.lon}&zoom=16.00`;
            feedVector.addFeature(feature);
        }
    }

    const update = debounce(() => {
        if (!feedLayer.getVisible()) return;
        fetchShips().then((ok) => { if (ok) redraw(); });
    }, 250);

    map.on('moveend', update);
    feedLayer.on('change:visible', () => { if (feedLayer.getVisible()) update(); });

    addOverlayLayer("Community Feed", feedLayer);

    const layers = map.getLayers();
    layers.remove(feedLayer);
    const idx = markerLayer ? layers.getArray().indexOf(markerLayer) : layers.getLength();
    layers.insertAt(idx >= 0 ? idx : layers.getLength(), feedLayer);

    if (feedLayer.getVisible()) update();
}

