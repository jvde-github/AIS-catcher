// Day and night: the page's theme, switched in the settings or the context menu, and what
// follows the theme - the base map, the charts, the themed rows of the settings panel.

import { settings } from '../../core/state.js';
import { updateMapLayer } from '../../map.js';
import { redraw } from '../../fleet.js';
import { saveSettings, effects } from '../../settings-store.js';
import { syncThemedSettings } from '../settings/settings.js';
import { loaded as loadedTab } from '../../tabs/index.js';
import { register } from '../../actions.js';

export function updateDarkMode() {
    document.documentElement.classList.toggle("dark", settings.dark_mode);
    loadedTab('plots')?.updateColors();
    updateMapLayer();
    redraw();
    syncThemedSettings();
}

function toggleDarkMode() {
    settings.dark_mode = !settings.dark_mode;
    updateDarkMode();
    saveSettings();
}

effects({
    dark_mode: () => updateDarkMode(),
});

register({
    toggleDarkMode: () => toggleDarkMode(),
});
