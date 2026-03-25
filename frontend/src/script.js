
import { registerActions, registerContextActions, registerChanges, registerInputs, initEvents } from './events.js';
import { settings, restoreDefaultSettings, persist, onSettingsChange, notifyChange, init as initSettings } from './settings.js';
import { init as initDecoder } from './tabs/decoder.js';
import { init as initRealtime, onActivate as onRealtimeActivate, onDeactivate as onRealtimeDeactivate, openRealtimeForMMSI, getRealtimeState } from './tabs/realtime.js';
import { init as initCharts, updateStatistics, updatePlots, updateAllChartColors, setGraphVisibility } from './tabs/charts.js';
import {
    init as initSettings_tab, updateSettingsTab, updateDarkMode, closeSettings,
} from './tabs/settings.js';
import { init as initTable, update as updateShipTable, refreshTable } from './tabs/table.js';
import { init as initLog, onActivate as onLogActivate, onDeactivate as onLogDeactivate } from './tabs/log.js';
import {
    shipsDB, planesDB, binaryDB, station, center, getShipCount,
    fetchShips, fetchPlanes, fetchBinary, fetchJSON,
} from './tabs/map/state.js';
import {
    init as initTableside,
    updateTableSort, updateSortMarkers, updateMarkerCount,
    updateTablecard, toggleTablecard, hideTablecard, toggleStatcard, closeTableSide,
} from './tabs/map/tableside.js';
import {
    init as initShipcard,
    card_mmsi, card_type,
    showShipcard, closeShipcard, populateShipcard, populatePlanecard,
    updateFocusMarker, updateShipcardTrackOption,
    shipcardVisible, isShipcardMax, shipcardMinIfMaxonMobile,
    toggleShipcardSize, displayShipcardIcons, rotateShipcardIcons,
    toggleMeasurecard, measurecardVisible,
    showBinaryMessageDialog, addShipcardItem,
    toggleShipcardPin, trackOptionString,
} from './tabs/map/shipcard.js';
import {
    getDistanceVal, getDistanceUnit,
    getSpeedVal, getSpeedUnit,
    getShipName, getICAOfromHexIdent, getICAO,
    getDeltaTimeVal,
} from './format.js';
import {
    init as initUI,
    showNotification, showDialog, closeDialog,
    showContextMenu, hideContextMenu,
    isAndroid, isKiosk, updateAndroid,
    showAboutDialog, showWelcome,
    toggleMenu, hideMenu, showMenu, hideMenuifSmall,
    toggleScreenSize, toggleInfoPanel, initFullScreen,
    copyText, copyClipboard,
    getContextMMSI, getContextType,
} from './ui.js';

let interval,
    activeReceiver = 0,
    paths = {},
    map,
    basemaps = {},
    overlapmaps = {},
    hover_feature = undefined,
    show_all_tracks = false,
    evtSourceMap = null,

    range_outline = undefined,
    range_outline_short = undefined,
    tab_title_station = "",
    tab_title_count = null,
    aboutContentLoaded = false;


const tab_title = "AIS-catcher";

const baseMapSelector = document.getElementById("baseMapSelector");

const plugins_main = [];
    
// Wrappers so HTML onclick handlers don't reference globals directly
function cardOpenADSBExchange()        { openADSBExchange(card_mmsi); }
function cardOpenAIScatcherSite()      { openAIScatcherSite(card_mmsi); }
function cardOpenFlightAware()         { openFlightAware(card_mmsi); }
function cardOpenPlaneSpotters()       { openPlaneSpotters(card_mmsi); }
function cardOpenRealtime()            { openRealtimeForMMSI(card_mmsi); }
function cardShowBinaryMessageDialog() { showBinaryMessageDialog(card_mmsi); }
function cardShowContextMenu(event)    { showContextMenu(event, card_mmsi, card_type, ['object', 'object-map', 'ctx-shipcard']); }
function cardToggleTrack()             { toggleTrack(card_mmsi); }

function ctxCopyCoordinates()      { copyCoordinates(getContextMMSI()); }
function ctxCopyText()             { copyText(getContextMMSI()); }
function ctxCopyICAOText()         { copyText(getICAOfromHexIdent(getContextMMSI())); }
function ctxZoomIn()               { mapResetViewZoom(13, getContextMMSI()); }
function ctxOpenAISHub()           { openAISHub(getContextMMSI()); }
function ctxOpenAIScatcherSite()   { openAIScatcherSite(getContextMMSI()); }
function ctxOpenGoogleSearch()     { openGoogleSearch(getContextMMSI()); }
function ctxOpenGoogleSearchICAO() { openGoogleSearch(getICAOfromHexIdent(getContextMMSI())); }
function ctxOpenRealtime()         { openRealtimeForMMSI(getContextMMSI()); }
function ctxOpenVesselFinder()     { openVesselFinder(getContextMMSI()); }
function ctxPinVessel()            { pinVessel(getContextMMSI()); }
function ctxShowNMEA()             { showNMEA(getContextMMSI()); }
function ctxShowShipcard()         { showShipcard('ship', getContextMMSI()); }
function ctxShowPlanecard()        { showShipcard('plane', getContextMMSI()); }
function ctxShowVesselDetail()     { showVesselDetail(getContextMMSI()); }
function ctxToggleTrack()          { toggleTrack(getContextMMSI()); }

// Wrappers for multi-arg / compound onclick patterns
function showSettingsMenu(e)      { showContextMenu(e, '', '', ['settings', 'center']); }
function showMapSettingsMenu(e)   { showContextMenu(e, '', '', ['settings', 'center', 'ctx-map']); }
function showMainContextMenu(e)   { showContextMenu(e, 0, '', ['settings']); }
function showChartsContextMenu(e) { showContextMenu(e, '', 'charts', ['settings', 'ctx-charts']); }
function activateMeasureMode()    { setMeasureMode(); showNotification('Shift+click on start point/object'); }
function showAboutAndInfo()       { toggleInfoPanel(); showAboutDialog(); }
function closeShipcard()          { showShipcard(null, null); }



// DOM event dispatch is handled by events.js — see initEvents() call in the init section below.

const refreshIntervalMs = 2500;
let range_update_time = null;
let updateInProgress = false;
let activeTileLayer = undefined;
let debounceUpdateCommunityFeed;

let hover_enabled_track = false,
    select_enabled_track = false,
    marker_tracks = new Set();

// Defaults — plugins.js used to override these before script.js ran.
// Now declared at module scope so all code below can reference them.
let communityFeed = false;
let aboutMDpresent = false;
let context = "aiscatcher";

if (typeof window.loadPlugins === 'undefined') {
    window.loadPlugins = function () { };
}

const hover_info = document.getElementById('hover-info');

let measures = [];


function getDefaultTrackColors() {
    return {
        [ShippingClass.CARGO]: '#00ff7f',
        [ShippingClass.B]: '#ff00ff',
        [ShippingClass.PASSENGER]: '#0000ff',
        [ShippingClass.SPECIAL]: '#a52a2a',
        [ShippingClass.TANKER]: '#ff0000',
        [ShippingClass.HIGHSPEED]: '#ffff00',
        [ShippingClass.OTHER]: '#12a5ed',
        [ShippingClass.UNKNOWN]: '#12a5ed',
        [ShippingClass.FISHING]: '#ff1493',
        [ShippingClass.ATON]: '#0000f1',
        [ShippingClass.STATION]: '#0000f1',
        [ShippingClass.SARTEPIRB]: '#ff0000',
        [ShippingClass.PLANE]: '#ff0040',
        [ShippingClass.HELICOPTER]: '#d01616'
    };
}

// NMEA Decoder — moved to tabs/decoder.js

function updateTitle() {
    document.title = (tab_title_count ? " (" + tab_title_count + ") " : "") + tab_title + " " + tab_title_station;
}

// some functions useful in plugins
function addTileLayer(title, layer) {
    basemaps[title] = layer;
}

function removeTileLayer(title) {
    delete basemaps[title];
}
function removeTileLayerAll() {
    basemaps = {};
}
function addOverlayLayer(title, layer) {
    overlapmaps[title] = layer;
}
function removeOverlayLayer(title) {
    delete overlapmaps[title];
}
function removeOverlayLayerAll() {
    overlapmaps = {};
}
function addControlToMap(c) {
    c.addTo(map);
}



function copyCoordinates(m) {
    let coords = "not found";

    if (m in shipsDB) coords = shipsDB[m].raw.lat + "," + shipsDB[m].raw.lon;
    if (copyClipboard(coords)) showNotification("Coordinates copied to clipboard");
}

let hoverMMSI = undefined;
let hoverType = undefined;

const rangeStyleFunction = function (feature) {
    let clr = undefined;

    if (feature.short) {
        clr = settings.dark_mode ? settings.range_color_dark_short : settings.range_color_short;
    } else {
        clr = settings.dark_mode ? settings.range_color_dark : settings.range_color;
    }

    return new ol.style.Style({
        stroke: new ol.style.Stroke({
            color: clr,
            width: feature === hover_feature ? 4 : 2
        })
    });
}

const shapeStyleFunction = function (feature) {

    const c = settings.shipoutline_inner;
    const o = settings.shipoutline_opacity;

    return new ol.style.Style({
        fill: new ol.style.Fill({
            color: `rgba(${parseInt(c.slice(-6, -4), 16)}, ${parseInt(c.slice(-4, -2), 16)}, ${parseInt(c.slice(-2), 16)}, ${o})`
        }),
        stroke: new ol.style.Stroke({
            color: hoverMMSI && hoverType == 'ship' && feature.ship.mmsi == hoverMMSI ? settings.shiphover_color : settings.shipoutline_border,
            width: 2
        }),
    });
}

const ShippingClass = {
    OTHER: 0,
    UNKNOWN: 1,
    CARGO: 2,
    B: 3,
    PASSENGER: 4,
    SPECIAL: 5,
    TANKER: 6,
    HIGHSPEED: 7,
    FISHING: 8,
    PLANE: 9,
    HELICOPTER: 10,
    STATION: 11,
    ATON: 12,
    SARTEPIRB: 13,
};

const trackStyleFunction = function (feature) {
    let w = Number(settings.track_weight);
    let c = '#12a5ed'; // Default fallback color

    // Use shipping class color if available
    if (feature.shipclass && settings.track_class_colors[feature.shipclass]) {
        c = settings.track_class_colors[feature.shipclass];
    }

    if (feature.mmsi == hoverMMSI && hoverType == 'ship') {
        c = settings.shiphover_color;
        w = w + 2;
    }
    else if (feature.mmsi == card_mmsi && card_type == 'ship') {
        c = settings.shipselection_color;
        w = w + 2;
    }

    return new ol.style.Style({
        stroke: new ol.style.Stroke({
            color: c,
            width: w,
            lineDash: feature.isDashed ? [6, 6] : undefined
        })
    });
}

const markerStyle = function (feature) {

    const length = (feature.ship.to_bow || 0) + (feature.ship.to_stern || 0);
    const mult = length >= 100 && length <= 200 ? 0.9 : length > 200 ? 1.1 : 0.75;

    return new ol.style.Style({
        image: new ol.style.Icon({
            src: SpritesAll,
            rotation: feature.ship.rot,
            offset: [feature.ship.cx, feature.ship.cy],
            size: [feature.ship.imgSize, feature.ship.imgSize],
            scale: settings.icon_scale * mult,
            opacity: 1
        })
    });
};


const planeStyleOld = function (feature) {

    return new ol.style.Style({
        image: new ol.style.Icon({
            src: SpritesAll,
            rotation: feature.plane.rot,
            offset: [feature.plane.cx, feature.plane.cy],
            size: [feature.plane.imgSize, feature.plane.imgSize],
            scale: settings.icon_scale * feature.plane.scaling,
            opacity: 1
        })
    });
};

const planeStyle = function (feature) {
    const altitude = feature.plane.altitude || 0;
    const shadowScale = Math.min(Math.max(altitude / 40000, 0.1), 1); // 0.1-1 scale based on height up to 40000ft

    return [
        /* Shadow/border layer
        new ol.style.Style({
            image: new ol.style.Icon({
                src: "https://www.aiscatcher.org/hub_test/sprites_hub.png",
                rotation: feature.plane.rot,
                offset: [feature.plane.cx, feature.plane.cy],
                size: [feature.plane.imgSize, feature.plane.imgSize],
                scale: (settings.icon_scale * feature.plane.scaling) * (1 + shadowScale * 1),
                opacity: 0.25
            })
        }), */
        // Main aircraft layer
        new ol.style.Style({
            image: new ol.style.Icon({
                src: SpritesAll,
                rotation: feature.plane.rot,
                offset: [feature.plane.cx, feature.plane.cy],
                size: [feature.plane.imgSize, feature.plane.imgSize],
                scale: settings.icon_scale * feature.plane.scaling,
                opacity: 1
            })
        })
    ];
};

function decodeHTMLEntities(text) {
    const textArea = document.createElement('textarea');
    textArea.innerHTML = text;
    return textArea.value;
}

const labelStyle = function (feature) {
    const font = settings.tooltipLabelFontSize + "px Arial";
    return new ol.style.Style({
        text: new ol.style.Text({
            text: decodeHTMLEntities('ship' in feature ?
                (feature.ship.shipname || feature.ship.mmsi.toString()) :
                (feature.plane.callsign || getICAO(feature.plane))),
            overflow: true,
            offsetY: 25,
            offsetX: 25,
            fill: new ol.style.Fill({
                color: settings.dark_mode ? settings.tooltipLabelColorDark : settings.tooltipLabelColor
            }),
            stroke: new ol.style.Stroke({
                color: settings.dark_mode ? settings.tooltipLabelShadowColorDark : settings.tooltipLabelShadowColor,
                width: 5
            }),
            font: font
        })
    });
};

const hoverCircleStyleFunction = function (feature) {
    const iconScale = settings.icon_scale || 1.0;
    const circleScale = settings.circle_scale || 6.0;
    const radiusScale = 1 + (circleScale - 2.0) * 0.08; // Scale radius slightly with line width
    return new ol.style.Style({
        image: new ol.style.Circle({
            radius: 16 * iconScale * radiusScale,
            stroke: new ol.style.Stroke({
                color: settings.shiphover_color,
                width: circleScale * iconScale
            })
        })
    });
}


const binaryStyle = function (feature) {
    const count = feature.get('binary_count') || feature.binary_count || 1;
    const isAssociated = feature.get('is_associated') || feature.is_associated;

    const zIndex = isAssociated ? 200 : 100;

    if (isAssociated) {
        // Style for binary features at ship locations (badge style)
        const outlineStyle = new ol.style.Style({
            image: new ol.style.Circle({
                radius: 12,
                fill: new ol.style.Fill({
                    color: 'rgba(255, 255, 255, 0)'
                }),
                stroke: new ol.style.Stroke({
                    color: 'white',
                    width: 2
                })
            }),
            zIndex: zIndex
        });

        const badgeStyle = new ol.style.Style({
            geometry: function (feature) {
                const center = feature.getGeometry().getCoordinates();
                return new ol.geom.Point(center);
            },
            image: new ol.style.Circle({
                radius: 8,
                fill: new ol.style.Fill({
                    color: 'rgba(220, 0, 0, 0.9)'
                }),
                stroke: new ol.style.Stroke({
                    color: 'white',
                    width: 1
                }),
                displacement: [10, 10]
            }),
            text: new ol.style.Text({
                text: count.toString(),
                font: 'bold 9px Arial',
                fill: new ol.style.Fill({
                    color: 'white'
                }),
                offsetX: 10,
                offsetY: -10,
                textAlign: 'center',
                textBaseline: 'middle'
            }),
            zIndex: zIndex + 1
        });

        return [outlineStyle, badgeStyle];
    } else {
        const circleStyle = new ol.style.Style({
            image: new ol.style.Circle({
                radius: 10,
                fill: new ol.style.Fill({
                    color: 'rgba(220, 0, 0, 0.9)'
                }),
                stroke: new ol.style.Stroke({
                    color: 'white',
                    width: 1.5
                })
            }),
            text: new ol.style.Text({
                text: count.toString(),
                font: 'bold 9px Arial', // Reduced from 10px
                fill: new ol.style.Fill({
                    color: 'white'
                }),
                textAlign: 'center',
                textBaseline: 'middle'
            }),
            zIndex: zIndex // Same z-index for the entire standalone badge
        });

        return [circleStyle];
    }
};


const markerVector = new ol.source.Vector({
    features: []
})

const binaryVector = new ol.source.Vector({
    features: []
})


const rangeVector = new ol.source.Vector({
    features: []
})

const shapeVector = new ol.source.Vector({
    features: []
});

const extraVector = new ol.source.Vector({
    features: []
});

const trackVector = new ol.source.Vector({
    features: []
});

const labelVector = new ol.source.Vector({
    features: []
});

