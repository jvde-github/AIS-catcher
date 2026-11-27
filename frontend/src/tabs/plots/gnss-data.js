// The GPS history from api/gnss.json, decoded for drawing. What it means (jams, gaps, jumps, the verdict)
// is worked out by the server once a minute: the events come with the history, the verdict from api/health.

export function decode(d) {
    if (!d || !d.reports) return null;
    d.n = d.reports.length;
    d.time = (i) => d.end - (d.n - 1 - i) * d.interval;
    d.rows = (d.satellites || []).map(([system, id, b, b64]) => {
        const s = atob(b64), v = new Uint8Array(s.length);
        for (let i = 0; i < s.length; i++) v[i] = s.charCodeAt(i);
        return { system, id, band: b, v };
    });
    return d;
}

// A tail, the newest slots as api/gnss.json?last= tells them, laid over the history it continues: the
// whole moves up by the slots that have passed and the tail takes its end. null when the tail does not
// reach back to what the history holds: then the whole is asked for again.
export function merge(d, tail) {
    if (!d || !tail || tail.interval !== d.interval) return null;
    const n = d.n, m = tail.n, k = Math.round((tail.end - d.end) / d.interval);
    if (k < 0 || m > n || m < k + 1) return null;
    const lay = (old, fresh) => {
        const out = old.slice(k);
        while (out.length < n) out.push(null);
        for (let i = 0; i < m; i++) out[n - m + i] = fresh[i];
        return out;
    };
    const none = (len) => new Array(len).fill(null);
    for (const name of ["reports", "fix", "used", "seen", "hdop", "position", "fixes"]) d[name] = lay(d[name], tail[name]);
    const signal = {};
    for (const name of new Set([...Object.keys(d.signal || {}), ...Object.keys(tail.signal || {})]))
        signal[name] = lay((d.signal || {})[name] || none(n), (tail.signal || {})[name] || none(m));
    d.signal = signal;
    // a satellite the tail does not name had no signal in it; one the history did not have starts with none
    const key = (r) => r.system + ":" + r.id + ":" + r.band, fresh = new Map((tail.rows || []).map((r) => [key(r), r]));
    const rows = [];
    for (const r of [...(d.rows || []), ...(tail.rows || []).filter((t) => !(d.rows || []).some((o) => key(o) === key(t)))]) {
        const v = new Uint8Array(n), old = (d.rows || []).includes(r) ? r.v : null, now = fresh.get(key(r));
        if (old) v.set(old.subarray(k));
        v.fill(0, n - m);
        if (now) v.set(now.v, n - m);
        if (v.some((x) => x)) rows.push({ system: r.system, id: r.id, band: r.band, v });
    }
    d.rows = rows;
    d.events = tail.events;
    d.end = tail.end;
    d.now = tail.now;
    return d;
}
