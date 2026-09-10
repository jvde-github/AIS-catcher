// Configuration is small: fetch a full snapshot only on revision changes, then
// let the host update the affected UI. Failed/stale replies never advance it.
export function createConfigUpdater({current, fetchConfig, apply, pluginsChanged, loadedPluginVersion = current.plugin_version}) {
    let wanted = current.config_version || '', pending = null;
    const loadedPlugins = loadedPluginVersion;
    let notifiedPlugins = loadedPlugins;
    function checkPlugins() {
        if (current.plugin_version !== loadedPlugins && current.plugin_version !== notifiedPlugins) {
            notifiedPlugins = current.plugin_version;
            pluginsChanged();
        }
    }
    async function update(version) {
        checkPlugins();
        wanted = version || wanted;
        if (pending || !wanted || wanted === current.config_version) return pending;
        const requested = wanted;
        pending = (async () => {
            try {
                const next = await fetchConfig();
                if (wanted !== requested || next.config_version !== requested) return;
                await apply(next, current);
                Object.assign(current, next);
                checkPlugins();
            } catch (error) {
                // Retry on the next feed; keep the last applied revision.
            }
        })();
        await pending;
        pending = null;
        if (wanted !== requested) return update(wanted);
    }
    return {update};
}

export const changed = (before, after) => JSON.stringify(before) !== JSON.stringify(after);
