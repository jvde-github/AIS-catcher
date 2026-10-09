// The card on the map: opening it on a vessel or an aircraft and closing it,
// where it goes and how big it is, pinning it, its two styles, and the ring
// that marks its target on the map. What goes in it is targetcard.js's and
// planecard.js's.

import { settings } from '../../core/state.js';
import { ships as shipsDB, planes as planesDB, cardMmsi as card_mmsi, cardType as card_type, setCard } from '../../core/store.js';
import { getShipName } from '../../core/names.js';
import { hasValidCoords } from '@aiscatcher/core/geo.js';
import { paneCramped } from '@aiscatcher/map/mapui.js';
import { map, markers, syncCircleFeature, trackLayer, labelLayer } from '../../map.js';
import { saveSettings, settingsPanel } from '../settings/settings.js';
import { showNotification } from '../../dialog.js';
import * as tracks from '../../tracks.js';
import * as targetcard from './targetcard.js';
import * as planecard from '../planecard/planecard.js';
import * as mapObjects from '../mapobjects/mapobjects.js';
import { register } from '../../actions.js';
import { openExt } from '../contextmenu/contextmenu.js';
import { openRealtimeForMMSI } from '../../tabs/index.js';
import { cardPanel } from '../../panels.js';
import { syncTableSelection } from '../tablecard/tablecard.js';

// { ui: the entry's map UI (card, dismiss, reveal, panTo), stopHover() }
let deps = null;

export function init(d) {
    deps = d;

    register({
        toggleTargetcardPin: () => toggleTargetcardPin(),
        toggleShipcardStyle: () => toggleShipcardStyle(),
        toggleTargetcardSize: () => toggleTargetcardSize(),
        targetcardSelectSelf: (e, dataset, el) => { if (settings.shipcard_style !== "tabs") targetcardselect(el); },
        showTargetcardClose: () => closeTargetcard(),
        toggleTrackCard: () => tracks.toggleTrack(card_mmsi),
        showBinaryMessageDialogCard: () => mapObjects.showBinaryMessageDialog(card_mmsi),
        openRealtimeForMMSICard: () => openRealtimeForMMSI(card_mmsi),
        openAISCatcherSiteCard: () => openExt('aiscatcher', card_mmsi),
        openFlightAwareCard: () => openExt('flightaware', card_mmsi),
        openPlaneSpottersCard: () => openExt('planespotters', card_mmsi),
        openADSBExchangeCard: () => openExt('adsbexchange', card_mmsi),

        // the card's rendered items
        rotateTargetcardIcons: () => deps.ui.card.footer.rotate(card_type),
        techInfo: (e) => { e.stopPropagation(); deps.ui.card.popover.toggle(document.getElementById("tech_popover"), document.getElementById("targetcard_tech_info")); },
        shiptypeInfo: (e) => { e.stopPropagation(); deps.ui.card.popover.toggle(document.getElementById("shiptype_popover"), document.getElementById("targetcard_shiptype_info")); },
        shipHistory: (e) => { e.stopPropagation(); deps.ui.card.section.open("changes"); },
        toggleTargetcardSection: (e, dataset, el) => { e.stopPropagation(); deps.ui.card.section.toggle(el?.dataset.section || dataset.section); },
    });
}

export function targetcardVisible() {
    return cardPanel.isOpen();
}

// ─── size ────────────────────────────────────────────────────────────────────

export function isTargetcardMax() {
    return deps.ui.card.isMax();
}

export function toggleTargetcardSize() {
    if (settings.shipcard_style === "tabs") return;   // the tabbed card stays maximised
    deps.ui.card.setMax(!deps.ui.card.isMax());
    fitTargetcard();
    if (isTargetcardMax()) adjustMapForTargetcard();
}

export function targetcardMinIfMaxonMobile() {
    if (targetcardVisible() && paneCramped() && isTargetcardMax()) toggleTargetcardSize();
}

// a classic card's row: a click keeps it open, or folds the card when it is the header
export function targetcardselect(e) {
    if (!deps.ui.card.keep.toggle(e)) toggleTargetcardSize();
    saveSettings();
}

// the card's size and kept rows as the loaded settings have them
export function restoreCardLayout() {
    if (!isTargetcardMax()) toggleTargetcardSize();

    if (deps.ui.card.keep.restore(settings.targetcard_rows)) {
        if (settings.targetcard_max != isTargetcardMax()) toggleTargetcardSize();
    } else {
        settings.targetcard_rows = [];
    }
}

