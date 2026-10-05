// The context menu: on the map, a vessel, the vessel card, a table row, the
// plots, the replay bar - what it offers for the vessel or place it was
// opened on (context_mmsi, context_type), the ticks beside its toggles
// (MENU_CHECKS), and the lookups, copies and dialogs its items open.

import { settings, isAndroid, isKiosk } from '../../core/state.js';
import { config } from '../../core/config.js';
import { cardMmsi as card_mmsi, cardType as card_type, markerTracks as marker_tracks } from '../../core/store.js';
import { getICAOFromHexIdent } from '@aiscatcher/core/text.js';
import { copyToClipboard } from '@aiscatcher/ui/components.js';
import { isAttributionPinned } from '../../map.js';
import { receiver, trackCutoff } from '../../data.js';
import { showDialog, showDialogPlain, showNotification, objectToTableHtml } from '../../dialog.js';
import { trackIsShown, toggleTrack, trackCutoffLabel } from '../../tracks.js';
import { vesselPosition, vesselLabel, isFollowing, toggleFollow, mapResetViewZoom } from '../follow/follow.js';
import { showTargetcard } from '../targetcard/card.js';
import { replaycardVisible, measurecardVisible } from '../../panels.js';
import { openRealtimeForMMSI } from '../../tabs/index.js';
import { GRAPH_KINDS } from '../../tabs/plots/graphs.js';
import * as replay from '../replay/replay.js';
import * as community from '../../overlays/community.js';
import * as fireworks from '../../overlays/fireworks.js';
import { register } from '../../actions.js';
import { receiverLabel, hasReceiverChoice } from '../receiver/receiver.js';

export let context_mmsi = null;
export let context_type = null;

// { menu: the entry's menu (ui.menu), built with MENU_CHECKS }
let menu = null;

export const MENU_CHECKS = {
    toggleTargetcardPin: () => settings.targetcard_pinned,
    toggleShipcardStyle: () => settings.shipcard_style === "tabs",
    toggleTrackCtx: () => trackIsShown(context_mmsi),
    toggleAllTracks: () => settings.show_all_tracks,
    toggleTrackCutoff: () => trackCutoff > 0,
    toggleLabel: () => settings.show_labels != "never",
    toggleRange: () => settings.show_range,
    toggleAttribution: () => isAttributionPinned(),
    toggleTicker: () => settings.ticker,
    toggleReplaycard: () => replaycardVisible(),
    toggleMeasurecard: () => measurecardVisible(),
    toggleCommunityPane: () => community.isPaneOpen(),
    toggleKioskMode: () => isKiosk(),
    ToggleFireworks: () => fireworks.isRunning(),
    toggleGraphVisibility: (el) => settings[GRAPH_KINDS[el.dataset.graph].setting],
    toggleDarkMode: () => settings.dark_mode,
    toggleScreenSize: () => !!document.fullscreenElement,
    replayToggleLabels: () => replay.getLabels(),
    replaySetSpeed: (el) => replay.getSpeed() == el.dataset.speed,
};

// https://stackoverflow.com/questions/51805395/navigator-clipboard-is-undefined
async function copyClipboard(t) {
    try {
        await copyToClipboard(t);
    } catch (error) {
        showDialog("Action", "No privilege for program to copy to the clipboard. Please select and copy (CTRL-C) the following string manually: " + t);
        return false;
    }
    return true;
}

export async function copyCoordinates(m) {
    const pos = vesselPosition(m);
    if (!pos) {
        showNotification("Ship not found", "error");
        return;
    }
    if (await copyClipboard(pos.lat + "," + pos.lon)) showNotification("Coordinates copied to clipboard", "success");
}

async function fetchJSON(l, m) {
    let response;
    try {
        response = await fetch(l + "?" + m);
    } catch (error) {
        showDialog("Error", error);
        return null;
    }
    if (!response.ok) {
        showDialog("Error", "Server returned " + response.status);
        return null;
    }
    return response.text();
}

async function showJSONTableDialog(url, m, copyContext) {
    const s = await fetchJSON(url, m);
    if (s == null) return;

    let obj;
    try {
        obj = JSON.parse(s);
    } catch (error) {
        showDialog("Error", "Invalid response from server");
        return;
    }
    // api/message decodes the stored NMEA on request and returns an array
    if (Array.isArray(obj)) obj = obj[0] || {};
    showDialogPlain(objectToTableHtml(obj, copyContext));
}

export async function showNMEA(m) {
    if (config.features.save_messages) {
        await showJSONTableDialog("api/message", m + "&receiver=" + receiver, true);
    } else if (config.features.managed) {
        showDialog("Error", 'Enable the "Msgs" setting in the viewer configuration of the control panel.');
    } else {
        showDialog("Error", 'Please enable "-N MSG on" in AIS-catcher settings.');
    }
}

export async function showVesselDetail(m) {
    await showJSONTableDialog("api/vessel", m + "&receiver=" + receiver, false);
}

export async function copyText(m) {
    if (await copyClipboard(m)) showNotification("Content copied to clipboard", "success");
}

const EXT_LINKS = {
    aiscatcher:    id => `https://www.aiscatcher.org/ship/details/${id}`,
    google:        id => `https://www.google.com/search?q=${id}`,
    vesselfinder:  id => `https://www.vesselfinder.com/vessels/details/${id}`,
    aishub:        id => `https://www.aishub.net/vessels?Ship[mmsi]=${id}`,
    planespotters: id => `https://www.planespotters.net/hex/${getICAOFromHexIdent(id)}`,
    adsbexchange:  id => `https://globe.adsbexchange.com/?icao=${getICAOFromHexIdent(id)}`,
    flightaware:   id => `https://flightaware.com/live/modes/${getICAOFromHexIdent(id)}/redirect`,
};
export function openExt(key, id) { window.open(EXT_LINKS[key](id)); }

