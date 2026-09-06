// Reuse the station's live DOM nodes so polling, section availability and
// chart tooltips keep their existing owners in either layout.
import { build as buildTabs } from './card-tabs.js';

export function build(mount, prefix, options) {
    const original = [...mount.children];
    const stats = original.filter(e => e.dataset.section === 'stats' && !e.classList.contains('card-section'));
    const summary = stats[0];
    const summaryClass = summary.className;
    const shell = buildTabs(mount, prefix, {
        ...options, label: 'Station details',
        tabs: [['summary', 'Summary'], ['activity', 'Activity'], ['messages', 'Messages']],
    });
    shell.head.style.setProperty('--sc-metric-columns', '3');
    summary.classList.add('sc-metrics');
    summary.classList.remove('mapcard-content-row', 'card-row');
    const values = [...summary.querySelectorAll(':scope > div > span:last-child')];
    values.forEach(e => e.classList.add('sc-metric-value'));
    shell.head.appendChild(summary);
    stats.slice(1).forEach(e => shell.panels.summary.appendChild(e));
    for (const [section, tab] of [['activity', 'activity'], ['msgrate', 'messages']]) {
        const group = document.createElement('div');
        group.className = 'sc-group';
        const label = document.createElement('div');
        label.className = 'sc-group-label';
        label.textContent = section === 'activity' ? 'Activity (last 7 days)' : 'Messages (last 30 min)';
        group.appendChild(label);
        original.filter(e => e.dataset.section === section && !e.classList.contains('card-section'))
            .forEach(e => group.appendChild(e));
        shell.panels[tab].appendChild(group);
    }
    return { ...shell, destroy() {
        shell.destroy();
        summary.className = summaryClass;
        values.forEach(e => e.classList.remove('sc-metric-value'));
        mount.replaceChildren(...original);
        mount.classList.remove('sc-host', 'sc-folded');
    } };
}
