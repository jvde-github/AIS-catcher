export function init(addOverlayLayer) {
    const ol = window.ol;

    const layer = new ol.layer.Tile({
        name: 'rainviewer_radar',
        title: 'RainViewer Radar',
        type: 'overlay',
        opacity: 0.7,
    });

    let intervalId = null;

    async function refresh() {
        if (document.hidden) return false;
        try {
            const response = await fetch("https://api.rainviewer.com/public/weather-maps.json?_=" + Date.now());
            const data = await response.json();
            const latestRadar = data.radar.past[data.radar.past.length - 1];
            layer.setSource(new ol.source.XYZ({
                url: `https://tilecache.rainviewer.com${latestRadar.path}/512/{z}/{x}/{y}/6/1_1.png`,
                attributions: '<a href="https://www.rainviewer.com/api.html" target="_blank">RainViewer.com</a>',
                crossOrigin: 'anonymous',
                maxZoom: 7,
            }));
            return true;
        } catch (error) {
            console.error("Error refreshing RainViewer layers:", error);
            return false;
        }
    }

    layer.on('change:visible', () => {
        if (layer.getVisible()) {
            refresh();
            if (intervalId === null) {
                intervalId = window.setInterval(refresh, 10 * 60 * 1000);
            }
        } else if (intervalId !== null) {
            window.clearInterval(intervalId);
            intervalId = null;
        }
    });

    document.addEventListener('visibilitychange', () => {
        if (!document.hidden && layer.getVisible()) refresh();
    });

    addOverlayLayer("RainViewer Radar", layer);
}
