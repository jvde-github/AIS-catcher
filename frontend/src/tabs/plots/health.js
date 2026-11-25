// The AIS view's verdict on top, as the server judges it every half minute (api/health): the status
// the header's AIS icon carries, the figures, the six checks behind it, and each receiver below.
// The verdicts are the server's: nothing is judged here.

import { head, figure, tone, summary, cell } from './head.js';
import { aisTitle } from '../../features/status/health-data.js';
import { formatBytes, sanitizeString } from '@aiscatcher/core/text.js';
import { getDistanceVal, getDistanceUnit } from '../../core/units.js';

const ago = (s) => (s === null ? "never" : s < 90 ? `${s} s ago` : s < 5400 ? `${Math.round(s / 60)} min ago` : `${Math.round(s / 3600)} h ago`);
const signed = (v) => (v > 0 ? "+" : v < 0 ? "−" : "") + Math.abs(v).toFixed(1);

const TITLE = { input: "Receiver input", reception: "Reception", rate: "Message rate", channels: "Channel balance", gain: "Gain", frequency: "Frequency" };
const HINT = {
    input: "Every receiver of the station is connected and delivers data.",
    reception: "AIS messages arrived in the last 15 minutes.",
    rate: "The last hour's messages against the median of the same hour on three or more earlier days. Under 40% of it counts as low.",
    channels: "Channel A against channel B over the last 15 minutes. Under 20% on one of them counts as low.",
    gain: "The gain is judged by what it does. A noise floor 6 dB above or below the week's median counts: the gain, the antenna, or something nearby. So does 1% or more of the samples at full scale: lower the gain.",
    frequency: "How far off frequency the messages come in, as the median over the last hour. 3 ppm or more counts: set the ppm correction of the receiver.",
};

const INPUT = { streaming: "Streaming", starting: "Starting", lost: "Device lost" };

function receiver(r) {
    const state = r.status === "down" && r.input !== "lost" ? "Not streaming" : r.status === "degraded" ? "Streaming, no messages" : INPUT[r.input] || r.input;
    // an input that streams and passed on no AIS message carries something else, as a GPS; or it heard nothing yet
    const other = r.last_msg_age_s === null;
    return `<article class="hc-rx" style="--tone:${tone(r.status)}">
        <div class="hc-rx-top"><i class="hc-rx-dot"></i><strong>${sanitizeString(r.label || "Receiver " + r.id)}</strong>${r.of ? `<small>${sanitizeString(r.of)}</small>` : ""}<span>${state}</span></div>
        <div class="hc-rx-figs">${cell(r.carries === "adsb" ? "ADS-B messages/min" : "AIS messages/min", other ? "—" : r.msgs_1m)}${cell("Last message", other ? "—" : ago(r.last_msg_age_s))}${cell("Data received", formatBytes(r.bytes))}</div>
        ${r.carries === "adsb" ? `<p class="hc-rx-note">Aircraft, not vessels: not counted as an AIS receiver.</p>` : other ? `<p class="hc-rx-note">${r.carries === "other" ? "No AIS messages: it carries other data, as a GPS. Not counted as a receiver." : "No AIS message yet."}</p>` : ""}</article>`;
}

