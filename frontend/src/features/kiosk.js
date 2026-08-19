// Kiosk mode: hides interactive chrome and rotates the shipcard through
// randomly selected visible ships.

import { settings, isKiosk } from '../core/state.js';
import { ships, clock, cardMmsi, hoverMmsi } from '../core/store.js';
import { fromLonLat } from 'ol/proj';
import { containsCoordinate } from 'ol/extent';

// { getMap, showShipcard, saveSettings } - the fleet, the clock and what is
// selected come from the store rather than being handed over at init.
let deps = null;
let kioskAnimationInterval = null;
const DEFAULT_ROTATION_SPEED = 5;

export function init(d) {
    deps = d;
}

export function setKiosk(enabled) {
    settings.kiosk = enabled;
    updateKiosk();
    deps.saveSettings();
}

export function setKioskRotationSpeed(speed) {
    const parsed = parseInt(speed);
    settings.kiosk_rotation_speed = parsed > 0 ? parsed : DEFAULT_ROTATION_SPEED;
    deps.saveSettings();

    if (isKiosk() && kioskAnimationInterval) {
        startKioskAnimation();
    }
}

export function setKioskPanMap(enabled) {
    settings.kiosk_pan_map = enabled;
    deps.saveSettings();
}

export function toggleKioskMode() {
    settings.kiosk = !settings.kiosk;
    updateKiosk();
    deps.saveSettings();
}

const originalDisplayValues = new Map();

function clearAndHide(element) {
    if (!originalDisplayValues.has(element)) {
        originalDisplayValues.set(element, element.style.display);
    }
    element.style.display = "none";
}

// The snapshot is dropped once it has been handed back. Keeping it would make
// the first value this element ever had authoritative forever, so any later
// writer - a feature gate, a receiver list - would find its own value reverted
// the next time kiosk mode ended.
function restoreOriginalDisplay(element) {
    const saved = originalDisplayValues.get(element);
    originalDisplayValues.delete(element);

    if (saved) element.style.display = saved;
    else element.style.removeProperty('display');
}

// `restart` is opt-out because most callers are entering or leaving kiosk mode;
// callers that only need the chrome re-hidden - opening a menu, say - must not
// yank the rotation back to a new ship under the user's cursor.
export function updateKiosk(restart = true) {
    const kiosk = isKiosk();
    if (!kiosk) stopKioskAnimation();
    else if (restart || !kioskAnimationInterval) startKioskAnimation();

    const toHide = document.querySelectorAll(kiosk ? ".nokiosk" : ".kiosk");
    const toShow = document.querySelectorAll(kiosk ? ".kiosk" : ".nokiosk");
    toHide.forEach(clearAndHide);
    toShow.forEach(restoreOriginalDisplay);
}

function selectRandomShipForKiosk() {
    const map = deps.getMap();

    const mapExtent = map.getView().calculateExtent(map.getSize());
    const visibleShips = Object.keys(ships).filter(mmsi => {
        const ship = ships[mmsi].raw;
        if (!ship.lat || !ship.lon || ship.lat === 0 || ship.lon === 0) {
            return false;
        }

        const shipCoords = fromLonLat([ship.lon, ship.lat]);
        return containsCoordinate(mapExtent, shipCoords);
    });

    if (visibleShips.length === 0) {
        return null;
    }

    const candidates = visibleShips.filter(mmsi =>
        mmsi != cardMmsi && mmsi != hoverMmsi
    );

    const finalCandidates = candidates.length > 0 ? candidates : visibleShips;

    const weights = finalCandidates.map(mmsi => {
        const ship = ships[mmsi].raw;
        const timeSinceUpdate = (clock - ship.last_signal) || 3600;

        // Higher weight for more recently updated ships
        if (timeSinceUpdate < 60) return 10;
        if (timeSinceUpdate < 300) return 5;
        if (timeSinceUpdate < 900) return 2;
        return 1;
    });

    const totalWeight = weights.reduce((sum, weight) => sum + weight, 0);
    let random = Math.random() * totalWeight;

    for (let i = 0; i < finalCandidates.length; i++) {
        random -= weights[i];
        if (random <= 0) {
            return finalCandidates[i];
        }
    }

    return finalCandidates[0];
}

function showKioskShip(mmsi) {
    if (!mmsi || !(mmsi in ships)) {
        console.log("Invalid MMSI or ship not found:", mmsi);
        return;
    }

    const ship = ships[mmsi].raw;
    if (!ship.lat || !ship.lon) {
        console.log("Ship has no valid coordinates:", mmsi);
        return;
    }

    const map = deps.getMap();
    const shipCoords = fromLonLat([ship.lon, ship.lat]);
    const pixel = map.getPixelFromCoordinate(shipCoords);
    deps.showShipcard('ship', mmsi, pixel);
}

function showRandomKioskShip() {
    const selectedMMSI = selectRandomShipForKiosk();
    if (selectedMMSI) {
        showKioskShip(selectedMMSI);
    }
}

function startKioskAnimation() {
    if (kioskAnimationInterval) {
        clearInterval(kioskAnimationInterval);
    }

    showRandomKioskShip();
    kioskAnimationInterval = setInterval(function () {
        showRandomKioskShip();
    }, settings.kiosk_rotation_speed * 1000);
}

function stopKioskAnimation() {
    if (kioskAnimationInterval) {
        clearInterval(kioskAnimationInterval);
        kioskAnimationInterval = null;
        console.log("Kiosk animation stopped");
    }
}