const planeVector = new ol.source.Vector({
    features: []
});

const markerLayer = new ol.layer.Vector({
    source: markerVector,
    style: markerStyle
})

const binaryLayer = new ol.layer.Vector({
    source: binaryVector,
    style: binaryStyle
})

const planeLayer = new ol.layer.Vector({
    source: planeVector,
    style: planeStyle,
    visible: false
})

const shapeLayer = new ol.layer.Vector({
    source: shapeVector,
    style: shapeStyleFunction
});

const extraLayer = new ol.layer.Vector({
    source: extraVector
});

const trackLayer = new ol.layer.Vector({
    source: trackVector,
    style: trackStyleFunction
});

const rangeLayer = new ol.layer.Vector({
    source: rangeVector,
    style: rangeStyleFunction
});

const labelLayer = new ol.layer.Vector({
    source: labelVector,
    style: labelStyle,
    declutter: settings.labels_declutter || true
});

const measureSource = new ol.source.Vector();

const measureStyle = new ol.style.Style({
    stroke: new ol.style.Stroke({
        color: 'green',
        lineDash: [20, 20],
        width: 2,
    })
});

const measureStyleWhite = new ol.style.Style({
    stroke: new ol.style.Stroke({
        color: 'white',
        lineDash: [20, 20],
        lineDashOffset: 20,
        width: 2,
    })
});

// label for the measure line
const measureLabelStyle = new ol.style.Style({
    text: new ol.style.Text({
        font: '14px Calibri,sans-serif',
        fill: new ol.style.Fill({
            color: 'rgba(255, 255, 255, 1)',
        }),
        backgroundFill: new ol.style.Fill({
            color: 'green',
        }),
        padding: [3, 3, 3, 3],
        textBaseline: 'bottom',
        offsetY: -15,
    }),
});

const calculateBearing = function (start, end) {
    const startLat = toRadians(start[1]);
    const startLon = toRadians(start[0]);
    const endLat = toRadians(end[1]);
    const endLon = toRadians(end[0]);

    const dLon = endLon - startLon;
    const y = Math.sin(dLon) * Math.cos(endLat);
    const x = Math.cos(startLat) * Math.sin(endLat) - Math.sin(startLat) * Math.cos(endLat) * Math.cos(dLon);

    const bearing = toDegrees(Math.atan2(y, x));
    return (bearing + 360) % 360; // Normalize to 0-360
};

const toRadians = function (degrees) {
    return degrees * Math.PI / 180;
};

const toDegrees = function (radians) {
    return radians * 180 / Math.PI;
};

const measureVector = new ol.layer.Vector({
    source: measureSource,
    style: function (feature) {
        return measureStyleFunction(feature);
    },
});

function measureStyleFunction(feature) {
    const styles = [];
    const geometry = feature.getGeometry();
    const type = geometry.getType();
    let point, label;
    if (type === 'LineString') {
        point = new ol.geom.Point(geometry.getLastCoordinate());
        label = `${feature.measureDistance} ${getDistanceUnit()}, ${feature.measureBearing} degrees`;
    }
    styles.push(measureStyle);
    styles.push(measureStyleWhite);
    if (label) {
        measureLabelStyle.setGeometry(point);
        measureLabelStyle.getText().setText(label);
        styles.push(measureLabelStyle);
    }
    return styles;
}

let shapeFeatures = {};
let markerFeatures = {};
const planeFeatures = {};

let stationFeature = undefined;
let hoverCircleFeature = undefined;
const measureCircleFeature = undefined;


// Function to create ship outline geometry in OpenLayers
function createShipOutlineGeometry(ship) {
    if (!ship) return null;
    const coordinate = [ship.lon, ship.lat];

    let heading = ship.heading;
    const { to_bow, to_stern, to_port, to_starboard } = ship;

    if (to_bow == null || to_stern == null || to_port == null || to_starboard == null) return null;

    if (heading == null) {
        if (ship.cog != null && ship.speed > 1) heading = ship.cog;
        else return new ol.geom.Circle(ol.proj.fromLonLat(coordinate), Math.max(to_bow, to_stern));
    }

    const deltaBow = calcOffset1M(coordinate, heading % 360);
    const deltaStarboard = calcOffset1M(coordinate, (heading + 90) % 360);

    const bow = calcMove(coordinate, deltaBow, to_bow);
    const stern = calcMove(coordinate, deltaBow, -to_stern);

    const A = calcMove(stern, deltaStarboard, to_starboard);
    const B = calcMove(stern, deltaStarboard, -to_port);
    const C = calcMove(B, deltaBow, 0.8 * (to_bow + to_stern));
    const Dmid = calcMove(C, deltaStarboard, 0.5 * (to_starboard + to_port));
    const D = calcMove(Dmid, deltaBow, 0.2 * (to_bow + to_stern));
    const E = calcMove(C, deltaStarboard, to_starboard + to_port);

    const shipOutlineCoords = [A, B, C, D, E, A].map(coord => ol.proj.fromLonLat(coord));
    return new ol.geom.Polygon([shipOutlineCoords]);
}

function showCommunity() {
    if (!communityFeed) {
        showDialog("Community feed is not available", "Enable by running with -X which will send your AIS data to www.aiscatcher.org and enable the options</br>Thank you for supporting AIS-catcher");
        return;
    }
}

async function showNMEA(m) {
    if (typeof message_save !== "undefined" && message_save) {
        const s = await fetchJSON("api/message", m);
        const obj = JSON.parse(s);

        let tableHtml = '<table class="mytable">';
        for (const key in obj) {
            let value = obj[key];
            if (Array.isArray(value)) {
                value = JSON.stringify(value).replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/'/g, "\\'").replace(/"/g, '\\"');
            }
            tableHtml += "<tr><td>" + key + "</td><td oncontextmenu='showContextMenu(event,\"" + value + ' ","ship",["settings","copy-text"])\'>' + value + "</td></tr>";
        }
        tableHtml += "</table>";

        showDialog("Message " + m, tableHtml);
    } else {
        showDialog("Error", 'Please enable "-N MSG on" in AIS-catcher settings.');
    }
}

async function showVesselDetail(m) {
    const s = await fetchJSON("api/vessel", m);
    const obj = JSON.parse(s);

    let tableHtml = '<table class="mytable">';
    for (const key in obj) {
        tableHtml += "<tr><td>" + key + "</td><td>" + obj[key] + "</td></tr>";
    }
    tableHtml += "</table>";

    showDialog("Vessel " + m, tableHtml);
}

function openGoogleSearch(m) {
    window.open("https://www.google.com/search?q=" + m);
}

function openAIScatcherSite(m) {
    window.open("https://www.aiscatcher.org/ship/details/" + m);
}

function openMarineTraffic(m) {
    window.open(" https://www.marinetraffic.com/en/ais/details/ships/mmsi:" + m);
}

function openShipXplorer(m) {
    window.open("https://www.shipxplorer.com/data/vessels/IMO-MMSI-" + m);
}

function openVesselFinder(m) {
    window.open("https://www.vesselfinder.com/vessels/details/" + m);
}

function openAISHub(m) {
    window.open("https://www.aishub.net/vessels?Ship[mmsi]=" + m);
}

function openPlaneSpotters(m) {
    window.open("https://www.planespotters.net/hex/" + getICAOfromHexIdent(m));
}

function openADSBExchange(m) {
    window.open("https://globe.adsbexchange.com/?icao=" + getICAOfromHexIdent(m));
}

function openFlightAware(m) {
    window.open("https://flightaware.com/live/modes/" + getICAOfromHexIdent(m) + "/redirect");
}

const mapMenu = document.getElementById("map-menu");

function hideMapMenu(event) {

    if (!mapMenu.contains(event.target)) {
        mapMenu.style.display = "none";
        document.removeEventListener("click", hideMapMenu);
    }
}

function showMapMenu(event) {

    hideContextMenu();
    if (event && event.preventDefault) {
        event.preventDefault();
        event.stopPropagation();
    }

    baseMapSelector.value = settings.dark_mode ? settings.map_night : settings.map_day;

    mapMenu.style.display = "block";

    mapMenu.style.left = "50%";
    mapMenu.style.top = "50%";
    mapMenu.style.transform = "translate(-50%, -50%)";

    document.addEventListener("click", function (event) {
        hideMapMenu(event);
    });
}

function checkLatestVersion() {
    if (typeof build_version === 'undefined' || build_version === 'unknown') {
        console.log('AIS-catcher: build_version not available, skipping version check');
        return;
    }

    fetch('https://www.aiscatcher.org/api/version')
        .then(response => {
            if (!response.ok) {
                throw new Error('Failed to fetch latest release info');
            }
            return response.json();
        })
        .then(data => {
            const latestVersion = data.tag_name;
            if (latestVersion && latestVersion !== build_version) {
                console.log('AIS-catcher: New version available! Current: ' + build_version + ', Latest: ' + latestVersion);
                showDialog('Update Available',
                    'A new version of AIS-catcher is available!<br><br>' +
                    'Current version: <b>' + build_version + '</b><br>' +
                    'Latest version: <b>' + latestVersion + '</b><br><br>' +
                    'Updating ensures you have the latest features and security updates.<br><br>' +
                    'Please visit <a href="https://docs.aiscatcher.org" target="_blank">docs.aiscatcher.org</a> for installation instructions or ' +
                    '<a href="https://github.com/jvde-github/AIS-catcher/releases/latest" target="_blank">GitHub Releases</a> to download the latest version.');
            } else {
                console.log('AIS-catcher: You are running the latest version (' + build_version + ')');
            }
        })
        .catch(error => {
            console.log('AIS-catcher: Could not check for updates - ' + error.message);
        });
}

function getStatusVal(ship) {
    const StringFromStatus = [
        "Under way using engine",
        "At anchor",
        "Not under command",
        "Restricted manoeuverability",
        "Constrained",
        "Moored",
        "Aground",
        "Engaged in Fishing",
        "Under way sailing",
        "Reserved for HSC",
        "Reserved for WIG",
        "Reserved",
        "Reserved",
        "Reserved",
        "AIS-SART is active",
        "Not available",
    ];

    return StringFromStatus[Math.min(ship.status, 15)];
}

function getShipTypeVal(s) {
    if (s < 20) return "Not available";
    if (s <= 29) return "WIG";
    if (s <= 30) return "Fishing";
    if (s <= 32) return "Towing";
    if (s <= 34) return "Dredging/Diving ops";
    if (s <= 35) return "Military";
    if (s <= 36) return "Sailing";
    if (s <= 37) return "Pleasure Craft";
    if (s <= 39) return "Reserved";
    if (s <= 49) return "High Speed Craft";
    if (s <= 50) return "Pilot";
    if (s <= 51) return "Search And Rescue";
    if (s <= 52) return "Tug";
    if (s <= 53) return "Port tender";
    if (s <= 54) return "Anti-pollution equipment";
    if (s <= 55) return "Law Enforcement";
    if (s <= 57) return "Local Vessel";
    if (s <= 58) return "Medical Transport";
    if (s <= 59) return "Noncombatant ship";
    if (s <= 69) return "Passenger";
    if (s <= 79) return "Cargo";
    if (s <= 89) return "Tanker";
    if (s <= 99) return "Other";

    if ((s >= 1500 && s <= 1920) || (s >= 8000 && s <= 8510)) {
        switch (s) {
            case 8000:
                return "Unknown (inland AIS)";
            case 8010:
                return "Motor Freighter";
            case 8020:
                return "Motor Tanker";
            case 8021:
                return "Motor Tanker (liquid)";
            case 8022:
                return "Motor Tanker (liquid)";
            case 8023:
                return "Motor Tanker (dry)";
            case 8030:
                return "Container";
            case 8040:
                return "Gas Tanker";
            case 8050:
                return "Motor Freighter (tug)";
            case 8060:
                return "Motor Tanker (tug)";
            case 8070:
                return "Motor Freighter (alongside)";
            case 8080:
                return "Motor Freighter (with tanker)";
            case 8090:
                return "Motor Freighter (pushing)";
            case 8100:
                return "Motor Freighter (pushing)";
            case 8110:
                return "Tug, Freighter";
            case 8120:
                return "Tug, Tanker";
            case 8130:
                return "Tug Freighter (coupled)";
            case 8140:
                return "Tug, freighter/tanker";
            case 8150:
                return "Freightbarge";
            case 8160:
                return "Tankbarge";
            case 8161:
                return "Tankbarge (liquid)";
            case 8162:
                return "Tankbarge (liquid)";
            case 8163:
                return "Tankbarge (dry)";
            case 8170:
                return "Freightbarge (with containers)";
            case 8180:
                return "Tankbarge (gas)";
            case 8210:
                return "Pushtow (one cargo barge)";
            case 8220:
                return "Pushtow (two cargo barges)";
            case 8230:
                return "Pushtow, (three cargo barges)";
            case 8240:
                return "Pushtow (four cargo barges)";
            case 8250:
                return "Pushtow (five cargo barges)";
            case 8260:
                return "Pushtow (six cargo barges)";
            case 8270:
                return "Pushtow (seven cargo barges)";
            case 8280:
                return "Pushtow (eigth cargo barges)";
            case 8290:
                return "Pushtow (nine or more barges)";
            case 8310:
                return "Pushtow (one tank/gas barge)";
            case 8320:
                return "Pushtow (two barges)";
            case 8330:
                return "Pushtow (three barges)";
            case 8340:
                return "Pushtow (four barges)";
            case 8350:
                return "Pushtow (five barges)";
            case 8360:
                return "Pushtow (six barges)";
            case 8370:
                return "Pushtow (seven barges)";
            case 8380:
                return "Pushtow (eight barges)";
            case 8390:
                return "Pushtow (nine or more barges)";
            case 8400:
                return "Tug (single)";
            case 8410:
                return "Tug (one or more tows)";
            case 8420:
                return "Tug (assisting)";
            case 8430:
                return "Pushboat (single)";
            case 8440:
                return "Passenger";
            case 8441:
                return "Ferry";
            case 8442:
                return "Red Cross";
            case 8443:
                return "Cruise";
            case 8444:
                return "Passenger";
            case 8450:
                return "Service, Police or Port Service";
            case 8460:
                return "Maintainance Craft";
            case 8470:
                return "Object (towed)";
            case 8480:
                return "Fishing";
            case 8490:
                return "Bunkership";
            case 8500:
                return "Barge, Tanker, Chemical";
            case 8510:
                return "Object";
            case 1500:
                return "General";
            case 1510:
                return "Unit Carrier Maritime";
            case 1520:
                return "bulk Carrier Maritime";
            case 1530:
                return "Tanker";
            case 1540:
                return "Liquified Gas Tanker";
            case 1850:
                return "Pleasure";
            case 1900:
                return "Fast Ship";
            case 1910:
                return "Hydrofoil";
            case 1920:
                return "Catamaran Fast";
        }
    }
    return "Unknown (" + s + ")";
}

// MMSI types from AIS-catcher
const OTHER = 0;
const CLASS_A = 1;
const CLASS_B = 2;
const BASESTATION = 3;
const SAR = 4;
const SARTEPIRB = 5;
const ATON = 6;

function headerClick() {
    window.open("https://www.aiscatcher.org");
}

function openWebControl() {
    if (typeof webcontrol_http !== "undefined" && webcontrol_http) {
        window.open(webcontrol_http, '_blank');
    }
}

function updateMapLayer() {

    if (activeTileLayer) {

        const overlays = JSON.parse(JSON.stringify(settings.map_overlay));
        settings.map_overlay = JSON.parse(JSON.stringify(overlays));

        setMapOpacity();
        triggerMapLayer();
    }
}

function setMap(key) {
    if (settings.dark_mode)
        settings.map_night = key;
    else
        settings.map_day = key;

    triggerMapLayer();
    saveSettings();
}

function triggerMapLayer() {

    if (activeTileLayer)
        activeTileLayer.setVisible(false);

    if (settings.dark_mode) {
        activeTileLayer = settings.map_night in basemaps ? basemaps[settings.map_night] : basemaps[Object.keys(basemaps)[0]];

    } else {
        activeTileLayer = settings.map_day in basemaps ? basemaps[settings.map_day] : basemaps[Object.keys(basemaps)[0]];
    }

    activeTileLayer.setVisible(true);

    if (settings.map_overlay.length > 0) {
        for (let i = 0; i < settings.map_overlay.length; i++) {
            if (settings.map_overlay[i] in overlapmaps)
                overlapmaps[settings.map_overlay[i]].setVisible(settings.map_overlay[i] in overlapmaps);
        }
    }

    const attributions = activeTileLayer.getSource().getAttributions();
    const mapAttributions = document.getElementById("map_attributions");
    if (typeof attributions === 'function') {
        const currentAttributions = attributions();
        mapAttributions.innerHTML = currentAttributions.join(', ');
    } else if (Array.isArray(attributions)) {
        mapAttributions.innerHTML = attributions.join(', ');
    }

}

