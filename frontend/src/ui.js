import { registerActions, registerContextActions } from './events.js';
import { settings, persist } from './settings.js';

let deps = {};

// ── Context state ─────────────────────────────────────────────────────────────

let context_mmsi = null;
let context_type  = null;

export const getContextMMSI = () => context_mmsi;
export const getContextType  = () => context_type;

// ── Clipboard ─────────────────────────────────────────────────────────────────

async function copyToClipboard(textToCopy) {
    if (navigator.clipboard && window.isSecureContext) {
        await navigator.clipboard.writeText(textToCopy);
    } else {
        const textArea = document.createElement("textarea");
        textArea.value = textToCopy;
        textArea.style.position = "absolute";
        textArea.style.left = "-999999px";
        document.body.prepend(textArea);
        textArea.select();
        try { document.execCommand("copy"); } catch (e) { console.error(e); } finally { textArea.remove(); }
    }
}

export async function copyClipboard(t) {
    try {
        await copyToClipboard(t);
    } catch {
        showDialog("Action", "No privilege for program to copy to the clipboard. Please select and copy (CTRL-C) the following string manually: " + t);
        return false;
    }
    return true;
}

export function copyText(m) {
    if (copyClipboard(m)) showNotification("Content copied to clipboard");
}

// ── Notification ──────────────────────────────────────────────────────────────

const notificationContainer = document.getElementById("notification-container");

export function showNotification(message) {
    const el = document.createElement("div");
    el.classList.add("notification");
    el.textContent = message;
    notificationContainer.appendChild(el);
    setTimeout(() => {
        el.style.opacity = 0;
        setTimeout(() => notificationContainer.removeChild(el), 500);
    }, 2000);
}

// ── Dialog ────────────────────────────────────────────────────────────────────

export function showDialog(title, message) {
    const dialogBox = document.getElementById("dialog-box");
    dialogBox.querySelector(".dialog-title").innerText = title;
    dialogBox.querySelector(".dialog-message").innerHTML = message;
    dialogBox.classList.remove("hidden");
}

export function closeDialog() {
    document.getElementById("dialog-box").classList.add("hidden");
}

// ── Context menu ──────────────────────────────────────────────────────────────

const contextMenu = document.getElementById("context-menu");

export function hideContextMenu() {
    contextMenu.style.display = "none";
    document.removeEventListener("click", hideContextMenu);
}

