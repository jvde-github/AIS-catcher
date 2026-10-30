// Day and night: the page's theme, the header's toggle, and what follows the
// theme - the base map, the charts, the themed rows of the settings panel.

import { settings } from '../../core/state.js';
import { updateMapLayer } from '../../map.js';
import { redraw } from '../../fleet.js';
import { saveSettings, effects } from '../../settings-store.js';
import { syncThemedSettings } from '../settings/settings.js';
import { loaded as loadedTab } from '../../tabs/index.js';
import { register } from '../../actions.js';

export function updateDarkMode() {
    document.documentElement.classList.toggle("dark", settings.dark_mode);
    document.getElementById("theme-toggle-moon")?.style.setProperty("display", settings.dark_mode ? "none" : "");
    document.getElementById("theme-toggle-sun")?.style.setProperty("display", settings.dark_mode ? "" : "none");
    const themeBtn = document.getElementById("theme-toggle-button");
    if (themeBtn) themeBtn.title = settings.dark_mode ? "Switch to day" : "Switch to night";
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
