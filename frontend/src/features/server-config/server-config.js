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
    stations: [],
    plugins: { loaded: [], errors: [] }, // loaded: [{ name, version }, ...]
};
}
