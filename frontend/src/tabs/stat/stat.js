// The statistics tab: the system, the receiver's throughput and outputs, and
// the message counts over five periods, from api/stat.json at every refresh.

import { getDistanceVal, getDistanceUnit } from '../../core/units.js';
import { getDeltaTimeVal, sanitizeString, formatBytes, isHttpUrl } from '@aiscatcher/core/text.js';
import { decodeHTMLEntities } from '@aiscatcher/ui/components.js';
import { receiver, onRefresh, refreshIntervalMs } from '../../data.js';
import { showDialogPlain, objectToTableHtml } from '../../dialog.js';
import * as community from '../../overlays/community.js';

// { onStation(name): the station's name as the statistics give it }
let deps = { onStation: () => {} };

export function init(d) {
    deps = d;
}

// fetches main statistics from the server
async function fetchStatistics() {
    try {
        const response = await fetch("api/stat.json?receiver=" + receiver);
        if (!response.ok) return;
        return await response.json();
    } catch (error) {
        return;
    }
}

function updateStat(stat, tf) {
    const errors = stat[tf].errors || {};
    const errorCount = document.getElementById("stat_" + tf + "_error_flagged");
    const icon = document.createElement("i");
    icon.className = "info_icon";
    icon.setAttribute("aria-hidden", "true");
    errorCount.replaceChildren((errors.flagged || 0).toLocaleString(), icon);
    const errorRow = errorCount.parentElement;
    errorRow.onclick = () => showDialogPlain(objectToTableHtml({
        "Errors": (errors.flagged || 0).toLocaleString(),
        "Undersized payload": (errors.undersized || 0).toLocaleString(),
        "Oversized payload": (errors.oversized || 0).toLocaleString(),
        "Invalid checksum": (errors.checksum || 0).toLocaleString(),
    }));
    errorRow.onkeydown = (event) => {
        if (event.key === "Enter" || event.key === " ") {
            event.preventDefault();
            errorRow.click();
        }
    };
    [0, 1, 2, 3].forEach((e) => (document.getElementById("stat_" + tf + "_channel" + e).innerText = stat[tf].channel[e].toLocaleString()));

    document.getElementById("stat_" + tf + "_count").innerText = stat[tf].count.toLocaleString();
    document.getElementById("stat_" + tf + "_dist").innerText = getDistanceVal(stat[tf].dist) + " " + getDistanceUnit();
    document.getElementById("stat_" + tf + "_vessel_count").innerText = stat[tf].vessels.toLocaleString();
    document.getElementById("stat_" + tf + "_msg123").innerText = (stat[tf].msg[0] + stat[tf].msg[1] + stat[tf].msg[2]).toLocaleString();
    document.getElementById("stat_" + tf + "_msg5").innerText = stat[tf].msg[4].toLocaleString();
    document.getElementById("stat_" + tf + "_msg18").innerText = stat[tf].msg[17].toLocaleString();
    document.getElementById("stat_" + tf + "_msg19").innerText = stat[tf].msg[18].toLocaleString();
    document.getElementById("stat_" + tf + "_msg68").innerText = (stat[tf].msg[5] + stat[tf].msg[7]).toLocaleString();
    document.getElementById("stat_" + tf + "_msg1214").innerText = (stat[tf].msg[11] + stat[tf].msg[13]).toLocaleString();
    document.getElementById("stat_" + tf + "_msg24").innerText = stat[tf].msg[23].toLocaleString();
    document.getElementById("stat_" + tf + "_msg4").innerText = stat[tf].msg[3].toLocaleString();
    document.getElementById("stat_" + tf + "_msg9").innerText = stat[tf].msg[8].toLocaleString();
    document.getElementById("stat_" + tf + "_msg21").innerText = (stat[tf].msg[20] + (stat[tf].msg[27] || 0)).toLocaleString();
    document.getElementById("stat_" + tf + "_msg27").innerText = stat[tf].msg[26].toLocaleString();

    let count_other = 0;
    [7, 10, 11, 13, 15, 16, 17, 20, 22, 23, 25, 26].forEach((i) => (count_other += stat[tf].msg[i - 1]));
    document.getElementById("stat_" + tf + "_msgother").innerText = count_other.toLocaleString();
}

function renderDevices(stat) {
    const el = document.getElementById("stat_devices");
    if (!el) return;
    const prod = stat.product || [], vend = stat.vendor || [], ser = stat.serial || [], mod = stat.model || [], rate = stat.sample_rate || [], dlab = stat.device_label || [];
    const n = Math.max(prod.length, vend.length, ser.length, mod.length, rate.length, dlab.length);
    el.replaceChildren();
    for (let i = 0; i < n; i++) {
        let name = dlab[i];
        if (!name) {
            name = prod[i] || mod[i] || "Device " + (i + 1);
            if (ser[i] && ser[i] !== "-") name += " (" + ser[i] + ")";
        }

        const label = document.createElement("span");
        label.textContent = i === 0 ? "Device" : "";
        const icon = document.createElement("i");
        icon.className = "info_icon";
        const value = document.createElement("span");
        value.className = "device-value";
        value.append(name, icon);
        const row = document.createElement("div");
        row.className = "device-row";
        row.title = "Show device details";
        row.append(label, value);
        const engines = String(mod[i] || "").split("\n").filter((e) => e !== "");
        const info = { Device: prod[i], Vendor: vend[i], Serial: ser[i] };
        if (engines.length > 1) engines.forEach((e, k) => (info["Engine " + (k + 1)] = e));
        else info.Engine = engines[0];
        info["Sample rate"] = rate[i];

        row.onclick = () => showDialogPlain(objectToTableHtml(info));
        el.appendChild(row);
    }
}

