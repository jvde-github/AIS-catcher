// Shared plumbing for XYZ tile overlays that poll a remote endpoint: owns the
// layer, the visible-only refresh interval and the visibilitychange refresh.

import TileLayer from 'ol/layer/Tile';
import XYZ from 'ol/source/XYZ';

const POLL_INTERVAL_MS = 10 * 60 * 1000;

export function createPollingTileLayer({ name, title, attributions, getSourceOptions }) {
    const layer = new TileLayer({
        name,
        title,
        type: 'overlay',
        opacity: 0.7,
    });

    // The source only exists once the overlay is switched on, so the credit is
    // kept on the layer too - the menu has to show it before first activation.
    if (attributions) layer.set('attributions', attributions);

    let intervalId = null;

    async function refresh() {
        if (document.hidden) return false;
        try {
            const options = await getSourceOptions();
            if (!options) return false;
            layer.setSource(new XYZ({ attributions, ...options }));
            return true;
        } catch (error) {
            console.error(`Error refreshing ${title} overlay:`, error);
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

    return layer;
}
