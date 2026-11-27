// The Outputs view: what leaves the installation and whether it gets out, as the server judges it
// every half minute (api/health). Two kinds: the feeds it sends on to other programs, and the
// services it answers from, as this viewer and the API. They are the installation's, not a
// station's. The verdicts are the server's: nothing is judged here.

import { head, figure, tone, cell, wrong } from './head.js';
import { WORDS, outputsTitle } from '../../features/status/health-data.js';
import { compactCount, formatBytes, sanitizeString } from '@aiscatcher/core/text.js';

const TITLE = { connected: "Connected", sending: "Sending", dropped: "Dropped messages", reconnects: "Reconnects", feed: "Community feed", services: "Services" };
const HINT = {
    connected: "Every output that dials out, as TCP, MQTT or a database, holds its connection. UDP, HTTP and a listener hold none.",
    sending: "Something went out in the last 15 minutes while messages came in.",
    dropped: "Messages an output could not pass on. A drop now and then is no trouble; drops minute after minute count.",
    reconnects: "A connection that is made again minute after minute counts.",
    feed: "The feed to the community hub, when the station shares.",
    services: "Every server that is set up, as this viewer and the API, listens on its port.",
};
// what a server offers, in the words of the pages
const OFFERS = { ui: "viewer", api_v0: "", api: "API", mcp: "MCP", system: "system", geojson: "GeoJSON", kml: "KML", metrics: "metrics" };
const GUIDE = "https://docs.aiscatcher.org/configuration/output/overview/";
const STATE_WORD = { connected: "Connected", disconnected: "Connecting", listening: "Listening", sending: "Sending", idle: "Idle" };

export function render(host, out) {
    host.hidden = !out;
    if (!out) return;
    const c = out.checks;
    const feeds = out.list.filter((o) => o.kind !== "service"), services = out.list.filter((o) => o.kind === "service"), n = feeds.length;
    const dials = feeds.filter((o) => o.kind === "connection"), up = dials.filter((o) => o.state === "connected").length;
    const shut = services.filter((o) => o.state === "closed"), clients = services.reduce((sum, o) => sum + o.clients, 0);
    const total = (key) => out.list.reduce((sum, o) => sum + o[key], 0);
    const name = (o) => sanitizeString(o.label || o.type);
    // the output an issue is of, for the check that names it
    const of = (code) => {
        const issue = out.issues.find((i) => i.code === code), o = issue && out.list.find((x) => x.id === issue.output);
        return o ? name(o) : "An output";
    };
    const bad = (state) => (wrong(state) ? "warn" : "");
    const many = (k, one) => `${k} ${one}${k === 1 ? "" : "s"}`;
    const figures = [
        // how many there are, and of those that hold a connection how many have it: never a fraction that reads as feeds failing
        figure("Feeds", n, !n ? "None configured" : dials.length ? `${up} of ${many(dials.length, "connection")} up` : "None holds a connection", bad(c.connected)),
        figure("Services", services.length, shut.length ? `${shut.length} not listening` : `${many(clients, "client")} now`, bad(c.services)),
        figure("Data sent", formatBytes(total("bytes_out")), "Since the start"),
        figure("Dropped", total("dropped").toLocaleString(), "Messages, since the start", bad(c.dropped)),
    ];
    // the viewer is a service itself, so the view is never empty: without a feed it says so here, with the way to add one
    const aside = n ? "" : `<p class="hc-hint">No feeds are configured: nothing is sent on to other programs. <a href="${GUIDE}" target="_blank" rel="noopener">Read the setup guide</a></p>`;
    const note = {
        connected: c.connected === "unknown" ? "No feed dials out" : `${up} of ${dials.length} up`,
        sending: c.sending === "ok" ? "Data going out" : c.sending === "unknown" ? "Nothing sent yet" : `${of("not_sending")}: nothing for 15 minutes`,
        dropped: c.dropped === "ok" ? "None lately" : c.dropped === "unknown" ? "Not available" : `${of("dropping")} keeps dropping`,
        reconnects: c.reconnects === "ok" ? "Stable" : c.reconnects === "unknown" ? "No feed dials out" : `${of("reconnecting")} keeps reconnecting`,
        feed: c.feed === "ok" ? "Connected" : c.feed === "unknown" ? "Not sharing" : c.feed === "down" ? "Not connected" : "Needs a look",
        services: c.services === "ok" ? `${services.length} listening` : c.services === "unknown" ? "Not available" : `${shut.length ? name(shut[0]) : "A port"} could not be opened`,
    };
    const checks = Object.keys(TITLE).map((k) => ({ state: c[k], title: TITLE[k], note: note[k], hint: HINT[k] }));

    const count = (label, value) => (value === undefined ? "" : cell(label, value.toLocaleString()));
    const sent = (n) => (n === undefined ? "" : `<div title="${n.toLocaleString()} messages"><span>Messages sent</span><strong>${compactCount(n)}</strong></div>`);
    const cells = (o) => count("Clients now", o.clients) + sent(o.messages) + cell("Data sent", formatBytes(o.bytes_out)) + count("Dropped", o.dropped) + count("Reconnects", o.reconnects);
    const card = (o, sub, state) => `<article class="hc-rx" style="--tone:${tone(o.status)}">
        <div class="hc-rx-top"><i class="hc-rx-dot"></i><strong>${name(o)}</strong>${sub ? `<small>${sub}</small>` : ""}<span>${state}</span></div>
        <div class="hc-rx-figs">${cells(o)}</div></article>`;
    const feed = (o) => {
        const issue = out.issues.find((i) => i.output === o.id);
        return card(o, o.label && !o.label.startsWith(o.type) ? sanitizeString(o.type) : "", issue ? WORDS[issue.code] || issue.code : STATE_WORD[o.state] || o.state);
    };
    const service = (o) => card(o, o.offers.map((x) => (x in OFFERS ? OFFERS[x] : sanitizeString(x))).filter(Boolean).join(", "), o.state === "closed" ? "Not listening" : "Listening");
    const group = (title, cards) => (cards.length ? `<div class="hc-group">${title}</div><div class="hc-receivers">${cards.join("")}</div>` : "");
    head(host, {
        state: out.status, title: outputsTitle(out), live: "Live", label: "Output health", figures, aside, checks,
        foot: { more: "Output details", details: group("Feeds", feeds.map(feed)) + group("Services", services.map(service)) },
    });
}