const dynamicStyle = document.createElement("style");
document.head.appendChild(dynamicStyle);

function applyDynamicStyling() {
    let style = ``

    if (!isAndroid())
        style += `
            @media only screen and (min-width: 750px) {
                #menubar {
                    position: fixed;
                    top: 70px;
                    left: 10px;
                    right: 0;
                    width: 500px;
                    border: solid;
                    border-color: var(--menu-border-color);
                    border-radius: 5px;
                    box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1), 0 4px 6px -2px rgba(0, 0, 0, 0.05);
                }
            }
    `;
    else style += " .settings_window { top: 72px; }\n";

    dynamicStyle.innerHTML = style;
}

function setMapOpacity() {
    for (const key in basemaps)
        basemaps[key].setOpacity(Number(settings.map_opacity));

    for (const key in overlapmaps) {
        if (key !== "Aircraft") {
            overlapmaps[key].setOpacity(Number(settings.map_opacity));
        }
    }
}

let clickTimeout = undefined;
let isMeasuring = false;
let measureMode = false;

function refreshMeasures() {
    measureSource.clear();

    const mapcardContent = document.getElementById('measurecardInner');
    mapcardContent.innerHTML = '';

    let content = '';

    measures = measures.filter(measure => {


        if ((measure.start_type == 'ship' && !(measure.start_value in shipsDB))) {
            showNotification('Ship out of range for measurement.');
            return false;
        }

        let sc = undefined, ss = undefined, from = undefined;
        if (measure.start_type == 'point') {
            sc = ol.proj.fromLonLat(measure.start_value);
            from = "point";
        } else {
            ss = shipsDB[measure.start_value].raw;
            sc = ol.proj.fromLonLat([shipsDB[measure.start_value].raw.lon, shipsDB[measure.start_value].raw.lat]);
            from = ss.shipname || ss.mmsi;
        }

        let ec = undefined, es = undefined, to = "";
        if ('end_type' in measure) {
            if (measure.end_type == 'point') {
                ec = ol.proj.fromLonLat(measure.end_value);
                to = "point";
            } else {
                es = shipsDB[measure.end_value].raw;
                ec = ol.proj.fromLonLat([shipsDB[measure.end_value].raw.lon, shipsDB[measure.end_value].raw.lat]);
                to = es.shipname || es.mmsi;
            }
        }

        let distance = 0, bearing = 0;

        if (sc && ec) {
            const geometry = new ol.geom.LineString([sc, ec]);

            const length = ol.sphere.getLength(geometry);
            distance = getDistanceVal(length / 1852);
            const coordinates = geometry.getCoordinates();
            const start = ol.proj.toLonLat(coordinates[0]);
            const end = ol.proj.toLonLat(coordinates[coordinates.length - 1]);
            bearing = calculateBearing(start, end).toFixed(0);

            if (measure.visible) {
                const feature = new ol.Feature(geometry);
                measureSource.addFeature(feature);
                feature.measureDistance = distance;
                feature.measureBearing = bearing;
            }
        }
        const icon = measure.visible ? 'visibility' : 'visibility_off';

        content += `<tr data-index="${measures.indexOf(measure)}"><td style="padding: 2px;"><i style="padding-left:2px; font-size: 18px;" class="${icon}_icon visibility_icon"></i></td><td style="padding: 0px;"><i style="font-size: 18px;" class="delete_icon"></i></td><td>${from}</td><td>${to}</td><td title="${distance} ${getDistanceUnit()}">${distance}</td><td title="${bearing} degrees">${bearing}</td></tr>`;

        return true;
    });

    mapcardContent.innerHTML = content;
}

document.addEventListener('DOMContentLoaded', () => {
    const mapcardContent = document.getElementById('measurecardInner');

    mapcardContent.addEventListener('click', (event) => {
        const row = event.target.closest('tr');
        if (row) {
            const measureIndex = row.getAttribute('data-index');
            if (event.target.classList.contains('visibility_icon')) {
                measures[measureIndex].visible = !measures[measureIndex].visible;
                refreshMeasures();
            } else if (event.target.classList.contains('delete_icon')) {
                measures.splice(measureIndex, 1);
                refreshMeasures();
            }
        }
    });

    refreshMeasures();
});

function startMeasurementAtPoint(t, v) {
    isMeasuring = true;
    measures.push({ start_value: v, start_type: t, visible: true });
    if (!measurecardVisible()) toggleMeasurecard();
    showNotification('Select end point or object');
    refreshMeasures();
    startMeasureMode();
}

function endMeasurement(t, v) {
    if (isMeasuring) {

        const lastMeasureIndex = measures.length - 1;
        measures[lastMeasureIndex] = {
            ...measures[lastMeasureIndex],
            end_value: v,
            end_type: t
        };

        isMeasuring = false;

        showNotification('Measurement added.');
        refreshMeasures();
        clearMeasureMode();
    }
}

function startMeasureMode() {
    measureMode = true;
    document.getElementById('map').classList.add('crosshair_cursor');
}

function clearMeasureMode() {
    measureMode = false;
    document.getElementById('map').classList.remove('crosshair_cursor');
}

function setMeasureMode() {
    measureMode = true;
    document.getElementById('map').classList.add('crosshair_cursor');
}

const handleClick = function (pixel, target, event) {
    if (clickTimeout) {
        clearTimeout(clickTimeout);
        clickTimeout = null;
        return;
    }

    const feature = target.closest('.ol-control') ? undefined : map.forEachFeatureAtPixel(pixel,
        function (feature) { if ('ship' in feature || 'plane' in feature || 'link' in feature || 'binary' in feature) { return feature; } }, { hitTolerance: 10 });

    const included = feature && 'ship' in feature && feature.ship.mmsi in shipsDB;
    const included_plane = feature && 'plane' in feature && feature.plane.hexident in planesDB;

    if (event.originalEvent.shiftKey || measureMode || isMeasuring) {
        measureMode = false;

        if (isMeasuring) {
            if (feature && 'ship' in feature && feature.ship.mmsi in shipsDB) {
                endMeasurement("ship", feature.ship.mmsi);
            }
            else {
                endMeasurement("point", ol.proj.toLonLat(map.getCoordinateFromPixel(pixel)));
            }
            return;
        }

        if (feature && 'ship' in feature && feature.ship.mmsi in shipsDB) {
            startMeasurementAtPoint("ship", feature.ship.mmsi);
            return;
        }
        else {
            startMeasurementAtPoint("point", ol.proj.toLonLat(map.getCoordinateFromPixel(pixel)));
        }

        return;
    }



    if (feature && 'link' in feature && !included) {
        window.open(feature.link, '_blank');
    } else if (feature && feature.binary === true && !feature.is_associated) {
        closeDialog();
        closeSettings();
        showBinaryMessageDialog(feature);
        return;
    } else if (feature && 'ship' in feature || included) {

        closeDialog();
        closeSettings();
        showShipcard('ship', feature.ship.mmsi, pixel);
    }
    else if (feature && 'plane' in feature || included_plane) {

        closeDialog();
        closeSettings();
        showShipcard('plane', feature.plane.hexident, pixel);
    }
    else {
        clickTimeout = setTimeout(function () {
            showShipcard(null, null);
            clickTimeout = null;
        }, 300);
    }
};

function initMap() {

    map = new ol.Map({

        target: 'map',
        view: new ol.View({
            center: ol.proj.fromLonLat([settings.lon || 0, settings.lat || 0]),
            zoom: settings.zoom || 6,
            enableRotation: false,
        }),
        controls: []
    })

    for (const [key, value] of Object.entries(basemaps)) {
        map.addLayer(value);
        value.setVisible(false);
    }

    for (const [key, value] of Object.entries(overlapmaps)) {
        map.addLayer(value);
        value.setVisible(false);
    }

    [rangeLayer, shapeLayer, trackLayer, markerLayer, labelLayer, extraLayer, binaryLayer, measureVector].forEach(layer => {
        map.addLayer(layer);
    });

    triggerMapLayer();

    let mapMoving = false;

    map.on('movestart', function () {
        mapMoving = true;
        stopHover();
    });

    map.on('moveend', function (evt) {
        mapMoving = false;
        debouncedSaveSettings();
        debouncedDrawMap();
        if (communityFeed)
            debounceUpdateCommunityFeed();
    });

    map.on('pointermove', function (evt) {
        if (evt.dragging || mapMoving) return;
        const pixel = map.getEventPixel(evt.originalEvent);
        handlePointerMove(pixel, evt.originalEvent.target);
    });

    map.on('click', function (evt) {
        handleClick(evt.pixel, evt.originalEvent.target, evt);
    });


    map.getTargetElement().addEventListener('pointerleave', function () {
        stopHover();
    });

    map.getTargetElement().addEventListener('contextmenu', function (evt) {

        const f = getFeature(map.getEventPixel(evt), map.getTargetElement())

        if (!f)
            showContextMenu(evt, 0, null, ['settings', 'ctx-map']);
        else if ('station' in f) {
            showContextMenu(evt, null, null, ["station", "ctx-map"]);
        }
        else if ('ship' in f)
            showContextMenu(evt, f.ship.mmsi, 'ship', ["ship", "ship-map"]);
        else if ('plane' in f)
            showContextMenu(evt, f.plane.hexident, 'plane', ["plane", "plane-map"]);
    });

    baseMapSelector.innerHTML = '';

    Object.keys(basemaps).forEach(key => {
        const option = document.createElement("option");
        option.value = key;
        option.textContent = key;
        baseMapSelector.appendChild(option);
    });
    baseMapSelector.value = settings.dark_mode ? settings.map_night : settings.map_day;

    baseMapSelector.addEventListener('change', function () { setMap(this.value); });

    const overlayContainer = document.getElementById('overlayContainer');

    Object.keys(overlapmaps).forEach(key => {
        const checkbox = document.createElement('input');
        checkbox.type = 'checkbox';
        checkbox.id = key;
        checkbox.name = key;
        checkbox.checked = settings.map_overlay.includes(key);

        const label = document.createElement('label');
        label.setAttribute('for', key);
        label.textContent = key;

        overlayContainer.appendChild(checkbox);
        overlayContainer.appendChild(label);

        overlayContainer.appendChild(document.createElement('br'));

        checkbox.addEventListener('change', function () {
            overlapmaps[key].setVisible(this.checked);

            if (this.checked) {
                if (!settings.map_overlay.includes(key)) {
                    settings.map_overlay.push(key);
                }
            } else {
                const index = settings.map_overlay.indexOf(key);
                if (index > -1) {
                    settings.map_overlay.splice(index, 1);
                }
            }
            saveSettings();
            redrawMap();
        });
    });

    setMapOpacity();
}

function toggleLabel() {
    if (settings.show_labels == "never") {
        settings.show_labels = "always";
    } else
        settings.show_labels = "never";

    saveSettings();
    redrawMap();
}

function addMarker(lat, lon, ch) {
    const latlon = ol.proj.fromLonLat([lon, lat]);
    let color = 'grey'; // Default color

    if (ch === "A") color = 'blue';
    if (ch === "B") color = 'red';

    const style = new ol.style.Style({
        image: new ol.style.Circle({
            radius: 30,
            stroke: new ol.style.Stroke({
                color: color,
                width: 10
            }),
            fill: new ol.style.Fill({
                color: 'rgba(0,0,0,0)'
            })
        })
    });

    const marker = new ol.Feature({
        geometry: new ol.geom.Point(latlon),
    });

    marker.setStyle(style);
    extraVector.addFeature(marker);

    setTimeout(function () {
        extraVector.removeFeature(marker);
    }, 1000);

}

function ToggleFireworks() {
    if (evtSourceMap == null) StartFireworks();
    else StopFireworks();
}

function StartFireworks() {
    if (evtSourceMap == null) {
        if (typeof realtime_enabled === "undefined" || !realtime_enabled) {
            showDialog("Error", "Cannot run Firework Mode. Please ensure that AIS-catcher is running with -N REALTIME on.");
            return;
        }

        evtSourceMap = new EventSource("api/signal");

        evtSourceMap.addEventListener(
            "nmea",
            function (e) {
                const jsonData = JSON.parse(e.data);

                if (jsonData.hasOwnProperty("channel") ** jsonData.hasOwnProperty("lat") && jsonData.hasOwnProperty("lon")) {
                    addMarker(jsonData.lat, jsonData.lon, jsonData.channel);
                }
            },
            false,
        );

        evtSourceMap.onerror = function (event) {
            StopFireworks();
            showDialog("Error", "Problem running Firework Mode, cannot reach server. Please ensure that AIS-catcher is running with -N REALTIME on.");
        };

        evtSourceMap.onopen = function (event) {
            showNotification("Fireworks Mode started");
            console.log("Fireworks connected");
        };
    }
}

function StopFireworks() {
    if (evtSourceMap != null) {
        showNotification("Fireworks Mode stopped");
        evtSourceMap.close();
        evtSourceMap = null;
    }
}


function setRangeColor(v, f) {
    settings[f] = v;
    redrawMap();
}

async function toggleRange() {
    const isSationSet = station && station.hasOwnProperty("lat") && station.hasOwnProperty("lon") && !(station.lat == 0 && station.lon == 0);

    if (!(typeof param_share_loc != "undefined" && param_share_loc) || !isSationSet) {
        showDialog("Error", "Unable to show range as station location not available");
        settings.show_range = false;
    } else settings.show_range = !settings.show_range;

    saveSettings();

    await fetchRange();
    drawRange();
}

function toggleFading() {
    settings.fading = !settings.fading;
}


async function fetchAbout() {
    let response;
    try {
        response = await fetch("about.md");
    } catch (error) {
        return;
    }
    return await response.text();
}

async function fetchRange(forcefetch = false) {
    const isSationSet = station && station.hasOwnProperty("lat") && station.hasOwnProperty("lon") && !(station.lat == 0 && station.lon == 0);

    if (!isSationSet || !settings.show_range) {
        settings.show_range = false;
        range_update_time = undefined;
        return;
    }

    const now = new Date();

    if (!forcefetch && range_update_time && Math.floor((now - range_update_time) / 1000 / 60) < 15) return;

    range_update_time = now;

    let response;
    let h;
    try {
        response = await fetch("api/history_full.json?receiver=" + activeReceiver);
        h = await response.json();
    } catch (error) {
        settings.show_range = false;

        return;
    }



    range_outline = [];
    range_outline_short = [];

    const N = h.day.stat[0].radar_a.length;

    const range = [];
    const range_short = [];

    for (let i = 0; i < N; i++) {
        let m = 0;
        for (let j = 0; j < h.minute.stat.length; j++) {
            m = Math.max(m, h.minute.stat[j].radar_a[i]);
            m = Math.max(m, h.minute.stat[j].radar_b[i]);
        }

        range_short.push(m);

        for (let j = 0; j < h.hour.stat.length; j++) {
            m = Math.max(m, h.hour.stat[j].radar_a[i]);
            m = Math.max(m, h.hour.stat[j].radar_b[i]);
        }

        const additionalDays = settings.range_timeframe == "7d" ? 7 : settings.range_timeframe == "30d" ? 30 : 0;

        for (let j = 0; j < Math.min(additionalDays, h.day.stat.length); j++) {
            m = Math.max(m, h.day.stat[j].radar_a[i]);
            m = Math.max(m, h.day.stat[j].radar_b[i]);
        }

        range.push(m);
    }

    const deltaNorth = calcOffset1M([station.lon, station.lat], 0)[0];
    const deltaEast = calcOffset1M([station.lon, station.lat], 90)[1];

    for (let i = 0; i < N; i++) {
        range_outline.push([
            station.lon + ((range[i] * deltaEast * 1000) / 0.5399568) * Math.sin(((i * 2) / N) * Math.PI),
            station.lat + ((range[i] * deltaNorth * 1000) / 0.5399568) * Math.cos(((i * 2) / N) * Math.PI),
        ]);

        range_outline.push([
            station.lon + ((range[i] * deltaEast * 1000) / 0.5399568) * Math.sin((((i + 1) * 2) / N) * Math.PI),
            station.lat + ((range[i] * deltaNorth * 1000) / 0.5399568) * Math.cos((((i + 1) * 2) / N) * Math.PI),
        ]);

        range_outline_short.push([
            station.lon + ((range_short[i] * deltaEast * 1000) / 0.5399568) * Math.sin(((i * 2) / N) * Math.PI),
            station.lat + ((range_short[i] * deltaNorth * 1000) / 0.5399568) * Math.cos(((i * 2) / N) * Math.PI),
        ]);

        range_outline_short.push([
            station.lon + ((range_short[i] * deltaEast * 1000) / 0.5399568) * Math.sin((((i + 1) * 2) / N) * Math.PI),
            station.lat + ((range_short[i] * deltaNorth * 1000) / 0.5399568) * Math.cos((((i + 1) * 2) / N) * Math.PI),
        ]);
    }

    range_outline = range_outline.map(point => ol.proj.fromLonLat(point));
    range_outline_short = range_outline_short.map(point => 0 * ol.proj.fromLonLat(point));
}

