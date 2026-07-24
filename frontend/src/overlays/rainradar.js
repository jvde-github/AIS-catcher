import { createPollingTileLayer } from './pollingtile.js';

export function init(addOverlayLayer) {
    const layer = createPollingTileLayer({
        name: 'rainviewer_radar',
        title: 'RainViewer Radar',
        getSourceOptions: async () => {
            const response = await fetch("https://api.rainviewer.com/public/weather-maps.json?_=" + Date.now());
            const data = await response.json();
            const latestRadar = data.radar.past[data.radar.past.length - 1];
            return {
                url: `https://tilecache.rainviewer.com${latestRadar.path}/512/{z}/{x}/{y}/6/1_1.png`,
                attributions: '<a href="https://www.rainviewer.com/api.html" target="_blank">RainViewer.com</a>',
                crossOrigin: 'anonymous',
                maxZoom: 7,
            };
        },
    });

    addOverlayLayer("RainViewer Radar", layer);
}
