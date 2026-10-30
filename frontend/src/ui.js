// The map's UI as one object the viewer imports: where the free map is (chrome),
// the vessel card's shell, the toolbar, and the stack of things that can be open.
// It is built at load, so everything it asks of other modules it asks in a
// function, when it needs the answer.

import * as mapui from '@aiscatcher/map/mapui.js';
import { settings, isKiosk } from './core/state.js';
import { config } from './core/config.js';
import { map } from './map.js';
import { isDialogOpen } from './dialog.js';
import { panels, hideMenu } from './panels.js';
import { settingsPanel } from './features/settings/settings.js';
import { searchPanel } from './features/search/search-panel.js';
import { closeTargetcard, targetcardVisible } from './features/targetcard/card.js';
import { closeStationcard, stationcardVisible } from './features/station/station.js';

const mqShort = window.matchMedia("(max-height: 800px)");

/* Room is a question about the map pane, which a side panel takes its slice
   out of — the same width the stylesheet asks about with @container mappane. */
function paneWidth() {
    const el = document.getElementById("map");
    return el && el.clientWidth ? el.clientWidth : window.innerWidth;
}

export function cardFullBleed() {
    return paneWidth() <= 500 || mqShort.matches;
}

// The ticker owns the top strip. A card that starts above it hides the very
// thing that announced the vessel, so cards begin below it while it is out.
function mapTopInset() {
    if (cardFullBleed()) return 0;

    // On a narrow screen the card takes the whole top anyway; giving the bar
    // its strip there would cost the card room it has none of to spare.
    if (paneWidth() <= 750) return ui.chrome.gap();

    return ui.chrome.inset("top");
}

/* The map pane already stops where a side panel begins, so its own width is
   the width on offer. */
function freeMapWidth() {
    const el = document.getElementById("map");
    if (el && el.clientWidth > 0) return el.clientWidth;

    const mapSize = map ? map.getSize() : null;
    return mapSize && mapSize[0] > 0 ? mapSize[0] : window.innerWidth;
}

// how a card on the map sits, for the vessel card and the station card
export const cardLayout = {
    fullBleed: () => cardFullBleed(),
    topInset: () => mapTopInset(),
    freeWidth: () => freeMapWidth(),
    preferDock: () => !!settings.targetcard_top_left,
};

export const ui = mapui.create({
    map: () => map,
    settings: () => settings,
    chrome: {
        fallback: (edge) => {
            const cls = document.body.classList;
            let inset = ui.chrome.gap();
            if (edge === "top" && cls.contains("ticker-open") && !cls.contains("ticker-bottom")) inset += ui.chrome.px("--size-ticker", 44);
            if (edge === "bottom" && cls.contains("ticker-open") && cls.contains("ticker-bottom")) inset += ui.chrome.px("--size-ticker", 44);
            return inset;
        },
    },
    card: {
        mount: document.getElementById("targetcard"),
        itemAvailable: (el) => el.id !== 'targetcard_realtime_option' || config.features.realtime,
        layout: {
            ...cardLayout,
            pinned: () => settings.targetcard_pinned ? { x: settings.targetcard_pinned_x, y: settings.targetcard_pinned_y } : null,
        },
    },
    /* top-down: Escape closes the first open one; a card taking the stage
       closes the ones marked stage. The dialog closes itself on Escape. */
    stack: [
        { isOpen: () => isDialogOpen() },
        { isOpen: () => searchPanel.isOpen(), close: () => searchPanel.close(true), stage: true },
        { isOpen: () => settingsPanel.isOpen(), close: () => settingsPanel.close(), stage: true },
        { isOpen: () => document.getElementById("menubar").classList.contains("visible"), close: () => hideMenu(), stage: true },
        { isOpen: () => stationcardVisible(), close: () => closeStationcard() },
        { isOpen: () => targetcardVisible(), close: () => closeTargetcard() },
    ],
    toolbar: {
        bar: document.getElementById("maptoolbar"),
        rail: document.querySelector(".map-button-box.map-pill:not(.map-bottom)"),
        wantWide: () => settings.map_toolbar === "wide" && !isKiosk() && !panels.replay,
        freeWidth: () => freeMapWidth(),
    },
});

window.addEventListener("resize", () => ui.chrome.invalidate());