let rangeFeature = undefined;
let rangeShortFeature = undefined;

function drawRange() {
    if (rangeFeature) {
        rangeVector.removeFeature(rangeFeature);
        rangeFeature = undefined;
    }

    if (rangeShortFeature) {
        rangeVector.removeFeature(rangeShortFeature);
        rangeShortFeature = undefined;
    }

    if (!settings.show_range) return;

    if (range_outline) {
        rangeFeature = new ol.Feature({
            geometry: new ol.geom.Polygon([range_outline])
        });

        rangeFeature.short = false;
        rangeFeature.rangering = true;
        rangeFeature.tooltip = "Station Range " + settings.range_timeframe;
        rangeVector.addFeature(rangeFeature);
    }

    if (range_outline_short) {
        rangeShortFeature = new ol.Feature({
            geometry: new ol.geom.Polygon([range_outline_short])
        });

        rangeShortFeature.tooltip = "Station Range 1h";
        rangeShortFeature.rangering = true;
        rangeShortFeature.short = true;
        rangeVector.addFeature(rangeShortFeature);
    }
}

let distanceFeatures = undefined;
let distanceLat = undefined;
let distanceLon = undefined;
let distanceMetric = undefined;

function removeDistanceCircles() {
    if (distanceFeatures) {
        for (let i = 0; i < distanceFeatures.length; i++)
            rangeVector.removeFeature(distanceFeatures[i]);
    }
    distanceFeatures = undefined;
}


function distanceCircleStyleFunction(feature) {
    let clr, width;

    if (feature === hover_feature) {
        clr = 'orange';
        clr = settings.distance_circle_color;

        width = 5;
    } else {
        clr = settings.distance_circle_color;
        width = 1;
    }

    return new ol.style.Style({
        stroke: new ol.style.Stroke({
            color: clr,
            width: width
        })
    });
}


function createDistanceGeometry(lat, lon, radius) {

    const deltaNorth = calcOffset1M([station.lon, station.lat], 0)[0];
    const deltaEast = calcOffset1M([station.lon, station.lat], 90)[1];
    const N = 50;

    let outline = [];
    for (let i = 0; i < N; i++) {
        outline.push([
            station.lon + ((radius * deltaEast)) * Math.sin(((i * 2) / N) * Math.PI),
            station.lat + ((radius * deltaNorth)) * Math.cos(((i * 2) / N) * Math.PI),
        ]);
    }

    outline = outline.map(point => ol.proj.fromLonLat(point));
    return new ol.geom.Polygon([outline]);
}

function updateDistanceCircles() {
    const lat = station.lat;
    const lon = station.lon;

    if (!settings.distance_circles) {
        removeDistanceCircles();
        return;
    }

    if (!lat || !lon) return;

    if (!distanceFeatures || distanceLat != lat || distanceLon != lon || distanceMetric != settings.metric) {

        removeDistanceCircles();

        distanceLat = lat;
        distanceLon = lon;
        distanceMetric = settings.metric;

        distanceFeatures = [];

        const conv = settings.metric === "DEFAULT" ? 1.852 : settings.metric === "SI" ? 1 : 1.609344;

        const range = [5000, 10000, 25000, 50000, 100000];

        for (let i = 0; i < range.length; i++) {

            const distanceCircle = new ol.Feature({
                geometry: createDistanceGeometry(lat, lon, range[i] * conv)
            });

            distanceCircle.tooltip = range[i] / 1000 + " " + getDistanceUnit().toUpperCase();
            distanceCircle.distancecircle = true;
            distanceCircle.setStyle(distanceCircleStyleFunction);
            rangeVector.addFeature(distanceCircle);
            distanceFeatures.push(distanceCircle);
        }
    }
}




// we calculate the lat/lon for 1m move in direction of heading
// underlying calculation uses an offset of 100m and then scales down to 1.

const cos100R = 0.9999999998770914; // cos(100m / R);
const sin100R = 1.567855942823164e-5; // sin(100m / R)
const rad = Math.PI / 180;
const radInv = 180 / Math.PI;


function calcOffset1M(coordinate, heading) {
    const lat = coordinate[1] * rad;
    const rheading = ((heading + 360) % 360) * rad;
    const sinLat = Math.sin(lat);
    const cosLat = Math.cos(lat);

    const sinLat2 = sinLat * cos100R + cosLat * sin100R * Math.cos(rheading);
    const lat2 = Math.asin(sinLat2);
    const deltaLon = Math.atan2(Math.sin(rheading) * sin100R * cosLat, cos100R - sinLat * sinLat2);

    return [(lat2 * radInv - coordinate[1]) / 100, (deltaLon * radInv) / 100];
}

function calcMove(coordinate, delta, distance) {
    return [coordinate[0] + delta[1] * distance, coordinate[1] + delta[0] * distance];
}

function updateReceiverSelect(receivers) {
    const wrap = document.getElementById("receiver-btn-wrap");
    if (!wrap) return;
    if (!receivers || receivers.length <= 1) {
        wrap.style.display = "none";
        return;
    }
    wrap.style.display = "flex";
    const btn = document.getElementById("receiver-btn");
    if (btn) btn.classList.toggle("active", activeReceiver !== 0);
    const dd = document.getElementById("receiver-dropdown");
    if (!dd) return;
    if (dd.children.length !== receivers.length) {
        dd.innerHTML = "";
        for (const r of receivers) {
            const item = document.createElement("div");
            item.className = "receiver-dropdown-item" + (r.idx === activeReceiver ? " active" : "");
            item.dataset.idx = r.idx;
            item.textContent = r.label;
            item.onclick = () => { onReceiverChange(r.idx); closeReceiverDropdown(); };
            dd.appendChild(item);
        }
    } else {
        for (const item of dd.children) {
            item.classList.toggle("active", parseInt(item.dataset.idx) === activeReceiver);
        }
    }
}

function toggleReceiverDropdown(event) {
    event.stopPropagation();
    const dd = document.getElementById("receiver-dropdown");
    if (!dd) return;
    dd.style.display = dd.style.display === "none" ? "block" : "none";
}

function closeReceiverDropdown() {
    const dd = document.getElementById("receiver-dropdown");
    if (dd) dd.style.display = "none";
}

function onReceiverChange(idx) {
    activeReceiver = parseInt(idx, 10) || 0;
    const btn = document.getElementById("receiver-btn");
    if (btn) btn.classList.toggle("active", activeReceiver !== 0);
    saveSettings();
    refresh_data();
}

let displayNames;
function getCountryName(isoCode) {
    if (!displayNames && typeof Intl !== "undefined" && typeof Intl.DisplayNames === "function") {
        displayNames = new Intl.DisplayNames(["en"], { type: "region" });
    }
    if (displayNames) {
        try {
            const countryName = displayNames.of(isoCode);
            return countryName ? countryName : "";
        } catch (error) {
            return isoCode;
        }
    } else {
        return isoCode;
    }
}

function getFlag(country) {
    return country ? `<span style="padding-right: 10px" title="` + getCountryName(country) + `" class="fi fi-${country.toLowerCase()}"></span> ` : "<span></span>";
}

function getFlagStyled(country, style) {
    return country ? `<span style="` + style + `" title="` + getCountryName(country) + `" class="fi fi-${country.toLowerCase()}"></span> ` : "<span></span>";
}

// Tabulator ships tab → tabs/table.js

document.addEventListener('click', function (event) {
    const wrap = document.getElementById('receiver-btn-wrap');
    if (wrap && !wrap.contains(event.target)) closeReceiverDropdown();
});

function getTooltipContent(ship) {
    let content = '<div class="tooltip-card">' +
        getFlagStyled(ship.country, "padding: 0px; margin: 0px; margin-right: 10px; margin-left: 3px; box-shadow: 1px 1px 2px rgba(0, 0, 0, 0.2); font-size: 26px; opacity: 70%") +
        '<div>' +
        (getShipName(ship) || ship.mmsi) + ' at ' + getSpeedVal(ship.speed) + ' ' + getSpeedUnit() + '<br>' +
        'Received ' + getDeltaTimeVal(ship.last_signal) + ' ago' +
        '</div>' +
        '</div>';

    if (ship.mmsi in binaryDB && binaryDB[ship.mmsi].ship_messages &&
        binaryDB[ship.mmsi].ship_messages.length > 0) {

        const meteoMessages = binaryDB[ship.mmsi].ship_messages.filter(msg =>
            msg.message && msg.message.dac == 1 &&
            (msg.message.fid == 31 || msg.message.fi == 31)
        );

        if (meteoMessages.length > 0) {
            content += getBinaryMessageContent(meteoMessages, true);
        }
    }

    return content;
}

function getTooltipContentPlane(plane) {
    const altitude = plane.airborne == 1 ? (plane.altitude ? Math.round(plane.altitude) + ' ft' : '-') : 'ground';
    const speed = plane.speed ? Math.round(plane.speed) : '-';
    return '<div class="tooltip-card">' +
        getFlagStyled(plane.country, "padding: 0px; margin: 0px; margin-right: 10px; margin-left: 3px; box-shadow: 1px 1px 2px rgba(0, 0, 0, 0.2); font-size: 26px; opacity: 70%") +
        '<div>' +
        (plane.callsign || plane.hexident) + ' at ' + altitude + '/' + speed + ' kts<br>' +
        'Received ' + getDeltaTimeVal(plane.last_signal) + ' ago' +
        '</div>' +
        '</div>';
}

function getTypeVal(ship) {
    switch (ship.mmsi_type) {
        case CLASS_A:
            return "Class A";
        case CLASS_B:
            let classB = "Class B";
            if (ship.cs_unit === 1) classB += " (SO)"; // SOTDMA - 5 watts
            else if (ship.cs_unit === 2) classB += " (CS)"; // CSTDMA - 2 watts
            return classB;
        case BASESTATION:
            return "Base Station";
        case SAR:
            return "SAR aircraft";
        case SARTEPIRB:
            return "AIS SART/EPIRB";
        case ATON:
            return "AtoN";
    }
    return "Unknown";
}

function getShipOpacity(ship) {
    if (settings.fading == false) return 1;

    const opacity = 1 - (ship.last_signal / 1800) * 0.8;
    return Math.max(0.2, Math.min(1, opacity));
}


function getShipCSSClassAndStyle(ship, opacity = 1, scale = settings.icon_scale) {
    getSprite(ship);
    let style = `opacity: ${opacity};`;

    style += `background-position: -${ship.cx - 0}px -${ship.cy - 0}px; width: 20px; height: 20px; transform: rotate(${ship.rot}rad) scale(${scale});`;

    return { class: "sprites", style: style, hint: ship.hint };
}

function getIconCSS(ship, opacity = 1) {
    const { class: classValue, style, hint } = getShipCSSClassAndStyle(ship, opacity);
    return `class="${classValue}" style="${style}" title="${hint}"`;
}

function getTableShiptype(ship, opacity = 1) {
    if (ship == null) return "";

    const { class: classValue, style, hint } = getShipCSSClassAndStyle(ship, opacity);
    return settings.table_shiptype_use_icon ? `<span class="${classValue}" style="${style}" title="${hint}"></span>` : hint;
}

function getIcon(ship) {
    const { class: classValue, style } = getShipCSSClassAndStyle(ship);
    return L.divIcon({ html: `<div class="${classValue}" style="${style}"></div>`, className: "undefined" });
}

function notImplemented() {
    showDialog("Warning", "Not implemented yet");
}




const showTooltipShip = (tooltip, mmsi, pixel, distance, angle = 0) => {

    tooltip.innerHTML = mmsi;

    if (pixel) {
        const [mapW, mapH] = map.getSize();
        const { offsetWidth: tw, offsetHeight: th } = tooltip;
        const dist = distance;

        // we position the tooltip top-right of the ship in the direction of the ship's course to minimize overlap with path
        const calculatePosition = (a) => {
            const rad = (a + 45) * Math.PI / 180;
            const s = Math.sin(rad);
            const c = -Math.cos(rad);
            const x = pixel[0] + dist * s + (s < 0 ? -tw : 0);
            const y = pixel[1] + dist * c + (c < 0 ? -th : 0);
            return { x, y };
        };

        let pos = calculatePosition(angle);
        // if it is off the screen, we try to move it to the opposite side
        if (pos.x + tw > mapW - 50 || pos.x < 0 || pos.y + th > mapH - 10 || pos.y < 0) {
            pos = calculatePosition(angle + 180);
        }

        // avoid that it is on top of controls or off screen
        pos.x = Math.min(Math.max(pos.x, 10), mapW - tw - 50);
        pos.y = Math.min(Math.max(pos.y, 10), mapH - th - 10);

        Object.assign(tooltip.style, {
            left: `${pos.x}px`,
            top: `${pos.y}px`,
            visibility: 'visible'
        });
    }
};


const stopHover = function () {

    if (!hoverMMSI) return;

    const hover_mmsi = hoverMMSI;

    debounceShowHoverTrack.cancel();

    hover_info.style.visibility = 'hidden';
    hover_info.style.left = '0px';
    hover_info.style.top = '0px';

    if (hover_enabled_track) hideTrack(hoverMMSI);

    const dc = hover_feature && ('distancecircle' in hover_feature || 'rangering' in hover_feature);
    const sf = hoverMMSI in shapeFeatures;

    hoverMMSI = undefined;
    hoverType = undefined;
    hover_feature = undefined;
    hover_enabled_track = false;

    if (dc) rangeLayer.changed();

    if (hoverType == 'ship' && sf)
        shapeLayer.changed();

    updateHoverMarker();
    trackLayer.changed();
}

function showHoverTrack(mmsi) {
    if (mmsi) {
        hover_enabled_track = !trackIsShown(hoverMMSI);
        if (hover_enabled_track) {
            showTrack(mmsi);
        }
    }
}

function toggleAttribution() {
    const attribution = document.getElementById('map_attributions');
    const currentDisplay = attribution.style.display;
    attribution.style.display = currentDisplay === 'none' ? 'block' : 'none';
}

function getTooltipContentBinary(mmsiOrBinary) {
    if (typeof mmsiOrBinary === 'number' || typeof mmsiOrBinary === 'string') {
        const mmsi = Number(mmsiOrBinary);
        if (mmsi in binaryDB && binaryDB[mmsi].length > 0) {
            const meteoMessages = binaryDB[mmsi].filter(msg =>
                msg.message &&
                msg.message.dac == 1 &&
                (msg.message.fid == 31 || msg.message.fi == 31)
            );

            if (meteoMessages.length > 0) {
                meteoMessages.sort((a, b) => b.timestamp - a.timestamp);

                let content = '';

                meteoMessages.forEach((meteoMsg, index) => {
                    if (index > 0) {
                        content += '<hr style="margin: 12px 0; border: 0; border-top: 1px dashed rgba(255,255,255,0.8);">';
                    }

                    // Add the individual message content
                    content += getBinaryMessageContent(meteoMsg);
                });

                return content;
            }
        }
        return ''; // No meteorological messages found
    }

    return getBinaryMessageContent(mmsiOrBinary);
}


const startHover = function (type, mmsi, pixel, feature) {

    if (type != 'ship' && type != 'tooltip' && type != 'plane' && type != 'binary') return;

    if (mmsi !== hoverMMSI || hoverType !== type) {
        stopHover();

        hoverMMSI = mmsi;
        hoverType = type;
        hover_feature = feature;

        if (type == 'ship' && (mmsi in shipsDB && shipsDB[mmsi].raw.lon && shipsDB[mmsi].raw.lat)) {
            const tooltip = shipsDB[mmsi]?.raw ? getTooltipContent(shipsDB[mmsi].raw) : mmsi;
            showTooltipShip(hover_info, tooltip, pixel, 15, shipsDB[mmsi].raw.cog);
            if (settings.show_track_on_hover && pixel) {
                debounceShowHoverTrack(mmsi);
            }
            if (mmsi in shapeFeatures) {
                shapeFeatures[mmsi].changed();
            }
            trackLayer.changed();
        } else if (type == 'plane' && (mmsi in planesDB && planesDB[mmsi].raw.lon && planesDB[mmsi].raw.lat)) {
            showTooltipShip(hover_info, planesDB[mmsi]?.raw ? getTooltipContentPlane(planesDB[mmsi].raw) : mmsi, pixel, 15, planesDB[mmsi].raw.heading);
        }
        else {
            showTooltipShip(hover_info, hoverMMSI, pixel, 0);

            if (hover_feature && ('distancecircle' in hover_feature || 'rangering' in hover_feature))
                rangeLayer.changed();
        }

        updateHoverMarker();
    }
}

