// What a new revision of the server's configuration means on this page: its
// features, its maps, the tab on screen, the plugins. server-config.js is the
// transport, shared with bootstrap, so it knows none of this.

import { config } from '../../core/config.js';
import { settings } from '../../core/state.js';
import { cardMmsi, cardType } from '../../core/store.js';
import { decodeHTMLEntities } from '@aiscatcher/ui/components.js';
import { ol, map, basemaps, overlapmaps, baseMapsChanged, updateMapURL } from '../../map.js';
import { redraw } from '../../fleet.js';
import { saveSettings } from '../../settings-store.js';
import { showDialog, showNotification } from '../../dialog.js';
import { setPageTitle } from '../../panels.js';
import { load as loadTab, loaded as loadedTab, showTab, resolveTab, applyTabFeatures } from '../../tabs/index.js';
import { updateSettingsTab } from '../settings/settings.js';
import { syncFocus } from '../station/station.js';
import * as replay from '../replay/replay.js';
import * as community from '../../overlays/community.js';
import { createServerMaps } from '../server-maps/server-maps.js';
import { createViewerConfig } from './server-config.js';

export function applyServerFeatures() {
    applyTabFeatures();
    document.querySelectorAll('.ctx-replay').forEach(el => { el.style.display = config.features.replay ? "" : "none"; });
}

let serverConfig = null;

export function initServerConfig() {
    const applyServerMaps = createServerMaps({
        ol, basemaps: () => basemaps, overlays: () => overlapmaps, getMap: () => map,
        refresh: baseMapsChanged,
    });

    serverConfig = createViewerConfig({
        current: config,
        applyMaps: applyServerMaps,
        applyRuntime: async (next, {restartChanged, aboutChanged}) => {
            applyServerFeatures();
            if (!next.features.realtime) loadedTab('realtime')?.hidden(true);
            setPageTitle({ station: decodeHTMLEntities(next.station) });
            community.updateSharingState(next.features.sharing, next.features.sharing_uuid);
            const oldTab = settings.tab;
            if (resolveTab() !== oldTab) showTab();
            if (!next.features.replay && replay.isActive()) replay.stop();
            if (aboutChanged && settings.tab === "about")
                loadTab('about').then(m => m.shown()).catch(console.error);
            if (restartChanged && next.restart_required?.length)
                showNotification(next.restart_required.join(", ") + ": required to apply these settings", "info");
            updateSettingsTab();
            syncFocus(true);
            redraw();
        },
        pluginsChanged: () => {
            showDialog('Plugins changed', '<p>Reload the viewer to apply the updated plugins.</p><button type="button" class="btn btn-primary" id="reload-viewer-plugins">Reload</button>');
            document.getElementById('reload-viewer-plugins').addEventListener('click', () => {
                saveSettings();
                updateMapURL();
                if (cardType === "ship" && cardMmsi) {
                    const url = new URL(location.href);
                    url.searchParams.set("mmsi", cardMmsi);
                    history.replaceState(null, "", url);
                }
                location.reload();
            });
        },
    });
    applyServerMaps(config.maps);
}

// a version the server reports: a newer one is fetched and applied
export function updateServerConfig(version) {
    return serverConfig.update(version);
}
