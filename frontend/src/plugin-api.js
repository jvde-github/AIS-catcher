// The plugin API: window.AISCatcher, and the same names as bare globals for
// plugins written before it. Plugins come from custom/plugins.js, which
// defines window.loadPlugins; the entry runs it once the features are bound.

import { settings } from './core/state.js';
import { config } from './core/config.js';
import { ships as shipsDB, planes as planesDB, station, cardMmsi as card_mmsi, cardType as card_type } from './core/store.js';
import { setShipNameProvider, setCallSignProvider } from './core/names.js';
import { ACTIONS } from './actions.js';
import {
    ol, addTileLayer, removeTileLayer, removeTileLayerAll, addOverlayLayer, removeOverlayLayer,
    removeOverlayLayerAll,
} from './map.js';
import { setShipFilter, setRefreshInterval } from './data.js';
import { showDialog, closeDialog, showNotification } from './dialog.js';
import { showTargetcard } from './features/targetcard/card.js';
import { showContextMenu, context_mmsi, context_type } from './features/contextmenu/contextmenu.js';
import { openFocus } from './features/follow/follow.js';
import * as targetcard from './features/targetcard/targetcard.js';

// Plugin contract version. Bumped when the plugin-facing API changes shape.
// Plugins guard with `if (typeof AISCatcher !== 'undefined' &&
// AISCatcher.PLUGIN_API_VERSION >= N)`. Public surface is window.AISCatcher
// (defined below). Bare-global aliases mirror AISCatcher for back-compat but
// are deprecated.
//   v4: addTargetcardItem() requires a function callback (CSP-clean).
//   v5: script.js is an ES module; override hooks (setShipFilter,
//       setRefreshInterval, setShipNameProvider, setCallSignProvider) and
//       map helpers (addTileLayer, removeTileLayerAll, ...) are on AISCatcher.
const PLUGIN_API_VERSION = 5;

export function installPluginApi() {
    window.ol = ol;

    // Public plugin API. Mutable state uses getters so plugins always see the
    // live value rather than a snapshot taken at namespace-build time.
    window.AISCatcher = {
        PLUGIN_API_VERSION,

        addTargetcardItem: targetcard.addItem,
        ACTIONS,

        addShipcardItem: targetcard.addItem,
        showShipcard: showTargetcard,

        showDialog,
        closeDialog,
        showNotification,
        showTargetcard,
        showContextMenu,
        openFocus,

        addTileLayer,
        removeTileLayer,
        removeTileLayerAll,
        addOverlayLayer,
        removeOverlayLayer,
        removeOverlayLayerAll,

        setShipFilter,
        setRefreshInterval,
        setShipNameProvider,
        setCallSignProvider,

        get config() { return config; },
        get settings() { return settings; },
        get station() { return station; },
        get shipsDB() { return shipsDB; },
        get planesDB() { return planesDB; },
        get card_mmsi() { return card_mmsi; },
        get card_type() { return card_type; },
        get context_mmsi() { return context_mmsi; },
        get context_type() { return context_type; },
    };

    // Mirror AISCatcher keys onto window so legacy plugins using bare globals
    // (addTargetcardItem, card_mmsi, ...) still resolve. Module top-level no
    // longer leaks to window since script.js is an ES module.
    for (const k of Object.keys(window.AISCatcher)) {
        if (k in window) continue;
        const desc = Object.getOwnPropertyDescriptor(window.AISCatcher, k);
        if (desc.get) {
            Object.defineProperty(window, k, { get: desc.get, configurable: true });
        } else {
            window[k] = desc.value;
        }
    }
}

export function loadPlugins() {
    if (typeof window.loadPlugins === 'undefined') {
        window.loadPlugins = function () { };
    }

    console.log("Starting plugin code");

    window.loadPlugins && window.loadPlugins();

    console.log("Plugin loading completed");
}
