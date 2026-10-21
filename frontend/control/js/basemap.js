// The background the panel's maps share: streets, a light grey canvas or
// satellite imagery, the seamarks over it, a switch between the three and the
// credits each source asks for. The choice is kept per browser, for every map.

import TileLayer from 'ol/layer/Tile.js';
import OSM from 'ol/source/OSM.js';
import XYZ from 'ol/source/XYZ.js';

const ICON = {
    streets: '<svg class="pe-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linejoin="round" aria-hidden="true"><path d="M3 6l6-2 6 2 6-2v14l-6 2-6-2-6 2z"/><path d="M9 4v14M15 6v14"/></svg>',
    light: '<svg class="pe-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linejoin="round" aria-hidden="true"><path d="M3 6l6-2 6 2 6-2v14l-6 2-6-2-6 2z" fill="currentColor" fill-opacity=".15"/></svg>',
    satellite: '<svg class="pe-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><circle cx="12" cy="12" r="9"/><path d="M3 12h18M12 3c2.5 2.6 3.8 5.6 3.8 9s-1.3 6.4-3.8 9c-2.5-2.6-3.8-5.6-3.8-9S9.5 5.6 12 3z"/></svg>'
};
const OSM_CREDIT = '© <a href="https://www.openstreetmap.org/copyright" target="_blank" rel="noopener">OpenStreetMap</a> contributors';
const SEAMARK_CREDIT = 'Seamarks © <a href="https://www.openseamap.org" target="_blank" rel="noopener">OpenSeaMap</a>';
const ESRI = 'https://server.arcgisonline.com/ArcGIS/rest/services/';
const BASEMAPS = {
    streets: {label: 'Streets', dim: 0.1, credit: OSM_CREDIT},
    light: {label: 'Light', url: ESRI + 'Canvas/World_Light_Gray_Base/MapServer/tile/{z}/{y}/{x}', maxZoom: 16,
            credit: 'Tiles © Esri — Esri, HERE, Garmin, ' + OSM_CREDIT},
    satellite: {label: 'Satellite', url: ESRI + 'World_Imagery/MapServer/tile/{z}/{y}/{x}', maxZoom: 19,
                credit: 'Imagery © Esri — Esri, Maxar, Earthstar Geographics and the GIS User Community'},
};
const KEY = 'places.basemap';

// Puts the background under the map's layers, and its switch and credits in
// `wrap`, the .pe-map the map sits in.
export function addBasemap(map, wrap) {
    const seamark = new TileLayer({
        className: 'place-seamark', // its own canvas: sharing one with the base map left it unrendered
        source: new XYZ({url: 'https://tiles.openseamap.org/seamark/{z}/{x}/{y}.png'}),
        opacity: 0.5
    });
    const base = new TileLayer({className: 'place-base'});
    let basemap = 'streets';
    try { if (BASEMAPS[localStorage.getItem(KEY)]) basemap = localStorage.getItem(KEY); } catch { /* private mode */ }
    // the street map is dimmed a little so what is drawn over it stands out
    base.on('postrender', e => {
        const dim = BASEMAPS[basemap].dim;
        if (!dim) return;
        e.context.save();
        e.context.setTransform(1, 0, 0, 1, 0, 0);
        e.context.fillStyle = `rgba(0, 0, 0, ${dim})`;
        e.context.fillRect(0, 0, e.context.canvas.width, e.context.canvas.height);
        e.context.restore();
    });

    const switcher = document.createElement('div');
    switcher.className = 'pe-toolbar pe-toolbar--layers';
    switcher.setAttribute('role', 'radiogroup');
    switcher.setAttribute('aria-label', 'Map style');
    switcher.innerHTML = Object.entries(BASEMAPS).map(([key, b]) =>
        `<button class="pe-segment" type="button" role="radio" aria-checked="false" data-basemap="${key}">${ICON[key]}${b.label}</button>`).join('');
    const credit = document.createElement('div');
    credit.className = 'pe-attribution';
    wrap.append(switcher, credit);

    function set(key, remember = true) {
        if (!BASEMAPS[key]) return;
        basemap = key;
        const b = BASEMAPS[key];
        base.setSource(b.url ? new XYZ({url: b.url, maxZoom: b.maxZoom, crossOrigin: 'anonymous'}) : new OSM());
        for (const seg of switcher.querySelectorAll('.pe-segment')) {
            const on = seg.dataset.basemap === key;
            seg.setAttribute('aria-checked', String(on));
            seg.tabIndex = on ? 0 : -1;
        }
        credit.innerHTML = b.credit + ' · ' + SEAMARK_CREDIT;
        if (remember) try { localStorage.setItem(KEY, key); } catch { /* private mode */ }
    }
    switcher.addEventListener('keydown', e => {
        if (e.key !== 'ArrowLeft' && e.key !== 'ArrowRight') return;
        e.preventDefault();
        const keys = Object.keys(BASEMAPS), next = keys[(keys.indexOf(basemap) + (e.key === 'ArrowRight' ? 1 : keys.length - 1)) % keys.length];
        set(next);
        switcher.querySelector(`[data-basemap="${next}"]`).focus();
    });
    for (const seg of switcher.querySelectorAll('.pe-segment'))
        seg.addEventListener('click', () => set(seg.dataset.basemap));

    set(basemap, false);
    map.getLayers().insertAt(0, seamark);
    map.getLayers().insertAt(0, base);
}
