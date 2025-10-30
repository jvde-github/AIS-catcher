
var interval,
    paths = {},
    map,
    basemaps = {},
    overlapmaps = {},
    station = {},
    shipsDB = {},
    binaryDB = {},
    planesDB = {},
    fetch_binary = false,
    hover_feature = undefined,
    show_all_tracks = false,
    evtSourceMap = null,
    logViewer = null,
    realtimeViewer = null,
    range_outline = undefined,
    range_outline_short = undefined,
    tab_title_station = "",
    tab_title_count = null,
    context_mmsi = null,
    context_type = null,
    aboutContentLoaded = false,
    tab_title = "AIS-catcher";

const baseMapSelector = document.getElementById("baseMapSelector");

var rtCount = 0;
var shipcardIconCount = undefined;
var shipcardIconMax = 3;
var shipcardIconOffset = 0;
var plugins_main = [];
var card_mmsi = null,
    card_type = null,
    hover_mmsi = null,
    build_string = "unknown";
var refreshIntervalMs = 2500;
var range_update_time = null;
let updateInProgress = false;
let activeTileLayer = undefined;
var hover_enabled_track = false,
    select_enabled_track = false,
    marker_tracks = new Set();

if (typeof window.loadPlugins === 'undefined') {
    window.loadPlugins = function () { };
    communityFeed = false;
    aboutMDpresent = false;
    context = "aiscatcher";
}

const hover_info = document.getElementById('hover-info');

let measures = [];

// default settings
var settings = {};
let isFetchingShips = false;

function restoreDefaultSettings() {
    settings = {
        counter: true,
        fading: false,
        android: false,
        kiosk: false,
        welcome: true,
        coordinate_format: "decimal",
        icon_scale: 1,
        track_weight: 1,
        show_range: false,
        distance_circles: true,
        distance_circle_color: '#1c71d8',
        map_day: "OpenStreetMap",
        map_overlay: [],
        map_night: "Dark Matter (no labels)",
        zoom: 3,
        lat: 0,
        lon: 0,
        table_shiptype_use_icon: true,
        tableside_column: "shipname",
        tableside_order: "ascending",
        range_timeframe: '24h',
        track_color: "#12a5ed",
        range_color: "#FFA500",
        range_color_short: "#FFDAB9",
        range_color_dark: "#4B4B4B",
        range_color_dark_short: "#303030",
        fix_center: false,
        center_point: "station",
        tooltipLabelColor: "#ffffff",
        tooltipLabelColorDark: "#ffffff",
        tooltipLabelShadowColor: "#000000",
        tooltipLabelShadowColorDark: "#000000",
        tooltipLabelFontSize: 9,
        shiphover_color: "#FFA500",
        shipselection_color: "#943b3e",
        shipoutline_border: "#A9A9A9",
        shipoutline_inner: "#808080",
        shipoutline_opacity: 0.9,
        show_circle_outline: false,
        dark_mode: false,
        center_radius: 0,
        show_station: true,
        metric: "DEFAULT",
        setcoord: true,
        tab: "map",
        show_labels: "never",
        labels_declutter: true,
        eri: true,
        loadURL: true,
        map_opacity: 0.5,
        show_track_on_hover: false,
        show_track_on_select: false,
        shipcard_pinned: false,
        shipcard_pinned_x: null,
        shipcard_pinned_y: null,
        kiosk_rotation_speed: 5,
        kiosk_pan_map: true,
        shiptable_columns: ["shipname", "mmsi", "imo", "callsign", "shipclass", "lat", "lon", "last_signal", "level", "distance", "bearing", "speed", "repeat", "ppm", "status"]
    };
}

function toggleInfoPanel() {
    const overlay = document.querySelector('.overlay');
    const infoPanel = document.querySelector('.info-panel');

    overlay.classList.toggle('active');
    infoPanel.classList.toggle('active');
}

function updateTitle() {
    document.title = (tab_title_count ? " (" + tab_title_count + ") " : "") + tab_title + " " + tab_title_station;
}

function applyDefaultSettings() {
    const t = settings.tab;

    let android = settings.android;
    let darkmode = settings.dark_mode;
    restoreDefaultSettings();

    settings.android = android;

    if (isAndroid()) settings.dark_mode = darkmode;

    updateSortMarkers();
    //setLatLonInDMS(settings.latlon_in_dms);
    setDarkMode(settings.dark_mode);
    setMetrics(settings.metric);
    updateMapLayer();
    setFading(settings.fading);

    updateFocusMarker();
    removeDistanceCircles();

    settings.tab = t;
    settings.welcome = false;

    redrawMap();
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

function decimalToDMS(l, isLatitude) {
    var degrees = Math.floor(Math.abs(l));
    var minutes = Math.floor((Math.abs(l) - degrees) * 60);
    var seconds = Number(((Math.abs(l) - degrees) * 60 - minutes) * 60).toFixed(1);
    var direction = isLatitude ? (l > 0 ? "N" : "S") : l > 0 ? "E" : "W";
    return degrees + "&deg" + minutes + "'" + seconds + '"' + direction;
}

function decimalToDDM(l, isLatitude) {
    var degrees = Math.floor(Math.abs(l));
    var minutes = ((Math.abs(l) - degrees) * 60).toFixed(2);
    var direction = isLatitude ? (l > 0 ? "N" : "S") : l > 0 ? "E" : "W";
    return degrees + "&deg;" + minutes + "'" + direction;
}

// transformations - for overwrite
getDimVal = (c) => {
    return settings.metric === "DEFAULT" || settings.metric === "SI" ? Number(c).toFixed(0) : Number(c * 3.2808399).toFixed(0);
};

getDimUnit = () => {
    return settings.metric === "DEFAULT" || settings.metric === "SI" ? "m" : "ft";
};

getDistanceConversion = (c) => (settings.metric === "DEFAULT" ? c : settings.metric === "SI" ? c * 1.852 : c * 1.15078);
getDistanceVal = (c) => Number(getDistanceConversion(c)).toFixed(1).toLocaleString();
getDistanceUnit = () => (settings.metric === "DEFAULT" ? "nmi" : settings.metric === "SI" ? "km" : "mi");
getSpeedVal = (c) => (settings.metric === "DEFAULT" ? Number(c).toFixed(1) : settings.metric === "SI" ? Number(c * 1.852).toFixed(1) : Number(c * 1.151).toFixed(1));
getSpeedUnit = () => (settings.metric === "DEFAULT" ? "kts" : settings.metric === "SI" ? "km/h" : "mph");
getShipDimension = (ship) => ship.to_bow != null && ship.to_stern != null && ship.to_port != null && ship.to_starboard != null
    ? getDimVal(ship.to_bow + ship.to_stern) + " " + getDimUnit() + " x " + getDimVal(ship.to_port + ship.to_starboard) + " " + getDimUnit()
    : null



getLatValFormat = (ship) => {
    const prefix = ship.approx ? "<i>" : "";
    const suffix = ship.approx ? "</i>" : "";
    let content = "";

    switch (settings.coordinate_format) {
        case "dms":
            content = decimalToDMS(ship.lat, true);
            break;
        case "ddm":
            content = decimalToDDM(ship.lat, true);
            break;
        default: // decimal
            content = Number(ship.lat).toFixed(5);
            break;
    }

    return prefix + content + suffix;
};

getLonValFormat = (ship) => {
    const prefix = ship.approx ? "<i>" : "";
    const suffix = ship.approx ? "</i>" : "";
    let content = "";

    switch (settings.coordinate_format) {
        case "dms":
            content = decimalToDMS(ship.lon, false);
            break;
        case "ddm":
            content = decimalToDDM(ship.lon, false);
            break;
        default: // decimal
            content = Number(ship.lon).toFixed(5);
            break;
    }

    return prefix + content + suffix;
};

getEtaVal = (ship) => ("0" + ship.eta_month).slice(-2) + "-" + ("0" + ship.eta_day).slice(-2) + " " + ("0" + ship.eta_hour).slice(-2) + ":" + ("0" + ship.eta_minute).slice(-2);
getShipName = (ship) => ship.shipname;
getCallSign = (ship) => ship.callsign;
getICAOfromHexIdent = (h) => h.toString(16).toUpperCase().padStart(6, '0')
getICAO = (plane) => getICAOfromHexIdent(plane.hexident)
includeShip = (ship) => true;

const getDeltaTimeVal = (s) => {
    const days = Math.floor(s / (24 * 3600));
    const hours = Math.floor((s % (24 * 3600)) / 3600);
    const minutes = Math.floor((s % 3600) / 60);
    const seconds = s % 60;

    let result = '';
    if (days > 0) result += `${days}d `;
    if (hours > 0 || days > 0) result += `${hours}h `;
    if (minutes > 0 || hours > 0 || days > 0) result += `${minutes}m `;
    if (seconds > 0 || (days === 0 && hours === 0 && minutes === 0)) result += `${seconds}s`;

    return result.trim();
};

const notificationContainer = document.getElementById("notification-container");

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

async function copyToClipboard(textToCopy) {
    // Navigator clipboard api needs a secure context (https)
    if (navigator.clipboard && window.isSecureContext) {
        await navigator.clipboard.writeText(textToCopy);
    } else {
        // Use the 'out of viewport hidden text area' trick
        const textArea = document.createElement("textarea");
        textArea.value = textToCopy;

        // Move textarea out of the viewport so it's not visible
        textArea.style.position = "absolute";
        textArea.style.left = "-999999px";

        document.body.prepend(textArea);
        textArea.select();

        try {
            document.execCommand("copy");
        } catch (error) {
            console.error(error);
        } finally {
            textArea.remove();
        }
    }
}

function openSettings() {
    document.querySelector(".settings_window").classList.add("active");
}

function closeSettings() {
    document.querySelector(".settings_window").classList.remove("active");
}

function closeTableSide() {
    document.querySelector(".tableside_window").classList.remove("active");
}

function setCoordinateFormat(format) {
    settings.coordinate_format = format;
    saveSettings();

    refresh_data();
    if (table != null) table = null;
}


function copyCoordinates(m) {
    let coords = "not found";

    if (m in shipsDB) coords = shipsDB[m].raw.lat + "," + shipsDB[m].raw.lon;
    if (copyClipboard(coords)) showNotification("Coordinates copied to clipboard");
}

let hoverMMSI = undefined;
let hoverType = undefined;

var rangeStyleFunction = function (feature) {
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

var shapeStyleFunction = function (feature) {

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

var trackStyleFunction = function (feature) {
    var w = Number(settings.track_weight);
    var c = settings.track_color;

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
            width: w
        })
    });
}


