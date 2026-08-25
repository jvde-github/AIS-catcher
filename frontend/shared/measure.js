/* ============================================================================
   measure.js — distance and bearing between two points or vessels (layer 2)

   Owns the list of measures, the map layer that draws them and the rows of the
   measure card. The host says what a vessel is, where the card is and how to
   tell the user something:

       const measure = create({
           source: new VectorSource(),                    // the layer's source (host owns the layer)
           rows: document.getElementById("measurecardInner"),
           vessel: (id) => ({ lon, lat, name }) | null,   // null: out of range
           notify: (text, kind) => ...,
           ensureCard: () => ...,                         // show the card for a new measure
           onChange: () => ...,                           // rows changed: persist, badge, ...
           distance: { value: (nm) => "1.2", unit: () => "nm" },
       });
       measure.handleMapClick(vesselId | null, () => lonLat);   // one click per end
       measure.preview(pixel) / measure.cancel();

   A measure is { start_type, start_value, end_type?, end_value?, visible }.
   ========================================================================= */

import OlFeature from "ol/Feature.js";
import Point from "ol/geom/Point.js";
import LineString from "ol/geom/LineString.js";
import Style from "ol/style/Style.js";
import Stroke from "ol/style/Stroke.js";
import Fill from "ol/style/Fill.js";
import Text from "ol/style/Text.js";
import { fromLonLat, toLonLat } from "ol/proj.js";
import { getLength } from "ol/sphere.js";

import { calculateBearing } from "./core/geo.js";
import { token } from "./color.js";

