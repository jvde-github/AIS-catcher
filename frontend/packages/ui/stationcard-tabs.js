// The station card on the shared map-card tabs: the first row of figures
// stays above the tabs, the other rows are the Summary, and the day of
// activity and the last half hour of messages have a tab each.
import { build as buildTabs, metrics } from './card-tabs.js';
import { fieldRows } from './components.js';

// a tab's chart under its label; the returned row is what a host hides when
// there is nothing to draw
function chart(panel, label, id, cls) {
    const group = document.createElement('div');
    group.className = 'sc-group';
    const title = document.createElement('div');
    title.className = 'sc-group-label';
    title.textContent = label;
    const row = document.createElement('div');
    row.className = 'mapcard-content-row card-row';
    const body = document.createElement('div');
    body.id = id;
    body.className = cls;
    row.appendChild(body);
    group.append(title, row);
    panel.appendChild(group);
    return row;
}

/* `o.fields` are the host's rows of fields ([{ key, label }] each), the first
   the figures above the tabs; `o.tab` and `o.onTab` as card-tabs.js has them.
   The charts draw into `<prefix>timeline` and `<prefix>msgrate`. Returns the
   value cells by key, the chart rows by name (activity, msgrate) and the tabs. */
export function build(mount, prefix, o) {
    const shell = buildTabs(mount, prefix, {
        ...o, label: 'Station details',
        tabs: [['summary', 'Summary'], ['activity', 'Activity'], ['messages', 'Messages']],
    });
    const cells = {
        ...metrics(shell.head, o.fields[0], prefix),
        ...fieldRows(shell.panels.summary, o.fields.slice(1).map((fields) => ({ fields })),
            { rowClass: 'mapcard-content-row card-row', idPrefix: prefix }),
    };
    const charts = {
        activity: chart(shell.panels.activity, 'Activity (last 24 hours)', prefix + 'timeline', 'station-activity-widget'),
        msgrate: chart(shell.panels.messages, 'Messages (last 30 min)', prefix + 'msgrate', 'hist-wrap'),
    };
    return { ...shell, cells, charts };
}