export function showContextMenu(event, mmsi, type, context) {
    if (event && event.preventDefault) {
        event.preventDefault();
        event.stopPropagation();
    }

    deps.hideMapMenu(event);

    document.getElementById("ctx_labelswitch").textContent      = settings.show_labels !== "never" ? "Hide ship labels" : "Show ship labels";
    document.getElementById("ctx_range").textContent             = settings.show_range ? "Hide station range" : "Show station range";
    document.getElementById("ctx_fading").textContent            = settings.fading ? "Show icons without fading" : "Show icons with fading";
    document.getElementById("ctx_fireworks").textContent         = deps.getEvtSourceMap() == null ? "Start Fireworks Mode" : "Stop Fireworks Mode";
    document.getElementById("ctx_label_shipcard_pin").textContent = settings.shipcard_pinned ? "Unpin Shipcard position" : "Pin Shipcard position";
    document.getElementById("ctx_signal_graphs").textContent     = settings.show_signal_graphs ? "Hide signal level graphs" : "Show signal level graphs";
    document.getElementById("ctx_ppm_graphs").textContent        = settings.show_ppm_graphs ? "Hide frequency shift graphs" : "Show frequency shift graphs";

    context_mmsi = mmsi;
    context_type = type;

    const classList = ["station", "settings", "plane-map", "ship-map", "plane", "ship", "ctx-map", "copy-text", "table-menu", "ctx-shipcard", "ctx-charts"];

    classList.forEach((className) => {
        if (context.includes('object'))     context.push(type);
        if (context.includes('object-map')) context.push(type + "-map");
        const shouldDisplay = context.includes(className);
        document.querySelectorAll("." + className).forEach((el) => {
            el.style.display = shouldDisplay ? "flex" : "none";
        });
    });

    if (typeof realtime_enabled === "undefined" || realtime_enabled === false) {
        document.querySelectorAll('.ctx-realtime').forEach((el) => { el.style.display = "none"; });
    }

    updateAndroid();
    deps.updateKiosk();

    if (deps.isShowAllTracks()) {
        document.querySelectorAll(".ctx-noalltracks").forEach((el) => { el.style.display = "none"; });
    }

    const hasActiveTracks = deps.isShowAllTracks() || deps.hasMarkerTracks();
    document.querySelectorAll(".ctx-removealltracks").forEach((el) => {
        el.style.display = hasActiveTracks ? "flex" : "none";
    });

    document.getElementById("ctx_menu_unpin").style.display = settings.fix_center && context.includes("ctx-map") ? "flex" : "none";
    document.getElementById("ctx_track").innerText = deps.trackOptionString(context_mmsi);

    contextMenu.style.display = "block";

    if (context.includes("center")) {
        contextMenu.style.left      = "50%";
        contextMenu.style.top       = "50%";
        contextMenu.style.transform = "translate(-50%, -50%)";
    } else {
        contextMenu.style.left      = event.pageX + 5 + "px";
        contextMenu.style.top       = event.pageY + 5 + "px";
        contextMenu.style.transform = "none";

        const rect = contextMenu.getBoundingClientRect();
        const vw   = window.innerWidth  && window.outerWidth  ? Math.min(window.innerWidth,  window.outerWidth)  : document.documentElement.clientWidth;
        const vh   = window.innerHeight && window.outerHeight ? Math.min(window.innerHeight, window.outerHeight) : document.documentElement.clientHeight;

        contextMenu.style.left = Math.max(0, Math.min(event.pageX + 5, vw - rect.width))  + "px";
        contextMenu.style.top  = Math.max(0, Math.min(event.pageY + 5, vh - rect.height)) + "px";
    }

    document.addEventListener("click", hideContextMenu);
}

// ── Menu bar ──────────────────────────────────────────────────────────────────

export function toggleMenu() {
    document.getElementById("menubar").classList.toggle("visible");
    document.getElementById("menubar_mini").classList.toggle("showflex");
    document.getElementById("menubar_mini").classList.toggle("hide");

    const menuButton = document.getElementById("header_menu_button");
    menuButton.classList.toggle("menu_icon");
    menuButton.classList.toggle("close_icon");
}

export function hideMenu() {
    if (document.getElementById("menubar").classList.contains("visible") && !isAndroid()) toggleMenu();
}

export function showMenu() {
    if (!document.getElementById("menubar").classList.contains("visible")) toggleMenu();
}

export function hideMenuifSmall() { hideMenu(); }

// ── Fullscreen ────────────────────────────────────────────────────────────────

export function toggleScreenSize() {
    const doc = window.document;
    const docEl = doc.documentElement;
    const requestFS = docEl.requestFullscreen || docEl.mozRequestFullScreen || docEl.webkitRequestFullScreen || docEl.msRequestFullscreen || docEl.webkitEnterFullscreen;
    const cancelFS  = doc.exitFullscreen || doc.mozCancelFullScreen || doc.webkitExitFullscreen || doc.msExitFullscreen || doc.webkitExitFullscreen;

    if (!doc.fullscreenElement && !doc.mozFullScreenElement && !doc.webkitFullscreenElement && !doc.msFullscreenElement) {
        requestFS.call(docEl);
    } else {
        cancelFS.call(doc);
    }
}

function handleFullScreenChange() {
    const icon = document.getElementById("screentoggle-id");
    if (document.fullscreenElement) {
        icon.innerHTML = "fullscreen_exit";
    } else {
        icon.innerHTML = "fullscreen";
    }
}