function updateHoverMarker() {

    if (hoverCircleFeature) {
        if (hoverType == 'ship' && hoverMMSI in shipsDB && shipsDB[hoverMMSI].raw.lon && shipsDB[hoverMMSI].raw.lat) {
            const center = ol.proj.fromLonLat([shipsDB[hoverMMSI].raw.lon, shipsDB[hoverMMSI].raw.lat]);
            hoverCircleFeature.setGeometry(new ol.geom.Point(center));
            return;
        }
        else if (hoverType == 'plane' && hoverMMSI in planesDB && planesDB[hoverMMSI].raw.lon && planesDB[hoverMMSI].raw.lat) {
            const center = ol.proj.fromLonLat([planesDB[hoverMMSI].raw.lon, planesDB[hoverMMSI].raw.lat]);
            hoverCircleFeature.setGeometry(new ol.geom.Point(center));
            return;
        }
        else {
            extraVector.removeFeature(hoverCircleFeature);
            hoverCircleFeature = undefined;
            hoverMMSI = undefined;
            hoverType = undefined;
            return;
        }
    }

    if (hoverType == 'ship' && hoverMMSI in shipsDB && shipsDB[hoverMMSI].raw.lon && shipsDB[hoverMMSI].raw.lat) {

        const center = ol.proj.fromLonLat([shipsDB[hoverMMSI].raw.lon, shipsDB[hoverMMSI].raw.lat]);
        hoverCircleFeature = new ol.Feature(new ol.geom.Point(center));
        hoverCircleFeature.setStyle(hoverCircleStyleFunction);
        hoverCircleFeature.mmsi = hoverMMSI;
        extraVector.addFeature(hoverCircleFeature);
    }
    else if (hoverType == 'plane' && hoverMMSI in planesDB && planesDB[hoverMMSI].raw.lon && planesDB[hoverMMSI].raw.lat) {
        const center = ol.proj.fromLonLat([planesDB[hoverMMSI].raw.lon, planesDB[hoverMMSI].raw.lat]);
        hoverCircleFeature = new ol.Feature(new ol.geom.Point(center));
        hoverCircleFeature.setStyle(hoverCircleStyleFunction);
        hoverCircleFeature.mmsi = hoverMMSI;
        extraVector.addFeature(hoverCircleFeature);
    }
}

const normalizePixel = (coord) => {
    const view = map.getView(),
        projection = view.getProjection(),
        centerX = view.getCenter()[0],
        worldWidth = ol.extent.getWidth(projection.getExtent());

    coord[0] -= Math.floor((coord[0] - centerX) / worldWidth + 0.5) * worldWidth;

    const [x, y] = map.getPixelFromCoordinate(coord),
        [width, height] = map.getSize();

    return [
        Math.max(0, Math.min(width - 1, x)),
        Math.max(0, Math.min(height - 1, y))
    ];
};

function getFeature(pixel, target) {
    const feature = target.closest('.ol-control') ? undefined : map.forEachFeatureAtPixel(pixel,
        function (feature) { if ('ship' in feature || 'plane' in feature || 'tooltip' in feature || 'binary' in feature) { return feature; } }, { hitTolerance: 10 });

    if (feature) return feature;
    return undefined;
}

const handlePointerMove = function (pixel, target) {
    const feature = getFeature(pixel, target);

    if (feature) {
        const geometry = feature.getGeometry();
        const geometryType = geometry.getType();
        if (geometryType === 'Point') {
            const coordinate = geometry.getCoordinates();
            pixel = normalizePixel(coordinate);
        }
    }

    if (feature && 'ship' in feature && feature.ship.mmsi in shipsDB) {
        const mmsi = feature.ship.mmsi;
        startHover('ship', mmsi, pixel, feature);
    }
    else if (feature && 'plane' in feature && feature.plane.hexident in planesDB) {
        const hexident = feature.plane.hexident;
        startHover('plane', hexident, pixel, feature);
    }
    else if (feature && feature.binary === true) {
        // Handle hover for binary features (both ship-associated and standalone)
        if (feature.is_associated && feature.binary_mmsi && feature.binary_mmsi in shipsDB) {
            // For ship-associated binary features, redirect to ship hover
            startHover('ship', feature.binary_mmsi, pixel, feature);
            return;
        } else if (feature.binary_messages && feature.binary_messages.length > 0) {
            // For standalone binary clusters
            let tooltipContent = `<div class="tooltip-card">`;

            // Find MMSI counts to show in tooltip
            if (feature.binary_mmsi_counts) {
                const mmsiEntries = Object.entries(feature.binary_mmsi_counts);
                if (mmsiEntries.length > 0) {
                    tooltipContent += '<div style="margin-top: 5px; font-size: 0.9em;">From: ';
                    mmsiEntries.slice(0, 3).forEach(([mmsi, count], index) => {
                        const shipName = (mmsi in shipsDB && shipsDB[mmsi].raw.shipname) ?
                            shipsDB[mmsi].raw.shipname : `MMSI ${mmsi}`;
                        tooltipContent += `${index > 0 ? ', ' : ''}${shipName} (${count})`;
                    });
                    if (mmsiEntries.length > 3) {
                        tooltipContent += ` and ${mmsiEntries.length - 3} more`;
                    }
                    tooltipContent += '</div>';
                }
            }
            tooltipContent += '</div>';

            // Add meteo data if available
            const meteoMessages = feature.binary_messages.filter(msg =>
                msg.message && msg.message.dac == 1 &&
                (msg.message.fid == 31 || msg.message.fi == 31)
            );

            if (meteoMessages.length > 0) {
                tooltipContent += getBinaryMessageContent(meteoMessages, true);
            }

            startHover('tooltip', tooltipContent, pixel, feature);
        } else {
            startHover('tooltip', "Binary Message", pixel, feature);
        }
    }
    else if (feature && 'tooltip' in feature) {
        startHover('tooltip', feature.tooltip, pixel, feature);
    } else if (hoverMMSI || hoverType) {
        stopHover();
    }

    if (isMeasuring) {
        const lastMeasureIndex = measures.length - 1;

        if (feature && 'ship' in feature) {
            measures[lastMeasureIndex] = {
                ...measures[lastMeasureIndex],
                end_value: feature.ship.mmsi,
                end_type: "ship"
            };
        }
        else {
            measures[lastMeasureIndex] = {
                ...measures[lastMeasureIndex],
                end_value: ol.proj.toLonLat(map.getCoordinateFromPixel(pixel)),
                end_type: "point"
            };
        }

        refreshMeasures();
    }
};

function debounce(func, wait) {
    let timeout;
    function debounced(...args) {
        clearTimeout(timeout);
        timeout = setTimeout(() => func.apply(this, args), wait);
    }
    debounced.cancel = function () {
        clearTimeout(timeout);
    };
    return debounced;
}

const debouncedSaveSettings = debounce(saveSettings, 250);
const debouncedDrawMap = debounce(redrawMap, 250);
const debounceShowHoverTrack = debounce(showHoverTrack, 250);


function updateMapURL() {
    if (isAndroid()) return;

    const view = map.getView();
    const center = ol.proj.toLonLat(view.getCenter()); // Converts the center coordinates to [lon, lat]
    const newURL = window.location.href.split("?")[0] + "?lat=" + center[1].toFixed(4) + "&lon=" + center[0].toFixed(4) + "&zoom=" + view.getZoom().toFixed(2) + "&tab=" + settings.tab;
    history.replaceState(null, null, newURL);
}


function saveSettings() {
    if (map !== undefined) {
        const view = map.getView();
        const center = ol.proj.toLonLat(view.getCenter()); // Convert the center coordinate to longitude and latitude
        settings.lat = center[1]; // Latitude
        settings.lon = center[0]; // Longitude
        settings.zoom = view.getZoom(); // Zoom level
    }


    const scRows = document.querySelectorAll(".shipcard-content-row");

    const selectedRows = [];
    scRows.forEach((row) => {
        const rowClasses = row.getAttribute("class");
        selectedRows.push(rowClasses.includes("shipcard-max-only") ? 0 : 1);
    });

    settings.shipcard_max = isShipcardMax();
    settings.shipcard_rows = selectedRows;
    settings.activeReceiver = activeReceiver;

    // Save realtime filter MMSIs and background streaming state
    const rtState = getRealtimeState();
    if (rtState) {
        settings.realtime_filter_mmsis = rtState.filterMMSIs;
        settings.realtime_background_streaming = rtState.backgroundStreaming;
    }

    persist();
    notifyChange();
}

function updateForLegacySettings() {
    if ('latlon_in_dms' in settings) {

        settings.coordinate_format = settings.latlon_in_dms ? "dms" : "decimal";
        delete settings.latlon_in_dms;
    }

    if (!("showPlanesAtFirst" in settings)) {
        settings.showPlanesAtFirst = true;
        settings.map_overlay.push("Aircraft");
    }
}


function loadSettings() {
    if (!urlParams.has("reset")) {
        try {
            const localStorageSettings = localStorage.getItem(context);
            if (localStorageSettings !== null) {
                const ls = JSON.parse(localStorageSettings);
                Object.assign(settings, ls);
            }
        } catch (error) {
            console.log(error);
            return;
        }
    }
    if (settings.activeReceiver) activeReceiver = settings.activeReceiver;
    if (!shipcardismax()) toggleShipcardSize();

    if (settings.shipcard_rows && settings.shipcard_rows.length > 0) {
        const rows = document.querySelectorAll(".shipcard-content-row");

        rows.forEach((row, index) => {
            if (settings.shipcard_rows[index] == 1) {
                row.setAttribute("class", "mapcard-content-row shipcard-content-row shipcard-row-selected");
            } else {
                row.setAttribute("class", "mapcard-content-row shipcard-content-row shipcard-max-only");
            }
        });
        if (settings.shipcard_max != shipcardismax()) {
            toggleShipcardSize();
        }
    }

    settings.android = false;
}

function convertStringBooleansToActual() {
    const booleanSettings = [
        'counter', 'fading', 'android', 'kiosk', 'welcome', 'show_range',
        'distance_circles', 'table_shiptype_use_icon', 'fix_center',
        'show_circle_outline', 'dark_mode', 'setcoord', 'eri', 'loadURL',
        'show_station', 'labels_declutter', 'show_track_on_hover',
        'show_track_on_select', 'shipcard_max', 'kiosk_pan_map',
        'show_signal_graphs', 'show_ppm_graphs'
    ];

    booleanSettings.forEach(key => {
        if (settings.hasOwnProperty(key)) {
            if (typeof settings[key] === 'string') {
                settings[key] = settings[key] === "true";
            }
        }
    });
}

function loadSettingsFromURL() {
    for (const [key, value] of urlParams.entries()) {
        if (settings.hasOwnProperty(key)) {
            if (key === 'map_overlay') {
                if (!Array.isArray(settings[key])) settings[key] = [];
                settings[key].push(value);
            } else {
                settings[key] = value;
            }
        }
    }

    convertStringBooleansToActual();
}

function mapResetViewZoom(z, m) {
    if (m && m in shipsDB) {
        const ship = shipsDB[m].raw;
        const view = map.getView();
        view.setCenter(ol.proj.fromLonLat([ship.lon, ship.lat]));
        view.setZoom(Math.min(view.getMaxZoom(), Math.max(z, view.getZoom() + 1)));
    }

    shipcardMinIfMaxonMobile();
}

function mapResetView(z) {

    const view = map.getView();
    view.setZoom(Math.min(view.getMaxZoom(), Math.max(z, view.getZoom() + 1)));
    shipcardMinIfMaxonMobile();
}


async function ToggleTrackOnMap(m) {

    if (marker_tracks.has(Number(m))) {
        marker_tracks.delete(Number(m));
        redrawMap();
    } else {
        marker_tracks.add(Number(m));
        await fetchTracks();
        shipcardMinIfMaxonMobile();
        redrawMap();
    }
}

async function toggleTrack(m) {
    if (settings.show_track_on_select && card_mmsi == m && card_type == 'ship') {
        select_enabled_track = !select_enabled_track;
    }
    else {
        ToggleTrackOnMap(m);
    }
    updateShipcardTrackOption(m);

}

async function showTrack(m) {
    if (!marker_tracks.has(Number(m))) {
        ToggleTrackOnMap(m);
    }
    updateShipcardTrackOption(m);

}

async function hideTrack(m) {
    if (marker_tracks.has(Number(m))) {
        ToggleTrackOnMap(m);
    }
    updateShipcardTrackOption(m);

}

function trackIsShown(m) {
    return marker_tracks.has(Number(m));
}

function pinStation() {
    pinVessel("STATION");
}

function pinVessel(m) {
    settings.center_point = m;
    settings.fix_center = true;
    saveSettings();
    drawStation();
}

function unpinCenter() {
    settings.fix_center = false;
    saveSettings();
    drawStation();
}


async function showAllTracks() {
    show_all_tracks = true;
    select_enabled_track = hover_enabled_track = false;
    await fetchTracks();
    redrawMap();
    updateShipcardTrackOption()
}

function deleteAllTracks() {
    show_all_tracks = false;
    marker_tracks = new Set();
    const p = {};

    if (card_type == 'ship' && card_mmsi && settings.show_track_on_select) {
        marker_tracks.add(Number(card_mmsi));
        select_enabled_track = true;

        if (paths[card_mmsi]) {
            p[card_mmsi] = paths[card_mmsi];
        }
    }

    paths = p;

    redrawMap(); updateShipcardTrackOption();
}


async function fetchTracks() {
    if (marker_tracks.size == 0 && show_all_tracks == false) return true;

    try {
        let a;
        if (show_all_tracks) a = await fetch("api/allpath.json?receiver=" + activeReceiver);
        else {

            for (const mmsi of marker_tracks) {
                if (!(mmsi in shipsDB)) {
                    ToggleTrackOnMap(mmsi);
                }
            }

            const mmsi_str = Array.from(marker_tracks).join(",");
            a = await fetch("api/path.json?" + mmsi_str + "&receiver=" + activeReceiver);
        }
        paths = await a.json();
    } catch (error) {
        console.log("Error loading path: " + error);
        paths = {};
        return false;
    }

    for (const mmsi in paths) {
        if (paths.hasOwnProperty(mmsi)) {
            if (mmsi in shipsDB && paths[mmsi].length > 0) {
                shipsDB[mmsi].raw.lat = paths[mmsi][0][0];
                shipsDB[mmsi].raw.lon = paths[mmsi][0][1];
            }
        }
    }
    return true;
}

function drawStation() {
    const hasNoStation = settings.show_station == false || station == null || !station.hasOwnProperty("lat") || !station.hasOwnProperty("lon");
    const hasMMSIcenter = settings.center_point && settings.center_point != "STATION" && settings.center_point in shipsDB;

    if (stationFeature) {
        extraVector.removeFeature(stationFeature);
        stationFeature = undefined;
    }

    if (hasNoStation) {
        return;
    }

    const radius = 10;
    const svgIconStyle = new ol.style.Style({
        image: new ol.style.Icon({
            anchor: [0.5, 0.5],
            scale: 0.3,
            color: 'white', //getComputedStyle(document.documentElement).getPropertyValue('--secondary-color'),
            src: 'data:image/svg+xml,%3Csvg xmlns="http://www.w3.org/2000/svg" height="48" viewBox="0 -960 960 960" width="48"%3E%3Cpath fill="white" d="M198-278q-60-58-89-133T80-560q0-74 29-149t89-133l35 35q-50 49-76.5 116.5T130-560q0 63 26.5 130.5T233-313l-35 35Zm92-92q-40-37-59-89.5T212-560q0-48 19-100.5t59-89.5l35 35q-29 29-46 72.5T262-560q0 35 17.5 79.5T325-405l-35 35Zm4 290 133-405q-17-12-27.5-31T389-560q0-38 26.5-64.5T480-651q38 0 64.5 26.5T571-560q0 25-10.5 44T533-485L666-80h-59l-29-90H383l-30 90h-59Zm108-150h156l-78-238-78 238Zm268-140-35-35q29-29 46-72.5t17-82.5q0-35-17.5-79.5T635-715l35-35q39 37 58.5 89.5T748-560q0 47-19.5 100T670-370Zm92 92-35-35q49-49 76-116.5T830-560q0-63-27-130.5T727-807l35-35q60 58 89 133t29 149q0 75-27.5 149.5T762-278Z"/%3E%3C/svg%3E',
        })
    });

    const CircleStyle = new ol.style.Style({
        image: new ol.style.Circle({
            radius: radius,
            stroke: new ol.style.Stroke({
                color: 'white',
                width: 3
            }),
            fill: new ol.style.Fill({
                color: getComputedStyle(document.documentElement).getPropertyValue('--tertiary-color')
            }),
        })
    });

    stationFeature = new ol.Feature({
        geometry: new ol.geom.Point(ol.proj.fromLonLat([station.lon, station.lat]))
    });

    stationFeature.setStyle([CircleStyle, svgIconStyle]);
    stationFeature.tooltip = "Receiving Station";
    stationFeature.station = true;
    extraVector.addFeature(stationFeature);

    // MMSI is driving center but does not exist
    if (!hasMMSIcenter && settings.center_point != "STATION") {
        settings.center_point = "STATION";
        settings.fix_center = false;
    }

    for (const k in center) delete center[k];
    if (settings.center_point == "STATION") {
        Object.assign(center, station);
    } else if (hasMMSIcenter) {
        const ship = shipsDB[settings.center_point].raw;
        if (ship.lat != null && ship.lon != null) {
            Object.assign(center, { lat: ship.lat, lon: ship.lon });
        }
    }

    if (settings.fix_center && center != null && center.hasOwnProperty("lat") && center.hasOwnProperty("lon")) {

        const view = map.getView();
        view.setCenter(ol.proj.fromLonLat([center.lon, center.lat]));

        settings.lat = center.lat;
        settings.lon = center.lon;
    }
}

