// The plots tab's two kinds of graph, each shown or hidden by a setting, and
// the time axis; eager, since the settings apply them before the tab loads.

import { settings } from '../../core/state.js';
import { saveSettings, effects } from '../../settings-store.js';
import { loaded as loadedTab } from '../index.js';
import { register } from '../../actions.js';

export const GRAPH_KINDS = {
    signal: { setting: 'show_signal_graphs', header: 'Signal Level' },
    ppm: { setting: 'show_ppm_graphs', header: 'Frequency Shift' }
};

function setGraphVisibility(type, show, save = true) {
    const kind = GRAPH_KINDS[type];
    if (kind) {
        settings[kind.setting] = show;
        document.querySelectorAll('.graph-panel').forEach(panel => {
            const header = panel.querySelector('header');
            if (header && header.textContent.includes(kind.header)) {
                panel.style.display = show ? '' : 'none';
            }
        });
    }
    if (save) saveSettings();
}

function toggleGraphVisibility(type) {
    const kind = GRAPH_KINDS[type];
    if (kind) setGraphVisibility(type, !settings[kind.setting]);
}

effects({
    plot_absolute_time: (v) => loadedTab('plots')?.setAbsoluteTime(v),
    show_signal_graphs: (v) => setGraphVisibility('signal', v, false),
    show_ppm_graphs: (v) => setGraphVisibility('ppm', v, false),
});

register({
    toggleGraphVisibility: (e, d) => toggleGraphVisibility(d.graph),
});
