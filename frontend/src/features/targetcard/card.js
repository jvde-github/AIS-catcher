// The card on the map, on a vessel, an aircraft or a station. This is the shell
// they share: opening and closing it, where it goes, pinning it, the ring on its
// target, its header's state, its footer's groups, the track option and the one
// rule of its tab and fold. What goes in it comes from a provider picked by type,
// each with the same shape { build, populate, open }: shipcard.js, planecard.js,
// station.js (a card of its own beside this one).

import { settings } from '../../core/state.js';
import {
    ships as shipsDB, planes as planesDB, cardMmsi as card_mmsi, cardType as card_type, markerTracks, setCard,
} from '../../core/store.js';
import { getShipName } from '../../core/names.js';
import { hasValidCoords } from '@aiscatcher/core/geo.js';
import { paneCramped } from '@aiscatcher/map/mapui.js';
import { map, markers, syncCircleFeature, trackLayer, labelLayer } from '../../map.js';
import { saveSettings, effects } from '../../settings-store.js';
import { showNotification } from '../../dialog.js';
import { ui } from '../../ui.js';
import * as tracks from '../../tracks.js';
import * as shipProvider from './shipcard.js';
import * as planeProvider from '../planecard/planecard.js';
import * as stationProvider from '../station/station.js';
import * as mapObjects from '../mapobjects/mapobjects.js';
import { nameAction, register } from '../../actions.js';
import { openExt } from '../contextmenu/contextmenu.js';
import { openRealtimeForMMSI } from '../../tabs/index.js';
import { cardPanel } from '../../panels.js';
import { syncTableSelection } from '../tablecard/tablecard.js';
import { stopHover } from '../hover/hover.js';

const providerOf = (type) => ({ ship: shipProvider, plane: planeProvider, station: stationProvider })[type];

// once, after the settings have loaded
export function buildCards() {
    ui.card.footer.reset();
    shipProvider.build();
    planeProvider.build();
    stationProvider.build();
    for (const group of ['ship', 'plane'])
        ui.card.footer.add({ icon: 'more_horiz', label: 'More', title: 'More options', action: 'rotateTargetcardIcons', group, more: true });
    ui.card.footer.show('ship');
}

export function targetcardVisible() {
    return cardPanel.isOpen();
}

export function fitTargetcard() {
    if (!targetcardVisible()) return;
    ui.card.fit();
}

// ─── the tab and the fold ────────────────────────────────────────────────────

/* The one rule, for every card as it opens on something (`fresh`: it was closed):
   a wide pane shows the remembered tab, so a new target opens unfolded; a cramped
   pane opens folded, and with the card already up keeps the fold as it is. */
export function openOnTab(layout, remembered, fresh) {
    if (!paneCramped()) layout.select(remembered, false);
    else if (fresh) layout.select(null, false);
    layout.scrollTop();
}

// ─── header ──────────────────────────────────────────────────────────────────

const VALIDATION = { 1: "card-validated", [-1]: "card-dubious" };

export function setValidation(v) {
    ui.card.header.state(VALIDATION[v] || "card-not-validated");
}

export function showOutOfRange() {
    document
        .getElementById("targetcard_content")
        .querySelectorAll("span:nth-child(2)")
        .forEach((e) => (e.innerHTML = null));
    ui.card.header.title().innerHTML = "<b style='color:red;'>Out of range</b>";
    ui.card.header.flag(null);
    document.getElementById("targetcard_mmsi").innerHTML = card_mmsi;

    updateFocusMarker();
}

// ─── footer ──────────────────────────────────────────────────────────────────

export function addItem(icon, txt, title, action, contextType = 'ship') {
    const name = nameAction(action);
    if (!name) return;
    if (icon.startsWith("fa")) icon = "question_mark";
    return ui.card.footer.add({ icon, label: txt, title, action: name, group: contextType });
}

export function updateTrackOption() {
    const trackOptionElement = document.getElementById("targetcard_track_option");

    if (settings.show_all_tracks || card_type == 'plane') {
        trackOptionElement.style.opacity = "0.5";
        trackOptionElement.style.pointerEvents = "none";
    } else {
        trackOptionElement.style.opacity = "1";
        trackOptionElement.style.pointerEvents = "auto";
    }

    // the button's tint says whether the track is on the map
    if (card_mmsi && card_type == 'ship') {
        const shown = markerTracks.has(Number(card_mmsi));
        trackOptionElement.classList.toggle("card-icon-active", shown);
        trackOptionElement.title = shown ? "Hide the track" : "Show the track";
    }
}

// ─── pin ─────────────────────────────────────────────────────────────────────

function pinTargetcard() {
    const at = ui.card.pin();
    settings.targetcard_pinned = true;
    settings.targetcard_pinned_x = at.x;
    settings.targetcard_pinned_y = at.y;
    showNotification("Card pinned to this position");
    saveSettings();
}

function unpinTargetcard() {
    ui.card.unpin();
    settings.targetcard_pinned = false;
    settings.targetcard_pinned_x = null;
    settings.targetcard_pinned_y = null;
    showNotification("Card unpinned");
    saveSettings();
}

function toggleTargetcardPin() {
    if (settings.targetcard_pinned) unpinTargetcard();
    else pinTargetcard();
}