function moveMapCenter(px) {
    const view = map.getView();
    const currentCenter = view.getCenter();
    const newCenter = map.getCoordinateFromPixel(px);
    const centerDiff = [newCenter[0] - currentCenter[0], newCenter[1] - currentCenter[1]];

    stopHover();
    view.animate({
        center: [currentCenter[0] + centerDiff[0], currentCenter[1] + centerDiff[1]],
        duration: 1000
    });
}


const shippingMappings = {
    [ShippingClass.OTHER]: { cx: 120, cy: 20, hint: 'Other', imgSize: 20 },
    [ShippingClass.UNKNOWN]: {
        cx: 120,
        cy: 20,
        hint: 'Unknown',
        imgSize: 20
    },
    [ShippingClass.CARGO]: { cx: 0, cy: 20, hint: 'Cargo', imgSize: 20 },
    [ShippingClass.TANKER]: { cx: 80, cy: 20, hint: 'Tanker', imgSize: 20 },
    [ShippingClass.PASSENGER]: {
        cx: 40,
        cy: 20,
        hint: 'Passenger',
        imgSize: 20
    },
    [ShippingClass.HIGHSPEED]: {
        cx: 100,
        cy: 20,
        hint: 'High Speed',
        imgSize: 20
    },
    [ShippingClass.SPECIAL]: {
        cx: 60,
        cy: 20,
        hint: 'Special',
        imgSize: 20
    },
    [ShippingClass.FISHING]: {
        cx: 140,
        cy: 20,
        hint: 'Fishing',
        imgSize: 20
    },
    [ShippingClass.ATON]: {
        cx: 0,
        cy: 40,
        hint: 'AtoN',
        imgSize: 20
    },
    [ShippingClass.PLANE]: { cx: 0, cy: 60, hint: 'Aircraft', imgSize: 25 },
    [ShippingClass.HELICOPTER]: {
        cx: 0,
        cy: 85,
        hint: 'Helicopter',
        imgSize: 25
    },
    [ShippingClass.B]: { cx: 20, cy: 20, hint: 'Class B', imgSize: 20 },
    [ShippingClass.STATION]: {
        cx: 20,
        cy: 40,
        hint: 'Base Station',
        imgSize: 20
    },
    [ShippingClass.SARTEPIRB]: {
        cx: 40,
        cy: 40,
        hint: 'SART/EPIRB',
        imgSize: 20
    }
}

function getSprite(ship) {
    const shipClass = ship.shipclass;
    const sprite = shippingMappings[shipClass] || {
        cx: 120,
        cy: 20,
        imgSize: 20,
        hint: ''
    }

    ship.rot = 0
    ship.cx = sprite.cx
    ship.cy = sprite.cy
    ship.imgSize = sprite.imgSize
    ship.hint = sprite.hint

    if (sprite.cy === 20) {
        if (ship.speed != null && ship.speed > 0.5 && ship.cog != null) {
            ship.cy = 0
            ship.rot = ship.cog * 3.1415926 / 180;
        }
    } else if ((shipClass == ShippingClass.HELICOPTER || shipClass == ShippingClass.PLANE) && ship.cog != null) {
        ship.rot = ship.cog * 3.1415926 / 180;
    }

    return
}
function getPlaneSprite(plane) {
    let sprite = shippingMappings[ShippingClass.PLANE];

    // Handle rotorcraft (TC=4, CA=7)
    if (plane.category && plane.category == 47) {
        sprite = shippingMappings[ShippingClass.HELICOPTER];
    }

    plane.scaling = 0.75; // Default scalin

    // Set scaling based on wake vortex category
    if (plane.category) {

        const ca = plane.category % 10;  // Extract CA value
        switch (ca) {
            case 1: // Light
                plane.scaling = 0.5;
                break;
            case 2: // Medium 1
            case 3: // Medium 2
                plane.scaling = 0.75;
                break;
            case 4: // High vortex
            case 5: // Heavy
                plane.scaling = 1.0;
                break;
            case 6: // High performance
                plane.scaling = 0.8;
                break;
            case 7: // Rotorcraft
                plane.scaling = 0.6;
                break;
        }
    }

    plane.scaling = plane.scaling * 1.2
    plane.rot = plane.heading * 3.1415926 / 180;
    plane.cx = sprite.cx;

    if (plane.airborne == 0)
        plane.cx = 25;
    else
        plane.cx = 50;

    plane.cy = sprite.cy;
    plane.imgSize = sprite.imgSize;
    plane.hint = sprite.hint;

    return sprite;
}

const SpritesAll =
    'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAKAAAABuCAYAAACgLRjpAAAAwnpUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHjabVBbDgMhCPznFD2CPHTxOG7XJr1Bj18UbHbbkjgMj4wA9NfzAbdhhAKSNy21lGQmVSo1I5rc2kRMMvGUmvElD6VHgSzF5tlDLdG/8vgRcNeM5ZOQ3qOwXwtVQl+/hMgdj4kGP0KohhCTFzAEWuxQqm7nFfa1wTL1BwNEr2P/xJtd78j2DxN1Rk6GzOoD8HgZuBlBQwtoMDWOLIZ5tqIf5N+dlsEbcl5ZgI6o50sAAAGEaUNDUElDQyBwcm9maWxlAAB4nH2RPUjDUBSFT1ulIhUFC1pxyFCd7KIijrUKRagQaoVWHUxe+gdNGpIUF0fBteDgz2LVwcVZVwdXQRD8AXEXnBRdpMT7kkKLGC883sd59xzeuw/wNypMNbvigKpZRjqZELK5VSH4Ch8iGMAQIhIz9TlRTMGzvu6pm+ouxrO8+/6sPiVvMsAnEMeZbljEG8Qzm5bOeZ84zEqSQnxOPGHQBYkfuS67/Ma56LCfZ4aNTHqeOEwsFDtY7mBWMlTiaeKoomqU78+6rHDe4qxWaqx1T/7CUF5bWeY6rVEksYgliBAgo4YyKrAQo10jxUSazhMe/hHHL5JLJlcZjBwLqEKF5PjB/+D3bM3C1KSbFEoA3S+2/TEGBHeBZt22v49tu3kCBJ6BK63trzaA2U/S620tegT0bwMX121N3gMud4DhJ10yJEcK0PIXCsD7GX1TDhi8BXrX3Lm1znH6AGRoVqkb4OAQGC9S9rrHu3s65/ZvT2t+P4Ewcqz7/a75AAANemlUWHRYTUw6Y29tLmFkb2JlLnhtcAAAAAAAPD94cGFja2V0IGJlZ2luPSLvu78iIGlkPSJXNU0wTXBDZWhpSHpyZVN6TlRjemtjOWQiPz4KPHg6eG1wbWV0YSB4bWxuczp4PSJhZG9iZTpuczptZXRhLyIgeDp4bXB0az0iWE1QIENvcmUgNC40LjAtRXhpdjIiPgogPHJkZjpSREYgeG1sbnM6cmRmPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5LzAyLzIyLXJkZi1zeW50YXgtbnMjIj4KICA8cmRmOkRlc2NyaXB0aW9uIHJkZjphYm91dD0iIgogICAgeG1sbnM6eG1wTU09Imh0dHA6Ly9ucy5hZG9iZS5jb20veGFwLzEuMC9tbS8iCiAgICB4bWxuczpzdEV2dD0iaHR0cDovL25zLmFkb2JlLmNvbS94YXAvMS4wL3NUeXBlL1Jlc291cmNlRXZlbnQjIgogICAgeG1sbnM6R0lNUD0iaHR0cDovL3d3dy5naW1wLm9yZy94bXAvIgogICAgeG1sbnM6ZGM9Imh0dHA6Ly9wdXJsLm9yZy9kYy9lbGVtZW50cy8xLjEvIgogICAgeG1sbnM6dGlmZj0iaHR0cDovL25zLmFkb2JlLmNvbS90aWZmLzEuMC8iCiAgICB4bWxuczp4bXA9Imh0dHA6Ly9ucy5hZG9iZS5jb20veGFwLzEuMC8iCiAgIHhtcE1NOkRvY3VtZW50SUQ9ImdpbXA6ZG9jaWQ6Z2ltcDpiNGQ1NGU4Ny1jZGQyLTQ0M2YtOWU2Mi1mYjJkYjFmOWNjOTkiCiAgIHhtcE1NOkluc3RhbmNlSUQ9InhtcC5paWQ6YWI4MDQ0MzItMTE2My00NGQ0LWE0MGUtNzUxMzE1YjZlMzZiIgogICB4bXBNTTpPcmlnaW5hbERvY3VtZW50SUQ9InhtcC5kaWQ6YjNhMDI1MzEtNmQ0OS00ZTE5LWE1NGYtOTE2OTE3MGYyMTMzIgogICBHSU1QOkFQST0iMi4wIgogICBHSU1QOlBsYXRmb3JtPSJNYWMgT1MiCiAgIEdJTVA6VGltZVN0YW1wPSIxNzM3ODI1Mjk4OTUwMzkxIgogICBHSU1QOlZlcnNpb249IjIuMTAuMzgiCiAgIGRjOkZvcm1hdD0iaW1hZ2UvcG5nIgogICB0aWZmOk9yaWVudGF0aW9uPSIxIgogICB4bXA6Q3JlYXRvclRvb2w9IkdJTVAgMi4xMCIKICAgeG1wOk1ldGFkYXRhRGF0ZT0iMjAyNTowMToyNVQxODoxNDo1OCswMTowMCIKICAgeG1wOk1vZGlmeURhdGU9IjIwMjU6MDE6MjVUMTg6MTQ6NTgrMDE6MDAiPgogICA8eG1wTU06SGlzdG9yeT4KICAgIDxyZGY6U2VxPgogICAgIDxyZGY6bGkKICAgICAgc3RFdnQ6YWN0aW9uPSJzYXZlZCIKICAgICAgc3RFdnQ6Y2hhbmdlZD0iLyIKICAgICAgc3RFdnQ6aW5zdGFuY2VJRD0ieG1wLmlpZDoxNTViZGM3Ny05OGRiLTRlZDctODU1MC1jYTMyZjgyZTk0ZGUiCiAgICAgIHN0RXZ0OnNvZnR3YXJlQWdlbnQ9IkdpbXAgMi4xMCAoTWFjIE9TKSIKICAgICAgc3RFdnQ6d2hlbj0iMjAyNS0wMS0yNVQxODoxNDo1OCswMTowMCIvPgogICAgPC9yZGY6U2VxPgogICA8L3htcE1NOkhpc3Rvcnk+CiAgPC9yZGY6RGVzY3JpcHRpb24+CiA8L3JkZjpSREY+CjwveDp4bXBtZXRhPgogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgCjw/eHBhY2tldCBlbmQ9InciPz5Kov3CAAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH6QEZEQ46qIXsNAAAIABJREFUeNrtnXtc1HW+/1/f69xnALkoAgoIchGVi2IB4jVXs1TKy8nMbtbeitPpt8efpzrbXn5nz+a27W6P2lUPa7ZZbZpmaVtrZa1QkqgpCoiaCWIwIDAw1+98v9/P+YOJgJlhvjDDOdVvXo/HPHDm85nn6/Oe72s+3+t8BcIK65su00aT1rTRpA8V777Yu7T3xd4VMt74xE3a8YmbQsYrzmG0xTlMyHhlGoO2TGMIGW9+iUo7v0QVMp42o1CrzSgMGW+5Oke7XJ2jiEcrItLMFtDM5lANkAa9hQa9OXRfEXoLQsijgC0UEDIeA2xhQshjWWxh2dDxQLNbQLMhrJfewihcHpSC2U/jXrOoCQC4195LsuyyOIKc/TS3udY2AcDrqr8mVZhfDIo3PnGTxuVc1QQAKvX+pNbmHUHxinMYzRwS2wQAxyhzUmWtFBSvTGPQ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'

async function updateMap() {
    let ok = false;

    ok = await fetchShips(activeReceiver);
    if (!ok) return;

    ok = await fetchTracks();
    if (!ok) return;

    if (planeLayer.isVisible()) {
        ok = await fetchPlanes();
        if (!ok) return;
    }

    if (binaryLayer.isVisible()) {
        ok = await fetchBinary(activeReceiver);
        if (!ok) return;
    }

    if (settings.setcoord == "true" || settings.setcoord == true) {
        if (station != null && station.hasOwnProperty("lat") && station.hasOwnProperty("lon")) {
            settings.setcoord = false;
            const view = map.getView();
            view.setCenter(ol.proj.fromLonLat([station.lon, station.lat]));
            saveSettings();
        }
    }

    if (shipcardVisible()) {
        if (card_type == "ship")
            populateShipcard();
        else if (card_type == "plane")
            populatePlanecard();
    }

    updateMarkerCount();
    await fetchRange();

    redrawMap();
}

function redrawBinaryMessages() {
    binaryVector.clear();

    const gridCells = {}; // For clustering standalone messages
    const gridSize = 0.01; // Grid size in degrees (approx. 1km)

    // Process messages for vessels
    for (const [mmsi, msgData] of Object.entries(binaryDB)) {
        if (!msgData.ship_messages || msgData.ship_messages.length === 0) continue;

        // Cache all messages
        const allMessages = msgData.ship_messages;

        // Check if the ship exists in shipsDB
        const shipExists = mmsi in shipsDB;
        const hasValidShipCoordinates = shipExists &&
            shipsDB[mmsi].raw.lat !== null &&
            shipsDB[mmsi].raw.lon !== null &&
            shipsDB[mmsi].raw.lat !== 0 &&
            shipsDB[mmsi].raw.lon !== 0 &&
            shipsDB[mmsi].raw.lat < 90 &&
            shipsDB[mmsi].raw.lon < 180;

        if (hasValidShipCoordinates) {
            // Prepare arrays for messages to show at ship or at their own locations
            const shipMessages = [];
            const standaloneMessages = [];

            const ship = shipsDB[mmsi].raw;
            const shipLat = ship.lat;
            const shipLon = ship.lon;

            // Sort messages by proximity to the ship
            allMessages.forEach(msg => {
                if (!msg.message_lat || !msg.message_lon) return;

                const msgLat = msg.message_lat;
                const msgLon = msg.message_lon;

                // Skip invalid coordinates
                if (msgLat === 0 || msgLon === 0 || msgLat > 90 || msgLon > 180) return;

                // Simple distance calculation
                const distanceThreshold = 0.05; // Approx 5km
                const distance = Math.sqrt(
                    Math.pow(msgLat - shipLat, 2) +
                    Math.pow(msgLon - shipLon, 2)
                );

                if (distance <= distanceThreshold) {
                    // Message is close to ship, show at ship location
                    shipMessages.push(msg);
                } else {
                    // Message is far from ship, show at its own location
                    standaloneMessages.push(msg);
                    addMessageToGridCell(msg, mmsi, gridCells, gridSize);
                }
            });

            // Create badge at ship location if there are messages to show there
            if (shipMessages.length > 0) {
                const point = new ol.geom.Point(ol.proj.fromLonLat([shipLon, shipLat]));
                const feature = new ol.Feature({
                    geometry: point
                });

                feature.binary = true;
                feature.ship = ship;
                feature.binary_mmsi = mmsi;
                feature.binary_count = shipMessages.length;
                feature.binary_messages = shipMessages;
                feature.is_associated = true;
                feature.setId(`binary-ship-${mmsi}`);

                binaryVector.addFeature(feature);
            }

        } else {
            // No associated ship with valid coordinates - add all messages to grid cells for clustering
            allMessages.forEach(msg => {
                if (!msg.message_lat || !msg.message_lon) return;
                addMessageToGridCell(msg, mmsi, gridCells, gridSize);
            });
        }
    }

    // Create features for grid cells (clustered standalone messages)
    createGridCellFeatures(gridCells);
}

