// The panels around the map and their one state: the vessel card, the counts
// card, the side table, the measure card and the replay bar, which of them
// may be open together, and the layout they leave - the ticker, the toolbar,
// the header's labels, the map pane's size. Features ask here what is open.

import { settings, isAndroid, isKiosk } from './core/state.js';
import { config } from './core/config.js';
import { decodeHTMLEntities } from '@aiscatcher/ui/components.js';
import * as mapui from '@aiscatcher/map/mapui.js';
import * as headerLib from '@aiscatcher/ui/header.js';
import { createPanelSwitch } from '@aiscatcher/ui/side-table.js';
import { map } from './map.js';
import { ui } from './ui.js';
import { saveSettings, effects } from './settings-store.js';
import { closeSettings } from './features/settings/settings.js';
import { closeTargetcard, positionAside, fitTargetcard } from './features/targetcard/card.js';
import { updateTablecard, tableHover } from './features/tablecard/tablecard.js';
import { updateMarkerCountTooltip } from './features/counts/counts.js';
import { showTab } from './tabs/index.js';
import * as measure from './features/measure/measure.js';
import * as ticker from './features/ticker/ticker.js';
import { register } from './actions.js';

// ─── map panels ──────────────────────────────────────────────────────────────
export const panels = {
    targetcard: false,
    statcard: false,
    table: false,
    measure: false,
    replay: false,
};

const panelSwitch = createPanelSwitch({
    workspace: document.querySelector('.mainspace-container'),
    card: { isOpen: () => panels.targetcard, close: () => closeTargetcard() },
    table: { isOpen: () => panels.table, close: () => hideTablecard() },
});

function normalisePanels() {
    if (panels.replay) {
        panels.table = false;
        panels.measure = false;
        closeSettings();
    }

    // both of these take the space, and the position, the targetcard occupies
    if ((panels.replay || panels.measure) && panels.targetcard) closeTargetcard();
}

export function setPanels(patch, opts) {
    if (patch.table) panelSwitch.opening('table');
    Object.assign(panels, patch);
    normalisePanels();
    applyPanels(opts);
}

function tickerWanted() {
    return !!settings.ticker && !panels.replay && !isKiosk() && settings.tab === "map";
}

let panelEls = null;

function panelElements() {
    if (panelEls) return panelEls;

    panelEls = {
        targetcard: document.getElementById("targetcard"),
        statcard: document.getElementById("statcard"),
        measure: document.getElementById("measurecard"),
        replay: document.getElementById("replaybar"),
        table: document.getElementById("tableside"),
        countersBtn: document.getElementById("counters-btn"),
        tableBtn: document.getElementById("table-btn"),
        popovers: [...document.querySelectorAll("#targetcard .card-popover")],
    };
    return panelEls;
}

// `reposition: false` when the caller places the targetcard itself straight after.
// showTargetcard knows the pixel that was clicked and this does not - and it must
// place the card after filling it, since the placement measures its height.
export function applyPanels(opts = {}) {
    const el = panelElements();
    const tickerOn = tickerWanted();

    el.targetcard.classList.toggle("visible", panels.targetcard);
    el.statcard.style.display = panels.statcard ? "block" : "none";
    el.measure.classList.toggle("visible", panels.measure);
    el.replay.classList.toggle("visible", panels.replay);
    el.table.classList.toggle("active", panels.table);

    document.body.classList.toggle("replay-open", panels.replay);
    document.body.classList.toggle("ticker-open", tickerOn);
    document.body.classList.toggle("ticker-bottom", !!settings.ticker_bottom);
    document.body.classList.toggle("table-open", panels.table);

    applyToolbarMode();
    applyMenuLabels();
    ui.chrome.invalidate();
    /* the map pane narrows for the panel — OpenLayers has to be told */
    if (map) mapui.trackResize(map);

    el.countersBtn?.classList.toggle("is-active", panels.statcard);
    el.tableBtn?.classList.toggle("is-active", panels.table);
    updateMeasureIndicator();

    if (!panels.targetcard) ui.card.popover.closeAll();
    if (!panels.statcard) resetCardPosition(el.statcard);
    if (!panels.measure) resetCardPosition(el.measure);

    ticker.setEnabled(tickerOn);

    if (panels.targetcard && opts.reposition !== false) positionAside(undefined, el.targetcard);
}

function resetCardPosition(el) {
    if (!el || !el.style.left) return;

    for (const prop of ["left", "top", "right", "bottom"]) el.style.removeProperty(prop);
}

function toggleStatcard() {
    if (!panels.statcard) updateMarkerCountTooltip();
    setPanels({ statcard: !panels.statcard });
}

function toggleTicker() {
    settings.ticker = !settings.ticker;
    saveSettings();
    applyPanels();
}

