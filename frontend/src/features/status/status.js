// The header's two icons on the station: AIS and GPS, each with a dot that says whether it works, and
// a title with why. A click opens the tab that shows it. Both are in the bar from the start, so that
// the tabs beside them are fitted once; the GPS icon stays, grey, without a GPS.

import { focus } from '../../data.js';
import { showDashboard } from '../../tabs/index.js';
import { register } from '../../actions.js';
import { fetchStatus, onHealth, view, words, aisTitle, gpsTitle } from './health-data.js';

function icon(id, label, part, title) {
    const el = document.getElementById(id);
    if (!el) return;
    el.dataset.state = part ? part.status : "none";
    const why = part ? part.issues.map((i) => words(i, part.receivers)).join(", ") : "";
    const text = `${label}: ${part ? title(part) : "not available"}${why && why !== title(part) ? ". " + why : ""}`;
    el.title = text;
    el.setAttribute("aria-label", text);
}

function draw(v) {
    icon("status-ais", "AIS", v.ais, aisTitle);
    icon("status-gps", "GPS", v.gps, gpsTitle);
}

// the server judges every half minute; this asks as often, in brief, and follows every answer in full the page gets
const poll = () => !document.hidden && fetchStatus(focus).then((v) => v && draw(v));

export function start() {
    onHealth((h) => draw(view(h, focus)));
    poll();
    setInterval(poll, 30000);
    document.addEventListener("visibilitychange", () => !document.hidden && poll());
}

register({
    openStatus: (e, d) => showDashboard(d.mode),
});