export function create(opts) {
    var source = opts.source;
    var rows = opts.rows;
    var vessel = opts.vessel;
    var notify = opts.notify || function () {};
    var ensureCard = opts.ensureCard || function () {};
    var onChange = opts.onChange || function () {};
    var distance = opts.distance;
    var mapEl = opts.mapEl || document.getElementById("map");
    var getMap = typeof opts.map === "function" ? opts.map : function () { return opts.map; };

    var measures = [];
    var measuring = false;   // a start is set, the end is not
    var armed = false;       // the next click starts a measure
    var previewLine = null;

    var lineStyle = new Style({ stroke: new Stroke({ color: token("--color-success", "green"), lineDash: [20, 20], width: 2 }) });
    var lineStyleWhite = new Style({ stroke: new Stroke({ color: "white", lineDash: [20, 20], lineDashOffset: 20, width: 2 }) });
    var labelStyle = new Style({
        text: new Text({
            font: "14px Calibri,sans-serif",
            fill: new Fill({ color: "rgba(255, 255, 255, 1)" }),
            backgroundFill: new Fill({ color: token("--color-success", "green") }),
            padding: [3, 3, 3, 3],
            textBaseline: "bottom",
            offsetY: -15,
        }),
    });

    function style(feature) {
        var styles = [lineStyle, lineStyleWhite];
        var geometry = feature.getGeometry();
        if (geometry.getType() === "LineString") {
            labelStyle.setGeometry(new Point(geometry.getLastCoordinate()));
            labelStyle.getText().setText(feature.measureDistance + " " + distance.unit() + ", " + feature.measureBearing + " degrees");
            styles.push(labelStyle);
        }
        return styles;
    }

    /* map coordinate and caption of one end, or null when a vessel is gone */
    function end(type, value) {
        if (type === "point") return { coord: fromLonLat(value), label: "(" + value[1].toFixed(1) + "," + value[0].toFixed(1) + ")" };
        var v = vessel(value);
        return v ? { coord: fromLonLat([v.lon, v.lat]), label: v.name || String(value) } : null;
    }

    function measured(geometry) {
        var c = geometry.getCoordinates();
        return {
            distance: distance.value(getLength(geometry) / 1852),
            bearing: calculateBearing(toLonLat(c[0]), toLonLat(c[c.length - 1])).toFixed(0),
        };
    }

    function lineFeature(a, b) {
        var geometry = new LineString([a, b]);
        var m = measured(geometry);
        var feature = new OlFeature(geometry);
        feature.measureDistance = m.distance;
        feature.measureBearing = m.bearing;
        return feature;
    }

    function refresh() {
        source.clear();
        var content = "";
        measures = measures.filter(function (measure, index) {
            var from = end(measure.start_type, measure.start_value);
            var to = "end_type" in measure ? end(measure.end_type, measure.end_value) : { coord: null, label: "" };
            if (!from || !to) { notify("Ship out of range for measurement.", "warning"); return false; }

            var dist = 0, bearing = 0;
            if (to.coord) {
                var feature = lineFeature(from.coord, to.coord);
                dist = feature.measureDistance; bearing = feature.measureBearing;
                if (measure.visible) source.addFeature(feature);
            }
            var icon = measure.visible ? "visibility" : "visibility_off";
            content += '<tr data-index="' + index + '">' +
                '<td style="padding: 2px;"><i style="padding-left:2px; font-size: 15px;" class="' + icon + '_icon visibility_icon"></i></td>' +
                "<td>" + from.label + "</td><td>" + to.label + "</td>" +
                "<td>" + dist + " " + distance.unit() + "</td><td>" + bearing + "°</td>" +
                '<td style="padding: 2px;"><i style="padding-left:2px; font-size: 15px;" class="delete_icon"></i></td></tr>';
            return true;
        });
        if (rows) rows.innerHTML = content;
        if (previewLine) { var p = previewLine; previewLine = null; preview(p.pixel); }
        onChange();
    }

    /* a line from the open measure's start to `pixel`, while the end is chosen */
    function preview(pixel) {
        var map = getMap();
        if (!measuring || !measures.length || !map) return;
        var from = end(measures[measures.length - 1].start_type, measures[measures.length - 1].start_value);
        if (!from) return;
        if (previewLine) source.removeFeature(previewLine.feature);
        var feature = lineFeature(from.coord, map.getCoordinateFromPixel(pixel));
        feature.isPreview = true;
        source.addFeature(feature);
        previewLine = { feature: feature, pixel: pixel };
    }

    function clearPreview() {
        if (previewLine) source.removeFeature(previewLine.feature);
        previewLine = null;
    }

    function setArmed(on) {
        armed = on;
        if (mapEl) mapEl.classList.toggle("crosshair_cursor", on);
    }

    function start(type, value) {
        measuring = true;
        measures.push({ start_value: value, start_type: type, visible: true });
        ensureCard();
        notify("Select end point or object");
        refresh();
        setArmed(true);
    }

    function setEnd(type, value) {
        var i = measures.length - 1;
        measures[i] = Object.assign({}, measures[i], { end_value: value, end_type: type });
    }

    function finish(type, value) {
        if (!measuring) return;
        clearPreview();
        setEnd(type, value);
        notify("Measurement added.", "success");
        cancel();
    }

    function cancel() {
        measuring = false;
        clearPreview();
        setArmed(false);
        refresh();
    }

    /* `vesselId` is null for a bare map point; `getLonLat` is lazy because the
       pixel-to-coordinate conversion is only needed in the point case */
    function handleMapClick(vesselId, getLonLat) {
        armed = false;
        if (measuring) {
            if (vesselId != null) finish("ship", vesselId); else finish("point", getLonLat());
            return;
        }
        if (vesselId != null) start("ship", vesselId); else start("point", getLonLat());
    }

    function updateEnd(vesselId, getLonLat) {
        if (!measuring) return;
        if (vesselId != null) setEnd("ship", vesselId); else setEnd("point", getLonLat());
        refresh();
    }

    if (rows) {
        rows.addEventListener("click", function (event) {
            var row = event.target.closest("tr");
            if (!row) return;
            var i = Number(row.getAttribute("data-index"));
            if (event.target.classList.contains("visibility_icon")) { measures[i].visible = !measures[i].visible; refresh(); }
            else if (event.target.classList.contains("delete_icon")) { measures.splice(i, 1); refresh(); }
        });
    }

    return {
        style: style,
        refresh: refresh,
        preview: preview,
        arm: function () { setArmed(true); },
        cancel: cancel,
        handleMapClick: handleMapClick,
        updateEnd: updateEnd,
        isActive: function () { return armed || measuring; },
        isMeasuring: function () { return measuring; },
        count: function () { return measures.length; },
        all: function () { return measures; },
        load: function (list) { measures = Array.isArray(list) ? list.slice() : []; refresh(); },
    };
}
