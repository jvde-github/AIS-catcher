const BASE_URL = 'https://www.aiscatcher.org/tiles/ducting/';
const MANIFEST_URL = BASE_URL + 'manifest.json';
const POLL_INTERVAL_MS = 10 * 60 * 1000;

export function init(addOverlayLayer) {
    const ol = window.ol;

    const layer = new ol.layer.Tile({
        name: 'atmospheric_ducting',
        title: 'Atmospheric Ducting',
        type: 'overlay',
        opacity: 0.7,
    });

    let intervalId = null;
    let activeCycle = null;

    async function refresh() {
        if (document.hidden) return false;
        try {
            const response = await fetch(MANIFEST_URL, { cache: 'no-store' });
            const manifest = await response.json();
            const sbd = (manifest.layers || []).find((l) => l.id === 'sbd');
            if (!sbd) return false;
            const frame = (sbd.frames || []).find((f) => f.id === 'now') || sbd.frames?.[0];
            if (!frame) return false;
            if (sbd.cycle === activeCycle) return true;

            layer.setSource(new ol.source.XYZ({
                url: BASE_URL + frame.url_template,
                maxZoom: manifest.zoom_max,
                attributions: 'Surface-based duct (NOAA GFS) · <a href="https://www.aiscatcher.org" target="_blank">aiscatcher.org</a>',
            }));
            activeCycle = sbd.cycle;
            return true;
        } catch (error) {
            console.error('Error refreshing ducting overlay:', error);
            return false;
        }
    }

    layer.on('change:visible', () => {
        if (layer.getVisible()) {
            refresh();
            if (intervalId === null) {
                intervalId = window.setInterval(refresh, POLL_INTERVAL_MS);
            }
        } else if (intervalId !== null) {
            window.clearInterval(intervalId);
            intervalId = null;
        }
    });

    document.addEventListener('visibilitychange', () => {
        if (!document.hidden && layer.getVisible()) refresh();
    });

    addOverlayLayer('Atmospheric Ducting', layer);
}
