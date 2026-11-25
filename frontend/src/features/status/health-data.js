// The server's health (api/health), read for what the viewer looks at: one station, or all of them
// as one. The server says what is wrong by code; the words for it are here.

const RANK = { ok: 0, degraded: 1, down: 2 };
const worse = (a, b) => (RANK[b] > RANK[a] ? b : a);

export const WORDS = {
    engine_stopped: "The receiver engine is stopped",
    input_lost: "lost its device",
    input_not_streaming: "is not streaming",
    no_messages: "No messages for 15 minutes",
    rate_low: "Fewer messages than usual for this hour",
    channel_a_low: "Little heard on channel A",
    channel_b_low: "Little heard on channel B",
    rf_noise_high: "Noise floor higher than usual",
    rf_noise_low: "Noise floor lower than usual",
    rf_clipping: "The signal clips: lower the gain",
    freq_offset: "The receiver is off frequency",
    no_data: "The GPS is silent",
    no_fix: "No fix",
    fix_2d: "2D fix only",
    jamming_suspected: "Jamming suspected",
    spoofing_suspected: "Position off: spoofing suspected",
    not_connected: "Not connected",
    not_sending: "Sends nothing",
    dropping: "Dropping messages",
    reconnecting: "Keeps reconnecting",
};

// of several stations a check is its worst, and one that passed somewhere counts over one still learning
const CHECKS = ["input", "reception", "rate", "channels", "gain", "frequency"];
const VERDICTS = ["down", "degraded", "ok", "learning", "unknown"];

let latest = null, at = 0, pending = null;

// told of every answer, whoever asked: the header then never says other than the card
const watchers = [];
export function onHealth(fn) {
    watchers.push(fn);
}

// one request serves the tabs and the statistics when they ask within ten seconds: the verdicts are
// worked out every half minute, and the ages in it need be no fresher
export function fetchHealth() {
    if (latest && Date.now() - at < 10000) return Promise.resolve(latest);
    if (!pending)
        pending = fetch("api/health")
            // 503 says nothing receives: the body tells why
            .then((r) => (r.ok || r.status === 503 ? r.json() : null))
            .then((h) => {
                if (h && h.stations) {
                    latest = h;
                    at = Date.now();
                    watchers.forEach((fn) => fn(h));
                }
                return latest;
            })
            .catch(() => latest)
            .finally(() => (pending = null));
    return pending;
}

export function words(issue, receivers) {
    const r = issue.receiver !== undefined && receivers ? receivers.find((x) => x.id === issue.receiver && x.station === issue.station) : null;
    const text = WORDS[issue.code] || issue.code;
    return issue.receiver !== undefined ? `${(r && r.label) || "Receiver " + issue.receiver} ${text.charAt(0).toLowerCase() + text.slice(1)}` : text;
}

// The header's two dots need two statuses and what is wrong, not the stations: asked in brief, of
// the station looked at. An answer in full, when the page has a fresh one, serves as well.
export function fetchStatus(focus) {
    if (latest && Date.now() - at < 30000) return Promise.resolve(view(latest, focus));
    const of = (h, type) => ({ status: h[type], issues: h.issues.filter((i) => i.type === type), receivers: [] });
    return fetch("api/health?view=brief" + (focus >= 0 ? "&station=" + focus : ""))
        .then((r) => (r.ok || r.status === 503 ? r.json() : null))
        .then((h) => h && { ais: of(h, "ais"), gps: h.gps ? of(h, "gps") : null })
        .catch(() => null);
}

// { ais, gps, outputs }: ais null when the station is not judged, gps null without a GPS, outputs null
// without one; the outputs are the installation's, whatever station is looked at
export function view(h, focus) {
    if (!h || !h.stations) return { ais: null, gps: null, outputs: null };
    const outputs = h.outputs ? { ...h.outputs, list: Object.entries(h.outputs.items).map(([id, o]) => ({ ...o, id })), issues: (h.issues || []).filter((i) => i.type === "output") } : null;
    const ids = focus >= 0 ? (h.stations[focus] ? [String(focus)] : []) : Object.keys(h.stations);
    const issues = (h.issues || []).filter((i) => (i.station === undefined ? focus < 0 : ids.includes(i.station)));
    const stations = ids.map((id) => h.stations[id]);
    let ais = null, gps = null;
    if (stations.length || focus < 0) {
        const ages = stations.map((s) => s.ais.last_msg_age_s).filter((v) => v !== null);
        // of several stations the noise floor furthest from its usual, the most clipping, the largest offset
        const rfs = stations.map((s) => s.ais.rf).filter(Boolean), off = (r) => Math.abs(r.noise_delta_db || 0);
        const clips = rfs.map((r) => r.clip_pct).filter((v) => v !== undefined);
        const ppms = rfs.map((r) => r.freq_offset_ppm).filter((v) => v !== undefined).sort((x, y) => Math.abs(y) - Math.abs(x));
        ais = {
            several: stations.length > 1,
            status: focus < 0 ? h.ais : stations.reduce((w, s) => worse(w, s.ais.status), "ok"),
            last: ages.length ? Math.min(...ages) : null,
            msgs: stations.reduce((n, s) => n + s.ais.msgs_1m, 0),
            a: stations.reduce((n, s) => n + s.ais.ch_a_1m, 0),
            b: stations.reduce((n, s) => n + s.ais.ch_b_1m, 0),
            checks: Object.fromEntries(CHECKS.map((c) => [c, VERDICTS.find((v) => stations.some((s) => s.ais.checks[c] === v)) || "unknown"])),
            rf: rfs.length ? { ...rfs.filter((r) => r.noise !== undefined).sort((x, y) => off(y) - off(x))[0], clip_pct: clips.length ? Math.max(...clips) : undefined, freq_offset_ppm: ppms[0] } : null,
            // of several stations each receiver is told with the station it feeds
            receivers: ids.flatMap((id) => Object.entries(h.stations[id].ais.receivers).map(([r, v]) => ({ ...v, id: r, station: id, of: ids.length > 1 ? h.stations[id].name || "Station " + id : "" }))),
            issues: issues.filter((i) => i.type === "ais"),
        };
    }
    // of several stations with a GPS, the one that needs a look
    const id = ids.filter((x) => h.stations[x].gps).sort((x, y) => RANK[h.stations[y].gps.status] - RANK[h.stations[x].gps.status])[0];
    if (id !== undefined) gps = { ...h.stations[id].gps, station: id, issues: issues.filter((i) => i.type === "gps" && i.station === id) };
    return { ais, gps, outputs };
}

export const aisTitle = (ais) => (ais.status === "ok" ? "AIS reception active" : ais.status === "down" ? "AIS not receiving" : "AIS reception needs a look");
export const outputsTitle = (out) => (out.status === "ok" ? "Outputs operating normally" : out.status === "down" ? "Feeds not connected" : "Outputs need a look");
export const gpsTitle = (gps) => {
    if (gps.status === "ok") return "GPS operating normally";
    const first = gps.issues.slice().sort((x, y) => RANK[y.level] - RANK[x.level])[0];
    return first ? WORDS[first.code] || first.code : "GPS needs a look";
};
