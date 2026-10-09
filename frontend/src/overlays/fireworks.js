// Fireworks mode: flashes a circle on the map for every message on api/signal.

import OlFeature from 'ol/Feature';
import Point from 'ol/geom/Point';
import Style from 'ol/style/Style';
import Stroke from 'ol/style/Stroke';
import Fill from 'ol/style/Fill';
import CircleStyle from 'ol/style/Circle';
import { fromLonLat } from 'ol/proj';
import { extraVector } from '../map.js';
import { config } from '../core/config.js';
import { register } from '../actions.js';
import { showDialog, showNotification } from '../dialog.js';

let evtSource = null;
let active = false;
let connected = false;
let consecutiveErrors = 0;
const MAX_CONSECUTIVE_ERRORS = 5;

export function isRunning() {
    return active;
}

export function toggle() {
    if (!active) start();
    else stop();
}

export function start() {
    if (active) return;
    if (!config.features.realtime) {
        showDialog("Error", "Cannot run Firework Mode. Please ensure that AIS-catcher is running with -N REALTIME on.");
        return;
    }
    active = true;
    connected = false;
    consecutiveErrors = 0;
    showNotification("Fireworks Mode started");
    openStream();
}

function openStream() {
    if (evtSource != null || document.hidden) return;

    evtSource = new EventSource("api/signal");

    evtSource.addEventListener(
        "nmea",
        function (e) {
            connected = true;
            consecutiveErrors = 0;

            let jsonData;
            try {
                jsonData = JSON.parse(e.data);
            } catch {
                return;
            }

            if (Object.hasOwn(jsonData, "channel") && Object.hasOwn(jsonData, "lat") && Object.hasOwn(jsonData, "lon")) {
                addMarker(jsonData.lat, jsonData.lon, jsonData.channel);
            }
        },
        false,
    );

    evtSource.onerror = function () {
        consecutiveErrors++;
        if (!connected || consecutiveErrors >= MAX_CONSECUTIVE_ERRORS) {
            stop();
            showDialog("Error", "Problem running Firework Mode, cannot reach server. Please ensure that AIS-catcher is running with -N REALTIME on.");
        }
    };

    evtSource.onopen = function () {
        connected = true;
        consecutiveErrors = 0;
        console.log("Fireworks connected");
    };
}

function closeStream() {
    if (evtSource != null) {
        evtSource.close();
        evtSource = null;
    }
}

export function stop() {
    if (!active) return;
    active = false;
    closeStream();
    showNotification("Fireworks Mode stopped");
}

document.addEventListener('visibilitychange', () => {
    if (document.hidden) closeStream();
    else if (active) openStream();
});

function addMarker(lat, lon, ch) {
    const latlon = fromLonLat([lon, lat]);
    let color = 'grey'; // Default color

    if (ch === "A") color = 'blue';
    if (ch === "B") color = 'red';

    let style = new Style({
        image: new CircleStyle({
            radius: 30,
            stroke: new Stroke({
                color: color,
                width: 10
            }),
            fill: new Fill({
                color: 'rgba(0,0,0,0)'
            })
        })
    });

    const marker = new OlFeature({
        geometry: new Point(latlon),
    });

    marker.setStyle(style);
    extraVector.addFeature(marker);

    setTimeout(function () {
        extraVector.removeFeature(marker);
    }, 1000);
}

register({
    ToggleFireworks: () => toggle(),
});