effects({
    targetcard_pinned: () => ui.card.markPinned(settings.targetcard_pinned),
});

// `handle` drags the card; a pinned card's place follows
export function dragTargetcardBy(handle) {
    ui.card.draggable(handle);
    ui.card.el.addEventListener("card:moved", (e) => {
        if (!settings.targetcard_pinned) return;
        settings.targetcard_pinned_x = e.detail.x;
        settings.targetcard_pinned_y = e.detail.y;
        saveSettings();
    });
}

// ─── place ───────────────────────────────────────────────────────────────────

function moveMapCenter(px) {
    stopHover();
    ui.panTo(px);
}

function adjustMapForTargetcard(pixel) {
    const db = card_type == 'ship' ? shipsDB : card_type == 'plane' ? planesDB : null;
    const raw = db && card_mmsi in db ? db[card_mmsi].raw : null;
    if (!raw || !raw.lat || !raw.lon) return;

    if (ui.reveal([raw.lon, raw.lat], pixel, card_type == 'ship' ? { minZoom: 4 } : undefined) === "panned") stopHover();
}

export function positionAside(pixel, aside) {
    stopHover();
    if (!aside.offsetParent) return;

    if (settings.kiosk && settings.kiosk_pan_map && card_type == 'ship' && card_mmsi in shipsDB) {
        moveMapCenter(pixel);
        const mapSize = map.getSize();
        pixel = [mapSize[0] / 2, mapSize[1] / 2];
    }

    /* beside the target the card already leaves it visible; only a docked
       card may end up covering it */
    const how = ui.card.open(pixel);
    if (how === "dock") adjustMapForTargetcard(pixel);
    return how;
}

// ─── the ring on the target ──────────────────────────────────────────────────

let selectCircleFeature = undefined;

export function updateFocusMarker() {
    const raw = card_type == 'ship' ? shipsDB[card_mmsi]?.raw : card_type == 'plane' ? planesDB[card_mmsi]?.raw : null;
    selectCircleFeature = syncCircleFeature(selectCircleFeature, raw, card_mmsi, markers.selectRing);
}

// ─── open and close ──────────────────────────────────────────────────────────

export function showTargetcard(type, m, pixel = undefined) {
    if (type === 'station') return stationProvider.open(m, { fresh: !stationProvider.stationcardVisible(), pixel });

    if (m != null) {
        ui.dismiss();   // a card takes the stage, wherever it was opened from
        stationProvider.closeStationcard();
    }

    const aside = document.getElementById("targetcard");
    const visible = targetcardVisible();

    let ship = m in shipsDB ? shipsDB[m].raw : null;
    const prev_mmsi = card_mmsi;

    // a vessel the receiver does not know is out of range: say so, no card
    if (type == 'ship' && m != null && !ship) {
        showNotification("MMSI " + m + " is out of range", "error");
        return;
    }

    if (m != null) cardPanel.opening();

    tracks.selectionLeaving(card_mmsi, m);

    if (m != null && !visible) {
        cardPanel.show(true, { reposition: false });
        tracks.selectionOpened();
    } else if (visible && m == null) {
        cardPanel.show(false);
    }

    if (type !== card_type) {
        document.querySelectorAll('#targetcard [data-context-type]').forEach(element => {
            if (element.dataset.contextType === type) {
                element.style.display = '';
            } else {
                element.style.display = 'none';
            }
        });

        ui.card.footer.show(type);
    }

    setCard(m, type);
    if (m != null) providerOf(type).open(m, { fresh: !visible });
    syncTableSelection();

    if (targetcardVisible()) {
        if (settings.show_track_on_select && card_type == 'ship') tracks.selectionShown(m);

        if (ship && (!visible || prev_mmsi !== m) && !hasValidCoords(ship.lat, ship.lon))
            showNotification("No position received for " + (getShipName(ship) || m), "error");
        positionAside(pixel, aside);

        providerOf(card_type)?.populate();

        // trigger reflow for iPad Safari
        aside.style.display = 'none';
        aside.offsetHeight;
        aside.style.display = '';

        fitTargetcard();
    }

    trackLayer.changed();
    labelLayer.changed();
    updateFocusMarker();
}

export function closeTargetcard() {
    showTargetcard(null, null);
}

// with each pull of the data
export function refreshTargetcard() {
    if (targetcardVisible()) providerOf(card_type)?.populate();
}

register({
    toggleTargetcardPin: () => toggleTargetcardPin(),
    showTargetcardClose: () => closeTargetcard(),
    toggleTrackCard: () => tracks.toggleTrack(card_mmsi),
    showBinaryMessageDialogCard: () => mapObjects.showBinaryMessageDialog(card_mmsi),
    openRealtimeForMMSICard: () => openRealtimeForMMSI(card_mmsi),
    openAISCatcherSiteCard: () => openExt('aiscatcher', card_mmsi),
    openFlightAwareCard: () => openExt('flightaware', card_mmsi),
    openPlaneSpottersCard: () => openExt('planespotters', card_mmsi),
    openADSBExchangeCard: () => openExt('adsbexchange', card_mmsi),
    rotateTargetcardIcons: () => ui.card.footer.rotate(card_type),
});
