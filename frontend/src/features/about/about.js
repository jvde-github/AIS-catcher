// The about panel that slides in from the header, the about dialog and the
// welcome that shows it once. The about tab's text is markdown.js's, loaded
// with the tab.

import { settings, isAndroid } from '../../core/state.js';
import { saveSettings } from '../settings/settings.js';
import { showDialog } from '../../dialog.js';
import { register } from '../../actions.js';

export function toggleInfoPanel() {
    const overlay = document.getElementById('info-overlay');
    const infoPanel = document.querySelector('.info-panel');

    overlay.classList.toggle('active');
    infoPanel.classList.toggle('active');
}

export function showAboutDialog() {
    const message = `
        <div style="display: flex; align-items: center; margin-top: 10px;">
        <span style="text-align: center; margin-right: 10px;"><i style="font-size: 40px" class="directions_aiscatcher_icon"></i></span>
        <span>
        <a href="https://www.aiscatcher.org" target="_blank" rel="noopener"><b style="font-size: 22px;">AIS-catcher</b></a>
        <br>
        <b style="font-size: 11px;">&copy; 2021-2026 jvde.github@gmail.com</b>
        </span>
        </div>
        <p>
        AIS-catcher is a research and educational tool, provided under the
        <a href="https://github.com/jvde-github/AIS-catcher/blob/e66a4481e62d8f1775700e5f51fb7ad9ea569a12/LICENSE" target="_blank" rel="noopener">GNU GPL v3 license</a>.
        It is not reliable for navigation and safety of life or property.
        Radio reception and handling regulations vary by region, so check your local administration's rules. Illegal use is strictly prohibited.
        </p>
        <p>
        The web-interface gratefully uses the following libraries:
        <a href="https://www.chartjs.org/docs/latest/charts/line.html" target="_blank" rel="noopener nofollow">chart.js</a>,
        <a href="https://www.chartjs.org/chartjs-plugin-annotation/latest/" target="_blank" rel="noopener nofollow">chart.js annotation plugin</a>,
        <a href="https://openlayers.org/" target="_blank" rel="noopener nofollow">openlayers</a>,
        <a href="https://fonts.google.com/icons?selected=Material+Icons" target="_blank" rel="noopener nofollow">Material Design Icons</a>,
        <a href="https://tabulator.info/" target="_blank" rel="noopener nofollow">tabulator</a>,
        <a href="https://github.com/markedjs/marked" target="_blank" rel="noopener">marked</a>, and
        <a href="https://github.com/lipis/flag-icons" target="_blank" rel="noopener">flag-icons</a>. Please consult the links for the respective licenses.
        </p>`;

    showDialog("About...", message);
}

// the about dialog on a first visit
export function showWelcome() {
    if ((settings.welcome == true || settings.welcome == "true") && !isAndroid()) showAboutDialog();

    settings.welcome = false;
    saveSettings();
}

register({
    toggleInfoPanel: () => toggleInfoPanel(),
    showAboutFromInfoPanel: () => { toggleInfoPanel(); showAboutDialog(); },
});