async function updateStatistics() {
    const stat = await fetchStatistics();

    if (stat) {
        // in bulk....
        const statText = (v) => (Array.isArray(v) ? v.join(", ") : v != null ? v : "");
        ["os", "tcp_clients", "hardware", "build_describe", "build_date", "station"].forEach(
            (e) => (document.getElementById("stat_" + e).textContent = statText(e === "station" ? decodeHTMLEntities(stat[e]) : stat[e])),
        );

        renderDevices(stat);

        if (stat.station_link != "") {
            const el = document.getElementById("stat_station");
            el.textContent = "";
            const a = document.createElement("a");
            a.href = isHttpUrl(stat.station_link) ? stat.station_link : "#";
            a.textContent = decodeHTMLEntities(stat.station);
            el.appendChild(a);
        }

        const statSharingElement = document.getElementById("stat_sharing");
        community.updateSharingState(stat.sharing, stat.sharing_uuid, stat.engine_running);
        const [sharingText, sharingClass] = community.sharingDisplay();
        statSharingElement.innerHTML = `<a href="${stat.sharing_link}" target="_blank" class="${sharingClass}">${sharingText}</a>`;

        document.getElementById("stat_update_time").textContent = Number(refreshIntervalMs / 1000).toFixed(1) + " s";
        let title = document.getElementById("stat_station").textContent;
        if (title != "" && title != null) deps.onStation(title);
        document.getElementById("stat_memory").innerText = stat.memory ? Number(stat.memory / 1000000).toFixed(1) + " MB" : "N/A";
        if (stat.track_time != null) {
            const t = stat.track_time;
            let age = "unlimited";
            if (t > 0) {
                const d = Math.floor(t / 86400), h = Math.floor((t % 86400) / 3600), m = Math.floor((t % 3600) / 60), s = t % 60;
                age = [d && d + "d", h && h + "h", m && m + "m", s && s + "s"].filter(Boolean).join(" ");
            }
            const mem = stat.track_memory >= 1024 ? Number(stat.track_memory / 1024).toFixed(0) + " MB" : stat.track_memory + " KB";
            document.getElementById("stat_track").innerText = age + " / " + mem;
        }
        document.getElementById("stat_received").innerText = formatBytes(stat.received);
        document.getElementById("stat_msg_rate").innerText = Number(stat.msg_rate).toFixed(1) + " msg/s";
        document.getElementById("stat_msg_min_rate").innerText = Number(stat.last_minute.count).toFixed(0) + " msg/min";
        document.getElementById("stat_run_time").innerHTML = getDeltaTimeVal(stat.run_time);

        updateStat(stat, "total");
        updateStat(stat, "session");
        updateStat(stat, "last_minute");
        updateStat(stat, "last_hour");
        updateStat(stat, "last_day");

        document.getElementById("stat_total_vessel_count").innerText = "-";
        document.getElementById("stat_session_vessel_count").innerText = stat.vessel_count;

        let outputSection = document.getElementById("output_stats");
        if (!outputSection) return;

        if (stat.outputs && stat.outputs.length > 0) {
            let html = "";
            for (let i = 0; i < stat.outputs.length; i++) {
                html += "<section>";
                const o = stat.outputs[i];
                const s = o.stats;
                const showStatus = o.type !== "UDP" && !o.type.startsWith("HTTP");

                let name = sanitizeString(o.description || o.type);
                if (o.link && isHttpUrl(o.link)) {
                    const href = sanitizeString(o.link);
                    name = `<a href="${href}" target="_blank" rel="noopener" title="${href}" style="color:inherit">${name}` +
                        ` <svg xmlns="http://www.w3.org/2000/svg" width="12" height="12" viewBox="0 -960 960 960" fill="currentColor" style="vertical-align:-1px"><path d="M200-120q-33 0-56.5-23.5T120-200v-560q0-33 23.5-56.5T200-840h280v80H200v560h560v-280h80v280q0 33-23.5 56.5T760-120H200Zm188-212-56-56 372-372H520v-80h320v320h-80v-184L388-332Z"/></svg></a>`;
                }
                html += `<div><span>Output</span><span>${name}</span></div>`;
                if (showStatus) {
                    const connected = s.connected ? "Connected" : "Not connected";
                    html += `<div><span>Status</span><span class="${s.connected ? "status-ok" : "status-bad"}">${connected}</span></div>`;
                }
                html += `<div><span>Bytes out / in</span><span>${formatBytes(s.bytes_out)} / ${formatBytes(s.bytes_in)}</span></div>`;
                if (o.type !== "UDP")
                    html += `<div><span>Connect ok / fail</span><span>${s.connect_ok} / ${s.connect_fail}</span></div>`;
                if (s.reconnects > 0)
                    html += `<div><span>Reconnects</span><span>${s.reconnects}</span></div>`;
                if (s.dropped > 0)
                    html += `<div><span>Dropped</span><span>${s.dropped}</span></div>`;
                html += "</section>";
            }
            outputSection.innerHTML = html;
        } else {
            outputSection.innerHTML = "";
        }
    }
}

onRefresh("stat", updateStatistics);
