import { sanitizeString } from './core/text.js';

export function visitListHTML(visits, serverTime) {
    if (!Array.isArray(visits) || !visits.length) return '<span class="dim-note">No recorded visits</span>';
    const stamp = t => t == null ? 'Unknown' : new Date(t * 1000).toLocaleString([], {day:'numeric', month:'short', hour:'2-digit', minute:'2-digit', second:'2-digit'});
    const duration = seconds => {
        if (seconds == null) return 'Unknown';
        if (seconds < 60) return `${Math.floor(seconds)} sec`;
        const minutes = Math.floor(seconds / 60);
        return minutes < 60 ? `${minutes} min` : `${Math.floor(minutes / 60)} h ${minutes % 60} min`;
    };
    return '<ol class="tl place-visits">' + [...visits].sort((a, b) =>
        (b.entered ?? -Infinity) - (a.entered ?? -Infinity)
    ).map(v => {
        const status = v.inside || v.exited != null ? '' : 'Observation ended; exit unknown';
        const elapsed = v.entered != null && (v.exited != null || (v.inside && Number.isFinite(serverTime)))
            ? Math.max(0, (v.exited ?? serverTime) - v.entered) : null;
        const state = v.inside ? 'inside' : v.exited != null ? 'completed' : 'ended';
        const stateLabel = status || (v.inside ? 'Inside' : 'Completed');
        return '<li class="tl-item place-visit"><div class="tl-head"><span class="tl-dot visit-dot-' + state + '" role="img" aria-label="' + stateLabel + '" title="' + stateLabel + '"></span>' +
            '<strong class="place-visit-name">' + sanitizeString(v.name) + '</strong>' +
            (elapsed != null ? '<span class="place-visit-badge">' + duration(elapsed) + '</span>' : '') +
            (status ? '<span class="place-visit-status">' + status + '</span>' : '') + '</div>' +
            '<div class="place-visit-times"><div><span>Entry</span><span>' + stamp(v.entered) + '</span></div>' +
            '<div><span>Exit</span><span>' + (v.inside ? '—' : stamp(v.exited)) + '</span></div>' +
            '<div><span>Duration</span><span>' + duration(elapsed) + '</span></div></div></li>';
    }).join('') + '</ol>';
}