// Helper function to add a message to the appropriate grid cell
function addMessageToGridCell(msg, mmsi, gridCells, gridSize) {
    const msgLat = msg.message_lat;
    const msgLon = msg.message_lon;

    // Skip invalid coordinates
    if (msgLat === 0 || msgLon === 0 || msgLat > 90 || msgLon > 180) return;

    // Create grid cell key for clustering
    const gridX = Math.floor(msgLon / gridSize);
    const gridY = Math.floor(msgLat / gridSize);
    const gridKey = `${gridX},${gridY}`;

    if (!gridCells[gridKey]) {
        gridCells[gridKey] = {
            messages: [],
            totalLat: 0,
            totalLon: 0,
            mmsiCounts: {}
        };
    }

    // Add to grid cell for clustering
    gridCells[gridKey].messages.push(msg);
    gridCells[gridKey].totalLat += msgLat;
    gridCells[gridKey].totalLon += msgLon;
    gridCells[gridKey].mmsiCounts[mmsi] =
        (gridCells[gridKey].mmsiCounts[mmsi] || 0) + 1;
}

// Helper function to create features for grid cells
function createGridCellFeatures(gridCells) {
    for (const [gridKey, gridData] of Object.entries(gridCells)) {
        if (gridData.messages.length === 0) continue;

        // Calculate average position for this grid cell
        const avgLat = gridData.totalLat / gridData.messages.length;
        const avgLon = gridData.totalLon / gridData.messages.length;

        const point = new ol.geom.Point(ol.proj.fromLonLat([avgLon, avgLat]));
        const feature = new ol.Feature({
            geometry: point
        });

        feature.binary = true;
        feature.binary_count = gridData.messages.length;
        feature.binary_messages = gridData.messages;
        feature.binary_mmsi_counts = gridData.mmsiCounts;
        feature.is_associated = false;
        feature.tooltip = `${gridData.messages.length} binary messages`;
        feature.setId(`binary-standalone-${gridKey}`);

        binaryVector.addFeature(feature);
    }
}

function redrawMap() {
    shapeFeatures = {};
    markerFeatures = {};

    markerVector.clear();
    binaryVector.clear();
    planeVector.clear();
    shapeVector.clear();
    labelVector.clear();
    trackVector.clear();

    labelLayer.declutter_ = settings.labels_declutter;

    const includeLabels = (settings.show_labels === "dynamic" && (map.getView().getZoom() > 11.5)) || settings.show_labels === "always";

    for (const [mmsi, entry] of Object.entries(shipsDB)) {
        const ship = entry.raw;
        if (ship.lat != null && ship.lon != null && ship.lat != 0 && ship.lon != 0 && ship.lat < 90 && ship.lon < 180) {
            getSprite(ship)

            const lon = ship.lon
            const lat = ship.lat

            const point = new ol.geom.Point(ol.proj.fromLonLat([lon, lat]))
            const feature = new ol.Feature({
                geometry: point
            })

            feature.ship = ship;

            markerFeatures[ship.mmsi] = feature
            markerVector.addFeature(feature)

            if (includeLabels)
                labelVector.addFeature(feature)

            if (map.getView().getZoom() > 11.5 && (ship.heading != null || settings.show_circle_outline)) {
                const shapeFeature = new ol.Feature({
                    geometry: createShipOutlineGeometry(ship)
                })
                shapeFeature.ship = ship
                shapeFeatures[ship.mmsi] = shapeFeature

                shapeVector.addFeature(shapeFeature)
            }
        }
        refreshMeasures();
    }

    redrawBinaryMessages();

    if (planeLayer.isVisible()) {

        for (const [hexident, entry] of Object.entries(planesDB)) {
            const plane = entry.raw;
            if (plane.lat != null && plane.lon != null && plane.lat != 0 && plane.lon != 0 && plane.lat < 90 && plane.lon < 180) {
                getPlaneSprite(plane)

                const lon = plane.lon
                const lat = plane.lat

                const point = new ol.geom.Point(ol.proj.fromLonLat([lon, lat]))
                const feature = new ol.Feature({
                    geometry: point
                })

                feature.plane = plane;

                markerFeatures[plane.hexident] = feature
                planeVector.addFeature(feature)

                if (includeLabels)
                    labelVector.addFeature(feature)
            }
        }
    }

    for (const [mmsi, entry] of Object.entries(paths)) {

        if (marker_tracks.has(Number(mmsi)) || show_all_tracks) {
            const path = paths[mmsi];
            const ship = shipsDB[mmsi]?.raw;
            const shipclass = ship?.shipclass;

            // Path: [lat, lon, start_time, end_time]
            if (path.length > 0 && path[0].length >= 4) {
                let currentSegment = [];
                let currentDashed = false;

                for (let i = 0; i < Math.min(path.length, 250); i++) {
                    const point = path[i];
                    const coord = ol.proj.fromLonLat([point[1], point[0]]);

                    let isDashed = false;
                    if (i > 0) {
                        const timeBetweenPoints = path[i - 1][2] - point[3]; // newer start_time - older end_time
                        isDashed = timeBetweenPoints > settings.track_trash_threshold;
                    }

                    if (currentSegment.length === 0) {
                        // First point
                        currentSegment.push(coord);
                        currentDashed = false; // First segment is always solid
                    } else if (currentDashed === isDashed) {
                        // Continue current segment
                        currentSegment.push(coord);
                    } else {
                        // Create feature for current segment
                        if (currentSegment.length > 1) {
                            const lineString = new ol.geom.LineString(currentSegment);
                            const feature = new ol.Feature(lineString);
                            feature.mmsi = mmsi;
                            feature.isDashed = currentDashed;
                            feature.shipclass = shipclass;
                            trackVector.addFeature(feature);
                        }
                        // Start new segment
                        currentSegment = [currentSegment[currentSegment.length - 1], coord];
                        currentDashed = isDashed;
                    }
                }

                // Add final segment
                if (currentSegment.length > 1) {
                    const lineString = new ol.geom.LineString(currentSegment);
                    const feature = new ol.Feature(lineString);
                    feature.mmsi = mmsi;
                    feature.isDashed = currentDashed;
                    feature.shipclass = shipclass;
                    trackVector.addFeature(feature);
                }
            }
        }
    }

    drawRange();
    updateFocusMarker();
    updateHoverMarker();

    updateMarkerCount();
    updateTablecard();

    drawStation(station);
    updateDistanceCircles();

}

document.getElementById('zoom-in').addEventListener('click', function () {
    const view = map.getView();
    const zoom = view.getZoom();
    view.setZoom(zoom + 1);
});

document.getElementById('zoom-out').addEventListener('click', function () {
    const view = map.getView();
    const zoom = view.getZoom();
    view.setZoom(zoom - 1);
});


function refresh_data() {
    if (!document.hidden && !updateInProgress) {
        updateInProgress = true;

        return (async () => {
            try {
                if (settings.tab === "map") {
                    await updateMap();
                } else if (settings.tab === "stat") {
                    await updateStatistics();
                } else if (settings.tab === "plots") {
                    await updatePlots();
                } else if (settings.tab === "ships") {
                    await updateShipTable();
                }
            } catch (error) {
                console.error("Error updating data:", error);
            } finally {
                updateInProgress = false;
            }
        })();
    }
    return Promise.resolve();
}

async function openFocus(m, z) {
    await fetchShips(activeReceiver, false);

    selectMapTab(m);

    const ship = shipsDB[m].raw;
    if (ship && ship.lon && ship.lat) {
        const shipCoords = ol.proj.fromLonLat([ship.lon, ship.lat]);
        const view = map.getView();
        view.setCenter(shipCoords);
    }

    if (z) mapResetView(z);
    else mapResetView(14);

}

// RealtimeViewer — moved to tabs/realtime.js
// LogViewer — moved to tabs/log.js

function activateTab(b, a) {
    // Block decoder tab if decoder is disabled
    if (a === "decoder" && (typeof decoder_enabled === "undefined" || !decoder_enabled)) {
        return;
    }

    hideMenuifSmall();

    Array.from(document.getElementById("menubar").children).forEach((e) => (e.className = e.className.replace(" active", "")));
    Array.from(document.getElementById("menubar_mini").children).forEach((e) => (e.className = e.className.replace(" active", "")));

    const tabcontent = document.getElementsByClassName("tabcontent");

    for (let i = 0; i < tabcontent.length; i++) tabcontent[i].style.display = "none";

    document.getElementById(a).style.display = "block";
    if (a === "map") document.getElementById("tableside").style.display = "flex";

    const tabElement = document.getElementById(a + "_tab");
    if (tabElement) tabElement.className += " active";

    const tabMiniElement = document.getElementById(a + "_tab_mini");
    if (tabMiniElement) tabMiniElement.className += " active";

    settings.tab = a;
    saveSettings();

    clearInterval(interval);
    //refresh_data();
    //interval = setInterval(refresh_data, refreshIntervalMs);

    refresh_data().then(() => {
        interval = setInterval(refresh_data, refreshIntervalMs);
    });

    if (a != "map") StopFireworks();
    if (a == "settings") updateSettingsTab();

    if (a === 'log') onLogActivate();
    else onLogDeactivate();

    if (a == "realtime") onRealtimeActivate();
    else onRealtimeDeactivate();

    if (a === "about") setupAbout();
}

function selectMapTab(m) {
    document.getElementById("map_tab").click();
    if (m in shipsDB) showShipcard('ship', m);
}

function selectTab() {
    if (settings.tab == "settings") settings.tab = "stat";

    // Check if requested tab is disabled and redirect to map
    if (settings.tab == "realtime" && !realtime_enabled) {
        settings.tab = "map";
    }
    if (settings.tab == "log" && (typeof log_enabled === "undefined" || log_enabled === false)) {
        settings.tab = "map";
    }
    if (settings.tab == "decoder" && (typeof decoder_enabled === "undefined" || decoder_enabled === false)) {
        settings.tab = "map";
    }

    if (settings.tab != "realtime" && settings.tab != "about" && settings.tab != "map" && settings.tab != "plots" && settings.tab != "ships" && settings.tab != "stat" && settings.tab != "log" && settings.tab != "decoder") {
        settings.tab = "stat";
        alert("Invalid tab specified");
    }
    activateTab(null, settings.tab);
    //document.getElementById(settings.tab + "_tab").click();
}

const originalDisplayValues = new Map();

function clearAndHide(element) {
    if (!originalDisplayValues.has(element)) {
        // Get computed style to handle cases where display isn't explicitly set
        const computedStyle = window.getComputedStyle(element);
        originalDisplayValues.set(element, computedStyle.display);
    }
    element.style.display = "none";
}

function restoreOriginalDisplay(element) {
    if (originalDisplayValues.has(element)) {
        element.style.display = originalDisplayValues.get(element);
    } else {
        // Fallback: remove the style property to use CSS default
        element.style.removeProperty('display');
    }
}
function updateKiosk() {
    if (isKiosk()) {
        startKioskAnimation();

        const nokioskElements = document.querySelectorAll(".nokiosk");
        const kioskElements = document.querySelectorAll(".kiosk");

        for (let i = 0; i < nokioskElements.length; i++) {
            clearAndHide(nokioskElements[i]);
        }

        for (let i = 0; i < kioskElements.length; i++) {
            restoreOriginalDisplay(kioskElements[i]);
        }
    } else {
        stopKioskAnimation();

        const kioskElements = document.querySelectorAll(".kiosk");
        const nokioskElements = document.querySelectorAll(".nokiosk");

        for (let i = 0; i < kioskElements.length; i++) {
            clearAndHide(kioskElements[i]);
        }

        for (let i = 0; i < nokioskElements.length; i++) {
            restoreOriginalDisplay(nokioskElements[i]);
        }
    }
}

let kioskAnimationInterval = null;