export function initFullScreen() {
    ["fullscreenchange", "mozfullscreenchange", "webkitfullscreenchange", "msfullscreenchange"].forEach(
        (e) => document.addEventListener(e, handleFullScreenChange)
    );
}

// ── Info panel ────────────────────────────────────────────────────────────────

export function toggleInfoPanel() {
    document.querySelector('.overlay').classList.toggle('active');
    document.querySelector('.info-panel').classList.toggle('active');
}

// ── Platform helpers ──────────────────────────────────────────────────────────

export function isAndroid() {
    return settings.android === true || settings.android === "true";
}

export function isKiosk() {
    return settings.kiosk === true || settings.kiosk === "true";
}

export function updateAndroid() {
    if (isAndroid()) {
        document.querySelectorAll(".noandroid").forEach((el) => el.style.setProperty("display", "none", "important"));
    } else {
        document.querySelectorAll(".android").forEach((el) => el.style.setProperty("display", "none", "important"));
    }
}

// ── About / welcome ───────────────────────────────────────────────────────────

export function showAboutDialog() {
    const message = `
        <div style="display: flex; align-items: center; margin-top: 10px;">
        <span style="text-align: center; margin-right: 10px;"><i style="font-size: 40px" class="directions_aiscatcher_icon"></i></span>
        <span>
        <a href="https://www.aiscatcher.org"><b style="font-size: 1.6em;">AIS-catcher</b></a>
        <br>
        <b style="font-size: 0.8em;">&copy; 2021-2026 jvde.github@gmail.com</b>
        </span>
        </div>
        <p>
        AIS-catcher is a research and educational tool, provided under the
        <a href="https://github.com/jvde-github/AIS-catcher/blob/e66a4481e62d8f1775700e5f51fb7ad9ea569a12/LICENSE">GNU GPL v3 license</a>.
        It is not reliable for navigation and safety of life or property.
        Radio reception and handling regulations vary by region, so check your local administration's rules. Illegal use is strictly prohibited.
        </p>
        <p>
        The web-interface gratefully uses the following libraries:
        <a href="https://www.chartjs.org/docs/latest/charts/line.html" rel="nofollow">chart.js</a>,
        <a href="https://www.chartjs.org/chartjs-plugin-annotation/latest/" rel="nofollow">chart.js annotation plugin</a>,
        <a href="https://openlayers.org/" rel="nofollow">openlayers</a>,
        <a href="https://fonts.google.com/icons?selected=Material+Icons" rel="nofollow">Material Design Icons</a>,
        <a href="https://tabulator.info/" rel="nofollow">tabulator</a>,
        <a href="https://github.com/markedjs/marked">marked</a>, and
        <a href="https://github.com/lipis/flag-icons">flag-icons</a>. Please consult the links for the respective licenses.
        </p>`;
    showDialog("About...", message);
}

export function showWelcome() {
    if (settings.welcome === true || (settings.welcome === "true" && !isAndroid())) showAboutDialog();
    settings.welcome = false;
    deps.saveSettings();
}

// ── Public API ────────────────────────────────────────────────────────────────

export function init(dependencies) {
    deps = dependencies;

    // data-action triggers
    registerActions({
        closeDialog, toggleMenu, toggleScreenSize, toggleInfoPanel,
        showAboutAndInfo:    () => { toggleInfoPanel(); showAboutDialog(); },
        showSettingsMenu:    (e) => showContextMenu(e, '', '',       ['settings', 'center']),
        showMapSettingsMenu: (e) => showContextMenu(e, '', '',       ['settings', 'center', 'ctx-map']),
    });
    // data-contextaction triggers (right-click zones)
    registerContextActions({
        showMainContextMenu:   (e) => showContextMenu(e, 0,  '',       ['settings']),
        showChartsContextMenu: (e) => showContextMenu(e, '', 'charts', ['settings', 'ctx-charts']),
        cardShowContextMenu:   (e) => showContextMenu(e, deps.getCardMMSI(), deps.getCardType(), ['object', 'object-map', 'ctx-shipcard']),
    });

    initFullScreen();
}
