// Kiosk mode: hides interactive chrome and rotates the shipcard through
// randomly selected visible ships.

import { settings, isKiosk } from '../core/state.js';
import { fromLonLat } from 'ol/proj';
import { containsCoordinate } from 'ol/extent';

// { getMap, getShipsDB, getShipsSince, getCardMmsi, getHoverMmsi,
//   showShipcard, saveSettings }
let deps = null;
let kioskAnimationInterval = null;

export function init(d) {
    deps = d;
}

export function setKiosk(enabled) {
    settings.kiosk = enabled;
    updateKiosk();
    deps.saveSettings();
}

export function setKioskRotationSpeed(speed) {
    settings.kiosk_rotation_speed = parseInt(speed);
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
}

const originalDisplayValues = new Map();

function clearAndHide(element) {
    if (!originalDisplayValues.has(element)) {
        originalDisplayValues.set(element, element.style.display);
    }
    element.style.display = "none";
}

function restoreOriginalDisplay(element) {
    const saved = originalDisplayValues.get(element);
    if (saved) {
        element.style.display = saved;
    } else {
        element.style.removeProperty('display');
    }
}

export function updateKiosk() {
    const kiosk = isKiosk();
    if (kiosk) startKioskAnimation();
    else stopKioskAnimation();

    const toHide = document.querySelectorAll(kiosk ? ".nokiosk" : ".kiosk");
    const toShow = document.querySelectorAll(kiosk ? ".kiosk" : ".nokiosk");
    toHide.forEach(clearAndHide);
    toShow.forEach(restoreOriginalDisplay);
}

function selectRandomShipForKiosk() {
    const map = deps.getMap();
    const shipsDB = deps.getShipsDB();
    const shipsSince = deps.getShipsSince();

    const mapExtent = map.getView().calculateExtent(map.getSize());
    const visibleShips = Object.keys(shipsDB).filter(mmsi => {
        const ship = shipsDB[mmsi].raw;
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
        mmsi != deps.getCardMmsi() && mmsi != deps.getHoverMmsi()
    );

    const finalCandidates = candidates.length > 0 ? candidates : visibleShips;

    const weights = finalCandidates.map(mmsi => {
        const ship = shipsDB[mmsi].raw;
        const timeSinceUpdate = (shipsSince - ship.last_signal) || 3600;

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
    const shipsDB = deps.getShipsDB();
    if (!mmsi || !(mmsi in shipsDB)) {
        console.log("Invalid MMSI or ship not found:", mmsi);
        return;
    }

    const ship = shipsDB[mmsi].raw;
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
