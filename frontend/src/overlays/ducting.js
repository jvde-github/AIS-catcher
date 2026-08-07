import { createPollingTileLayer } from './pollingtile.js';

const BASE_URL = 'https://www.aiscatcher.org/tiles/ducting/';
const MANIFEST_URL = BASE_URL + 'manifest.json';

export function init(addOverlayLayer) {
    let activeCycle = null;

    const layer = createPollingTileLayer({
        name: 'atmospheric_ducting',
        title: 'Atmospheric Ducting',
        attributions: 'Surface-based duct (NOAA GFS) · <a href="https://www.aiscatcher.org" target="_blank">aiscatcher.org</a>',
        getSourceOptions: async () => {
            const response = await fetch(MANIFEST_URL, { cache: 'no-store' });
            const manifest = await response.json();
            const sbd = (manifest.layers || []).find((l) => l.id === 'sbd');
            if (!sbd) return null;
            const frame = (sbd.frames || []).find((f) => f.id === 'now') || sbd.frames?.[0];
            if (!frame) return null;
            if (sbd.cycle === activeCycle) return null;
            activeCycle = sbd.cycle;
            return {
                url: BASE_URL + frame.url_template,
                maxZoom: manifest.zoom_max,
            };
        },
    });

    addOverlayLayer('Atmospheric Ducting', layer);
}