// what a save keeps of the card: the tabbed card neither folds nor keeps
// rows, so those stay as classic left them
export function saveCardLayout() {
    if (settings.shipcard_style !== "tabs") {
        settings.targetcard_max = isTargetcardMax();
        settings.targetcard_rows = deps.ui.card.keep.state();
    }
}

export function fitTargetcard() {
    if (!targetcardVisible()) return;
    deps.ui.card.fit();
}

// ─── pin and style ───────────────────────────────────────────────────────────

function pinTargetcard() {
    const at = deps.ui.card.pin();
    settings.targetcard_pinned = true;
    settings.targetcard_pinned_x = at.x;
    settings.targetcard_pinned_y = at.y;
    showNotification("Card pinned to this position");
    saveSettings();
}

function unpinTargetcard() {
    deps.ui.card.unpin();
    settings.targetcard_pinned = false;
    settings.targetcard_pinned_x = null;
    settings.targetcard_pinned_y = null;
    showNotification("Card unpinned");
    saveSettings();
}

export function toggleTargetcardPin() {
    if (settings.targetcard_pinned) unpinTargetcard();
    else pinTargetcard();
}

export function applyTargetcardPinStyling() {
    deps.ui.card.markPinned(settings.targetcard_pinned);
}

export function toggleShipcardStyle() {
    settings.shipcard_style = settings.shipcard_style === "tabs" ? "classic" : "tabs";
    setTargetcardStyle(settings.shipcard_style);
    if (settingsPanel.sync) settingsPanel.sync();
    saveSettings();
}

export function setTargetcardStyle(style) {
    targetcard.setStyle(style);
    planecard.setStyle(style);
    document.querySelectorAll('#targetcard [data-context-type]').forEach(el => {
        el.style.display = el.dataset.contextType === card_type ? '' : 'none';
    });
    deps.ui.card.footer.show(card_type);
    fitTargetcard();
}

// ─── place ───────────────────────────────────────────────────────────────────

function moveMapCenter(px) {
    deps.stopHover();
    deps.ui.panTo(px);
}

export function adjustMapForTargetcard(pixel) {
    const db = card_type == 'ship' ? shipsDB : card_type == 'plane' ? planesDB : null;
    const raw = db && card_mmsi in db ? db[card_mmsi].raw : null;
    if (!raw || !raw.lat || !raw.lon) return;

    if (deps.ui.reveal([raw.lon, raw.lat], pixel, card_type == 'ship' ? { minZoom: 4 } : undefined) === "panned") deps.stopHover();
}

export function positionAside(pixel, aside) {
    deps.stopHover();
    if (!aside.offsetParent) return;

    if (settings.kiosk && settings.kiosk_pan_map && card_type == 'ship' && card_mmsi in shipsDB) {
        moveMapCenter(pixel);
        const mapSize = map.getSize();
        pixel = [mapSize[0] / 2, mapSize[1] / 2];
    }

    /* beside the target the card already leaves it visible; only a docked
       card may end up covering it */
    const how = deps.ui.card.open(pixel);
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
    targetcard.resetHistory();
    if (m != null) deps.ui.dismiss();   // a card takes the stage, wherever it was opened from

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

        deps.ui.card.footer.show(type);
    }

    setCard(m, type);
    if (type == 'ship' && m != null) targetcard.loadVessel(m);
    syncTableSelection();

    if (targetcardVisible()) {
        if (settings.show_track_on_select && card_type == 'ship') tracks.selectionShown(m);

        /* a fresh card opens at the size the setting asks for, except on a
           cramped pane which always opens compact; a target out of range has
           nothing to show and opens compact too; switching ship with the
           card up keeps the size the user has */
        const known = card_type != 'ship' || ship != null;
        const wantMax = known && settings.targetcard_open_max && !paneCramped();
        if (settings.shipcard_style !== "tabs" && !visible && isTargetcardMax() !== wantMax) toggleTargetcardSize();
        if (ship && (!visible || prev_mmsi !== m) && !hasValidCoords(ship.lat, ship.lon))
            showNotification("No position received for " + (getShipName(ship) || m), "error");
        positionAside(pixel, aside);

        if (card_type == 'ship') targetcard.populate();
        else if (card_type == 'plane') { planecard.open(!visible); planecard.populate(); }

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