var markerStyle = function (feature) {

    var length = (feature.ship.to_bow || 0) + (feature.ship.to_stern || 0);
    var mult = length >= 100 && length <= 200 ? 0.9 : length > 200 ? 1.1 : 0.75;

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


var planeStyleOld = function (feature) {

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

var planeStyle = function (feature) {
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

var labelStyle = function (feature) {
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

hoverCircleStyleFunction = function (feature) {
    return new ol.style.Style({
        image: new ol.style.Circle({
            radius: 20,
            stroke: new ol.style.Stroke({
                color: settings.shiphover_color,
                width: 8
            })
        })
    });
}

selectCircleStyleFunction = function (feature) {
    return new ol.style.Style({
        image: new ol.style.Circle({
            radius: 15,
            stroke: new ol.style.Stroke({
                color: settings.shipselection_color,
                width: 8
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


var markerVector = new ol.source.Vector({
    features: []
})

var binaryVector = new ol.source.Vector({
    features: []
})


var rangeVector = new ol.source.Vector({
    features: []
})

var shapeVector = new ol.source.Vector({
    features: []
});

var extraVector = new ol.source.Vector({
    features: []
});

var trackVector = new ol.source.Vector({
    features: []
});

var labelVector = new ol.source.Vector({
    features: []
});

var planeVector = new ol.source.Vector({
    features: []
});

var markerLayer = new ol.layer.Vector({
    source: markerVector,
    style: markerStyle
})

var binaryLayer = new ol.layer.Vector({
    source: binaryVector,
    style: binaryStyle
})

var planeLayer = new ol.layer.Vector({
    source: planeVector,
    style: planeStyle,
    visible: false
})

var shapeLayer = new ol.layer.Vector({
    source: shapeVector,
    style: shapeStyleFunction
});

var extraLayer = new ol.layer.Vector({
    source: extraVector
});

var trackLayer = new ol.layer.Vector({
    source: trackVector,
    style: trackStyleFunction
});

var rangeLayer = new ol.layer.Vector({
    source: rangeVector,
    style: rangeStyleFunction
});

var labelLayer = new ol.layer.Vector({
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
let planeFeatures = {};

let stationFeature = undefined;
let hoverCircleFeature = undefined;
let measureCircleFeature = undefined;
let selectCircleFeature = undefined;


async function fetchJSON(l, m) {
    try {
        response = await fetch(l + "?" + m);
    } catch (error) {
        showialog("Error", error);
    }
    return response.text();
}


// Function to create ship outline geometry in OpenLayers
function createShipOutlineGeometry(ship) {
    if (!ship) return null;
    const coordinate = [ship.lon, ship.lat];

    let heading = ship.heading;
    let { to_bow, to_stern, to_port, to_starboard } = ship;

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

    let shipOutlineCoords = [A, B, C, D, E, A].map(coord => ol.proj.fromLonLat(coord));
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
        for (let key in obj) {
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
    s = await fetchJSON("api/vessel", m);
    let obj = JSON.parse(s);

    let tableHtml = '<table class="mytable">';
    for (let key in obj) {
        tableHtml += "<tr><td>" + key + "</td><td>" + obj[key] + "</td></tr>";
    }
    tableHtml += "</table>";

    showDialog("Vessel " + m, tableHtml);
}

function showBinaryMessageDialog(featureOrMmsi) {
    let title, content;

    if (typeof featureOrMmsi === 'object') {
        if (!featureOrMmsi.binary_messages || featureOrMmsi.binary_messages.length === 0) {
            showDialog("Binary Message", "No message content available");
            return;
        }

        title = "Binary Messages";
        content = getBinaryMessageList(featureOrMmsi.binary_messages);

    } else if (typeof featureOrMmsi === 'number' || typeof featureOrMmsi === 'string') {
        const mmsi = Number(featureOrMmsi);

        if (!binaryDB[mmsi] || !binaryDB[mmsi].ship_messages || binaryDB[mmsi].ship_messages.length === 0) {
            showDialog("Binary Messages", "No binary messages available for this vessel");
            return;
        }

        const shipName = mmsi in shipsDB ? (shipsDB[mmsi].raw.shipname || `MMSI ${mmsi}`) : `MMSI ${mmsi}`;
        title = `Binary Messages for ${shipName}`;
        content = getBinaryMessageList(binaryDB[mmsi].ship_messages);
    }

    showDialog(title, content);
}

function getBinaryMessageList(messages) {
    if (!messages || messages.length === 0) {
        return "<p>No messages available</p>";
    }

    const sortedMessages = [...messages].sort((a, b) => b.timestamp - a.timestamp);

    let content = '<div class="binary-messages-list">';

    content += `<div class="binary-message-count">${sortedMessages.length} message${sortedMessages.length > 1 ? 's' : ''} available</div>`;

    sortedMessages.forEach((msg, index) => {
        if (index > 0) {
            content += '<hr style="margin: 15px 0; border: 0; border-top: 1px solid rgba(0,0,0,0.1);">';
        }

        content += '<div class="binary-message-item">';

        content += `<div class="binary-message-header">
                      <span class="binary-message-time">${msg.formattedTime || new Date(msg.timestamp * 1000).toLocaleTimeString()}</span>`;

        if (msg.message && msg.message.mmsi) {
            const shipName = msg.message.mmsi in shipsDB ?
                (shipsDB[msg.message.mmsi].raw.shipname || `MMSI ${msg.message.mmsi}`) :
                `MMSI ${msg.message.mmsi}`;

            content += `<span class="binary-message-source"> from ${shipName}</span>`;
        }

        content += '</div>';

        if (msg.message && msg.message.dac == 1 && (msg.message.fid == 31 || msg.message.fi == 31)) {
            content += getBinaryMessageContent(msg, false);
        } else {
            content += '<div class="binary-message-details">';
            content += `<div><strong>Message Type:</strong> ${msg.message ? `DAC ${msg.message.dac}, FI ${msg.message.fid || msg.message.fi}` : 'Unknown'}</div>`;

            // Add a collapsible section for raw data
            content += `<details class="binary-raw-data">
                          <summary>Show Raw Data</summary>
                          <pre>${JSON.stringify(msg.message, null, 2)}</pre>
                        </details>`;

            content += '</div>';
        }

        content += '</div>';
    });

    content += '</div>';

    return content;
}

function copyText(m) {
    if (copyClipboard(m)) showNotification("Content copied to clipboard");
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

const contextMenu = document.getElementById("context-menu");

function hideContextMenu(event) {
    contextMenu.style.display = "none";
    document.removeEventListener("click", hideContextMenu);
}

function showContextMenu(event, mmsi, type, context) {

    if (event && event.preventDefault) {
        event.preventDefault();
        event.stopPropagation();
    }

    hideMapMenu(event);

    document.getElementById("ctx_labelswitch").textContent = settings.show_labels != "never" ? "Hide ship labels" : "Show ship labels";
    document.getElementById("ctx_range").textContent = settings.show_range ? "Hide station range" : "Show station range";
    document.getElementById("ctx_fading").textContent = settings.fading ? "Show icons without fading" : "Show icons with fading";
    document.getElementById("ctx_fireworks").textContent = evtSourceMap == null ? "Start Fireworks Mode" : "Stop Fireworks Mode";
    document.getElementById("ctx_label_shipcard_pin").textContent = settings.shipcard_pinned ? "Unpin Shipcard position" : "Pin Shipcard position";

    context_mmsi = mmsi;
    context_type = type;

    const classList = ["station", "settings", "plane-map", "ship-map", "plane", "ship", "ctx-map", "copy-text", "table-menu", "ctx-shipcard"];

    classList.forEach((className) => {
        if (context.includes('object')) {
            context.push(type);
        }
        if (context.includes('object-map')) {
            context.push(type + "-map");
        }
        const shouldDisplay = context.includes(className);
        const elements = document.querySelectorAll("." + className);
        elements.forEach((element) => {
            element.style.display = shouldDisplay ? "flex" : "none";
        });
    });

    // we might have made non-android items visible in the context menu, so hide non-android items if needed
    updateAndroid();
    updateKiosk();

    if (show_all_tracks) {
        document.querySelectorAll(".ctx-noalltracks").forEach(function (element) {
            element.style.display = "none";
        });
    }

    if (show_all_tracks || marker_tracks.size > 0) {
        document.querySelectorAll(".ctx-removealltracks").forEach(function (element) {
            element.style.display = "flex";
        });
    } else {
        document.querySelectorAll(".ctx-removealltracks").forEach(function (element) {
            element.style.display = "none";
        });
    }

    document.getElementById("ctx_menu_unpin").style.display = settings.fix_center && context.includes("ctx-map") ? "flex" : "none";
    document.getElementById("ctx_track").innerText = trackOptionString(context_mmsi);

    contextMenu.style.display = "block";

    if (context.includes("center")) {
        contextMenu.style.left = "50%";
        contextMenu.style.top = "50%";
        contextMenu.style.transform = "translate(-50%, -50%)";
    } else {
        contextMenu.style.left = event.pageX + 5 + "px";
        contextMenu.style.top = event.pageY + 5 + "px";
        contextMenu.style.transform = "none";

        var contextMenuRect = contextMenu.getBoundingClientRect();
        var viewportWidth = window.innerWidth && window.outerWidth ? Math.min(window.innerWidth, window.outerWidth) : document.documentElement.clientWidth;
        var viewportHeight = window.innerHeight && window.outerHeight ? Math.min(window.innerHeight, window.outerHeight) : document.documentElement.clientHeight;

        var maxX = viewportWidth - contextMenuRect.width;
        var maxY = viewportHeight - contextMenuRect.height;

        var adjustedX = Math.max(0, Math.min(event.pageX + 5, maxX));
        var adjustedY = Math.max(0, Math.min(event.pageY + 5, maxY));

        contextMenu.style.left = adjustedX + "px";
        contextMenu.style.top = adjustedY + "px";
    }

    document.addEventListener("click", function (event) {
        hideContextMenu();
    });
}

function showDialog(title, message) {
    var dialogBox = document.getElementById("dialog-box");
    var dialogTitle = dialogBox.querySelector(".dialog-title");
    var dialogMessage = dialogBox.querySelector(".dialog-message");

    dialogTitle.innerText = title;
    dialogMessage.innerHTML = message;
    dialogBox.classList.remove("hidden");
}

function closeDialog() {
    var dialogBox = document.getElementById("dialog-box");
    dialogBox.classList.add("hidden");
}

function showNotification(message) {
    const notificationElement = document.createElement("div");
    notificationElement.classList.add("notification");
    notificationElement.textContent = message;

    notificationContainer.appendChild(notificationElement);

    setTimeout(() => {
        notificationElement.style.opacity = 0;
        setTimeout(() => {
            notificationContainer.removeChild(notificationElement);
        }, 500);
    }, 2000);
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

function updateMapLayer() {

    if (activeTileLayer) {

        overlays = JSON.parse(JSON.stringify(settings.map_overlay));
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

    var attributions = activeTileLayer.getSource().getAttributions();
    var mapAttributions = document.getElementById("map_attributions");
    if (typeof attributions === 'function') {
        var currentAttributions = attributions();
        mapAttributions.innerHTML = currentAttributions.join(', ');
    } else if (Array.isArray(attributions)) {
        mapAttributions.innerHTML = attributions.join(', ');
    }

}

var dynamicStyle = document.createElement("style");
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
    for (let key in basemaps)
        basemaps[key].setOpacity(Number(settings.map_opacity));

    for (let key in overlapmaps) {
        if (key !== "Aircraft") {
            overlapmaps[key].setOpacity(Number(settings.map_opacity));
        }
    }
}

var clickTimeout = undefined;
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
        let icon = measure.visible ? 'visibility' : 'visibility_off';

        content += `<tr data-index="${measures.indexOf(measure)}"><td style="padding: 2px;"><i style="padding-left:2px" class="${icon}_icon visibility_icon"></i></td><td style="padding: 0px;"><i class="delete_icon"></i></td><td>${from}</td><td>${to}</td><td title="${distance} ${getDistanceUnit()}">${distance}</td><td title="${bearing} degrees">${bearing}</td></tr>`;

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

    let included = feature && 'ship' in feature && feature.ship.mmsi in shipsDB;
    let included_plane = feature && 'plane' in feature && feature.plane.hexident in planesDB;

    if (event.originalEvent.ctrlKey || measureMode || isMeasuring) {
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

    for (let [key, value] of Object.entries(basemaps)) {
        map.addLayer(value);
        value.setVisible(false);
    }

    for (let [key, value] of Object.entries(overlapmaps)) {
        map.addLayer(value);
        value.setVisible(false);
    }

    [rangeLayer, shapeLayer, trackLayer, markerLayer, labelLayer, extraLayer, binaryLayer, measureVector].forEach(layer => {
        map.addLayer(layer);
    });

    triggerMapLayer();

    map.on('pointermove', function (evt) {
        if (evt.dragging) {
            stopHover();
            return;
        }
        const pixel = map.getEventPixel(evt.originalEvent);
        handlePointerMove(pixel, evt.originalEvent.target);
    });

    map.on('click', function (evt) {
        handleClick(evt.pixel, evt.originalEvent.target, evt);
    });

    map.on('moveend', function (evt) {
        debouncedSaveSettings();
        debouncedDrawMap();
        if (communityFeed)
            debounceUpdateCommunityFeed();
    });


    map.getTargetElement().addEventListener('pointerleave', function () {
        stopHover();
    });

    map.getTargetElement().addEventListener('contextmenu', function (evt) {

        const f = getFeature(map.getEventPixel(evt), map.getTargetElement())

        if (!f)
            showContextMenu(evt, 0, null, ['settings', 'ctx-map']);
        else if ('station' in f) {
            showContextMenu(evt, null, null, ["station"]);
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

function setMetrics(s) {
    if (s.toUpperCase() == "DEFAULT") settings.metric = "DEFAULT";
    else if (s.toUpperCase() == "METRIC") settings.metric = "SI";
    else if (s.toUpperCase() == "IMPERIAL") settings.metric = "IMPERIAL";
    else settings.metric = "DEFAULT";

    showNotification("Switched units to " + s);
    saveSettings();

    refresh_data();
    if (table != null) table = null;
}

function setTableIcon(s) {
    settings.table_shiptype_use_icon = s;
    saveSettings();

    refresh_data();
    if (table != null) table = null;
}

function getMetrics() {
    if (settings.metric == "DEFAULT") return "Default";
    if (settings.metric == "SI") return "Metric";
    if (settings.metric == "IMPERIAL") return "Imperial";
    return "Default";
}

function addMarker(lat, lon, ch) {
    var latlon = ol.proj.fromLonLat([lon, lat]);
    var color = 'grey'; // Default color

    if (ch === "A") color = 'blue';
    if (ch === "B") color = 'red';

    var style = new ol.style.Style({
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

    var marker = new ol.Feature({
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
        if (typeof realtime_enabled === "undefined" || realtime_enabled === false) {
            showDialog("Error", "Cannot run Firework Mode. Please ensure that AIS-catcher is running with -N REALTIME on.");
            return;
        }

        evtSourceMap = new EventSource("api/signal");

        evtSourceMap.addEventListener(
            "nmea",
            function (e) {
                var jsonData = JSON.parse(e.data);

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

function updateMarkerCountTooltip() {
    if (shipsDB == null) {
        ["statcard_stationary", "statcard_moving", "statcard_class_b_stationary", "statcard_class_b_moving", "statcard_station", "statcard_aton", "statcard_heli", "statcard_sarte"].forEach(function (id) {
            document.getElementById(id).innerHTML = "";
        });

        return;
    }

    let cStationary = 0,
        cMoving = 0,
        cClassBstationary = 0,
        cClassBmoving = 0,
        cStation = 0,
        cAton = 0,
        cHeli = 0,
        cSarte = 0;

    for (let [key, m] of Object.entries(shipsDB)) {
        if (key in shipsDB) {
            let ship = shipsDB[key].raw;
            switch (ship.shipclass) {
                case ShippingClass.ATON:
                    cAton++;
                    break;
                case ShippingClass.PLANE:
                    cHeli++;
                    break;
                case ShippingClass.HELICOPTER:
                    cHeli++;
                    break;
                case ShippingClass.STATION:
                    cStation++;
                    break;
                case ShippingClass.SARTEPIRB:
                    cSarte++;
                    break;
                case ShippingClass.B:
                    if (ship.speed != null && ship.speed > 0.5) cClassBmoving++;
                    else cClassBstationary++;
                    break;
                default:
                    if (ship.speed != null && ship.speed > 0.5) cMoving++;
                    else cStationary++;
                    break;
            }
        }
    }

    flashNumber("statcard_stationary", cStationary);
    flashNumber("statcard_moving", cMoving);
    flashNumber("statcard_station", cStation);
    flashNumber("statcard_aton", cAton);
    flashNumber("statcard_heli", cHeli);
    flashNumber("statcard_sarte", cSarte);
    flashNumber("statcard_class_b_stationary", cClassBstationary);
    flashNumber("statcard_class_b_moving", cClassBmoving);
}

function updateTableSort(event) {
    const header = event.currentTarget;

    const column = header.getAttribute("data-column");
    const currentOrder = header.classList.contains("ascending") ? "ascending" : "descending";

    const newOrder = currentOrder === "descending" ? "ascending" : "descending";

    settings.tableside_column = column;
    settings.tableside_order = newOrder;

    saveSettings();
    updateSortMarkers();
    updateTablecard();
}

function updateSortMarkers() {
    const allHeaders = document.querySelectorAll("[data-column]");

    allHeaders.forEach((otherHeader) => {
        otherHeader.classList.remove("ascending");
        otherHeader.classList.remove("descending");

        if (otherHeader.getAttribute("data-column") === settings.tableside_column) {
            otherHeader.classList.add(settings.tableside_order);
        }
    });
}

function compareNumber(valueA, valueB) {
    if (valueA == null && valueB == null) return settings.tableside_order === "ascending" ? 1 : -1;
    if (valueA == null) return settings.tableside_order === "ascending" ? 1 : -1;
    if (valueB == null) return settings.tableside_order === "ascending" ? -1 : 1;
    return valueA - valueB;
}

function compareString(valueA, valueB) {
    if (valueA == null && valueB == null) return 0;
    if (valueA == null) return 1;
    if (valueB == null) return -1;

    return (valueA + "").localeCompare(valueB + "");
}

document.getElementById('shipSearchSide').addEventListener('input', updateTablecard);

function updateTablecard() {
    if (!document.getElementById("tableside").classList.contains("active")) return;

    var tableBody = document.getElementById("tablecardBody");
    tableBody.innerHTML = "";

    if (shipsDB == null) return;

    let shipKeys = Object.keys(shipsDB);

    column = settings.tableside_column;
    order = settings.tableside_order;

    const sortFunctions = {
        flag: (a, b) => compareString(shipsDB[a].raw.country, shipsDB[b].raw.country),
        shipname: (a, b) => compareString(getShipName(shipsDB[a].raw), getShipName(shipsDB[b].raw)),
        distance: (a, b) => compareNumber(shipsDB[a].raw.distance, shipsDB[b].raw.distance),
        speed: (a, b) => compareNumber(shipsDB[a].raw.speed, shipsDB[b].raw.speed),
        type: (a, b) => compareNumber(shipsDB[a].raw.shipclass, shipsDB[b].raw.shipclass),
        last_signal: (a, b) => compareNumber(shipsDB[a].raw.last_signal, shipsDB[b].raw.last_signal),
    };

    if (column in sortFunctions) {
        shipKeys.sort((keyA, keyB) => {
            const comparisonResult = sortFunctions[column](keyA, keyB);
            return order === "ascending" ? comparisonResult : -comparisonResult;
        });
    }

    var filter = document.getElementById('shipSearchSide').value.toLowerCase();

    let addedRows = 0;

    for (let i = 0; i < shipKeys.length; i++) {
        if (addedRows > 100) break;

        let key = shipKeys[i];
        if (key in shipsDB) {
            let ship = shipsDB[key].raw;
            let shipName = String(getShipName(ship) || ship.mmsi);
            if (!filter || shipName.toLowerCase().includes(filter)) {
                var row = tableBody.insertRow();

                row.addEventListener("mouseover", function (e) {
                    startHover('ship', ship.mmsi);
                });
                row.addEventListener("mouseout", function (e) {
                    stopHover();
                });
                row.addEventListener("click", function (e) {
                    showShipcard('ship', ship.mmsi);
                });
                row.addEventListener("contextmenu", function (e) {
                    showContextMenu(event, ship.mmsi, "ship", ["object", "object-map"]);
                });

                var cell1 = row.insertCell(0);
                cell1.innerHTML = getFlagStyled(ship.country, "padding: 0px; margin: 0px; box-shadow: 1px 1px 2px rgba(0, 0, 0, 0.2); font-size: 16px;")

                var cell2 = row.insertCell(1);
                cell2.innerText = shipName;

                var cell3 = row.insertCell(2);
                cell3.innerHTML = ship.distance ? (getDistanceVal(ship.distance) + (ship.repeat > 0 ? " (R)" : "")) : null
                cell3.title = ship.distance != null ? getDistanceVal(ship.distance) + " " + getDistanceUnit() + (ship.repeat > 0 ? " (R)" : "") : "";

                var cell4 = row.insertCell(3);
                cell4.innerHTML = ship.speed != null ? getSpeedVal(ship.speed) : "";
                cell4.title = ship.speed != null ? getSpeedVal(ship.speed) + " " + getSpeedUnit() : "";

                var cell5 = row.insertCell(4);
                cell5.innerHTML = getTableShiptype(ship);

                var cell6 = row.insertCell(5);
                cell6.innerHTML = getDeltaTimeVal(ship.last_signal);
            }
        }
    }
}

function flashNumber(id, newValue) {

    const element = document.getElementById(id);
    const oldValue = parseInt(element.innerText) || 0;

    if (newValue != oldValue) {
        element.classList.add("flash-up");
    }

    element.innerText = newValue;

    setTimeout(() => {
        element.classList.remove("flash-up");
    }, 500);
}

function updateMarkerCount() {

    let count = 0;
    if (shipsDB != null) {
        count = Object.keys(shipsDB).length;
    }

    flashNumber("markerCount", count);

    if (document.getElementById("statcard").style.display == "block") updateMarkerCountTooltip();
}

function toggleStatcard() {
    if (document.getElementById("statcard").style.display == "block") hideStatcard();
    else showStatcard();
}

function showStatcard() {
    updateMarkerCountTooltip();
    document.getElementById("statcard").style.display = "block";
}

function hideStatcard() {
    document.getElementById("statcard").style.display = "none";
}

function toggleTablecard() {
    if (!document.getElementById("tableside").classList.contains("active") && window.innerWidth < 800) {
        settings.tab = "ships";
        selectTab();
        return;
    }

    document.getElementById("tableside").classList.toggle("active");
    var elements = document.querySelectorAll(".map-button-box");
    elements.forEach(function (element) {
        element.classList.toggle("active");
    });

    updateTablecard();
}

function hideTablecard() {
    if (document.getElementById("tableside").classList.contains("active")) {
        toggleTablecard();
    }
}

function setRangeSwitch(b) {
    if (b != settings.show_range) {
        toggleRange();
    }
}

function setRangeColor(v, f) {
    settings[f] = v;
    redrawMap();
}

function setRangeTimePeriod(v) {
    settings.range_timeframe = v;
    saveSettings();
    fetchRange(true).then(() => drawRange());
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

function setFading(b) {
    if (b != settings.fading) {
        toggleFading();
    }
}

function toggleFading() {
    settings.fading = !settings.fading;
}

function showPlugins() {
    showDialog("Plugins", "<pre>Loaded plugins:\n" + plugins + "</pre>");
}

function showServerErrors() {
    showDialog("Server Errors", server_message == "" ? "None" : ("<pre>" + server_message + "</pre>"));
}

async function fetchAbout() {
    try {
        response = await fetch("about.md");
    } catch (error) {
        return;
    }
    aboutmd = await response.text();
    return aboutmd;
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

    try {
        response = await fetch("api/history_full.json");
        h = await response.json();
    } catch (error) {
        settings.show_range = false;
        setPulseError();
        return;
    }

    setPulseOk();

    range_outline = [];
    range_outline_short = [];

    const N = h.day.stat[0].radar_a.length;

    const range = [];
    const range_short = [];

    for (let i = 0; i < N; i++) {
        let m = 0;
        for (j = 0; j < h.minute.stat.length; j++) {
            m = Math.max(m, h.minute.stat[j].radar_a[i]);
            m = Math.max(m, h.minute.stat[j].radar_b[i]);
        }

        range_short.push(m);

        for (j = 0; j < h.hour.stat.length; j++) {
            m = Math.max(m, h.hour.stat[j].radar_a[i]);
            m = Math.max(m, h.hour.stat[j].radar_b[i]);
        }

        const additionalDays = settings.range_timeframe == "7d" ? 7 : settings.range_timeframe == "30d" ? 30 : 0;

        for (j = 0; j < Math.min(additionalDays, h.day.stat.length); j++) {
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

var distanceFeatures = undefined;
var distanceLat = undefined;
var distanceLon = undefined;
var distanceMetric = undefined;

function removeDistanceCircles() {
    if (distanceFeatures) {
        for (var i = 0; i < distanceFeatures.length; i++)
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

        for (var i = 0; i < range.length; i++) {

            let distanceCircle = new ol.Feature({
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

/*
let view_offset = 0;
 
function readUint8(view) {
const val = view.getUint8(view_offset);
view_offset += 1;
return val;
}
 
function readUint16(view) {
const val = view.getUint16(view_offset);
view_offset += 2;
return val;
}
 
function readUint32(view) {
const val = view.getUint32(view_offset);
view_offset += 4;
return val;
}
 
function readUint64(view) {
const high = readUint32(view);
const low = readUint32(view);
return high * 2 ** 32 + low;
}
 
function readInt8(view) {
const val = view.getInt8(view_offset);
view_offset += 1;
return val;
}
 
function readInt16(view) {
const val = view.getInt16(view_offset);
view_offset += 2;
return val;
}
 
function readInt32(view) {
const val = view.getInt32(view_offset);
view_offset += 4;
return val;
}
 
function readInt64(view) {
const high = readInt32(view);
const low = readUint32(view);
return high * 2 ** 32 + low;
}
 
function readString(view) {
const length = readUint8(view);
let str = "";
for (let i = 0; i < length; i++) {
    str += String.fromCharCode(readUint8(view));
}
return str;
}
 
function readFloat(view) {
return readInt16(view) / 1000.0;
}
 
function readFloatLow(view) {
return readInt16(view) / 10.0;
}
 
function readLatLon(view) {
const lat = readInt32(view) / 6000000.0;
const lon = readInt32(view) / 6000000.0;
return { lat, lon };
}
 
function deserialize(view, time) {
function setNullIf(value, condition) {
    if (value == condition) return null;
    return value;
}
 
function setNullIfLess(value, condition) {
    if (value < condition) return null;
    return value;
}
 
function setNullIfGreater(value, condition) {
    if (value > condition) return null;
    return value;
}
 
// Now, deserialize the Ship structure
const ship = {};
ship.mmsi = readUint32(view);
const latLon = readLatLon(view);
ship.lat = setNullIfGreater(latLon.lat, 90);
ship.lon = setNullIfGreater(latLon.lon, 180);
ship.distance = setNullIfLess(readFloatLow(view), 0);
ship.bearing = setNullIfLess(readFloatLow(view), 0);
ship.level = setNullIfGreater(readFloatLow(view), 1023);
ship.count = readInt16(view);
ship.ppm = setNullIfGreater(readFloatLow(view), 1023);
 
let approx_validated = readInt8(view);
ship.approx = (approx_validated & 1) == 1;
ship.validated = (approx_validated >> 1) & (1 == 1);
 
ship.heading = setNullIfGreater(readFloatLow(view), 510);
ship.cog = setNullIfGreater(readFloatLow(view), 359.9);
ship.speed = setNullIfLess(readFloatLow(view), 0);
ship.to_bow = setNullIf(readInt16(view), -1);
ship.to_stern = setNullIf(readInt16(view), -1);
ship.to_starboard = setNullIf(readInt16(view), -1);
ship.to_port = setNullIf(readInt16(view), -1);
ship.last_group = readUint64(view);
ship.group_mask = readUint64(view);
ship.shiptype = readInt16(view);
 
let shipclass_mmsitype = readUint8(view);
ship.shipclass = shipclass_mmsitype >> 4;
ship.mmsi_type = shipclass_mmsitype & 15;
 
ship.msg_type = readUint32(view);
ship.channels = readInt8(view);
ship.country = String.fromCharCode(readInt8(view), readInt8(view));
ship.status = readInt8(view);
ship.draught = setNullIfLess(readFloatLow(view), 0);
 
ship.eta_month = setNullIf(readInt8(view), 0);
ship.eta_day = setNullIf(readInt8(view), 0);
ship.eta_hour = setNullIf(readInt8(view), 24);
ship.eta_minute = setNullIf(readInt8(view), 60);
 
ship.imo = setNullIf(readInt32(view), 0);
ship.callsign = readString(view);
ship.shipname = readString(view);
ship.destination = readString(view);
ship.received = readUint64(view);
ship.last_signal = time - ship.received;
 
return ship;
}
 
async function fetchShipsBinary(noDoubleFetch = true) {
if (isFetchingShips && noDoubleFetch) {
    console.log("A fetch operation is already running.");
    return false;
}
 
let ships = {};
station = {};
let arrayBuffer;
 
isFetchingShips = true;
try {
    response = await fetch("sb");
    const blob = await response.blob();
    arrayBuffer = await blob.arrayBuffer();
} catch (error) {
    setPulseError();
    console.log("failed loading ships: " + error);
    return false;
} finally {
    isFetchingShips = false;
}
 
center = {};
 
setPulseOk();
 
shipsDB2 = {};
let view = new DataView(arrayBuffer);
view_offset = 0;
 
let time = readUint64(view);
let count = readInt32(view);
 
let hasstation = readInt8(view) == 1;
if (hasstation) {
    station = readLatLon(view);
    let own_mmsi = readUint32(view);
}
 
while (view_offset < view.byteLength) {
    const ship = deserialize(view, time);
    if (includeShip(ship)) {
        const entry = {};
        entry.raw = ship;
        shipsDB2[ship.mmsi] = entry;
    }
}
 
if (String(settings.center_point).toUpperCase() == "STATION") {
    center = station;
} else if (settings.center_point in shipsDB) {
    let ship = shipsDB[settings.center_point].raw;
    center = { lat: ship.lat, lon: ship.lon };
}
 
return true;
}
*/

function sanitizeString(input) {
    const map = {
        '&': '&amp;',
        '<': '&lt;',
        '>': '&gt;',
        '"': '&quot;',
        "'": '&#039;',
        '`': '&#96;'
    };
    return input.replace(/[&<>"'`]/g, function (match) {
        return map[match];
    });
}

function formatTime(timestamp) {
    const date = new Date(timestamp * 1000);
    return date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
}

async function fetchBinary() {
    binaryDB = {};
    standaloneBinaryMessages = []; // For messages not at ship locations

    try {
        response = await fetch("api/binmsgs.json");
        const messages = await response.json();

        // Group messages by MMSI
        messages.forEach((msg, index) => {
            if (msg.message && msg.message.mmsi && msg.message.lat && msg.message.lon) {
                const mmsi = msg.message.mmsi;

                if (!binaryDB[mmsi]) {
                    binaryDB[mmsi] = {
                        ship_messages: [], // Messages to show at ship location
                        standalone_messages: [] // Messages to show at their own locations
                    };
                }

                msg.formattedTime = formatTime(msg.timestamp);
                msg.index = index + 1;

                // Store the message's own coordinates 
                msg.message_lat = msg.message.lat;
                msg.message_lon = msg.message.lon;

                // We'll categorize messages later when drawing them
                binaryDB[mmsi].ship_messages.push(msg);
            }
        });

        setPulseOk();
        return true;
    } catch (error) {
        setPulseError();
        console.log("Failed loading binary:", error);
        return false;
    }
}

async function fetchShips(noDoubleFetch = true) {
    if (isFetchingShips && noDoubleFetch) {
        console.log("A fetch operation is already running.");
        return false;
    }

    let ships = {};

    isFetchingShips = true;
    try {
        response = await fetch("api/ships_array.json");
    } catch (error) {
        setPulseError();
        console.log("failed loading ships: " + error);
        return false;
    } finally {
        isFetchingShips = false;
    }
    ships = await response.json();

    setPulseOk();

    const keys = [
        "mmsi",
        "lat",
        "lon",
        "distance",
        "bearing",
        "level",
        "count",
        "ppm",
        "approx",
        "heading",
        "cog",
        "speed",
        "to_bow",
        "to_stern",
        "to_starboard",
        "to_port",
        "last_group",
        "group_mask",
        "shiptype",
        "mmsi_type",
        "shipclass",
        "msg_type",
        "country",
        "status",
        "draught",
        "eta_month",
        "eta_day",
        "eta_hour",
        "eta_minute",
        "imo",
        "callsign",
        "shipname",
        "destination",
        "last_signal",
        "flags",
        "validated",
        "channels",
        "altitude",
        "received_stations"
    ];

    shipsDB = {};
    station = {};

    ships.values.forEach((v) => {
        const s = Object.fromEntries(keys.map((k, i) => [k, v[i]]));

        const flags = s.flags;
        s.validated2 = (flags & 3) == 2 ? -1 : flags & 3;


        s.repeat = (flags >> 2) & 3;
        s.virtual_aid = (flags >> 4) & 1;
        s.approximate = (flags >> 5) & 1;
        s.channels2 = (flags >> 6) & 0b1111;

        // Check for discrepancies and show error
        if (s.validated !== s.validated2) {
            console.log(`Mismatch in validated: ${s.validated} != ${s.validated2}`);
        }
        if (s.channels !== s.channels2) {
            console.log(`Mismatch in channels: ${s.channels} != ${s.channels2}`);
        }

        if (includeShip(s)) {
            s.shipname = sanitizeString(s.shipname);
            s.callsign = sanitizeString(s.callsign);

            const entry = {};
            entry.raw = s;
            shipsDB[s.mmsi] = entry;
        }
    });

    if (ships.hasOwnProperty("station")) station = ships.station;

    center = {};
    if (String(settings.center_point).toUpperCase() == "STATION") {
        center = station;
    } else if (settings.center_point in shipsDB) {
        let ship = shipsDB[settings.center_point].raw;
        center = { lat: ship.lat, lon: ship.lon };
    }

    tab_title_count = ships.values.length;
    updateTitle();

    return true;
}

async function fetchPlanes() {

    let planes = {};

    try {
        response = await fetch("api/planes_array.json");
    } catch (error) {
        setPulseError();
        console.log("failed loading planes: " + error);
        return false;
    } finally {
    }

    planes = await response.json();
    setPulseOk();

    const keys = [
        "hexident",
        "lat",
        "lon",
        "altitude",
        "speed",
        "heading",
        "vertrate",
        "squawk",
        "callsign",
        "airborne",
        "nMessages",
        "last_signal",
        "category",
        "level",
        "country",
        "distance",
        "message_types",
        "message_subtypes",
        "group_mask",
        "last_group",
        "bearing"
    ];

    planesDB = {};

    planes.values.forEach((v) => {
        const p = Object.fromEntries(keys.map((k, i) => [k, v[i]]));

        p.shipclass = ShippingClass.PLANE;

        p.validated = 1;
        p.name = p.callsign || p.hexident;

        // Store in database 
        const entry = {};
        entry.raw = p;
        planesDB[p.hexident] = entry;
    });


    return true;
}

function toggleScreenSize() {
    var doc = window.document;
    var docEl = doc.documentElement;

    var requestFullScreen = docEl.requestFullscreen || docEl.mozRequestFullScreen || docEl.webkitRequestFullScreen || docEl.msRequestFullscreen || docEl.webkitEnterFullscreen;
    var cancelFullScreen = doc.exitFullscreen || doc.mozCancelFullScreen || doc.webkitExitFullscreen || doc.msExitFullscreen || doc.webkitExitFullscreen;

    if (!doc.fullscreenElement && !doc.mozFullScreenElement && !doc.webkitFullscreenElement && !doc.msFullscreenElement) {
        requestFullScreen.call(docEl);
    } else {
        cancelFullScreen.call(doc);
    }
}

function addShipcardItem(icon, txt, title, onclick, contextType = 'ship') {
    const div = document.createElement("div");
    div.title = title;
    div.setAttribute("data-context-type", contextType);
    if (icon.startsWith("fa")) {
        icon = "question_mark";
    }
    div.innerHTML = '<i class="' + icon + '_icon"></i><span>' + txt + "</span>";
    div.setAttribute("onclick", onclick);
    document.getElementById("shipcard_footer").appendChild(div);
}

function hideMenu() {
    if (document.getElementById("menubar").classList.contains("visible") && !isAndroid()) {
        toggleMenu();
    }
}

function showMenu() {
    if (!document.getElementById("menubar").classList.contains("visible")) {
        toggleMenu();
    }
}

function hideMenuifSmall() {
    hideMenu();
}

function toggleMenu() {
    document.getElementById("menubar").classList.toggle("visible");
    document.getElementById("menubar_mini").classList.toggle("showflex");
    document.getElementById("menubar_mini").classList.toggle("hide");

    var menuButton = document.getElementById("header_menu_button");
    menuButton.classList.toggle("menu_icon");
    menuButton.classList.toggle("close_icon");
}

function initFullScreen() {
    document.addEventListener("fullscreenchange", handleFullScreenChange);
    document.addEventListener("mozfullscreenchange", handleFullScreenChange);
    document.addEventListener("webkitfullscreenchange", handleFullScreenChange);
    document.addEventListener("msfullscreenchange", handleFullScreenChange);
}

function handleFullScreenChange() {
    var icon = document.getElementById("screentoggle-id");
    if (document.fullscreenElement) {
        icon.innerHTML = "fullscreen_exit";
    } else {
        icon.innerHTML = "fullscreen";
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

    let sinLat2 = sinLat * cos100R + cosLat * sin100R * Math.cos(rheading);
    let lat2 = Math.asin(sinLat2);
    let deltaLon = Math.atan2(Math.sin(rheading) * sin100R * cosLat, cos100R - sinLat * sinLat2);

    return [(lat2 * radInv - coordinate[1]) / 100, (deltaLon * radInv) / 100];
}

function calcMove(coordinate, delta, distance) {
    return [coordinate[0] + delta[1] * distance, coordinate[1] + delta[0] * distance];
}

function setMapSetting(a, v) {
    settings[a] = v;
    saveSettings();
    redrawMap();
}

function average(d) {
    const b = d.chart.data.datasets[0].data;
    if (b.length == 1) return b[0].y;

    let start = 0;
    if (d.chart.data.datasets.length > 1) start = 1;

    var c = 0;

    for (a = 0; a < b.length; a++) {
        if (b[a].x != 0) {
            for (i = start; i < d.chart.data.datasets.length; i++) {
                if (!d.chart.getDatasetMeta(i).hidden) {
                    c += d.chart.data.datasets[i].data[a].y;
                }
            }
        }
    }

    return c / (b.length - 1);
}


const graph_annotation = {
    type: "line",
    borderColor: "rgba(12,118,170)",
    borderDash: [6, 6],
    borderDashOffset: 0,
    borderWidth: 3,
    scaleID: "y",
    value: (a) => average(a),
};

const graph_options_count = {
    responsive: true,
    plugins: {
        legend: {
            display: true,
        },
        annotation: {
            annotations: {
                graph_annotation,
            },
        },
    },
    elements: {
        point: {
            radius: 1,
        },
    },
    animation: false,
    tooltips: {
        mode: "index",
        intersect: false,
    },
    hover: {
        mode: "index",
        intersect: false,
    },
    scales: {
        y: {
            stacked: true,
            beginAtZero: true,
            ticks: {
                display: true,
                align: 'center'  // Aligns tick labels
            },
            grid: {
                display: true,  // Only the primary axis should display grid lines
            },
            title: {
                display: true,
                text: 'Message Count',
                font: {
                    size: 12
                },
                color: '#666'
            }
        },
        y_right: {
            stacked: true,
            beginAtZero: true,
            ticks: {
                display: true,
                align: 'center'  // Aligns tick labels
            },
            grid: {
                display: false,  // Disable grid lines on the secondary axis
            },
            position: 'right',
            title: {
                display: true,
                text: 'Vessel Count',
                font: {
                    size: 12
                },
                color: '#666'
            }
        },
        x: {
            ticks: {
                display: true
            },
            title: {
                display: true,
                text: 'Time',
                font: {
                    size: 16
                },
                color: '#666'
            }
        },
    }

};

const graph_options_single = {
    responsive: true,
    plugins: {
        legend: {
            display: false,
        },
        annotation: {
            annotations: {
                graph_annotation,
            },
        },
    },
    elements: {
        point: {
            radius: 1,
        },
    },
    animation: false,
    tooltips: {
        mode: "index",
        intersect: false,
    },
    hover: {
        mode: "index",
        intersect: false,
    },
    scales: {
        y: {
            stacked: true,
            beginAtZero: true,
            ticks: {
                display: true,
            },
        },
        x: {
            ticks: {
                display: true,
            },
        },
    },
};

const graph_options_level = {
    responsive: true,
    plugins: {
        legend: {
            display: false,
        },
    },
    elements: {
        point: {
            radius: 1,
        },
    },
    animation: false,
    tooltips: {
        mode: "index",
        intersect: false,
    },
    hover: {
        mode: "index",
        intersect: false,
    },
    scales: {
        y: {
            beginAtZero: true,
            ticks: {
                display: true,
            },
        },
        x: {
            ticks: {
                display: true,
            },
        },
    },
};

const graph_options_distance = {
    responsive: true,
    plugins: {
        legend: {
            display: true,
        },
    },
    elements: {
        point: {
            radius: 1,
        },
    },
    animation: false,
    tooltips: {
        mode: "index",
        intersect: false,
    },
    hover: {
        mode: "index",
        intersect: false,
    },
    scales: {
        y: {
            beginAtZero: true,
            ticks: {
                display: true,
            },
        },
        x: {
            ticks: {
                display: true,
            },
        },
    },
};

plot_count = {
    type: "scatter",
    data: {
        datasets: [
            {
                label: "Vessel Count",
                data: [],
                type: "line",
                showLine: true,
                fill: false,
                pointStyle: false,
                borderWidth: 2,
                yAxisID: 'y_right', // correctly reference the right y-axis
            },
            {
                label: "Class A",
                data: [],
                showLine: true,
                fill: "origin",
                borderWidth: 2,
                pointStyle: false,
            },
            {
                label: "Class B",
                data: [],
                showLine: true,
                fill: "-1",
                borderWidth: 2,
                pointStyle: false,
            },
            {
                label: "Base Station",
                data: [],
                showLine: true,
                fill: "-1",
                borderWidth: 2,
                pointStyle: false,
            },
            {
                label: "Other",
                data: [],
                showLine: true,
                fill: "-1",
                borderWidth: 2,
                pointStyle: false,
            },
        ],
    },
    options: graph_options_count,
};

plot_distance = {
    type: "scatter",
    data: {
        datasets: [
            {
                label: "NE",
                data: [],
                showLine: true,
                pointStyle: false,
                borderWidth: 2,
            },
            {
                label: "SE",
                data: [],
                showLine: true,
                pointStyle: false,
                borderWidth: 2,
            },
            {
                label: "SW",
                data: [],
                showLine: true,
                pointStyle: false,
                borderWidth: 2,
            },
            {
                label: "NW",
                data: [],
                showLine: true,
                pointStyle: false,
                borderWidth: 2,
            },
            {
                label: "Max",
                data: [],
                showLine: true,
                backgroundColor: "rgb(211,211,211,0.9)",
                fill: true,
                pointStyle: false,
                borderWidth: 0,
            },
        ],
    },
    options: graph_options_distance,
};

plot_single = {
    type: "scatter",
    data: {
        datasets: [
            {
                label: "Data",
                data: [],
                showLine: true,
                pointStyle: false,
                borderWidth: 2,
            },
        ],
    },
    options: graph_options_single,
};

plot_level = {
    type: "scatter",
    data: {
        datasets: [
            {
                label: "Max",
                data: [],
                showLine: true,
                pointStyle: false,
                borderWidth: 1,
            },
            {
                label: "Min",
                data: [],
                showLine: true,
                pointStyle: false,
                borderWidth: 1,
                fill: "-1",
            },
        ],
    },
    options: graph_options_level,
};

var plot_radar = {
    type: "polarArea",
    animation: false,
    responsive: true,
    plugins: {
        legend: {
            display: true,
        },
    },
    data: {
        datasets: [
            {
                label: "Class B",
                borderWidth: 1,
            },
            {
                label: "Class A",
                borderWidth: 1,
            },
        ],
    },
    options: {
        legend: {
            display: false,
        },
        scale: {
            ticks: {
                min: 0,
            },
        },
    },
};

function cssvar(name) {
    return getComputedStyle(document.body).getPropertyValue(name);
}

function updateChartColors(c, colorVariables) {
    c.data.datasets.forEach((dataset, index) => {
        const color = cssvar(colorVariables[index]);

        dataset.backgroundColor = color;
        dataset.borderColor = color;
    });

    c.options.scales.x.ticks.color = cssvar("--chart-color");
    c.options.scales.x.grid.color = cssvar("--chart-grid-color");

    c.options.scales.y.ticks.color = cssvar("--chart-color");
    c.options.scales.y.grid.color = cssvar("--chart-grid-color");

    c.options.plugins.legend.labels.color = cssvar("--chart-color");
}

function updateColorMulti(c) {
    const colorVariables = ["--chart4-color", "--chart1-color", "--chart2-color", "--chart5-color", "--chart6-color"];
    updateChartColors(c, colorVariables);
}

function updateColorSingle(c) {
    const colorVariables = ["--chart4-color", "--chart1-color", "--chart2-color", "--chart5-color", "--chart6-color"];
    updateChartColors(c, colorVariables);
}

function updateColorRadar(c) {
    const colorVariables = ["--chart2-color", "--chart4-color", "--chart2-color", "--chart5-color", "--chart4-color"];
    c.data.datasets.forEach((dataset, index) => {
        const color = cssvar(colorVariables[index]);

        dataset.backgroundColor = color;
        dataset.borderColor = color;
    });

    c.options.scales.r.grid.color = cssvar("--chart-grid-color");
    c.options.scales.r.ticks.color = cssvar("--chart-color");
    c.options.scales.r.ticks.backdropColor = cssvar("--panel-color");
}

function cloneChartConfig(config) {
    const clone = JSON.parse(JSON.stringify(config));

    if (clone.options?.plugins?.annotation?.annotations?.graph_annotation) {
        clone.options.plugins.annotation.annotations.graph_annotation.value =
            (a) => average(a);
    }
    return clone;
}

function initPlots() {
    if (typeof Chart === "undefined") return;

    const chartConfigs = [
        { varName: "chart_radar_hour", id: "chart-radar-hour", ctx: "2d", config: plot_radar, clone: true },
        { varName: "chart_radar_day", id: "chart-radar-day", ctx: "2d", config: plot_radar, clone: true },
        { varName: "chart_seconds", id: "chart-seconds", config: plot_count, clone: true },
        { varName: "chart_minutes", id: "chart-minutes", config: plot_count, clone: true },
        { varName: "chart_hours", id: "chart-hours", config: plot_count, clone: true },
        { varName: "chart_days", id: "chart-days", config: plot_count, clone: true },
        { varName: "chart_ppm", id: "chart-ppm", config: plot_single, clone: true },
        { varName: "chart_ppm_minute", id: "chart-ppm-minute", config: plot_single, clone: true },
        //{ varName: "chart_level", id: "chart-level", config: plot_level, clone: false },
        { varName: "chart_distance_hour", id: "chart-distance-hour", config: plot_distance, clone: false },
        { varName: "chart_distance_day", id: "chart-distance-day", config: plot_distance, clone: false },
        { varName: "chart_minute_vessel", id: "chart-vessels-minute", config: plot_single, clone: false }
    ];

    for (const { varName, id, ctx, config, clone } of chartConfigs) {
        try {
            const canvas = document.getElementById(id);
            if (!canvas) {
                console.warn(`Canvas element not found: ${id}`);
                continue;
            }

            const context = ctx === "2d" ? canvas.getContext("2d") : canvas;
            const chartConfig = clone ? cloneChartConfig(config) : config;

            window[varName] = new Chart(context, chartConfig);
        } catch (error) {
            console.error(`Failed to initialize chart ${id}:`, error);
        }
    }
    chart_level = new Chart(document.getElementById("chart-level"), plot_level);
    chart_level_hour = new Chart(document.getElementById("chart-level-hour"), plot_level);
}

function shipcardismax() {
    return document.getElementById("shipcard").classList.contains("shipcard-ismax");
}

function shipcardselect(e) {
    if (shipcardismax()) {
        e.classList.toggle("shipcard-max-only");
        e.classList.toggle("shipcard-row-selected");
    } else toggleShipcardSize();

    saveSettings();
}

function toggleShipcardSize() {
    Array.from(document.getElementsByClassName("shipcard-min-only")).forEach((e) => e.classList.toggle("visible"));
    Array.from(document.getElementsByClassName("shipcard-max-only")).forEach((e) => e.classList.toggle("hide"));

    document.getElementById("shipcard").classList.toggle("shipcard-ismax");
    document.getElementById("shipcard_minmax_button").classList.toggle("keyboard_arrow_down_icon");
    document.getElementById("shipcard_minmax_button").classList.toggle("keyboard_arrow_up_icon");

    var e = document.getElementById("shipcard_content").children;

    if (shipcardismax()) {
        for (let i = 0; i < e.length; i++) {
            if (
                (e[i].classList.contains("shipcard-max-only") && e[i].classList.contains("shipcard-row-selected")) ||
                (!e[i].classList.contains("shipcard-max-only") && !e[i].classList.contains("shipcard-row-selected"))
            )
                e[i].classList.toggle("shipcard-row-selected");

            const aside = document.getElementById("shipcard");
            if (aside.style.top && aside.getBoundingClientRect().bottom > window.innerHeight) {

                if (card_mmsi in shipsDB && card_type == "ship") {
                    let pixel = map.getPixelFromCoordinate(ol.proj.fromLonLat([shipsDB[card_mmsi].raw.lon, shipsDB[card_mmsi].raw.lat]));
                    positionAside(pixel, aside);
                }
                else if (card_mmsi in planesDB && card_type == "plane") {
                    let pixel = map.getPixelFromCoordinate(ol.proj.fromLonLat([planesDB[card_mmsi].raw.lon, planesDB[card_mmsi].raw.lat]));
                    positionAside(pixel, aside);
                }
            }
        }
    } else {
        for (let i = 0; i < e.length; i++) {
            if (e[i].classList.contains("shipcard-row-selected")) e[i].classList.toggle("shipcard-row-selected");
        }
    }
}

function setPulseOk() {
    document.getElementById("pulse-id").className = "header-pulse-ok";

    const logoControl = document.getElementById('aiscatcher-logo-control');
    if (logoControl) {
        logoControl.classList.remove('connected', 'disconnected');
        logoControl.classList.add('connected');
        logoControl.title = `AIS-catcher - Connected`;
    }
}

function setPulseError() {
    document.getElementById("pulse-id").className = "header-pulse-error";

    const logoControl = document.getElementById('aiscatcher-logo-control');
    if (logoControl) {
        logoControl.classList.remove('connected', 'disconnected');
        logoControl.classList.add('disconnected');
        logoControl.title = `AIS-catcher - Disconnected`;
    }
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

// fetches main statistics from the server
async function fetchStatistics() {
    try {
        response = await fetch("api/stat.json");
    } catch (error) {
        setPulseError();
        return;
    }
    statistics = await response.json();
    setPulseOk();
    return statistics;
}

function updateStat(stat, tf) {
    [0, 1, 2, 3].forEach((e) => (document.getElementById("stat_" + tf + "_channel" + e).innerText = stat[tf].channel[e].toLocaleString()));

    document.getElementById("stat_" + tf + "_count").innerText = stat[tf].count.toLocaleString();
    document.getElementById("stat_" + tf + "_dist").innerText = getDistanceVal(stat[tf].dist) + " " + getDistanceUnit();
    document.getElementById("stat_" + tf + "_vessel_count").innerText = stat[tf].vessels.toLocaleString();
    document.getElementById("stat_" + tf + "_msg123").innerText = (stat[tf].msg[0] + stat[tf].msg[1] + stat[tf].msg[2]).toLocaleString();
    document.getElementById("stat_" + tf + "_msg5").innerText = stat[tf].msg[4].toLocaleString();
    document.getElementById("stat_" + tf + "_msg18").innerText = stat[tf].msg[17].toLocaleString();
    document.getElementById("stat_" + tf + "_msg19").innerText = stat[tf].msg[18].toLocaleString();
    document.getElementById("stat_" + tf + "_msg68").innerText = (stat[tf].msg[5] + stat[tf].msg[7]).toLocaleString();
    document.getElementById("stat_" + tf + "_msg1214").innerText = (stat[tf].msg[11] + stat[tf].msg[13]).toLocaleString();
    document.getElementById("stat_" + tf + "_msg24").innerText = stat[tf].msg[23].toLocaleString();
    document.getElementById("stat_" + tf + "_msg4").innerText = stat[tf].msg[3].toLocaleString();
    document.getElementById("stat_" + tf + "_msg9").innerText = stat[tf].msg[8].toLocaleString();
    document.getElementById("stat_" + tf + "_msg21").innerText = stat[tf].msg[20].toLocaleString();
    document.getElementById("stat_" + tf + "_msg27").innerText = stat[tf].msg[26].toLocaleString();

    var count_other = 0;
    [7, 10, 11, 13, 15, 16, 17, 20, 22, 23, 25, 26].forEach((i) => (count_other += stat[tf].msg[i - 1]));
    document.getElementById("stat_" + tf + "_msgother").innerText = count_other.toLocaleString();
}

async function updateStatistics() {
    var stat = await fetchStatistics();

    if (stat) {
        // in bulk....
        ["os", "tcp_clients", "hardware", "build_describe", "build_date", "station", "product", "vendor", "serial", "model", "sample_rate", "received"].forEach(
            (e) => (document.getElementById("stat_" + e).innerHTML = stat[e]),
        );

        if (stat.station_link != "") document.getElementById("stat_station").innerHTML = "<a href='" + stat.station_link + "'>" + stat.station + "</a>";

        var statSharingElement = document.getElementById("stat_sharing");

        statSharingElement.innerHTML = `<a href="${stat.sharing_link}" target="_blank">${stat.sharing ? 'Yes' : 'No'}</a>`;
        statSharingElement.style.color = stat.sharing ? "green" : "red";


        document.getElementById("stat_update_time").textContent = Number(refreshIntervalMs / 1000).toFixed(1) + " s";
        var title = document.getElementById("stat_station").textContent;
        if (title != "" && title != null) {
            tab_title_station = title;
            updateTitle();
        }
        document.getElementById("stat_memory").innerText = stat.memory ? Number(stat.memory / 1000000).toFixed(1) + " MB" : "N/A";
        document.getElementById("stat_msg_rate").innerText = Number(stat.msg_rate).toFixed(1) + " msg/s";
        document.getElementById("stat_msg_min_rate").innerText = Number(stat.last_minute.count).toFixed(0) + " msg/min";
        document.getElementById("stat_run_time").innerHTML = getDeltaTimeVal(stat.run_time);

        updateStat(stat, "total");
        updateStat(stat, "session");
        updateStat(stat, "last_minute");
        updateStat(stat, "last_hour");
        updateStat(stat, "last_day");

        document.getElementById("stat_total_vessel_count").innerText = "-";
        document.getElementById("stat_session_vessel_count").innerText = stat.vessel_count;
    }
}

function updateChartMulti(b, f, c) {
    if (b.hasOwnProperty(f)) {
        var hA = [];
        var hB = [];
        var hT = [];
        var hS = [];
        var hV = [];


        const source = b[f];
        for (let i = 0; i < source.time.length; i++) {
            let cA = source.stat[i].msg[0] + source.stat[i].msg[1] + source.stat[i].msg[2] + source.stat[i].msg[4];
            hA.push({ x: source.time[i], y: cA });
            let cB = source.stat[i].msg[17] + source.stat[i].msg[18] + source.stat[i].msg[23];
            hB.push({ x: source.time[i], y: cB });
            let cS = source.stat[i].msg[3];
            hS.push({ x: source.time[i], y: cS });
            let cT = source.stat[i].count - cA - cB - cS;
            hT.push({ x: source.time[i], y: cT });
            let cV = source.stat[i].vessels;
            hV.push({ x: source.time[i], y: cV });
        }

        c.data.datasets[0].data = hV;
        c.data.datasets[1].data = hA;
        c.data.datasets[2].data = hB;
        c.data.datasets[3].data = hS;
        c.data.datasets[4].data = hT;

        c.update();
    }
}

function updateChartDistance(b, f, c) {
    if (b.hasOwnProperty(f)) {
        var hNE = [];
        var hSE = [];
        var hSW = [];
        var hNW = [];
        var hM = [];

        let source = b[f];

        if (source.stat[0].radar_a.length == 0) return;
        let N = source.stat[0].radar_a.length;
        let N4 = N / 4;
        for (let i = 0; i < source.time.length; i++) {
            let count = [0, 0, 0, 0];
            for (let j = 0; j < N; j++) count[Math.floor(j / N4)] = Math.max(count[Math.floor(j / N4)], source.stat[i].radar_a[j]);

            hNE.push({ x: source.time[i], y: getDistanceConversion(count[0]) }); // source.stat[i].radar[0]
            hSE.push({ x: source.time[i], y: getDistanceConversion(count[1]) });
            hSW.push({ x: source.time[i], y: getDistanceConversion(count[2]) });
            hNW.push({ x: source.time[i], y: getDistanceConversion(count[3]) });
            hM.push({ x: source.time[i], y: getDistanceConversion(source.stat[i].dist) });
        }
        c.data.datasets[0].data = hNE;
        c.data.datasets[1].data = hSE;
        c.data.datasets[2].data = hSW;
        c.data.datasets[3].data = hNW;
        c.data.datasets[4].data = hM;

        c.update();
    }
}

function updateChartSingle(b, f1, f2, c) {
    if (b.hasOwnProperty(f1)) {
        var h = [];

        const source = b[f1];
        for (let i = 0; i < source.time.length; i++) {
            h.push({ x: source.time[i], y: source.stat[i][f2] });
        }
        c.data.datasets[0].data = h;
        c.update();
    }
}

/*
function updateChartLevel(b, f1, f2, c) {
    if (b.hasOwnProperty(f1)) {
        const source = b[f1];

        var h = [];

        for (let i = 0; i < source.time.length; i++) {
            h.push({ x: source.time[i], y: source.stat[i].level_min == 0 ? null : source.stat[i].level_min });
        }
        c.data.datasets[1].data = h;

        var h = [];
        for (let i = 0; i < source.time.length; i++) {
            h.push({ x: source.time[i], y: source.stat[i].level_max == 0 ? null : source.stat[i].level_max });
        }
        c.data.datasets[0].data = h;

        c.update();
    }
}
*/

function updateChartLevel(chartData, timeframe, chartName, chart) {
    if (!chartData?.hasOwnProperty(timeframe)) {
        return;
    }

    const timeSeriesData = chartData[timeframe];

    const minLevelData = [];
    for (let i = 0; i < timeSeriesData.time.length; i++) {
        minLevelData.push({
            x: timeSeriesData.time[i],
            y: timeSeriesData.stat[i].level_min === 0 ? null : timeSeriesData.stat[i].level_min
        });
    }
    chart.data.datasets[1].data = minLevelData;

    const maxLevelData = [];
    for (let i = 0; i < timeSeriesData.time.length; i++) {
        maxLevelData.push({
            x: timeSeriesData.time[i],
            y: timeSeriesData.stat[i].level_max === 0 ? null : timeSeriesData.stat[i].level_max
        });
    }
    chart.data.datasets[0].data = maxLevelData;

    chart.update();
}

function updateRadar(b, f, c) {
    if (b.hasOwnProperty(f)) {
        var data_a = [],
            data_b = [];
        let idx = 0;
        if (b[f].stat.length > 1) idx = 1;
        let N = b[f].stat[idx].radar_a.length;
        for (var i = 0; i < N; i++) {
            data_a.push({
                r: getDistanceConversion(b[f].stat[idx].radar_a[i]),
                theta: (i * 360) / N,
            });
            data_b.push({
                r: getDistanceConversion(b[f].stat[idx].radar_b[i]),
                theta: (i * 360) / N,
            });
        }
        c.data.datasets[0].data = data_b;
        c.data.datasets[1].data = data_a;
        c.update();
    }
}

async function updatePlots() {
    const unit = getDistanceUnit().toUpperCase();
    document.querySelectorAll(".distunit").forEach((u) => {
        u.textContent = unit;
    });

    if (true) {
        try {
            response = await fetch("api/history_full.json");
        } catch (error) {
            setPulseError();
        }
        b = await response.json();
    } else {
        b = JSON.parse(chart_json);
    }
    setPulseOk();

    updateChartMulti(b, "second", chart_seconds);
    updateChartMulti(b, "minute", chart_minutes);
    updateChartMulti(b, "hour", chart_hours);
    updateChartMulti(b, "day", chart_days);

    updateChartSingle(b, "minute", "ppm", chart_ppm_minute);
    updateChartSingle(b, "hour", "ppm", chart_ppm);
    updateChartLevel(b, "minute", "level", chart_level);
    updateChartLevel(b, "hour", "level", chart_level_hour);
    updateChartSingle(b, "minute", "vessels", chart_minute_vessel);

    //plot_radar.options.ticks.scale.max = 200;
    updateChartDistance(b, "hour", chart_distance_hour);
    updateChartDistance(b, "day", chart_distance_day);

    updateRadar(b, "hour", chart_radar_hour);
    updateRadar(b, "day", chart_radar_day);
}

function tableRowClick(m) {
    ship = shipsDB[m].raw;
    if (ship.lat == null || ship.lon == null) return;

    selectMapTab(m);
}

var table = null;

function downloadCSV() {
    if (table) table.download("csv", "data.csv");
}

document.getElementById("shipSearch").addEventListener("input", function (e) {
    const query = e.target.value;
    searchShips(query);
});

function searchShips(query) {
    if (table) {
        table.setFilter(customShipFilter, { query: query });
    }
}

function customShipFilter(data, filterParams) {
    const query = filterParams.query.toLowerCase();
    return data.shipname.toLowerCase().includes(query) ||
        data.mmsi.toString().includes(query) ||
        data.callsign.toLowerCase().includes(query) ||
        data.shipclass.toString().includes(query) ||
        (data.last_signal != null && getDeltaTimeVal(data.last_signal).includes(query)) ||
        (data.count != null && data.count.toString().includes(query)) ||
        (data.ppm != null && data.ppm.toString().includes(query)) ||
        (data.level != null && data.level.toString().includes(query)) ||
        (data.distance != null && getDistanceVal(data.distance).includes(query)) ||
        (data.bearing != null && data.bearing.toString().includes(query)) ||
        (data.lat != null && getLatValFormat(data).includes(query)) ||
        (data.lon != null && getLonValFormat(data).includes(query)) ||
        (data.speed != null && getSpeedVal(data.speed).toString().includes(query)) ||
        (data.validated != null && (data.validated == 1 ? "yes" : data.validated == -1 ? "no" : "pending").includes(query)) ||
        (data.channels != null && getStringfromChannels(data.channels).includes(query)) ||
        (data.group_mask != null && getStringfromGroup(data.group_mask).includes(query));
}

let tableFirstTime = true;

async function updateShipTable() {
    const ok = await fetchShips();
    if (!ok) return;

    const data = Object.values(shipsDB).map((ship) => ship.raw);

    if (table == null) {
        table = new Tabulator("#shipTable", {
            index: "mmsi",
            rowFormatter: function (row) {
                const ship = row.getData();
                const borderColor = ship.validated == 1 ? "#7CFC00" : ship.validated == -1 ? "red" : "lightgrey";
                row.getElement().style.borderLeft = `10px solid ${borderColor}`;
            },
            data: data,
            persistence: true,
            pagination: "local",
            paginationSize: 50,
            rowHeight: 37,
            paginationSizeSelector: [20, 50, 100],
            columns: [
                {
                    title: "Shipname",
                    field: "shipname",
                    sorter: "string",
                    formatter: function (cell) {
                        const ship = cell.getRow().getData();
                        return getFlag(ship.country) + getShipName(ship);
                    },
                },
                { title: "MMSI", field: "mmsi", sorter: "number" },
                { title: "IMO", field: "imo", sorter: "number" },
                { title: "Dest", field: "destination", sorter: "string" },
                {
                    title: "ETA",
                    field: "eta",
                    sorter: "string",
                    formatter: function (cell) {
                        const ship = cell.getRow().getData();
                        return ship.eta_month != null && ship.eta_hour != null && ship.eta_day != null && ship.eta_minute != null ? getEtaVal(ship) : null;
                    },
                },
                {
                    title: "Callsign",
                    field: "callsign",
                    sorter: "string",
                    formatter: function (cell) {
                        const ship = cell.getRow().getData();
                        return ship != null ? getCallSign(ship) : "";
                    },
                },
                {
                    title: "Flag",
                    field: "country",
                    sorter: "string",
                    formatter: function (cell) {
                        const ship = cell.getRow().getData();
                        return ship != null ? getCountryName(ship.country) : "";
                    },
                },
                {
                    title: "Status",
                    field: "status",
                    sorter: "string",
                    formatter: function (cell) {
                        const ship = cell.getRow().getData();
                        return ship != null ? getStatusVal(ship) : "";
                    },
                },
                {
                    title: "Type",
                    field: "shipclass",
                    sorter: "number",
                    formatter: function (cell) {
                        const ship = cell.getRow().getData();
                        return getTableShiptype(ship);
                    },
                },
                {
                    title: "Class",
                    field: "class",
                    sorter: "string",
                    formatter: function (cell) {
                        const ship = cell.getRow().getData();
                        return getTypeVal(ship);
                    },
                },
                {
                    title: "Dim",
                    field: "dimension",
                    sorter: "number",
                    formatter: function (cell) {
                        const ship = cell.getRow().getData();
                        return ship ? getShipDimension(ship) || "" : "";
                    },
                },
                {
                    title: "Draught",
                    field: "draught",
                    sorter: "number",
                    formatter: function (cell) {
                        const ship = cell.getRow().getData();
                        return ship.draught ? getDimVal(ship.draught) + " " + getDimUnit() : null;
                    },
                },
                {
                    title: "Last",
                    field: "last_signal",
                    sorter: "number",
                    formatter: function (cell) {
                        const value = cell.getValue();
                        return value != null ? getDeltaTimeVal(value) : "";
                    },
                },
                { title: "Count", field: "count", sorter: "number" },
                {
                    title: "PPM",
                    field: "ppm",
                    sorter: "number",
                    formatter: function (cell) {
                        const value = cell.getValue();
                        return value != null ? Number(value).toFixed(1) : "";
                    },
                },
                {
                    title: "RSSI",
                    field: "level",
                    sorter: "number",
                    formatter: function (cell) {
                        const value = cell.getValue();
                        return value != null ? Number(value).toFixed(1) : "";
                    },
                },
                {
                    title: "Dist",
                    field: "distance",
                    sorter: "number",
                    formatter: function (cell) {
                        const ship = cell.getRow().getData();
                        return (ship != null && ship.distance != null)
                            ? getDistanceVal(ship.distance) + (ship.repeat > 0 ? " (R)" : "")
                            : "";
                    },
                },
                {
                    title: "Brg",
                    field: "bearing",
                    sorter: "number",
                    formatter: function (cell) {
                        const value = cell.getValue();
                        return value != null ? Number(value).toFixed(0) + "&deg;" : "";
                    },
                },
                {
                    title: "Lat",
                    field: "lat",
                    sorter: "number",
                    formatter: function (cell) {
                        const ship = cell.getRow().getData();
                        const color = ship.validated == 1 ? "green" : ship.validated == -1 ? "red" : "inherited";
                        return "<div style='color:" + color + "'>" + (ship.lat != null ? getLatValFormat(ship) : "") + "</div>";
                    },
                },
                {
                    title: "Lon",
                    field: "lon",
                    sorter: "number",
                    formatter: function (cell) {
                        const ship = cell.getRow().getData();
                        const color = ship.validated == 1 ? "green" : ship.validated == -1 ? "red" : "inherited";
                        return "<div style='color:" + color + "'>" + (ship.lon != null ? getLonValFormat(ship) : "") + "</div>";
                    },
                },
                {
                    title: "Spd",
                    field: "speed",
                    sorter: "number",
                    formatter: function (cell) {
                        const value = cell.getValue();
                        return value ? getSpeedVal(value) + " " + getSpeedUnit() : null;
                    },
                },
                {
                    title: "Valid",
                    field: "validated",
                    sorter: "string",
                    formatter: function (cell) {
                        const value = cell.getValue();
                        const txt = value == 1 ? "Yes" : value == -1 ? "No" : "Pending";
                        return txt;
                    },
                },
                {
                    title: "Repeated",
                    field: "repeat",
                    sorter: "string",
                    formatter: function (cell) {
                        const value = cell.getValue();
                        return value == 1 ? "Yes" : "No";
                    },
                },
                {
                    title: "Ch",
                    field: "channels",
                    sorter: "number",
                    formatter: function (cell) {
                        const value = cell.getValue();
                        return getStringfromChannels(value);
                    },
                },
                {
                    title: "Msg Type",
                    field: "msg_type",
                    sorter: "number",
                    formatter: function (cell) {
                        const value = cell.getValue();
                        return getStringfromMsgType(value);
                    },
                },
                {
                    title: "MSG6",
                    field: "MSG6",
                    sorter: "number",
                    formatter: function (cell) {
                        const value = cell.getValue();
                        return value & (1 << 6) ? "Yes" : "No";
                    },
                },
                {
                    title: "MSG8",
                    field: "MSG8",
                    sorter: "number",
                    formatter: function (cell) {
                        const value = cell.getValue();
                        return value & (1 << 8) ? "Yes" : "No";
                    },
                },
                {
                    title: "MSG27",
                    field: "MSG27",
                    sorter: "number",
                    formatter: function (cell) {
                        const value = cell.getValue();
                        return value & (1 << 27) ? "Yes" : "No";
                    },
                },
                {
                    title: "Src",
                    field: "group_mask",
                    sorter: "number",
                    formatter: function (cell) {
                        const value = cell.getValue();
                        return getStringfromGroup(value);
                    },
                },
            ],
        });
        table.on("rowContext", function (e, row) {
            showContextMenu(e, row.getData().mmsi, "ship", ["settings", "ship", "table-menu"]);
        });
        table.on("rowClick", function (e, row) {
            tableRowClick(row.getData().mmsi);
        });
        table.on("tableBuilt", function () {
            if (tableFirstTime) {
                setShipTableColumns();
                tableFirstTime = false;
            }

            populateShipTableColumnVisibilityMenu();
        });
    } else {
        if (data.length == 0) {
            table.clearData();
            return;
        }
        table.updateOrAddData(data);
        table.getRows().forEach((row) => {
            const mmsi = row.getData().mmsi;
            if (!(mmsi in shipsDB)) row.delete();
        });
        var sorters = table.getSorters();
        table.setSort(sorters);
    }
}

function populateShipTableColumnVisibilityMenu() {
    const shipTableColumnVisibilityMenu = document.getElementById("shipTableColumnVisibilityMenu");
    shipTableColumnVisibilityMenu.innerHTML = "";

    table.getColumns().forEach((column) => {
        const checkboxId = `checkbox-${column.getField()}`;
        const checkboxLabel = column.getDefinition().title;

        const checkboxItem = document.createElement("div");
        checkboxItem.classList.add("ship-table-column-dropdown-item");

        const checkbox = document.createElement("input");
        checkbox.type = "checkbox";
        checkbox.id = checkboxId;
        checkbox.value = column.getField();
        checkbox.checked = column.isVisible();
        checkbox.addEventListener("change", updateShipTableColumnVisibility);

        const label = document.createElement("label");
        label.setAttribute("for", checkboxId);
        label.textContent = checkboxLabel;

        checkboxItem.appendChild(checkbox);
        checkboxItem.appendChild(label);

        shipTableColumnVisibilityMenu.appendChild(checkboxItem);
    });
}

function updateShipTableColumnVisibility() {
    const checkboxes = document.querySelectorAll("#shipTableColumnVisibilityMenu input[type='checkbox']");

    settings.shiptable_columns = Array.from(checkboxes)
        .filter((checkbox) => checkbox.checked)
        .map((checkbox) => checkbox.value);

    setShipTableColumns();
}

function resetShipTableColumns() {
    settings.shiptable_columns = ["shipname", "mmsi", "imo", "callsign", "shipclass", "lat", "lon", "last_signal", "level", "distance", "bearing", "speed", "repeat", "ppm", "status"];
    setShipTableColumns();
}

function setShipTableColumns() {

    saveSettings();

    const allColumns = table.getColumns();

    const updatedColumns = allColumns.map(column => {
        const field = column.getField();
        return {
            ...column.getDefinition(),
            visible: settings.shiptable_columns.includes(field)
        };
    });

    table.setColumns(updatedColumns);
    populateShipTableColumnVisibilityMenu();
}

document.getElementById("shipTableColumnDropdownToggle").addEventListener("click", populateShipTableColumnVisibilityMenu);
const dropdownToggle = document.getElementById('shipTableColumnDropdownToggle');
const dropdownMenu = document.getElementById('shipTableColumnVisibilityMenu');

function toggleDropdown() {
    dropdownMenu.style.display = dropdownMenu.style.display === 'block' ? 'none' : 'block';
}

dropdownToggle.addEventListener('click', function (event) {
    event.stopPropagation();
    toggleDropdown();
});

document.addEventListener('click', function (event) {
    if (!dropdownMenu.contains(event.target) && event.target !== dropdownToggle) {
        dropdownMenu.style.display = 'none';
    }
});

const resetButton = document.getElementById('shipTableColumnReset');
resetButton.addEventListener('click', function (event) {
    resetShipTableColumns();
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
            return "Ship (Class A)";
        case CLASS_B:
            return "Ship (Class B)";
        case BASESTATION:
            return "Base Station";
        case SAR:
            return "SAR aircraft";
        case SARTEPIRB:
            return "AIS SART/EPIRB";
        case ATON:
            return "Aid-to-Navigation";
    }
    return "Unknown";
}

function getShipOpacity(ship) {
    if (settings.fading == false) return 1;

    let opacity = 1 - (ship.last_signal / 1800) * 0.8;
    return Math.max(0.2, Math.min(1, opacity));
}


function getShipCSSClassAndStyle(ship, opacity = 1) {
    getSprite(ship);
    let style = `opacity: ${opacity};`;
    let scale = settings.icon_scale;

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



function updateFocusMarker() {
    if (selectCircleFeature) {
        if (card_type == 'ship' && card_mmsi in shipsDB && shipsDB[card_mmsi].raw.lon && shipsDB[card_mmsi].raw.lat) {
            const center = ol.proj.fromLonLat([shipsDB[card_mmsi].raw.lon, shipsDB[card_mmsi].raw.lat]);
            selectCircleFeature.setGeometry(new ol.geom.Point(center));
            return;
        }
        else if (card_type == 'plane' && card_mmsi in planesDB && planesDB[card_mmsi].raw.lon && planesDB[card_mmsi].raw.lat) {
            const center = ol.proj.fromLonLat([planesDB[card_mmsi].raw.lon, planesDB[card_mmsi].raw.lat]);
            selectCircleFeature.setGeometry(new ol.geom.Point(center));
            return;
        }
        else {
            extraVector.removeFeature(selectCircleFeature);
            selectCircleFeature = undefined;
            return;
        }
    }

    if (card_type == 'ship' && card_mmsi in shipsDB && shipsDB[card_mmsi].raw.lon && shipsDB[card_mmsi].raw.lat) {

        const center = ol.proj.fromLonLat([shipsDB[card_mmsi].raw.lon, shipsDB[card_mmsi].raw.lat]);
        selectCircleFeature = new ol.Feature(new ol.geom.Point(center));
        selectCircleFeature.setStyle(selectCircleStyleFunction);
        selectCircleFeature.mmsi = card_mmsi;
        extraVector.addFeature(selectCircleFeature);
    }
    else if (card_type == 'plane' && card_mmsi in planesDB && planesDB[card_mmsi].raw.lon && planesDB[card_mmsi].raw.lat) {

        const center = ol.proj.fromLonLat([planesDB[card_mmsi].raw.lon, planesDB[card_mmsi].raw.lat]);
        selectCircleFeature = new ol.Feature(new ol.geom.Point(center));
        selectCircleFeature.setStyle(selectCircleStyleFunction);
        selectCircleFeature.mmsi = card_mmsi;
        extraVector.addFeature(selectCircleFeature);
    }
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
            let x = pixel[0] + dist * s + (s < 0 ? -tw : 0);
            let y = pixel[1] + dist * c + (c < 0 ? -th : 0);
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

    let hover_mmsi = hoverMMSI;

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

function getBinaryMessageContent(binary, showMultiple = false) {
    const messages = Array.isArray(binary) ? binary : [binary];

    if (messages.length === 0) return '';

    messages.sort((a, b) => b.timestamp - a.timestamp);

    const messagesToShow = showMultiple ? Math.min(messages.length, 3) : 1;

    let content = '<div class="meteo-tooltip" style="max-width: 320px;">';

    for (let i = 0; i < messagesToShow; i++) {
        const msg = messages[i].message;

        if (i > 0) {
            content += '<hr style="margin: 10px 0; border: 0; border-top: 1px solid rgba(255,255,255,0.3);">';
        }

        // Determine if it's just meteo or also has hydro data
        const hasHydroData =
            ('watercurrent' in msg && msg.watercurrent !== undefined && msg.watercurrent !== null) ||
            ('currentspeed' in msg && msg.currentspeed !== undefined && msg.currentspeed !== null) ||
            ('currentdir' in msg && msg.currentdir !== undefined && msg.currentdir !== null) ||
            ('watertemp' in msg && msg.watertemp !== undefined && msg.watertemp !== null) ||
            ('waterlevel' in msg && msg.waterlevel !== undefined && msg.waterlevel !== null);

        content += `<h4 style="margin: 5px 0; font-size: 12px; color: #eee;">${messages[i].formattedTime} - ${hasHydroData ? 'Meteo & Hydro' : 'Meteo'}`;
        if (messagesToShow > 1) {
            content += ` (${i + 1}/${messagesToShow})`;
        }
        content += '</h4>';

        content += '<div style="display: grid; grid-template-columns: 1fr 1fr; grid-gap: 5px;">';

        // Wind data
        if ('wspeed' in msg && msg.wspeed !== undefined && msg.wspeed !== null) {
            content += '<div style="display: flex; justify-content: space-between; padding: 3px;">';
            content += '<div style="font-size: 11px; color: #ccc;">Wind:</div>';
            content += '<div style="font-size: 11px; font-weight: bold;">' + msg.wspeed.toFixed(1) + ' knots';
            if ('wdir' in msg && msg.wdir !== undefined && msg.wdir !== null && msg.wdir !== 360) {
                content += ' from ' + msg.wdir + '&deg';
            }
            content += '</div></div>';
        }

        // Air temperature
        if ('airtemp' in msg && msg.airtemp !== undefined && msg.airtemp !== null) {
            content += '<div style="display: flex; justify-content: space-between; padding: 3px;">';
            content += '<div style="font-size: 11px; color: #ccc;">Air Temp:</div>';
            content += '<div style="font-size: 11px; font-weight: bold;">' + msg.airtemp.toFixed(1) + '&deg C</div>';
            content += '</div>';
        }

        // Air pressure
        if ('pressure' in msg && msg.pressure !== undefined && msg.pressure !== null) {
            content += '<div style="display: flex; justify-content: space-between; padding: 3px;">';
            content += '<div style="font-size: 11px; color: #ccc;">Pressure:</div>';
            content += '<div style="font-size: 11px; font-weight: bold;">' + msg.pressure.toFixed(1) + ' hPa';
            if ('pressuretend' in msg && msg.pressuretend !== undefined && msg.pressuretend !== null) {
                content += ' (' + ['steady', 'decreasing', 'increasing'][msg.pressuretend] + ')';
            }
            content += '</div></div>';
        }

        // Water current speed and direction
        const currentSpeed = msg.watercurrent || msg.currentspeed;
        const currentDir = msg.currentdir || msg.currentdirection;

        if (currentSpeed !== undefined && currentSpeed !== null) {
            content += '<div style="display: flex; justify-content: space-between; padding: 3px;">';
            content += '<div style="font-size: 11px; color: #ccc;">Current:</div>';
            content += '<div style="font-size: 11px; font-weight: bold;">' + currentSpeed.toFixed(1) + ' knots';
            if (currentDir !== undefined && currentDir !== null && currentDir !== 360) {
                content += ' to ' + currentDir + '&deg';
            }
            content += '</div></div>';
        }

        // Water level
        if ('waterlevel' in msg && msg.waterlevel !== undefined && msg.waterlevel !== null) {
            content += '<div style="display: flex; justify-content: space-between; padding: 3px;">';
            content += '<div style="font-size: 11px; color: #ccc;">Water Level:</div>';
            content += '<div style="font-size: 11px; font-weight: bold;">' + msg.waterlevel.toFixed(2) + ' m</div>';
            content += '</div>';
        }

        // Water temperature
        if ('watertemp' in msg && msg.watertemp !== undefined && msg.watertemp !== null) {
            content += '<div style="display: flex; justify-content: space-between; padding: 3px;">';
            content += '<div style="font-size: 11px; color: #ccc;">Water Temp:</div>';
            content += '<div style="font-size: 11px; font-weight: bold;">' + msg.watertemp.toFixed(1) + '&deg C</div>';
            content += '</div>';
        }

        // Wave data
        if ('waveheight' in msg && msg.waveheight !== undefined && msg.waveheight !== null) {
            content += '<div style="display: flex; justify-content: space-between; padding: 3px;">';
            content += '<div style="font-size: 11px; color: #ccc;">Wave:</div>';
            content += '<div style="font-size: 11px; font-weight: bold;">' + msg.waveheight.toFixed(1) + ' m';
            if ('wavedir' in msg && msg.wavedir !== undefined && msg.wavedir !== null && msg.wavedir !== 360) {
                content += ' from ' + msg.wavedir + '&deg';
            }
            if ('waveperiod' in msg && msg.waveperiod !== undefined && msg.waveperiod !== null) {
                content += ', ' + msg.waveperiod + 's';
            }
            content += '</div></div>';
        }

        // Swell data
        if ('swellheight' in msg && msg.swellheight !== undefined && msg.swellheight !== null) {
            content += '<div style="display: flex; justify-content: space-between; padding: 3px;">';
            content += '<div style="font-size: 11px; color: #ccc;">Swell:</div>';
            content += '<div style="font-size: 11px; font-weight: bold;">' + msg.swellheight.toFixed(1) + ' m';
            if ('swelldir' in msg && msg.swelldir !== undefined && msg.swelldir !== null && msg.swelldir !== 360) {
                content += ' from ' + msg.swelldir + '&deg';
            }
            if ('swellperiod' in msg && msg.swellperiod !== undefined && msg.swellperiod !== null) {
                content += ', ' + msg.swellperiod + 's';
            }
            content += '</div></div>';
        }

        // Visibility
        if ('visibility' in msg && msg.visibility !== undefined && msg.visibility !== null) {
            content += '<div style="display: flex; justify-content: space-between; padding: 3px;">';
            content += '<div style="font-size: 11px; color: #ccc;">Visibility:</div>';
            content += '<div style="font-size: 11px; font-weight: bold;">' + msg.visibility.toFixed(1) + ' nm</div>';
            content += '</div>';
        }

        // End grid
        content += '</div>';
    }

    // If there are more messages than we show
    if (messages.length > messagesToShow) {
        content += `<div style="text-align: center; font-size: 10px; margin-top: 5px; font-style: italic; color: #ccc;"></div>`;
    }

    content += '</div>';
    return content;
}

const startHover = function (type, mmsi, pixel, feature) {

    if (type != 'ship' && type != 'tooltip' && type != 'plane' && type != 'binary') return;

    if (mmsi !== hoverMMSI || hoverType !== type) {
        stopHover();

        hoverMMSI = mmsi;
        hoverType = type;
        hover_feature = feature;

        if (type == 'ship' && (mmsi in shipsDB && shipsDB[mmsi].raw.lon && shipsDB[mmsi].raw.lat)) {
            let tooltip = shipsDB[mmsi]?.raw ? getTooltipContent(shipsDB[mmsi].raw) : mmsi;
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

    let view = map.getView();
    let center = ol.proj.toLonLat(view.getCenter()); // Converts the center coordinates to [lon, lat]
    let newURL = window.location.href.split("?")[0] + "?lat=" + center[1].toFixed(4) + "&lon=" + center[0].toFixed(4) + "&zoom=" + view.getZoom().toFixed(2) + "&tab=" + settings.tab;
    history.replaceState(null, null, newURL);
}


function saveSettings() {
    if (map !== undefined) {
        var view = map.getView();
        var center = ol.proj.toLonLat(view.getCenter()); // Convert the center coordinate to longitude and latitude
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

    localStorage[context] = JSON.stringify(settings);

    updateMapURL();
    updateSettingsTab();
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
                settings = { ...settings, ...ls };
            }
        } catch (error) {
            console.log(error);
            return;
        }
    }
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
    // settings.kiosk = false;
}

function convertStringBooleansToActual() {
    const booleanSettings = [
        'counter', 'fading', 'android', 'kiosk', 'welcome', 'show_range',
        'distance_circles', 'table_shiptype_use_icon', 'fix_center',
        'show_circle_outline', 'dark_mode', 'setcoord', 'eri', 'loadURL',
        'show_station', 'labels_declutter', 'show_track_on_hover',
        'show_track_on_select', 'shipcard_max', 'kiosk_pan_map'
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
        let ship = shipsDB[m].raw;
        let view = map.getView();
        view.setCenter(ol.proj.fromLonLat([ship.lon, ship.lat]));
        view.setZoom(Math.min(view.getMaxZoom(), Math.max(z, view.getZoom() + 1)));
    }

    shipcardMinIfMaxonMobile();
}

function mapResetView(z) {

    let view = map.getView();
    view.setZoom(Math.min(view.getMaxZoom(), Math.max(z, view.getZoom() + 1)));
    shipcardMinIfMaxonMobile();
}

function shipcardVisible() {
    return document.getElementById("shipcard").classList.contains("visible");
}

function measurecardVisible() {
    return document.getElementById("measurecard").classList.contains("visible");
}

function toggleMeasurecard() {
    if (shipcardVisible() && !measurecardVisible()) showShipcard(null, null);
    document.getElementById("measurecard").classList.toggle("visible");
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
    let p = {};

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
        if (show_all_tracks) a = await fetch("api/allpath.json");
        else {

            for (var mmsi of marker_tracks) {
                if (!(mmsi in shipsDB)) {
                    ToggleTrackOnMap(mmsi);
                }
            }

            var mmsi_str = Array.from(marker_tracks).join(",");
            a = await fetch("api/path.json?" + mmsi_str);
        }
        paths = await a.json();
    } catch (error) {
        console.log("Error loading path: " + error);
        paths = {};
        return false;
    }

    for (var mmsi in paths) {
        if (paths.hasOwnProperty(mmsi)) {
            if (mmsi in shipsDB && paths[mmsi].length > 0) {
                shipsDB[mmsi].raw.lat = paths[mmsi][0][0];
                shipsDB[mmsi].raw.lon = paths[mmsi][0][1];
            }
        }
    }
    return true;
}

function trackOptionString(mmsi) {
    const hover_track = mmsi == hoverType == 'ship' && hoverMMSI && hover_enabled_track;
    const select_track = card_type == 'ship' && mmsi == card_mmsi && select_enabled_track;
    const track_shown = marker_tracks.has(Number(mmsi));

    if (hover_track || select_track) return "Show Track";
    return track_shown ? "Hide Track" : "Show Track";
}

function updateShipcardTrackOption() {
    const trackOptionElement = document.getElementById("shipcard_track_option");

    if (show_all_tracks || card_type == 'plane') {
        trackOptionElement.style.opacity = "0.5";
        trackOptionElement.style.pointerEvents = "none";
    } else {
        trackOptionElement.style.opacity = "1";
        trackOptionElement.style.pointerEvents = "auto";
    }

    if (card_mmsi && card_type == 'ship') {
        document.getElementById("shipcard_track").innerText = trackOptionString(card_mmsi);
    }
}

function isShipcardMax() {
    var e = document.getElementById("shipcard").classList;
    return e.contains("shipcard-ismax");
}

function getStringfromMsgType(m) {
    let s = "";
    let delim = "";
    for (i = 1; i <= 27; i++)
        if ((m & (1 << i)) != 0) {
            s += delim + Number(i).toFixed(0);
            delim = ", ";
        }
    return s;
}

function getStringfromGroup(m) {
    let s = "";
    let delim = "";
    let count = 0;

    for (i = 0; i < 32; i++) {
        let mask = 1 << i;
        if ((m & mask) !== 0) {
            if (count == 4) {
                s += delim + "...";
                break;
            }
            s += delim + Number(i + 1).toFixed(0);
            delim = ", ";
            count++;
        }
    }
    return s;
}

function getStringfromChannels(m) {
    let s = "";
    let delim = "";
    for (i = 0; i <= 3; i++)
        if ((m & (1 << i)) != 0) {
            s += delim + String.fromCharCode(65 + i);
            delim = ", ";
        }
    return s;
}

function setShipcardValidation(v) {
    document.getElementById("shipcard_header").classList.remove("shipcard-validated", "shipcard-not-validated", "shipcard-dubious");

    switch (v) {
        case 1:
            document.getElementById("shipcard_header").classList.add("shipcard-validated");
            break;
        case -1:
            document.getElementById("shipcard_header").classList.add("shipcard-dubious");
            break;
        default:
            document.getElementById("shipcard_header").classList.add("shipcard-not-validated");
    }
}

function updateMessageButton() {
    const messageButton = document.querySelector('#shipcard_footer [onclick="showBinaryMessageDialog(card_mmsi)"]');
    if (!messageButton) return;

    const iconElement = messageButton.querySelector('i.mail_icon');
    if (!iconElement) return;

    const hasBinaryMsgs = card_mmsi &&
        binaryDB &&
        card_mmsi in binaryDB &&
        binaryDB[card_mmsi].ship_messages &&
        binaryDB[card_mmsi].ship_messages.length > 0;

    const existingBadge = iconElement.querySelector('.message-badge');
    if (existingBadge) {
        existingBadge.remove();
    }

    if (hasBinaryMsgs) {
        const count = binaryDB[card_mmsi].ship_messages.length;
        messageButton.style.display = '';

        const badge = document.createElement('span');
        badge.className = 'message-badge';
        badge.textContent = count;
        iconElement.appendChild(badge);
    } else {
        messageButton.style.display = 'none';
    }
}

function populateShipcard() {

    if (card_type != 'ship') return;

    if (!(card_mmsi in shipsDB)) {
        document
            .getElementById("shipcard_content")
            .querySelectorAll("span:nth-child(2)")
            .forEach((e) => (e.innerHTML = null));
        document.getElementById("shipcard_header_title").innerHTML = "<b style='color:red;'>Out of range</b>";
        document.getElementById("shipcard_header_flag").innerHTML = "";
        document.getElementById("shipcard_mmsi").innerHTML = card_mmsi;

        updateFocusMarker();
        return;
    }

    let ship = shipsDB[card_mmsi].raw;

    document.getElementById("shipcard_header_flag").innerHTML = getFlagStyled(ship.country, "padding: 0px; margin: 0px; margin-right: 5px; box-shadow: 2px 2px 3px rgba(0, 0, 0, 0.5); font-size: 26px;");
    document.getElementById("shipcard_header_title").innerHTML = (getShipName(ship) || ship.mmsi);

    setShipcardValidation(ship.validated);

    // verbatim copies
    ["destination", "mmsi", "count", "imo", "received_stations"].forEach((e) => (document.getElementById("shipcard_" + e).innerHTML = ship[e]));

    // round and add units
    [
        { id: "cog", u: "&deg", d: 0 },
        { id: "bearing", u: "&deg", d: 0 },
        { id: "heading", u: "&deg", d: 0 },
        { id: "level", u: "dB", d: 1 },
        { id: "ppm", u: "ppm", d: 1 },
    ].forEach((el) => (document.getElementById("shipcard_" + el.id).innerHTML = ship[el.id] ? Number(ship[el.id]).toFixed(el.d) + " " + el.u : null));

    document.getElementById("shipcard_country").innerHTML = getCountryName(ship.country);
    document.getElementById("shipcard_callsign").innerHTML = getCallSign(ship);
    document.getElementById("shipcard_msgtypes").innerHTML = getStringfromMsgType(ship.msg_type);

    document.getElementById("shipcard_last_group").innerHTML = getStringfromGroup(ship.last_group);
    document.getElementById("shipcard_sources").innerHTML = getStringfromGroup(ship.group_mask);

    document.getElementById("shipcard_channels").innerHTML = getStringfromChannels(ship.channels);
    document.getElementById("shipcard_type").innerHTML = getTypeVal(ship);
    document.getElementById("shipcard_shiptype").innerHTML = getShipTypeVal(ship.shiptype);
    document.getElementById("shipcard_status").innerHTML = getStatusVal(ship);
    document.getElementById("shipcard_last_signal").innerHTML = getDeltaTimeVal(ship.last_signal);
    document.getElementById("shipcard_eta").innerHTML = ship.eta_month != null && ship.eta_hour != null && ship.eta_day != null && ship.eta_minute != null ? getEtaVal(ship) : null;
    document.getElementById("shipcard_lat").innerHTML = ship.lat ? getLatValFormat(ship) : null;
    document.getElementById("shipcard_lon").innerHTML = ship.lon ? getLonValFormat(ship) : null;
    document.getElementById("shipcard_altitude").innerHTML = ship.altitude;

    document.getElementById("shipcard_speed").innerHTML = ship.speed ? getSpeedVal(ship.speed) + " " + getSpeedUnit() : null;
    document.getElementById("shipcard_distance").innerHTML = ship.distance ? (getDistanceVal(ship.distance) + " " + getDistanceUnit() + (ship.repeat > 0 ? " (R)" : "")) : null;
    document.getElementById("shipcard_draught").innerHTML = ship.draught ? getDimVal(ship.draught) + " " + getDimUnit() : null;
    document.getElementById("shipcard_dimension").innerHTML = getShipDimension(ship);

    updateShipcardTrackOption(card_mmsi);
    updateMessageButton();

}

function getCategory(plane) {
    if (!plane || !plane.category) return "-";

    const categories = {
        41: "< 7 MT",
        42: "7 - 34 MT",
        43: "34 - 136 MT",
        44: "High vortex",
        45: "> 136 MT",
        46: "High perf",
        47: "Rotorcraft",
        31: "Glider",
        32: "LTA",
        33: "Parachutist",
        34: "Ultralight",
        36: "UAV",
        37: "Space",
        21: "Emergency",
        23: "Service"
    };

    return categories[plane.category] || plane.category.toString() || "-";
}

function populatePlanecard() {

    if (card_type != 'plane') return;

    document
        .getElementById("shipcard_content")
        .querySelectorAll("span:nth-child(2)")
        .forEach((e) => (e.innerHTML = null));

    if (!(card_mmsi in planesDB)) {
        document
            .getElementById("shipcard_content")
            .querySelectorAll("span:nth-child(2)")
            .forEach((e) => (e.innerHTML = null));
        document.getElementById("shipcard_header_title").innerHTML = "<b style='color:red;'>Out of range</b>";
        document.getElementById("shipcard_header_flag").innerHTML = "";
        document.getElementById("shipcard_mmsi").innerHTML = card_mmsi;

        updateFocusMarker();
        return;
    }

    let plane = planesDB[card_mmsi].raw;

    setShipcardValidation(plane.validated);

    // Set header
    document.getElementById("shipcard_header_title").textContent = (plane.callsign || getICAO(plane));
    document.getElementById("shipcard_header_flag").innerHTML = getFlagStyled(plane.country, "padding: 0px; margin: 0px; margin-right: 5px; box-shadow: 2px 2px 3px rgba(0, 0, 0, 0.5); font-size: 26px;");

    // Populate plane fields
    document.getElementById("shipcard_plane_country").innerHTML = getCountryName(plane.country);
    document.getElementById("shipcard_plane_type").innerHTML = "ADSB";
    document.getElementById("shipcard_plane_callsign").textContent = plane.callsign || "-";
    document.getElementById("shipcard_plane_hexident").textContent = getICAO(plane);
    document.getElementById("shipcard_plane_category").textContent = getCategory(plane);
    document.getElementById("shipcard_plane_squawk").textContent = plane.squawk || "-";
    document.getElementById("shipcard_plane_speed").innerHTML = plane.speed ? getSpeedVal(plane.speed) + " " + getSpeedUnit() : null;

    document.getElementById("shipcard_plane_altitude").textContent = plane.airborne == 1 ? (plane.altitude ? `${plane.altitude} ft` : "-") : "on ground";
    document.getElementById("shipcard_plane_lat").innerHTML = plane.lat ? getLatValFormat(plane) : null;
    document.getElementById("shipcard_plane_lon").innerHTML = plane.lon ? getLonValFormat(plane) : null;
    document.getElementById("shipcard_plane_vertrate").textContent = plane.vertrate ? `${plane.vertrate} ft/min` : "-";
    document.getElementById("shipcard_plane_last_signal").textContent = getDeltaTimeVal(plane.last_signal);;
    document.getElementById("shipcard_plane_messages").textContent = plane.nMessages || "-";
    document.getElementById("shipcard_plane_downlink").textContent = getStringfromMsgType(plane.message_types);
    document.getElementById("shipcard_plane_TC").textContent = getStringfromMsgType(plane.message_subtypes);
    document.getElementById("shipcard_plane_distance").innerHTML = plane.distance ? (getDistanceVal(plane.distance) + " " + getDistanceUnit()) : null;

    document.getElementById("shipcard_plane_last_group").innerHTML = getStringfromGroup(plane.last_group);
    document.getElementById("shipcard_plane_sources").innerHTML = getStringfromGroup(plane.group_mask);

    [
        { id: "heading", u: "&deg", d: 0 },
        { id: "level", u: "dB", d: 1 },
        { id: "bearing", u: "&deg", d: 0 }
    ].forEach((el) => (document.getElementById("shipcard_plane_" + el.id).innerHTML = plane[el.id] ? Number(plane[el.id]).toFixed(el.d) + " " + el.u : null));

    updateShipcardTrackOption(card_mmsi);
}

function shipcardMinIfMaxonMobile() {
    if (shipcardVisible() && window.matchMedia("(max-height: 1000px) and (max-width: 500px)").matches && isShipcardMax()) {
        toggleShipcardSize();
    }
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
    let svgIconStyle = new ol.style.Style({
        image: new ol.style.Icon({
            anchor: [0.5, 0.5],
            scale: 0.3,
            color: 'white', //getComputedStyle(document.documentElement).getPropertyValue('--secondary-color'),
            src: 'data:image/svg+xml,%3Csvg xmlns="http://www.w3.org/2000/svg" height="48" viewBox="0 -960 960 960" width="48"%3E%3Cpath fill="white" d="M198-278q-60-58-89-133T80-560q0-74 29-149t89-133l35 35q-50 49-76.5 116.5T130-560q0 63 26.5 130.5T233-313l-35 35Zm92-92q-40-37-59-89.5T212-560q0-48 19-100.5t59-89.5l35 35q-29 29-46 72.5T262-560q0 35 17.5 79.5T325-405l-35 35Zm4 290 133-405q-17-12-27.5-31T389-560q0-38 26.5-64.5T480-651q38 0 64.5 26.5T571-560q0 25-10.5 44T533-485L666-80h-59l-29-90H383l-30 90h-59Zm108-150h156l-78-238-78 238Zm268-140-35-35q29-29 46-72.5t17-82.5q0-35-17.5-79.5T635-715l35-35q39 37 58.5 89.5T748-560q0 47-19.5 100T670-370Zm92 92-35-35q49-49 76-116.5T830-560q0-63-27-130.5T727-807l35-35q60 58 89 133t29 149q0 75-27.5 149.5T762-278Z"/%3E%3C/svg%3E',
        })
    });

    let CircleStyle = new ol.style.Style({
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

    if (settings.center_point == "STATION") {
        center = station;
    } else {
        center = {};
        if (hasMMSIcenter) {
            let ship = shipsDB[settings.center_point].raw;
            if (ship.lat != null && ship.lon != null) {
                center = { lat: ship.lat, lon: ship.lon };
            }
        }
    }

    if (settings.fix_center && center != null && center.hasOwnProperty("lat") && center.hasOwnProperty("lon")) {

        let view = map.getView();
        view.setCenter(ol.proj.fromLonLat([center.lon, center.lat]));

        settings.lat = center.lat;
        settings.lon = center.lon;
    }
}

function moveMapCenter(px) {
    let view = map.getView();
    let currentCenter = view.getCenter();
    let newCenter = map.getCoordinateFromPixel(px);
    let centerDiff = [newCenter[0] - currentCenter[0], newCenter[1] - currentCenter[1]];

    stopHover();
    view.animate({
        center: [currentCenter[0] + centerDiff[0], currentCenter[1] + centerDiff[1]],
        duration: 1000
    });
}

function adjustMapForShipcard(pixel) {

    let lat = null, lon = null;
    if (card_type == 'ship' && card_mmsi in shipsDB) {
        lat = shipsDB[card_mmsi].raw.lat;
        lon = shipsDB[card_mmsi].raw.lon;
    } else if (card_type == 'plane' && card_mmsi in planesDB) {
        lat = planesDB[card_mmsi].raw.lat;
        lon = planesDB[card_mmsi].raw.lon;
    }

    if (lat && lon) {
        let view = map.getView();

        let currentExtent = view.calculateExtent(map.getSize());
        let shipCoords = ol.proj.fromLonLat([lon, lat]);

        if (!ol.extent.containsCoordinate(currentExtent, shipCoords)) {
            view.animate({
                center: shipCoords,
                duration: 1000
            });
            return;
        }

        // Use the provided pixel if defined, otherwise calculate it
        if (!pixel) {
            pixel = map.getPixelFromCoordinate(shipCoords);
        }

        let shipcard = document.getElementById("shipcard");
        let shipcardRect = shipcard.getBoundingClientRect();
        let mapElement = map.getTargetElement();
        let mapRect = mapElement.getBoundingClientRect();

        let isUnderShipcard = (
            pixel[0] + mapRect.left >= shipcardRect.left &&
            pixel[0] + mapRect.left <= shipcardRect.right &&
            pixel[1] + mapRect.top >= shipcardRect.top &&
            pixel[1] + mapRect.top <= shipcardRect.bottom
        );

        if (isUnderShipcard) {
            let newPixel = [...pixel];
            let margin = 10; // Margin in pixels

            if (shipcardRect.bottom + margin + 20 <= mapRect.bottom) { // 20 is an approximate marker height
                newPixel[1] = shipcardRect.bottom + margin;
            }
            else if (shipcardRect.right + margin + 20 <= mapRect.right) { // 20 is an approximate marker width
                newPixel[0] = shipcardRect.right + margin;
            }
            newPixel = pixel;

            moveMapCenter(newPixel);
        }
    }
}

function pinShipcard() {
    settings.shipcard_pinned = true;
    settings.shipcard_pinned_x = parseInt(shipcard.style.left) || 0;
    settings.shipcard_pinned_y = parseInt(shipcard.style.top) || 0;

    applyShipcardPinStyling();
    showNotification("Shipcard pinned to current position");
    saveSettings();
}

function unpinShipcard() {
    settings.shipcard_pinned = false;
    settings.shipcard_pinned_x = null;
    settings.shipcard_pinned_y = null;

    applyShipcardPinStyling();

    showNotification("Shipcard unpinned");
    saveSettings();
}

function applyShipcardPinStyling() {
    const shipcard = document.getElementById("shipcard");
    if (settings.shipcard_pinned) {
        shipcard.classList.add("pinned");
        document.getElementById("shipcard_drag_handle").classList.add("opacity-25");
    }
    else {
        shipcard.classList.remove("pinned");
        document.getElementById("shipcard_drag_handle").classList.remove("opacity-25");
    }
}

function toggleShipcardPin() {
    if (settings.shipcard_pinned) {
        unpinShipcard();
    } else {
        pinShipcard();
    }
}

function positionAside(pixel, aside) {

    stopHover();

    if (settings.kiosk && settings.kiosk_pan_map && card_type == 'ship' && card_mmsi in shipsDB) {
        moveMapCenter(pixel);
        const mapSize = map.getSize();
        pixel = [mapSize[0] / 2, mapSize[1] / 2];
    }

    if (settings.shipcard_pinned && settings.shipcard_pinned_x !== null && settings.shipcard_pinned_y !== null) {
        aside.style.left = `${settings.shipcard_pinned_x}px`;
        aside.style.top = `${settings.shipcard_pinned_y}px`;
        return;
    }

    aside.style.left = "";
    aside.style.top = "";

    if (pixel) {
        const margin = 35;
        let marginRight = document.getElementById("tableside").classList.contains("active") ? 592 : 30;

        const mapSize = map.getSize();
        const shipCardRect = aside.getBoundingClientRect();
        const shipCardWidth = shipCardRect.width;
        const shipCardHeight = shipCardRect.height;

        const rightSpace = mapSize[0] - (pixel[0] + shipCardWidth + margin + marginRight);
        const leftSpace = pixel[0] - (shipCardWidth + margin);

        if ((rightSpace > 0 || leftSpace > 0) && mapSize[1] > shipCardHeight + 2 * margin) {
            let topPosition = pixel[1] - (shipCardHeight / 2);
            topPosition = Math.max(margin, Math.min(mapSize[1] - shipCardHeight - margin, topPosition));

            aside.style.top = `${topPosition}px`;

            if (rightSpace >= 0) {
                aside.style.left = `${pixel[0] + margin}px`;
            } else if (leftSpace >= 0) {
                aside.style.left = `${pixel[0] - shipCardWidth - margin}px`;
            } else {
                aside.style.left = `${(mapSize[0] - shipCardWidth) / 2}px`;
            }
        }
    }
    adjustMapForShipcard(pixel);
}

function displayShipcardIcons(type) {
    let icons = document.querySelectorAll('#shipcard_footer > div');
    let idx = 0;

    for (let icon of icons) {
        // Hide icons that don't match current context type
        if (icon.dataset.contextType !== type && icon.dataset.contextType) {
            icon.style.display = "none";
            continue;
        }

        // Show if within offset range or is More button
        const isMoreButton = icon.querySelector('i').classList.contains('more_horiz_icon');
        const isInRange = idx >= shipcardIconOffset[type] && idx < shipcardIconOffset[type] + shipcardIconMax;
        icon.style.display = (isInRange || isMoreButton) ? "flex" : "none";
        idx++;
    }
}

function rotateShipcardIcons() {
    shipcardIconOffset[card_type] += shipcardIconMax;
    if (shipcardIconOffset[card_type] >= shipcardIconCount[card_type]) {
        shipcardIconOffset[card_type] = 0;
    }
    displayShipcardIcons(card_type);
}

function prepareShipcard() {
    // Initialize offset/count objects if needed
    shipcardIconOffset = shipcardIconOffset || { ship: 0, plane: 0 };
    shipcardIconCount = shipcardIconCount || { ship: 0, plane: 0 };

    // Count icons for each context
    shipcardIconCount.ship = document.querySelectorAll('#shipcard_footer > div[data-context-type="ship"]').length;
    shipcardIconCount.plane = document.querySelectorAll('#shipcard_footer > div[data-context-type="plane"]').length;

    // Add More button for each context if needed
    if (shipcardIconCount.ship > shipcardIconMax + 1) {
        addShipcardItem('more_horiz', 'More', 'More options', 'rotateShipcardIcons()', 'ship');
    }
    if (shipcardIconCount.plane > shipcardIconMax + 1) {
        addShipcardItem('more_horiz', 'More', 'More options', 'rotateShipcardIcons()', 'plane');
    }

    displayShipcardIcons('ship');
}

function showShipcard(type, m, pixel = undefined) {
    const aside = document.getElementById("shipcard");
    const visible = shipcardVisible();

    let ship = m in shipsDB ? shipsDB[m].raw : null;
    let ship_old = card_mmsi in shipsDB ? shipsDB[card_mmsi].raw : null;


    if (select_enabled_track && (card_mmsi != m || m == null)) {
        select_enabled_track = false;

        if (!(card_mmsi == hoverMMSI && hover_enabled_track && hoverType == 'ship')) {
            hideTrack(card_mmsi);
        }
    }

    if (m != null && !visible) {
        if (measurecardVisible()) toggleMeasurecard();
        aside.classList.toggle("visible");

        select_enabled_track = false;


    } else if (visible && m == null) {
        aside.classList.toggle("visible");
    }


    if (type !== card_type) {
        document.querySelectorAll('#shipcard_content [data-context-type]').forEach(element => {
            if (element.dataset.contextType === type) {
                element.style.display = '';
            } else {
                element.style.display = 'none';
            }
        });

        displayShipcardIcons(type);
    }

    card_mmsi = m;
    card_type = type;


    if (shipcardVisible()) {
        if (settings.show_track_on_select && card_type == 'ship') {
            if (hoverMMSI === m && hover_enabled_track && hoverType == 'ship') {
                hover_enabled_track = false;
                select_enabled_track = true;
            }
            else if (!trackIsShown(m)) {
                select_enabled_track = true;
                showTrack(m);
            }
        }


        if (isShipcardMax()) {
            toggleShipcardSize();
        }
        if (!visible) shipcardMinIfMaxonMobile();
        positionAside(pixel, aside);

        if (card_type == 'ship') populateShipcard();
        else if (card_type == 'plane') populatePlanecard();

        // trigger reflow for iPad Safari
        aside.style.display = 'none';
        aside.offsetHeight;
        aside.style.display = '';
    }

    trackLayer.changed();
    updateFocusMarker();
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
    let shipClass = ship.shipclass;
    let sprite = shippingMappings[shipClass] || {
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

var SpritesAll =
    'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAKAAAABuCAYAAACgLRjpAAAAwnpUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHjabVBbDgMhCPznFD2CPHTxOG7XJr1Bj18UbHbbkjgMj4wA9NfzAbdhhAKSNy21lGQmVSo1I5rc2kRMMvGUmvElD6VHgSzF5tlDLdG/8vgRcNeM5ZOQ3qOwXwtVQl+/hMgdj4kGP0KohhCTFzAEWuxQqm7nFfa1wTL1BwNEr2P/xJtd78j2DxN1Rk6GzOoD8HgZuBlBQwtoMDWOLIZ5tqIf5N+dlsEbcl5ZgI6o50sAAAGEaUNDUElDQyBwcm9maWxlAAB4nH2RPUjDUBSFT1ulIhUFC1pxyFCd7KIijrUKRagQaoVWHUxe+gdNGpIUF0fBteDgz2LVwcVZVwdXQRD8AXEXnBRdpMT7kkKLGC883sd59xzeuw/wNypMNbvigKpZRjqZELK5VSH4Ch8iGMAQIhIz9TlRTMGzvu6pm+ouxrO8+/6sPiVvMsAnEMeZbljEG8Qzm5bOeZ84zEqSQnxOPGHQBYkfuS67/Ma56LCfZ4aNTHqeOEwsFDtY7mBWMlTiaeKoomqU78+6rHDe4qxWaqx1T/7CUF5bWeY6rVEksYgliBAgo4YyKrAQo10jxUSazhMe/hHHL5JLJlcZjBwLqEKF5PjB/+D3bM3C1KSbFEoA3S+2/TEGBHeBZt22v49tu3kCBJ6BK63trzaA2U/S620tegT0bwMX121N3gMud4DhJ10yJEcK0PIXCsD7GX1TDhi8BXrX3Lm1znH6AGRoVqkb4OAQGC9S9rrHu3s65/ZvT2t+P4Ewcqz7/a75AAANemlUWHRYTUw6Y29tLmFkb2JlLnhtcAAAAAAAPD94cGFja2V0IGJlZ2luPSLvu78iIGlkPSJXNU0wTXBDZWhpSHpyZVN6TlRjemtjOWQiPz4KPHg6eG1wbWV0YSB4bWxuczp4PSJhZG9iZTpuczptZXRhLyIgeDp4bXB0az0iWE1QIENvcmUgNC40LjAtRXhpdjIiPgogPHJkZjpSREYgeG1sbnM6cmRmPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5LzAyLzIyLXJkZi1zeW50YXgtbnMjIj4KICA8cmRmOkRlc2NyaXB0aW9uIHJkZjphYm91dD0iIgogICAgeG1sbnM6eG1wTU09Imh0dHA6Ly9ucy5hZG9iZS5jb20veGFwLzEuMC9tbS8iCiAgICB4bWxuczpzdEV2dD0iaHR0cDovL25zLmFkb2JlLmNvbS94YXAvMS4wL3NUeXBlL1Jlc291cmNlRXZlbnQjIgogICAgeG1sbnM6R0lNUD0iaHR0cDovL3d3dy5naW1wLm9yZy94bXAvIgogICAgeG1sbnM6ZGM9Imh0dHA6Ly9wdXJsLm9yZy9kYy9lbGVtZW50cy8xLjEvIgogICAgeG1sbnM6dGlmZj0iaHR0cDovL25zLmFkb2JlLmNvbS90aWZmLzEuMC8iCiAgICB4bWxuczp4bXA9Imh0dHA6Ly9ucy5hZG9iZS5jb20veGFwLzEuMC8iCiAgIHhtcE1NOkRvY3VtZW50SUQ9ImdpbXA6ZG9jaWQ6Z2ltcDpiNGQ1NGU4Ny1jZGQyLTQ0M2YtOWU2Mi1mYjJkYjFmOWNjOTkiCiAgIHhtcE1NOkluc3RhbmNlSUQ9InhtcC5paWQ6YWI4MDQ0MzItMTE2My00NGQ0LWE0MGUtNzUxMzE1YjZlMzZiIgogICB4bXBNTTpPcmlnaW5hbERvY3VtZW50SUQ9InhtcC5kaWQ6YjNhMDI1MzEtNmQ0OS00ZTE5LWE1NGYtOTE2OTE3MGYyMTMzIgogICBHSU1QOkFQST0iMi4wIgogICBHSU1QOlBsYXRmb3JtPSJNYWMgT1MiCiAgIEdJTVA6VGltZVN0YW1wPSIxNzM3ODI1Mjk4OTUwMzkxIgogICBHSU1QOlZlcnNpb249IjIuMTAuMzgiCiAgIGRjOkZvcm1hdD0iaW1hZ2UvcG5nIgogICB0aWZmOk9yaWVudGF0aW9uPSIxIgogICB4bXA6Q3JlYXRvclRvb2w9IkdJTVAgMi4xMCIKICAgeG1wOk1ldGFkYXRhRGF0ZT0iMjAyNTowMToyNVQxODoxNDo1OCswMTowMCIKICAgeG1wOk1vZGlmeURhdGU9IjIwMjU6MDE6MjVUMTg6MTQ6NTgrMDE6MDAiPgogICA8eG1wTU06SGlzdG9yeT4KICAgIDxyZGY6U2VxPgogICAgIDxyZGY6bGkKICAgICAgc3RFdnQ6YWN0aW9uPSJzYXZlZCIKICAgICAgc3RFdnQ6Y2hhbmdlZD0iLyIKICAgICAgc3RFdnQ6aW5zdGFuY2VJRD0ieG1wLmlpZDoxNTViZGM3Ny05OGRiLTRlZDctODU1MC1jYTMyZjgyZTk0ZGUiCiAgICAgIHN0RXZ0OnNvZnR3YXJlQWdlbnQ9IkdpbXAgMi4xMCAoTWFjIE9TKSIKICAgICAgc3RFdnQ6d2hlbj0iMjAyNS0wMS0yNVQxODoxNDo1OCswMTowMCIvPgogICAgPC9yZGY6U2VxPgogICA8L3htcE1NOkhpc3Rvcnk+CiAgPC9yZGY6RGVzY3JpcHRpb24+CiA8L3JkZjpSREY+CjwveDp4bXBtZXRhPgogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgCjw/eHBhY2tldCBlbmQ9InciPz5Kov3CAAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH6QEZEQ46qIXsNAAAIABJREFUeNrtnXtc1HW+/1/f69xnALkoAgoIchGVi2IB4jVXs1TKy8nMbtbeitPpt8efpzrbXn5nz+a27W6P2lUPa7ZZbZpmaVtrZa1QkqgpCoiaCWIwIDAw1+98v9/P+YOJgJlhvjDDOdVvXo/HPHDm85nn6/Oe72s+3+t8BcIK65su00aT1rTRpA8V777Yu7T3xd4VMt74xE3a8YmbQsYrzmG0xTlMyHhlGoO2TGMIGW9+iUo7v0QVMp42o1CrzSgMGW+5Oke7XJ2jiEcrItLMFtDM5lANkAa9hQa9OXRfEXoLQsijgC0UEDIeA2xhQshjWWxh2dDxQLNbQLMhrJfewihcHpSC2U/jXrOoCQC4195LsuyyOIKc/TS3udY2AcDrqr8mVZhfDIo3PnGTxuVc1QQAKvX+pNbmHUHxinMYzRwS2wQAxyhzUmWtFBSvTGPQ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'

async function updateMap() {
    let ok = false;

    ok = await fetchShips();
    if (!ok) return;

    ok = await fetchTracks();
    if (!ok) return;

    if (planeLayer.isVisible()) {
        ok = await fetchPlanes();
        if (!ok) return;
    }

    if (binaryLayer.isVisible()) {
        ok = await fetchBinary();
        if (!ok) return;
    }

    if (settings.setcoord == "true" || settings.setcoord == true) {
        if (station != null && station.hasOwnProperty("lat") && station.hasOwnProperty("lon")) {
            settings.setcoord = false;
            let view = map.getView();
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

    for (let [mmsi, entry] of Object.entries(shipsDB)) {
        let ship = entry.raw;
        if (ship.lat != null && ship.lon != null && ship.lat != 0 && ship.lon != 0 && ship.lat < 90 && ship.lon < 180) {
            getSprite(ship)

            const lon = ship.lon
            const lat = ship.lat

            const point = new ol.geom.Point(ol.proj.fromLonLat([lon, lat]))
            var feature = new ol.Feature({
                geometry: point
            })

            feature.ship = ship;

            markerFeatures[ship.mmsi] = feature
            markerVector.addFeature(feature)

            if (includeLabels)
                labelVector.addFeature(feature)

            if (map.getView().getZoom() > 11.5 && (ship.heading != null || settings.show_circle_outline)) {
                var shapeFeature = new ol.Feature({
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

        for (let [hexident, entry] of Object.entries(planesDB)) {
            let plane = entry.raw;
            if (plane.lat != null && plane.lon != null && plane.lat != 0 && plane.lon != 0 && plane.lat < 90 && plane.lon < 180) {
                getPlaneSprite(plane)

                const lon = plane.lon
                const lat = plane.lat

                const point = new ol.geom.Point(ol.proj.fromLonLat([lon, lat]))
                var feature = new ol.Feature({
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

    for (let [mmsi, entry] of Object.entries(paths)) {

        if (marker_tracks.has(Number(mmsi)) || show_all_tracks) {
            const path = paths[mmsi];
            const coordinates = [];
            for (var i = 0; i < Math.min(path.length, 250); i++) {
                coordinates.push(ol.proj.fromLonLat([path[i][1], path[i][0]]));
            }

            const lineString = new ol.geom.LineString(coordinates);
            const feature = new ol.Feature(lineString);
            feature.mmsi = mmsi;
            trackVector.addFeature(feature);
        }
    }

    drawRange();
    updateFocusMarker();
    updateHoverMarker();

    updateMarkerCount();
    updateTablecard();

    drawStation(station);
    updateDistanceCircles();

    //await plotRange();

}
function updateDarkMode() {
    document.documentElement.classList.toggle("dark", settings.dark_mode);

    const chartsToUpdateMulti = [chart_minutes, chart_hours, chart_days, chart_seconds];
    const chartsToUpdateLevel = [chart_level, chart_level_hour];
    const chartsToUpdateSingle = [chart_distance_day, chart_distance_hour, chart_ppm, chart_minute_vessel, chart_ppm_minute];
    const chartsToUpdateRadar = [chart_radar_day, chart_radar_hour];

    chartsToUpdateMulti.forEach((chart) => {
        updateColorMulti(chart);
        chart.update();
    });

    chartsToUpdateSingle.forEach((chart) => {
        updateColorSingle(chart);
        chart.update();
    });

    chartsToUpdateLevel.forEach((chart) => {
        const colorVariables = ["--chart1-color", "--chart1-color", "--chart1-color"];
        updateChartColors(chart, colorVariables);
        chart.update();
    });

    chartsToUpdateRadar.forEach((chart) => {
        updateColorRadar(chart);
        chart.update();
    });

    updateMapLayer();
    redrawMap();
}


document.getElementById('zoom-in').addEventListener('click', function () {
    var view = map.getView();
    var zoom = view.getZoom();
    view.setZoom(zoom + 1);
});

document.getElementById('zoom-out').addEventListener('click', function () {
    var view = map.getView();
    var zoom = view.getZoom();
    view.setZoom(zoom - 1);
});


function setDarkMode(b) {
    settings.dark_mode = b;
    updateDarkMode();
    saveSettings();
}

function toggleDarkMode() {
    settings.dark_mode = !settings.dark_mode;
    updateDarkMode();
    saveSettings();
}

/*
function refresh_data() {
    if (!document.hidden && !updateInProgress) {
        updateInProgress = true;

        (async () => {
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
}
*/

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
    await fetchShips(false);

    selectMapTab(m);

    let ship = shipsDB[m].raw;
    if (ship && ship.lon && ship.lat) {
        let shipCoords = ol.proj.fromLonLat([ship.lon, ship.lat]);
        let view = map.getView();
        view.setCenter(shipCoords);
    }

    if (z) mapResetView(z);
    else mapResetView(14);

    //showTrack(m);
}

function updateSettingsTab() {
    document.getElementById("settings_darkmode").checked = settings.dark_mode;
    document.getElementById("settings_coordinate_format").value = settings.coordinate_format;
    document.getElementById("settings_metric").value = getMetrics().toLowerCase();
    document.getElementById("settings_show_station").checked = settings.show_station;
    document.getElementById("settings_fading").checked = settings.fading;
    document.getElementById("settings_shiphover_color").value = settings.shiphover_color;
    document.getElementById("settings_shipselection_color").value = settings.shipselection_color;

    document.getElementById("settings_show_range").checked = settings.show_range;
    document.getElementById("settings_distance_circles").checked = settings.distance_circles;
    document.getElementById("settings_distance_circle_color").value = settings.distance_circle_color;

    document.getElementById("settings_labels_declutter").checked = settings.labels_declutter;
    document.getElementById("settings_tooltipLabelFontsize").value = settings.tooltipLabelFontSize;

    document.getElementById("settings_show_labels").value = settings.show_labels.toLowerCase();

    document.getElementById("settings_shipoutline_border").value = settings.shipoutline_border;
    document.getElementById("settings_shipoutline_inner").value = settings.shipoutline_inner;
    document.getElementById("settings_shipoutline_opacity").value = settings.shipoutline_opacity;
    document.getElementById("settings_show_circle_outline").value = settings.show_circle_outline;

    document.getElementById("settings_track_color").value = settings.track_color;
    document.getElementById("settings_range_color").value = settings.range_color;
    document.getElementById("settings_range_timeframe").value = settings.range_timeframe;
    document.getElementById("settings_range_color_short").value = settings.range_color_short;
    document.getElementById("settings_range_color_dark").value = settings.range_color_dark;
    document.getElementById("settings_range_color_dark_short").value = settings.range_color_dark_short;

    document.getElementById("settings_map_opacity").value = settings.map_opacity;
    document.getElementById("settings_icon_scale").value = settings.icon_scale;
    document.getElementById("settings_track_weight").value = settings.track_weight;

    document.getElementById("settings_tooltipLabelColor").value = settings.tooltipLabelColor;
    document.getElementById("settings_tooltipLabelShadowColor").value = settings.tooltipLabelShadowColor;

    document.getElementById("settings_tooltipLabelColorDark").value = settings.tooltipLabelColorDark;
    document.getElementById("settings_tooltipLabelShadowColorDark").value = settings.tooltipLabelShadowColorDark;
    document.getElementById("settings_table_shiptype_use_icon").checked = settings.table_shiptype_use_icon;
    document.getElementById("settings_show_track_on_hover").checked = settings.show_track_on_hover;
    document.getElementById("settings_show_track_on_select").checked = settings.show_track_on_select;

    document.getElementById("settings_kiosk_mode").checked = settings.kiosk;
    document.getElementById("settings_kiosk_rotation_speed").value = settings.kiosk_rotation_speed;
    document.getElementById("settings_kiosk_pan_map").checked = settings.kiosk_pan_map;

    updateKioskSpeedDisplay(settings.kiosk_rotation_speed);
}

class RealtimeViewer {
    constructor() {
        this.content = document.getElementById('realtime_content');
        this.eventSource = null;
        this.lineCount = 0;
        this.maxLines = 50;
    }

    connect() {
        if (this.eventSource) return;
        this.eventSource = new EventSource("api/sse");

        this.eventSource.addEventListener('nmea', (event) => {
            if (this.lineCount > this.maxLines) {
                this.content.innerHTML = "";
                this.lineCount = 0;
            }
            this.content.innerText = event.data + "\n" + this.content.innerText;
            this.lineCount++;
        });

        this.eventSource.addEventListener('error', () => {
            this.content.innerText = "Connection error. Server is not reachable or reverse web proxy not configured for Server-Side Events.";
        });

        this.eventSource.addEventListener('open', () => {
            this.content.innerText = "";
            //showNotification("Realtime NMEA connection established");
        });
    }

    disconnect() {
        if (this.eventSource) {
            this.eventSource.close();
            this.eventSource = null;
            //showNotification("Realtime NMEA connection closed");
        }
    }
}

class LogViewer {
    constructor() {
        this.logState = document.getElementById('log_state');
        this.logScroll = document.getElementById('log_scroll');
        this.logContent = document.getElementById('log_content');
        this.eventSource = null;
    }

    connect() {
        if (this.eventSource) return;
        this.eventSource = new EventSource("api/log");

        this.eventSource.addEventListener('log', (event) => {
            try {
                const data = JSON.parse(event.data);
                this.appendFormattedLog(data);
                this.scrollToBottom();
            } catch (error) {
                console.error('Error handling log event:', error);
                this.appendRawLog(event.data);
            }
        });

        this.eventSource.addEventListener('open', () => {
            this.updateConnectionState(true);
        });

        this.eventSource.addEventListener('error', () => {
            this.updateConnectionState(false);
        });

        this.logContent.innerHTML = '';
    }

    appendFormattedLog(data) {
        const logEntry = document.createElement('div');
        logEntry.className = 'log-entry ' + (data.level || 'info');

        const icon = document.createElement('span');
        switch (data.level) {
            case 'error':
                icon.className = 'error_od_icon';
                break;
            case 'warning':
                icon.className = 'warning_od_icon';
                break;
            default:
                icon.className = '';
        }
        icon.style.flexShrink = '0';
        icon.style.marginTop = '0.2rem';
        icon.style.width = '1.2em';
        icon.style.height = '1.2em';

        const content = document.createElement('div');
        content.style.flex = '1';

        const timestamp = document.createElement('div');
        timestamp.className = 'timestamp';
        timestamp.textContent = data.time;
        timestamp.style.fontSize = '0.75rem';
        timestamp.style.marginBottom = '0.05rem';

        const message = document.createElement('div');
        message.textContent = data.message.replace(/^"|"$/g, '');

        content.appendChild(timestamp);
        content.appendChild(message);
        logEntry.appendChild(icon);
        logEntry.appendChild(content);

        this.logContent.appendChild(logEntry);
    }

    appendRawLog(message) {
        const logEntry = document.createElement('div');
        logEntry.textContent = message + '\n';
        this.logContent.appendChild(logEntry);
    }

    disconnect() {
        if (this.eventSource) {
            this.eventSource.close();
            this.eventSource = null;
        }
        this.updateConnectionState(false);
    }

    scrollToBottom() {
        requestAnimationFrame(() => {
            this.logScroll.scrollTop = this.logScroll.scrollHeight;
        });
    }

    updateConnectionState(connected) {
        const stateIcon = document.createElement('span');
        stateIcon.className = connected ? 'info_od_icon' : 'error_od_icon';
        stateIcon.style.display = 'inline-block';
        stateIcon.style.verticalAlign = 'middle';
        stateIcon.style.marginRight = '0.5em';
        stateIcon.style.width = '1em';
        stateIcon.style.height = '1em';

        const status = connected ? 'Connected' : 'Disconnected';
        this.logState.innerHTML = '';
        this.logState.display = 'none';
        if (!connected) {
            this.logState.appendChild(stateIcon);
            this.logState.appendChild(document.createTextNode(`Status: ${status}`));
        }
    }
}


function activateTab(b, a) {
    hideMenuifSmall();

    Array.from(document.getElementById("menubar").children).forEach((e) => (e.className = e.className.replace(" active", "")));
    Array.from(document.getElementById("menubar_mini").children).forEach((e) => (e.className = e.className.replace(" active", "")));

    tabcontent = document.getElementsByClassName("tabcontent");

    for (i = 0; i < tabcontent.length; i++) tabcontent[i].style.display = "none";

    document.getElementById(a).style.display = "block";
    if (a === "map") document.getElementById("tableside").style.display = "flex";

    document.getElementById(a + "_tab").className += " active";
    document.getElementById(a + "_tab_mini").className += " active";

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

    if (a == "log") {

        logViewer = new LogViewer();
        logViewer.connect();
    }
    if (a != 'log' && logViewer) {
        logViewer.disconnect();
        logViewer = null;
    }

    if (a == "realtime") {
        realtimeViewer = new RealtimeViewer();
        realtimeViewer.connect();
    }
    if (a != 'realtime' && realtimeViewer) {
        realtimeViewer.disconnect();
        realtimeViewer = null;
    }
    if (a === "about") {
        setupAbout();
    }
}

function selectMapTab(m) {
    document.getElementById("map_tab").click();
    if (m in shipsDB) showShipcard('ship', m);
}

function selectTab() {
    if (settings.tab == "settings") settings.tab = "stat";

    if (settings.tab != "realtime" && settings.tab != "about" && settings.tab != "map" && settings.tab != "plots" && settings.tab != "ships" && settings.tab != "stat" && settings.tab != "log") {
        settings.tab = "stat";
        alert("Invalid tab specified");
    }
    activateTab(null, settings.tab);
    //document.getElementById(settings.tab + "_tab").click();
}

function updateAndroid() {
    if (isAndroid()) {
        var elements = document.querySelectorAll(".noandroid");
        for (var i = 0; i < elements.length; i++) {
            elements[i].style.display = "none";
        }
    } else {
        var elements = document.querySelectorAll(".android");
        for (var i = 0; i < elements.length; i++) {
            elements[i].style.display = "none";
        }
    }
}

var originalDisplayValues = new Map();

function clearAndHide(element) {
    if (!originalDisplayValues.has(element)) {
        // Get computed style to handle cases where display isn't explicitly set
        var computedStyle = window.getComputedStyle(element);
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
function setKiosk(enabled) {
    settings.kiosk = enabled;
    updateKiosk();
    saveSettings();
}

function setKioskRotationSpeed(speed) {
    settings.kiosk_rotation_speed = parseInt(speed);
    saveSettings();


    if (isKiosk() && kioskAnimationInterval) {
        startKioskAnimation();
    }
}

function setKioskPanMap(enabled) {
    settings.kiosk_pan_map = enabled;
    saveSettings();
}

function updateKioskSpeedDisplay(value) {
    document.getElementById("kiosk_speed_display").textContent = value + "s";
}

function updateKiosk() {
    if (isKiosk()) {
        startKioskAnimation();

        var nokioskElements = document.querySelectorAll(".nokiosk");
        var kioskElements = document.querySelectorAll(".kiosk");

        for (var i = 0; i < nokioskElements.length; i++) {
            clearAndHide(nokioskElements[i]);
        }

        for (var i = 0; i < kioskElements.length; i++) {
            restoreOriginalDisplay(kioskElements[i]);
        }
    } else {
        stopKioskAnimation();

        var kioskElements = document.querySelectorAll(".kiosk");
        var nokioskElements = document.querySelectorAll(".nokiosk");

        for (var i = 0; i < kioskElements.length; i++) {
            clearAndHide(kioskElements[i]);
        }

        for (var i = 0; i < nokioskElements.length; i++) {
            restoreOriginalDisplay(nokioskElements[i]);
        }
    }
}

var kioskAnimationInterval = null;

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

function showAboutDialog() {
    const message = `
        <div style="display: flex; align-items: center; margin-top: 10px;">
        <span style="text-align: center; margin-right: 10px;"><i style="font-size: 40px" class="directions_aiscatcher_icon"></i></span>
        <span>
        <a href="https://www.aiscatcher.org"><b style="font-size: 1.6em;">AIS-catcher</b></a>
        <br>
        <b style="font-size: 0.8em;">&copy; 2021-2025 jvde.github@gmail.com</b>
        </span>
        </div>
        <p>
        AIS-catcher is a research and educational tool, provided under the
        <a href="https://github.com/jvde-github/AIS-catcher/blob/e66a4481e62d8f1775700e5f51fb7ad9ea569a12/LICENSE">GNU GPL v3 license</a>.
        It is not reliable for navigation and safety of life or property.
        Radio reception and handling regulations vary by region, so check your local administration's rules. Illegal use is strictly prohibited.
        </p>
        <p>
        The web-interface gratefully uses the following libraries:
        <a href="https://www.chartjs.org/docs/latest/charts/line.html" rel="nofollow">chart.js</a>,
        <a href="https://www.chartjs.org/chartjs-plugin-annotation/latest/" rel="nofollow">chart.js annotation plugin</a>,
        <a href="https://openlayers.org/" rel="nofollow">openlayers</a>,
        <a href="https://fonts.google.com/icons?selected=Material+Icons" rel="nofollow">Material Design Icons</a>,
        <a href="https://tabulator.info/" rel="nofollow">tabulator</a>,
        <a href="https://github.com/markedjs/marked">marked</a>, and
        <a href="https://github.com/lipis/flag-icons">flag-icons</a>. Please consult the links for the respective licenses.
        </p>`;

    showDialog("About...", message);
}

function showWelcome() {
    if (settings.welcome == true || (settings.welcome == "true" && !isAndroid())) showAboutDialog();

    settings.welcome = false;
    saveSettings();
}

function isAndroid() {
    return settings.android === true || settings.android === "true";
}

function isKiosk() {
    return settings.kiosk === true || settings.kiosk === "true";
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
            var scriptElement = document.createElement("script");
            scriptElement.src = "https://cdn.jsdelivr.net/npm/marked/marked.min.js";
            document.head.appendChild(scriptElement);

            scriptElement.onload = function () {
                document.getElementById("about_content").innerHTML = marked.parse(s);
                aboutContentLoaded = true;
            };
        })
        .catch((error) => {
            alert("Error loading about.md: " + error);
            aboutMDpresent = false;
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

let rainviewerRadar = new ol.layer.Tile({
    name: 'rainviewer_radar',
    title: 'RainViewer Radar',
    type: 'overlay',
    opacity: 0.7,
});

let rainviewerClouds = new ol.layer.Tile({
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
        const radarSource = new ol.source.XYZ({
            url: `https://tilecache.rainviewer.com/v2/radar/${latestRadar.time}/512/{z}/{x}/{y}/6/1_1.png`,
            attributions: '<a href="https://www.rainviewer.com/api.html" target="_blank">RainViewer.com</a>'
        });
        rainviewerRadar.setSource(radarSource);

        // Update clouds layer
        const latestClouds = data.satellite.infrared[data.satellite.infrared.length - 1];
        const cloudsSource = new ol.source.XYZ({
            url: `https://tilecache.rainviewer.com/${latestClouds.path}/512/{z}/{x}/{y}/0/0_0.png`,
            attributions: '<a href="https://www.rainviewer.com/api.html" target="_blank">RainViewer.com</a>'
        });
        rainviewerClouds.setSource(cloudsSource);

    } catch (error) {
        console.error("Error refreshing RainViewer layers:", error);
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


let mdabout = "This content can be defined by the owner of the station";

console.log("Starting plugin code");


loadPlugins && loadPlugins();

var button = document.getElementById('xchange'); // Get the button by its ID


if (communityFeed) {
    button.classList.remove('fill-red');
    button.classList.add('fill-green');

    var feedVector = new ol.source.Vector({
        features: []
    });

    var feederStyle = function (feature) {
        return new ol.style.Style({
            image: new ol.style.Icon({
                src: "https://www.aiscatcher.org/hub_test/sprites_hub.png",
                rotation: feature.ship.rot,
                offset: [feature.ship.cx, feature.ship.cy],
                size: [feature.ship.imgSize, feature.ship.imgSize],
                scale: settings.icon_scale * 0.8,
                opacity: 1
            })
        })
    }

    var feedLayer = new ol.layer.Vector({
        source: feedVector,
        style: feederStyle
    });


    function redrawCommunityFeed(db) {

        feedVector.clear();

        for (let [mmsi, entry] of Object.entries(CshipsDB)) {
            let ship = entry.raw;
            if (ship.lat != null && ship.lon != null && ship.lat != 0 && ship.lon != 0 && ship.lat < 90 && ship.lon < 180) {
                getSprite(ship)

                const lon = ship.lon
                const lat = ship.lat

                const point = new ol.geom.Point(ol.proj.fromLonLat([lon, lat]))
                var feature = new ol.Feature({
                    geometry: point
                })

                feature.ship = ship;
                feature.link = 'https://www.aiscatcher.org/?zoom=12&mmsi=' + ship.mmsi
                feature.tooltip = ship.shipname || ship.mmsi
                feedVector.addFeature(feature)
            }
        }
    }


    let CshipsDB = {}

    async function fetchCommunityShips(noDoubleFetch = true) {

        let ships = {};

        try {
            const extent = map.getView().calculateExtent(map.getSize());
            const extentInLatLon = ol.proj.transformExtent(extent, 'EPSG:3857', 'EPSG:4326');

            let minLon = extentInLatLon[0] - 0.0045;
            let minLat = extentInLatLon[1] - 0.0045;
            let maxLon = extentInLatLon[2] + 0.0045;
            let maxLat = extentInLatLon[3] + 0.0045;

            let zoom = map.getView().getZoom();

            response = await fetch("https://www.aiscatcher.org/hub_test/ships_array.json?" + zoom + "," + minLat + "," + minLon + "," + maxLat + "," + maxLon + "," + -1);
        } catch (error) {
            console.log("failed loading ships: " + error);
            return false;
        }

        ships = await response.json();


        const keys = [
            "mmsi",
            "lat",
            "lon",
            "distance",
            "bearing",
            "level",
            "count",
            "ppm",
            "approx",
            "heading",
            "cog",
            "speed",
            "to_bow",
            "to_stern",
            "to_starboard",
            "to_port",
            "last_group",
            "group_mask",
            "shiptype",
            "mmsi_type",
            "shipclass",
            "validated",
            "msg_type",
            "channels",
            "country",
            "status",
            "draught",
            "eta_month",
            "eta_day",
            "eta_hour",
            "eta_minute",
            "imo",
            "callsign",
            "shipname",
            "destination",
            "last_signal",
        ];

        CshipsDB = {};
        ships.values.forEach((v) => {
            const s = Object.fromEntries(keys.map((k, i) => [k, v[i]]));
            const entry = {};
            entry.raw = s;
            CshipsDB[s.mmsi] = entry;
        });

        return true;
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
            if (ok) {
                redrawCommunityFeed(CshipsDB);
            }
        });
    }
    addOverlayLayer("Community Feed", feedLayer);
}

let urlParams = new URLSearchParams(window.location.search);
restoreDefaultSettings();

console.log("Plugin loading completed");

console.log("Load settings");
loadSettings();

console.log("Load settings from URL parameters");

loadSettingsFromURL();
updateForLegacySettings();

updateAndroid();

applyDynamicStyling();

console.log("Setup tabs");
initFullScreen();
initPlots();

initMap();

updateDarkMode();

console.log("Switch to active tab");
selectTab();

if (urlParams.get("mmsi")) openFocus(urlParams.get("mmsi"), urlParams.get("zoom"));
updateSortMarkers();
saveSettings();
prepareShipcard();

if (aboutMDpresent == false) {
    document.getElementById("about_tab").style.display = "none";
    document.getElementById("about_tab_mini").style.display = "none";
}

if (typeof realtime_enabled === "undefined" || realtime_enabled === false) {
    document.getElementById("realtime_tab").style.display = "none";
    document.getElementById("realtime_tab_mini").style.display = "none";
}

if (typeof log_enabled === "undefined" || log_enabled === false) {
    document.getElementById("log_tab").style.display = "none";
    document.getElementById("log_tab_mini").style.display = "none";
}

showWelcome();
updateKiosk();
applyShipcardPinStyling();

if (isAndroid()) showMenu();

main();