function selectRandomShipForKiosk() {

    const mapExtent = map.getView().calculateExtent(map.getSize());
    const visibleShips = Object.keys(shipsDB).filter(mmsi => {
        const ship = shipsDB[mmsi].raw;
        if (!ship.lat || !ship.lon || ship.lat === 0 || ship.lon === 0) {
            return false;
        }

        const shipCoords = ol.proj.fromLonLat([ship.lon, ship.lat]);
        return ol.extent.containsCoordinate(mapExtent, shipCoords);
    });

    if (visibleShips.length === 0) {
        return null;
    }

    const candidates = visibleShips.filter(mmsi =>
        mmsi != card_mmsi && mmsi != hoverMMSI
    );

    const finalCandidates = candidates.length > 0 ? candidates : visibleShips;

    const weights = finalCandidates.map(mmsi => {
        const ship = shipsDB[mmsi].raw;
        const timeSinceUpdate = ship.last_signal || 3600;

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
    if (!mmsi || !(mmsi in shipsDB)) {
        console.log("Invalid MMSI or ship not found:", mmsi);
        return;
    }

    const ship = shipsDB[mmsi].raw;
    if (!ship.lat || !ship.lon) {
        console.log("Ship has no valid coordinates:", mmsi);
        return;
    }

    const shipCoords = ol.proj.fromLonLat([ship.lon, ship.lat]);
    const pixel = map.getPixelFromCoordinate(shipCoords);
    //    startHover('ship', mmsi, pixel);
    showShipcard('ship', mmsi, pixel);

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

function toggleKioskMode() {
    settings.kiosk = !settings.kiosk;
    updateKiosk();
}

// for overwrite and insert code where needed
function main() {
    plugins_main.forEach(function (p) {
        p();
    });
}

function setupAbout() {

    if (aboutContentLoaded) {
        return;
    }

    fetchAbout()
        .then((s) => {
            document.getElementById("about_content").innerHTML = marked.parse(s);
            aboutContentLoaded = true;
        })
        .catch((error) => {
            alert("Error loading about.md: " + error);
        });
}

addTileLayer("OpenStreetMap", new ol.layer.Tile({
    source: new ol.source.OSM({ maxZoom: 19 })
}));

addTileLayer("Positron", new ol.layer.Tile({
    source: new ol.source.XYZ({
        url: 'https://{a-d}.basemaps.cartocdn.com/light_all/{z}/{x}/{y}{r}.png',
        attributions: '&copy; <a href="https://www.openstreetmap.org/copyright">OSM</a> &copy; <a href="https://carto.com/attributions">CARTO</a>',
        maxZoom: 20
    })
}));

addTileLayer("Positron (no labels)", new ol.layer.Tile({
    source: new ol.source.XYZ({
        url: 'https://{a-d}.basemaps.cartocdn.com/light_nolabels/{z}/{x}/{y}{r}.png',
        attributions: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors &copy; <a href="https://carto.com/attributions">CARTO</a>',
        maxZoom: 20
    })
}));

addTileLayer("Dark Matter", new ol.layer.Tile({
    source: new ol.source.XYZ({
        url: 'https://{a-d}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png',
        attributions: '&copy; <a href="https://www.openstreetmap.org/copyright">OSM</a> &copy; <a href="https://carto.com/attributions">CARTO</a>',
        maxZoom: 20
    })
}));

addTileLayer("Dark Matter (no labels)", new ol.layer.Tile({
    source: new ol.source.XYZ({
        url: 'https://{a-d}.basemaps.cartocdn.com/dark_nolabels/{z}/{x}/{y}{r}.png',
        attributions: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors &copy; <a href="https://carto.com/attributions">CARTO</a>',
        maxZoom: 20
    })
}));

addTileLayer("Voyager", new ol.layer.Tile({
    source: new ol.source.XYZ({
        url: 'https://{a-d}.basemaps.cartocdn.com/rastertiles/voyager/{z}/{x}/{y}{r}.png',
        attributions: '&copy; <a href="https://www.openstreetmap.org/copyright">OSM</a> &copy; <a href="https://carto.com/attributions">CARTO</a>',
        maxZoom: 20
    })
}));

addTileLayer("Voyager (no labels)", new ol.layer.Tile({
    source: new ol.source.XYZ({
        url: 'https://{a-d}.basemaps.cartocdn.com/rastertiles/voyager_nolabels/{z}/{x}/{y}{r}.png',
        attributions: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors &copy; <a href="https://carto.com/attributions">CARTO</a>',
        maxZoom: 20
    })
}));

addTileLayer("Satellite", new ol.layer.Tile({
    source: new ol.source.XYZ({
        url: 'https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}',
        attributions: 'Esri et al.'
        // maxZoom is not specified, so it defaults to the OpenLayers default
    })
}));


addOverlayLayer("OpenSeaMap", new ol.layer.Tile({
    source: new ol.source.XYZ({
        url: 'http://tiles.openseamap.org/seamark/{z}/{x}/{y}.png',
        attributions: 'Map data: &copy; <a href="http://www.openseamap.org">OpenSeaMap</a>'
    })
}));

addOverlayLayer("NOAA", new ol.layer.Tile({
    source: new ol.source.TileWMS({
        url: 'https://gis.charttools.noaa.gov/arcgis/rest/services/MCS/ENCOnline/MapServer/exts/MaritimeChartService/WMSServer?',
        params: {
            'LAYERS': '1,2,3,4,5,6,7',
            'FORMAT': 'image/png',
            'TRANSPARENT': 'true',
            'VERSION': '1.3.0'
        },
        serverType: 'geoserver'
    })
}));

const rainviewerRadar = new ol.layer.Tile({
    name: 'rainviewer_radar',
    title: 'RainViewer Radar',
    type: 'overlay',
    opacity: 0.7,
});

const rainviewerClouds = new ol.layer.Tile({
    name: 'rainviewer_clouds',
    title: 'RainViewer Clouds',
    type: 'overlay',
});

async function refreshRainviewerLayers() {
    try {
        // Get latest timestamps from RainViewer API
        const response = await fetch("https://api.rainviewer.com/public/weather-maps.json");
        const data = await response.json();

        // Update radar layer
        const latestRadar = data.radar.past[data.radar.past.length - 1];
        rainviewerRadar.setSource(new ol.source.XYZ({
            url: `https://tilecache.rainviewer.com/v2/radar/${latestRadar.time}/512/{z}/{x}/{y}/6/1_1.png`,
            attributions: '<a href="https://www.rainviewer.com/api.html" target="_blank">RainViewer.com</a>',
            maxZoom: 7,
        }));

        // Update clouds layer
        const latestClouds = data.satellite?.infrared?.[data.satellite.infrared.length - 1];
        if (latestClouds) {
            rainviewerClouds.setSource(new ol.source.XYZ({
                url: `https://tilecache.rainviewer.com/${latestClouds.path}/512/{z}/{x}/{y}/0/0_0.png`,
                attributions: '<a href="https://www.rainviewer.com/api.html" target="_blank">RainViewer.com</a>',
                maxZoom: 7,
            }));
        }
        return true;

    } catch (error) {
        console.error("Error refreshing RainViewer layers:", error);
        return false;
    }
}

addOverlayLayer("RainViewer Radar", rainviewerRadar);
addOverlayLayer("RainViewer Clouds", rainviewerClouds);

rainviewerRadar.on('change:visible', function (evt) {
    if (evt.target.getVisible()) {
        refreshRainviewerLayers();
        window.setInterval(refreshRainviewerLayers, 2 * 60 * 1000);
    }
});

rainviewerClouds.on('change:visible', function (evt) {
    if (evt.target.getVisible()) {
        refreshRainviewerLayers();
        window.setInterval(refreshRainviewerLayers, 2 * 60 * 1000);
    }
});

function makeDraggable(dragHandle, dragTarget) {
    const moveThreshold = 15;
    let isDragging = false;
    let startX, startY, offsetX, offsetY;

    // add class cursor-move to dragHandle
    dragHandle.classList.add('cursor-move');

    dragHandle.addEventListener('pointerdown', (e) => {
        e.preventDefault();

        startX = e.clientX;
        startY = e.clientY;
        offsetX = startX - dragTarget.offsetLeft;
        offsetY = startY - dragTarget.offsetTop;

        const onPointerMove = (e) => {
            const dx = e.clientX - startX;
            const dy = e.clientY - startY;

            if (!isDragging && (dx * dx + dy * dy) > moveThreshold * moveThreshold) {
                isDragging = true;
                dragTarget.classList.add('dragging');
            }

            if (isDragging) {
                const newX = e.clientX - offsetX;
                const newY = e.clientY - offsetY;

                dragTarget.style.left = `${newX}px`;
                dragTarget.style.top = `${newY}px`;

                // Update pinned position if shipcard is pinned
                if (settings.shipcard_pinned && dragTarget.id === 'shipcard') {
                    settings.shipcard_pinned_x = newX;
                    settings.shipcard_pinned_y = newY;
                    saveSettings();
                }
            }
        };

        const onPointerUp = (e) => {
            if (isDragging) {
                isDragging = false;
                dragTarget.classList.remove('dragging');
            }

            dragHandle.releasePointerCapture(e.pointerId);
            dragHandle.removeEventListener('pointermove', onPointerMove);
            dragHandle.removeEventListener('pointerup', onPointerUp);
        };

        dragHandle.setPointerCapture(e.pointerId);
        dragHandle.addEventListener('pointermove', onPointerMove);
        dragHandle.addEventListener('pointerup', onPointerUp);
    });
}

if (!window.matchMedia('(max-width: 500px), (max-height: 800px)').matches) {
    document.querySelectorAll('aside').forEach((aside) => {
        const dragHandle = aside.querySelector('.draggable');
        if (dragHandle) {
            makeDraggable(dragHandle, aside);
        }
    });
}
else {
    // hide all spans with class "draggable-hide-if-not-active"
    document.querySelectorAll('.draggable-hide-if-not-active').forEach((span) => {
        span.style.display = 'none';
    });
}

addOverlayLayer("Aircraft", planeLayer);


const mdabout = "This content can be defined by the owner of the station";

console.log("Starting plugin code");


loadPlugins && loadPlugins();

const button = document.getElementById('xchange'); // Get the button by its ID


if (communityFeed) {
    button.classList.remove('fill-red');
    button.classList.add('fill-green');

    const feedVector = new ol.source.Vector({ features: [] });

    // Simple style: grey dot for ships, teal for AtoN/base, orange for SAR/heli/plane.
    // Moving ships with a known direction get a small directional triangle.
    // type: 0=moving, 1=stationary, 2=AtoN/base, 3=SAR/heli/plane
    // Derive sprite coordinates from the compact binary flags.
    // Sprite sheet columns (cx, cy=0 moving / cy=20 stationary, imgSize=20):
    //   cx=0  → generic ship (first column — no specific type known)
    //   cx=20 → Class B
    //   cx=0, cy=40 → AtoN
    //   cx=20,cy=40 → base station
    //   cx=0, cy=60, imgSize=25 → SAR / heli / plane
    function getFeedSprite(ship) {
        const type = ship.type;      // 0=moving, 1=stationary, 2=AtoN/base, 3=SAR/heli/plane
        const classB = ship.class_b; // 1 = Class B transponder
        const dir = ship.direction;  // degrees or null

        switch (type) {
            case 2: // AtoN / base station
                return { cx: 0, cy: 40, imgSize: 20, rot: 0 };
            case 3: // SAR / helicopter / plane
                return { cx: 0, cy: 60, imgSize: 25, rot: dir != null ? dir * Math.PI / 180 : 0 };
            default: { // 0 = moving, 1 = stationary
                const cx = classB ? 20 : 0;
                const isMoving = type === 0 && dir != null;
                const cy = isMoving ? 0 : 20;
                const rot = isMoving ? dir * Math.PI / 180 : 0;
                return { cx, cy, imgSize: 20, rot };
            }
        }
    }

    const feederStyle = function (feature) {
        const sp = getFeedSprite(feature.ship);
        return new ol.style.Style({
            image: new ol.style.Icon({
                src: "https://api.aiscatcher.org/community/v1/sprites.png",
                offset: [sp.cx, sp.cy],
                size: [sp.imgSize, sp.imgSize],
                rotation: sp.rot,
                scale: settings.icon_scale * 0.8,
                opacity: 0.85
            })
        });
    };

    const feedLayer = new ol.layer.Vector({
        source: feedVector,
        style: feederStyle
    });

    // Decode the compact binary format (big-endian, 12-byte header + 11 bytes/ship)
    function decodeCompactShips(buffer) {
        const view = new DataView(buffer);
        let offset = 0;

        if (view.byteLength < 12) return [];

        // Header: uint64 timestamp (skip) + int32 count
        offset += 8; // skip timestamp
        const count = view.getInt32(offset, false); offset += 4;

        const ships = [];
        for (let i = 0; i < count; i++) {
            if (offset + 11 > view.byteLength) break;

            const latRaw = view.getInt32(offset, false); offset += 4;
            const lonRaw = view.getInt32(offset, false); offset += 4;
            const flags = view.getUint8(offset); offset += 1;
            const dirRaw = view.getUint16(offset, false); offset += 2;

            const lat = (latRaw === 0x7FFFFFFF) ? null : latRaw / 600000.0;
            const lon = (lonRaw === 0x7FFFFFFF) ? null : lonRaw / 600000.0;

            if (lat === null || lon === null) continue;

            ships.push({
                lat,
                lon,
                type: flags & 0x03,
                approx: (flags >> 2) & 1,
                class_b: (flags >> 3) & 1,
                direction: (dirRaw === 0xFFFF) ? null : dirRaw / 10.0
            });
        }
        return ships;
    }

    function redrawCommunityFeed() {
        feedVector.clear();
        for (const ship of communityShips) {
            const feature = new ol.Feature({
                geometry: new ol.geom.Point(ol.proj.fromLonLat([ship.lon, ship.lat]))
            });
            feature.ship = ship;
            feature.link = `https://www.aiscatcher.org/livemap?lat=${ship.lat}&lon=${ship.lon}&zoom=16.00`;
            feedVector.addFeature(feature);
        }
    }

    let communityShips = [];

    async function fetchCommunityShips() {
        try {
            const extent = map.getView().calculateExtent(map.getSize());
            const extentInLatLon = ol.proj.transformExtent(extent, 'EPSG:3857', 'EPSG:4326');

            const minLon = extentInLatLon[0] - 0.0045;
            const minLat = extentInLatLon[1] - 0.0045;
            const maxLon = extentInLatLon[2] + 0.0045;
            const maxLat = extentInLatLon[3] + 0.0045;
            const zoom = map.getView().getZoom();

            const url = `https://api.aiscatcher.org/community/v1/ships?${zoom},${minLat},${minLon},${maxLat},${maxLon}`;
            const response = await fetch(url);
            const buffer = await response.arrayBuffer();
            communityShips = decodeCompactShips(buffer);
            return true;
        } catch (error) {
            console.log("Failed loading community ships: " + error);
            return false;
        }
    }

    debounceUpdateCommunityFeed = debounce(updateCommunityFeed, 250);

    feedLayer.on('change:visible', function () {
        if (feedLayer.getVisible()) {
            debounceUpdateCommunityFeed();
        }
    });

    function updateCommunityFeed() {
        if (!feedLayer.isVisible()) return;
        fetchCommunityShips().then((ok) => {
            if (ok) redrawCommunityFeed();
        });
    }
    addOverlayLayer("Community Feed", feedLayer);
}

// Register all HTML-callable actions with the event dispatcher.
registerActions({
    // Global UI / header — toggleMenu, toggleInfoPanel, toggleScreenSize, closeDialog, showAboutAndInfo, showSettingsMenu, showMapSettingsMenu registered by initUI()
    headerClick, openWebControl, toggleReceiverDropdown,
    // Settings panel — applyDefaultSettings, showPlugins, resetTrackColorsToDefault, openSettings, closeSettings, toggleDarkMode, showServerErrors registered by initSettings_tab()
    // Table tab — downloadCSV, resetShipTableColumns registered by initTable()
    hideTablecard, updateTableSort,
    // Map controls
    unpinCenter, showAllTracks, deleteAllTracks, ToggleFireworks,
    toggleLabel, toggleKioskMode, toggleFading, toggleRange,
    toggleStatcard, toggleTablecard, showCommunity, showMapMenu,
    activateMeasureMode, toggleMeasurecard,
    // Shipcard
    toggleShipcardSize, cardShowContextMenu, closeShipcard, shipcardselect,
    cardShowBinaryMessageDialog, cardOpenRealtime, cardOpenAIScatcherSite,
    cardToggleTrack, cardOpenFlightAware, cardOpenPlaneSpotters, cardOpenADSBExchange,
    // Context menu items (via data-action on list items)
    toggleShipcardPin, pinStation, ctxCopyText, ctxCopyICAOText, ctxCopyCoordinates,
    ctxToggleTrack, ctxPinVessel, ctxZoomIn, ctxShowShipcard, ctxShowPlanecard,
    ctxOpenAIScatcherSite, ctxShowVesselDetail, ctxShowNMEA, ctxOpenRealtime,
    ctxOpenGoogleSearch, ctxOpenGoogleSearchICAO, ctxOpenVesselFinder, ctxOpenAISHub,
    // Charts — toggleSignalGraph/togglePPMGraph registered by initCharts()
    // Realtime tab — registered by initRealtime()
    // Decoder tab — registered by initDecoder()
});

// showMainContextMenu, showChartsContextMenu, cardShowContextMenu registered by initUI()

// registerChanges/registerInputs for settings — handled by initSettings_tab()
// registerChanges for charts — handled by initCharts()
// registerChanges for realtime — handled by initRealtime()

initSettings(context);
initDecoder();
initLog();
initUI({
    getEvtSourceMap:      () => evtSourceMap,
    isShowAllTracks:      () => show_all_tracks,
    hasMarkerTracks:      () => marker_tracks.size > 0,
    hideMapMenu,
    trackOptionString,
    updateKiosk,
    getCardMMSI:          () => card_mmsi,
    getCardType:          () => card_type,
    saveSettings,
});
initSettings_tab({
    // Map functions
    redrawMap, updateMapLayer, setMapOpacity,
    removeDistanceCircles, setRangeColor, fetchRange, drawRange,
    toggleRange, toggleFading, updateFocusMarker, updateSortMarkers,
    // State
    ShippingClass,
    getDefaultTrackColors,
    isAndroid,
    resetTable:          refreshTable,
    isKioskAnimating:    () => !!kioskAnimationInterval,
    startKioskAnimation, updateKiosk,
    getPlugins:          () => plugins,
    getServerMessage:    () => server_message,
    // Coordinator
    saveSettings, refresh_data,
});
initRealtime({ selectMapTab });
initTable({
    getActiveReceiver: () => activeReceiver,
    selectMapTab,
    saveSettings,
});
initCharts({
    getActiveReceiver:    () => activeReceiver,
    getRefreshIntervalMs: () => refreshIntervalMs,
    setStationTitle:      (title) => { tab_title_station = title; updateTitle(); },
    saveSettings,
});
initEvents(activateTab);

// Script.js listener: map URL + dark-mode redraws on settings change.
// (updateSettingsTab is now registered by initSettings_tab via onSettingsChange)
onSettingsChange(() => {
    updateMapURL();
});

const urlParams = new URLSearchParams(window.location.search);
restoreDefaultSettings(getDefaultTrackColors());

console.log("Plugin loading completed");

console.log("Load settings");
loadSettings();
if (typeof server_receivers !== 'undefined') updateReceiverSelect(server_receivers);

console.log("Load settings from URL parameters");

loadSettingsFromURL();
updateForLegacySettings();

applyDynamicStyling();

console.log("Setup tabs");
updateAllChartColors();

initMap();

initShipcard({
    map,
    extraVector,
    trackLayer,
    toggleTrack, showTrack, hideTrack, trackIsShown,
    isShowAllTracks:        () => show_all_tracks,
    isSelectTrackEnabled:   () => select_enabled_track,
    setSelectTrackEnabled:  v => { select_enabled_track = v; },
    isHoverTrackEnabled:    () => hover_enabled_track,
    setHoverTrackEnabled:   v => { hover_enabled_track = v; },
    getHoverMMSI:           () => hoverMMSI,
    getHoverType:           () => hoverType,
    stopHover,
    moveMapCenter,
    saveSettings,
});

initTableside({
    showShipcard,
    startHover,
    stopHover,
    saveSettings,
    selectTab,
});

updateDarkMode();

console.log("Switch to active tab");
selectTab();

if (urlParams.get("mmsi")) openFocus(urlParams.get("mmsi"), urlParams.get("zoom"));
updateSortMarkers();
saveSettings();

if (aboutMDpresent == false) {
    document.getElementById("about_tab").style.display = "none";
    document.getElementById("about_tab_mini").style.display = "none";
}

// realtime tab visibility handled by tabs/realtime.js init()
// log tab visibility handled by tabs/log.js init()
// decoder tab visibility handled by tabs/decoder.js init()

if (typeof webcontrol_http === "undefined" || !webcontrol_http) {
    document.getElementById("webcontrol_tab").style.display = "none";
    document.getElementById("webcontrol_tab_mini").style.display = "none";
}

showWelcome();
updateKiosk();
updateAndroid();

if (isAndroid()) showMenu();

main();

// Ensure chart colors are applied after all stylesheets are loaded and charts are fully rendered (fixes Firefox iframe issue)
window.addEventListener('load', () => {
    // Use requestAnimationFrame to ensure charts have completed their initial render
    requestAnimationFrame(() => {
        requestAnimationFrame(() => {
            updateAllChartColors();
        });
    });
});

//checkLatestVersion();