function toggleTablecard() {
    if (!panels.table && window.innerWidth < 800) {
        showTab('ships');
        return;
    }

    setPanels({ table: !panels.table });
    updateTablecard();
}

function hideTablecard() {
    tableHover.clear();
    if (panels.table) toggleTablecard();
}

export function updateMeasureIndicator() {
    const btn = document.getElementById("measure-btn");
    if (!btn) return;

    const n = measure.count();
    btn.classList.toggle("is-active", panels.measure || n > 0);
    btn.title = n ? "Measure distance (" + n + ")" : "Measure distance";
}

export function measurecardVisible() {
    return panels.measure;
}

export function toggleMeasurecard() {
    if (!panels.measure) ui.dismiss();
    setPanels({ measure: !panels.measure });
}

export function replaycardVisible() {
    return panels.replay;
}

let header = null;
export function applyMenuLabels() {
    if (!header)
        header = headerLib.create({
            box: document.querySelector(".header-title"),
            title: document.querySelector(".header-title > span"),
            pill: document.getElementById("menubar_mini"),
            chip: document.getElementById("header-focus"),
            mode: () => settings.menu_labels,
        });
    header.apply();
}

export function applyToolbarMode() {
    ui.toolbar.apply();
}

// the platform: what only one of the two shows, and the menu as a floating box on a desktop
const dynamicStyle = document.createElement("style");
document.head.appendChild(dynamicStyle);
const androidStyle = document.createElement("style");
document.head.appendChild(androidStyle);

function applyPlatform() {
    dynamicStyle.innerHTML = isAndroid() ? "" : `
        @media only screen and (min-width: 750px) {
            #menubar {
                position: fixed;
                top: 70px;
                left: 10px;
                right: 0;
                width: 500px;
                border: solid;
                border-color: var(--color-menu-border);
                border-radius: 5px;
                box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1), 0 4px 6px -2px rgba(0, 0, 0, 0.05);
            }
        }
    `;
    androidStyle.textContent = (isAndroid() ? ".noandroid" : ".android") + " { display: none !important; }";
}

// ─── the page's title ────────────────────────────────────────────────────────

const pageTitle = { station: decodeHTMLEntities(config.station), count: null };

// the station's name, the number of vessels, or both
export function setPageTitle(parts) {
    Object.assign(pageTitle, parts);
    document.title = (pageTitle.count ? " (" + pageTitle.count + ") " : "") + "AIS-catcher " + pageTitle.station;
}

// ─── the side menu ───────────────────────────────────────────────────────────

export function hideMenu() {
    if (document.getElementById("menubar").classList.contains("visible") && !isAndroid()) {
        toggleMenu();
    }
}

export function showMenu() {
    if (!document.getElementById("menubar").classList.contains("visible")) {
        toggleMenu();
    }
}

function toggleMenu() {
    const menubar = document.getElementById("menubar");
    menubar.classList.toggle("visible");
    document.getElementById("menubar_mini").classList.toggle("showflex");
    document.getElementById("menubar_mini").classList.toggle("hidden");

    const menuButton = document.getElementById("header_menu_button");
    menuButton.classList.toggle("menu_icon");
    menuButton.classList.toggle("close_icon");

    syncMenuScrim();
}

function syncMenuScrim() {
    const menubar = document.getElementById("menubar");
    const floats = ["absolute", "fixed"].includes(getComputedStyle(menubar).position);
    document.getElementById("menubar-overlay").classList
        .toggle("active", menubar.classList.contains("visible") && floats);
}

window.matchMedia("(min-width: 750px)").addEventListener("change", syncMenuScrim);

const relayout = () => applyPanels();

effects({
    map_toolbar: () => applyToolbarMode(),
    menu_labels: () => applyMenuLabels(),
    ticker: relayout,
    ticker_bottom: relayout,
    android: applyPlatform,
});

register({
    toggleMenu: () => toggleMenu(),
    hideMenu: () => hideMenu(),
    toggleStatcard: () => toggleStatcard(),
    toggleTicker: () => toggleTicker(),
    toggleTablecard: () => toggleTablecard(),
    hideTablecard: () => hideTablecard(),
    toggleMeasurecard: () => toggleMeasurecard(),
    toggleTickerSide: () => {
        settings.ticker_bottom = !settings.ticker_bottom;
        const btn = document.getElementById("ticker_side");
        if (btn) {
            const label = settings.ticker_bottom ? "Move the bar to the top" : "Move the bar to the bottom";
            btn.title = label;
            btn.setAttribute("aria-label", label);
        }
        saveSettings();
        applyPanels();
        fitTargetcard();
    },
});

// the vessel card's place among them
export const cardPanel = {
    isOpen: () => panels.targetcard,
    show: (on, opts) => setPanels(on ? { targetcard: true, measure: false } : { targetcard: false }, opts),
    opening: () => panelSwitch.opening('card'),
};