// reach: { vessels, range } of the last hour, for a station whose receiver reports nothing of the band
export function render(host, ais, reach) {
    host.hidden = !ais;
    if (!ais) return;
    const a = ais.a, b = ais.b, pa = a + b ? Math.round((100 * a) / (a + b)) : 0;
    // the receivers that are there for AIS, as the server counts them: all of them when none has passed on a message
    const mine = ais.receivers.some((r) => r.carries === "ais") ? ais.receivers.filter((r) => r.carries === "ais") : ais.receivers;
    const up = mine.filter((r) => r.input === "streaming").length, n = mine.length;
    const has = (...codes) => ais.issues.find((i) => codes.includes(i.code));
    const c = { ...ais.checks }, rf = ais.rf || {};
    const stopped = has("engine_stopped");
    if (stopped) c.input = "down";

    const low = has("rate_low"), channel = has("channel_a_low", "channel_b_low"), noise = has("rf_noise_high", "rf_noise_low"), clip = has("rf_clipping"), offset = has("freq_offset");
    const level = rf.noise !== undefined ? `${signed(rf.noise)} ${rf.noise_unit || "dB"}` : "", delta = rf.noise_delta_db;
    const figures = [
        figure("Messages/min", ais.msgs, "Last minute"),
        figure("Receivers", n ? `${up}<small>/${n}</small>` : "—", n ? "Streaming" : "Heard through a feed", c.input === "degraded" || c.input === "down" ? "warn" : ""),
        ...(ais.rf ? [
            figure("Noise change", delta != null ? `${signed(delta)} <small>dB</small>` : "—", delta != null ? (ais.several ? "Largest deviation" : `Now ${level}`) : level ? `Now ${level}` : "Not available", noise ? "warn" : ""),
            figure("Clipping", rf.clip_pct !== undefined ? `${rf.clip_pct.toFixed(1)}<small>%</small>` : "—", rf.clip_pct !== undefined ? (ais.several ? "Highest station value" : "RF samples") : "Not available", clip ? "warn" : ""),
        ] : [
            // heard through the network, there is no noise or clipping to tell: how far it reaches stands there
            figure("Vessels", reach && reach.vessels != null ? reach.vessels.toLocaleString() : "—", "Last hour"),
            figure("Max range", reach && reach.range != null ? `${getDistanceVal(reach.range)} <small>${getDistanceUnit()}</small>` : "—", reach && reach.range != null ? "Last hour" : "Not shown"),
        ]),
    ];
    const aside = a + b ? `<div class="hc-split-labels"><span style="color:var(--color-chart-1)">Channel A <b>${pa}%</b></span><span>Message distribution</span><span style="color:var(--color-chart-2)">Channel B <b>${100 - pa}%</b></span></div>
        <div class="hc-split" role="img" aria-label="Channel A ${pa}%, channel B ${100 - pa}%"><i style="flex:${a};background:var(--color-chart-1)"></i><i style="flex:${b};background:var(--color-chart-2)"></i></div>` : "";

    // what each check found, in a few words
    const quiet = has("no_messages");
    const silent = quiet && quiet.receiver !== undefined ? ais.receivers.find((r) => r.id === quiet.receiver && r.station === quiet.station) : null;
    const note = {
        input: stopped ? "The receiver engine is stopped" : !n ? "Heard through a feed" : `${up}/${n} streaming${mine.some((r) => r.input === "lost") ? ", device lost" : ""}`,
        reception: c.reception === "ok" ? "Messages arriving" : c.reception === "unknown" ? "Nothing received yet" : `${silent ? sanitizeString(silent.label || "Receiver " + silent.id) + ": no" : "No"} messages for 15 minutes`,
        rate: c.rate === "ok" ? "As usual for this hour" : c.rate === "learning" ? "Learning baseline" : low ? `${low.value}% of usual for this hour` : "Fewer than usual for this hour",
        channels: c.channels === "ok" ? "Both channels active" : c.channels === "unknown" ? "Too few messages to judge" : channel ? `Channel ${channel.code === "channel_a_low" ? "A" : "B"} low: ${channel.value}% of messages` : "One channel low",
        gain: c.gain === "ok" ? (delta == null ? "No clipping" : rf.clip_pct === undefined ? "Noise within baseline" : "Noise within baseline, no clipping") : c.gain === "learning" ? "Learning the noise baseline" : c.gain === "unknown" ? "Not available"
            : [noise && `Noise ${signed(noise.value)} dB ${noise.value > 0 ? "above" : "below"} baseline`, clip && `${clip.value}% of samples clip`].filter(Boolean).join(", ") || "Needs a look",
        frequency: c.frequency === "ok" ? `${signed(rf.freq_offset_ppm)} ppm, within ±3` : c.frequency === "unknown" ? "Not available" : offset ? `${signed(offset.value)} ppm: set the ppm correction` : "Off frequency",
    };
    const checks = Object.keys(TITLE).map((k) => ({ state: c[k], title: TITLE[k], note: note[k], hint: HINT[k] }));

    head(host, {
        state: ais.status, title: aisTitle(ais),
        live: ais.last === null ? "Unknown" : ais.last < 90 ? "Live" : ago(ais.last), label: "AIS receiver health", figures, aside, checks,
        foot: { ...summary(checks), more: "Receiver details", details: n ? `<div class="hc-receivers">${ais.receivers.map(receiver).join("")}</div>` : "" },
    });
}
