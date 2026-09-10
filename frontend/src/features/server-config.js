import {createConfigUpdater, changed} from '../../shared/server-config.js';

// Bootstrap fetches current settings before the viewer is initialized. The
// embedded config remains a fallback for offline pages and older backends.
export async function loadInitialServerConfig() {
    window.__LOADED_PLUGIN_VERSION__ = window.__SERVER_CONFIG__?.plugin_version;
    try {
        const response = await fetch('api/config.json', {cache:'no-store'});
        if (response.ok) window.__SERVER_CONFIG__ = await response.json();
    } catch { /* Keep the embedded configuration. */ }
}

export function initialServerConfig() {
    return window.__SERVER_CONFIG__ || {
    build: { version: 'unknown', describe: 'unknown' },
    context: 'aiscatcher',
    station: '',
    webcontrol_http: '',
    features: {
        share_location: false, save_messages: false, replay: true,
        realtime: false, log: false, decoder: false,
        managed: false, about_md: false,
    },
    receivers: [],
    plugins: { loaded: [], errors: [] }, // loaded: [{ name, version }, ...]
};
}

export function applyServerFeatures(next) {
    for (const [enabled, tab] of [
        [next.features.about_md, "about"], [next.features.realtime, "realtime"],
        [next.features.log, "log"], [next.features.decoder, "decoder"],
        [next.webcontrol_http, "webcontrol"],
    ]) {
        for (const suffix of ["_tab", "_tab_mini"]) {
            const el = document.getElementById(tab + suffix);
            if (el) el.style.display = enabled ? "" : "none";
        }
    }
    document.querySelectorAll('.ctx-replay').forEach(el => { el.style.display = next.features.replay ? "" : "none"; });
}

// Transport, version comparison and config publication belong here. The host
// supplies effects on its currently open tabs, map and selection.
export function createViewerConfig({current, applyMaps, applyRuntime, pluginsChanged}) {
    return createConfigUpdater({
        current,
        loadedPluginVersion:window.__LOADED_PLUGIN_VERSION__ || current.plugin_version,
        fetchConfig:async () => {
            const response = await fetch('api/config.json', {cache:'no-store'});
            if (!response.ok) throw Error('Unable to fetch viewer settings');
            return response.json();
        },
        apply:async (next, previous) => {
            const changes = {
                receiversChanged:changed(previous.receivers,next.receivers),
                restartChanged:changed(previous.restart_required,next.restart_required),
                aboutChanged:previous.about_version !== next.about_version,
            };
            const mapsChanged = changed(previous.maps,next.maps);
            Object.assign(current,{...next,config_version:previous.config_version});
            if (mapsChanged) applyMaps(next.maps);
            applyServerFeatures(next);
            await applyRuntime(next,changes);
        },
        pluginsChanged,
    });
}