let replayPausedByMenu = false;


export function showContextMenu(event, mmsi, type, context, anchorEl) {

    if (event && event.preventDefault) {
        event.preventDefault();
        event.stopPropagation();
    }

    context_mmsi = mmsi;
    context_type = type;

    // reading a menu over a moving fleet means the vessel it refers to has
    // sailed on by the time you pick an item
    if (replay.isPlaying()) {
        replay.pause();
        replayPausedByMenu = true;
    }

    if (context.includes('object')) context.push(type);
    if (context.includes('object-map')) context.push(type + "-map");

    const unpinCovered = isFollowing(context_mmsi) || (context.includes("station") && isFollowing("STATION"));
    const unpinContext = context.includes("ctx-map") || context.includes("ctx-replay-map");
    document.getElementById("ctx_unpin").innerText =
        isFollowing("STATION") ? "Unfollow station" : "Unfollow " + vesselLabel(settings.center_point);
    document.getElementById("ctx_receiver_label").innerText = "Receiver: " + receiverLabel() + "\u2026";
    document.getElementById("ctx_follow").innerText = isFollowing(context_mmsi) ? "Unfollow vessel" : "Follow vessel";
    document.getElementById("ctx_follow_station").innerText = isFollowing("STATION") ? "Unfollow station" : "Follow station";
    document.getElementById("ctx_trackcutoff").innerText = trackCutoff ? "Tracks from " + trackCutoffLabel() : "Tracks from now";

    const hidden = (el) =>
        (el.id === "ctx_receiver" && !hasReceiverChoice()) ||
        (el.classList.contains("ctx-realtime") && !config.features.realtime) ||
        (el.classList.contains("ctx-replay") && !config.features.replay) ||
        (el.classList.contains("noandroid") && isAndroid()) ||
        (el.classList.contains("android") && !isAndroid()) ||
        (el.classList.contains("nokiosk") && isKiosk()) ||
        (el.classList.contains("kiosk") && !isKiosk()) ||
        (el.classList.contains("ctx-noalltracks") && settings.show_all_tracks) ||
        (el.classList.contains("ctx-removealltracks") && !(settings.show_all_tracks || marker_tracks.size > 0 || trackCutoff)) ||
        (el.classList.contains("ctx-selectedtracks") && (settings.show_all_tracks || marker_tracks.size === 0)) ||
        (el.id === "ctx_menu_unpin" && !(settings.fix_center && unpinContext && !unpinCovered)) ||
        // from the card's own menu the card is already open
        (el.dataset.action === "showTargetcardCtx" && context.includes("ctx-targetcard"));

    menu.open({
        tags: context,
        show: (el, byTag) => byTag && !hidden(el),
        meta: (() => {
            const pos = context.includes("ship") ? vesselPosition(context_mmsi) : null;
            return context.includes("ship") ? { copyTextCtx: String(context_mmsi),
                copyCoordinatesCtx: pos ? Number(pos.lat).toFixed(2) + ", " + Number(pos.lon).toFixed(2) : "" } : {};
        })(),
        anchor: anchorEl,
        center: context.includes("center"),
        x: event ? event.pageX : 0,
        y: event ? event.pageY : 0,
        onClose: () => {
            if (replayPausedByMenu) {
                replayPausedByMenu = false;
                replay.play();
            }
        },
    });
}

export function init(d) {
    menu = d.menu;
    menu.add([
        { action: "replayToggleLabels", label: "Ship labels", icon: "label", group: "replay", tags: ["ctx-replay-map", "ctx-replay-ship"], check: true },
        ...replay.speeds().map((sp) => ({ action: "replaySetSpeed", label: sp + "\u00d7", group: "replay", tags: ["ctx-replay-map", "ctx-replay-ship"], check: true, data: { speed: String(sp) } })),
    ]);
}

register({
    copyTextCtx: () => copyText(context_mmsi),
    copyTextICAO: () => copyText(getICAOFromHexIdent(context_mmsi)),
    toggleTrackCtx: () => toggleTrack(context_mmsi),
    toggleFollowCtx: () => toggleFollow(context_mmsi),
    mapResetViewZoomCtx: () => mapResetViewZoom(13, context_mmsi),
    showTargetcardCtx: (e, d) => showTargetcard(d.kind, context_mmsi),
    openAISCatcherSiteCtx: () => openExt('aiscatcher', context_mmsi),
    showVesselDetailCtx: () => showVesselDetail(context_mmsi),
    showNMEACtx: () => showNMEA(context_mmsi),
    openRealtimeForMMSICtx: () => openRealtimeForMMSI(context_mmsi),
    copyCoordinatesCtx: () => copyCoordinates(context_mmsi),
    openGoogleSearchCtx: (e, d) => openExt('google', d.icao ? getICAOFromHexIdent(context_mmsi) : context_mmsi),
    openVesselFinderCtx: () => openExt('vesselfinder', context_mmsi),
    openAISHubCtx: () => openExt('aishub', context_mmsi),
    replayMenu: (e) => showContextMenu(e, 0, null, ['ctx-replay-map']),
    targetcardContextMenu: (e) => showContextMenu(e, card_mmsi, card_type, ['object', 'object-map', 'ctx-targetcard']),
    mapSettingsContextMenu: (e, d, el) => showContextMenu(e, '', '', ['settings', 'ctx-map'], el),
    mainspaceContextMenu: (e) => showContextMenu(e, 0, '', ['settings']),
    plotsContextMenu: (e) => showContextMenu(e, '', 'charts', ['settings', 'ctx-charts']),
    showNMEAContextCopy: (e, d) => showContextMenu(e, d.copy || '', 'ship', ['copy-text']),
});
