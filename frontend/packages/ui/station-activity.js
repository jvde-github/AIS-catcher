// A station's activity as a strip of slots, each coloured by what the station did
// then. Shared by the map's station card and a station's page, on the website and
// in the viewer. `timeline`: { 'YYYY-MM-DD': [state, ...] }, a UTC day's slots
// spread evenly over its 24 hours (the hub reports twelve, the viewer twenty-four).
const STATES = {
    active: ['', 'Receiving'],
    connected: ['quiet', 'Connected'],
    not_active: ['offline', 'Not active'],
    no_data: ['no-data', 'No data'],
    system_error: ['system-error', 'System error'],
};

// the legend names the states the strip can hold; `only` limits it to those a host reports
function legendHTML(only) {
    const keys = only || Object.keys(STATES);
    return '<div class="activity-legend">' + keys.filter(k => STATES[k]).map(k => STATES[k]).map(([style, name]) =>
        '<span><i class="' + style + '" aria-hidden="true"></i>' + name + '</span>').join('') + '</div>';
}

function slotHTML(date, hour, value, nativeTooltips) {
    const [style, name] = Object.hasOwn(STATES, value) ? STATES[value] : STATES.no_data;
    const description = date + ' ' + String(hour).padStart(2, '0') + ':00 UTC · ' + name;
    return '<i class="' + style + '" aria-label="' + description + '"' + (nativeTooltips ? ' title="' + description + '"' : '') + '></i>';
}

// one strip of the last 24 hours, ending with the slot that is running now
function renderLastDay(container, timeline, nativeTooltips, now, midnight, only) {
    const day = offset => {
        const date = new Date(midnight + offset * 86400000).toISOString().slice(0, 10);
        const reported = timeline?.[date];
        return { date, slots: Array.isArray(reported) && reported.length ? reported : Array(12).fill('no_data') };
    };
    const all = [];
    for (const { date, slots } of [day(-1), day(0)])
        slots.forEach((value, i) => all.push({ date, hour: Math.floor(i * 24 / slots.length), start: Date.parse(date + 'T00:00:00Z') + i * 86400000 / slots.length, value }));
    const shown = all.filter(s => s.start <= now && s.start > now - 86400000);
    const hours = shown.map(s => slotHTML(s.date, s.hour, s.value, nativeTooltips)).join('');
    container.innerHTML = '<div class="station-activity one-day"><div class="activity-day"><div class="activity-slots">' + hours
        + '</div><div class="activity-ends"><span>24 h ago</span><span>now</span></div></div></div>' + legendHTML(only);
}

/* o: { nativeTooltips: a title on each slot; fillWeek: seven days ending today, reported or not;
   lastDay: one strip of the last 24 hours; now: ms; states: the states the legend names } */
export function renderStationActivity(container, timeline, { nativeTooltips = true, fillWeek = false, lastDay = false, now = Date.now(), states } = {}) {
    const today = new Date(now).toISOString().slice(0, 10);
    const midnight = Date.parse(today + 'T00:00:00Z');
    if (lastDay) return renderLastDay(container, timeline, nativeTooltips, now, midnight, states);
    const dates = fillWeek
        ? Array.from({ length: 7 }, (_, i) => new Date(midnight - (6 - i) * 86400000).toISOString().slice(0, 10))
        : Object.keys(timeline || {}).filter(date => /^\d{4}-\d{2}-\d{2}$/.test(date)).sort().slice(-7);
    if (!dates.length) {
        container.innerHTML = '<p class="activity-empty">No activity recorded.</p>';
        return;
    }
    const days = dates.map(date => {
        const reported = timeline?.[date];
        const slots = Array.isArray(reported) && reported.length ? reported : Array(12).fill('no_data');
        const label = new Date(date + 'T00:00:00Z').toLocaleDateString('en-GB', { day: 'numeric', month: 'short', timeZone: 'UTC' });
        const hours = slots.map((value, i) => slotHTML(date, Math.floor(i * 24 / slots.length), value, nativeTooltips)).join('');
        return '<div class="activity-day"><span>' + label + '</span><div class="activity-slots">' + hours + '</div></div>';
    }).join('');
    container.innerHTML = '<div class="station-activity">' + days + '</div>' + legendHTML(states);
}
