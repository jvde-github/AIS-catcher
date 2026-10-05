// The replay bar: opening it hands the map over from the live layers to
// replay.js, its play button, its scrubber and the time labels that follow
// the playhead.

import { settings } from '../../core/state.js';
import { debounce } from '@aiscatcher/ui/components.js';
import { formatTime } from '@aiscatcher/core/text.js';
import { markerLayer, trackLayer, labelLayer, shapeLayer, planeLayer, redraw as redrawMap } from '../../map.js';
import { panels, setPanels } from '../../panels.js';
import { closeTargetcard } from '../targetcard/card.js';
import { stopHover } from '../hover/hover.js';
import * as measure from '../measure/measure.js';
import * as mapObjects from '../mapobjects/mapobjects.js';
import * as replay from './replay.js';
import { register } from '../../actions.js';

export function init() {
    replay.init({
        setLiveLayers: setLiveLayersVisible,
        onStateChange: updateReplaycard,
    });
}

// While replay owns the map the live layers step aside, so the two never draw
// the same vessel twice. Exiting just puts them back and lets the normal
// refresh rebuild from shipsDB.
function setLiveLayersVisible(on) {
    [markerLayer, trackLayer, labelLayer, shapeLayer, mapObjects.objectLayer]
        .forEach(l => l.setVisible(on));
    planeLayer.setVisible(on && Array.isArray(settings.map_overlay) && settings.map_overlay.includes("Aircraft"));
}

export function toggleReplaycard() {
    // which panels step aside is normalisePanels's business, not this function's
    setPanels({ replay: !panels.replay });

    if (panels.replay) {
        measure.cancel();

        // the bar owns the map from the moment it opens
        closeTargetcard();
        stopHover();
        setLiveLayersVisible(false);

        replay.refreshBounds().then(() => {
            // the user may have closed the bar while the fetch was in flight
            if (!panels.replay) return;
            updateReplaycard();
            replayShowAt(replayScrubTime(document.getElementById("replayScrub").value));
        });
    } else {
        replayLoadAt.cancel();
        stopReplay();
    }
}

// opening the bar already loaded the frame; this load is the fallback
async function replayToggle() {
    if (replay.isLoading()) return;

    if (replay.isPlaying()) {
        replay.pause();
        return;
    }

    if (!replay.isActive()) {
        replayLoadAt.cancel();
        const at = replayScrubTime(document.getElementById("replayScrub").value);

        if (!(await replayShowAt(at))) return;
    }
    replay.play();
}

function stopReplay() {
    replay.stop();
    redrawMap();
    updateReplaycard();
}

// The scrubber's 0..1000 value mapped onto the replayable timeline.
function replayScrubTime(value) {
    const tl = replay.getTimeline();
    return tl.start + (tl.end - tl.start) * (Number(value) / 1000);
}

// Dragging the slider is the primary way in: the labels follow immediately,
// and shortly after the drag settles the fleet for that moment is loaded and
// drawn. Play then only starts the clock on what is already on screen.
const replayLoadAt = debounce((at) => replayShowAt(at), 250);

function replaySeek(el) {
    const tl = replay.getTimeline();
    if (tl.end <= tl.start) return;

    const at = replayScrubTime(el.value);

    if (replay.isActive()) {
        replay.seek(at);
        return;
    }

    updateReplaycard();
    replayLoadAt(at);
}

async function replayShowAt(at) {
    const ok = await replay.load(at);
    updateReplaycard();
    return ok;
}

// The playhead as wall-clock time. Seconds matter on a short span; over a
// multi-day one the date does instead, and there is no room for both.
function replayStamp(unixSec, spanSec) {
    if (spanSec > 86400) {
        const d = new Date(unixSec * 1000);
        return d.toLocaleDateString(undefined, { month: "short", day: "numeric" }) + " " +
            d.toLocaleTimeString(undefined, { hour: "2-digit", minute: "2-digit" });
    }
    return formatTime(unixSec);
}

// Time left, collapsing to days once it stops fitting as h:mm:ss.
function replayRemain(sec) {
    const s = Math.max(0, Math.round(sec));

    if (s >= 86400) {
        const days = Math.floor(s / 86400);
        const hours = Math.floor((s % 86400) / 3600);
        return days + "d" + (hours ? " " + hours + "h" : "");
    }

    const h = Math.floor(s / 3600);
    const m = Math.floor((s % 3600) / 60);
    const ss = String(s % 60).padStart(2, "0");
    return h > 0 ? h + ":" + String(m).padStart(2, "0") + ":" + ss : m + ":" + ss;
}

// Runs at animation rate while playing, so element lookups are cached and every
// write is guarded: most of the bar only changes on discrete events, and the
// time labels only when the displayed second does.
let replayEls = null;
const replayShown = {};

function updateReplaycard() {
    const bar = document.getElementById("replaybar");
    if (!bar || !bar.classList.contains("visible")) return;

    if (!replayEls)
        replayEls = {
            scrub: document.getElementById("replayScrub"),
            speed: document.getElementById("replaySpeed"),
            labels: document.getElementById("replayLabels"),
            play: document.getElementById("replayPlay"),
            fill: document.getElementById("replayFill"),
            elapsed: document.getElementById("replayElapsed"),
            remaining: document.getElementById("replayRemaining"),
        };
    const ui = replayEls, shown = replayShown;
    const { start, end } = replay.getTimeline();

    bar.classList.toggle("playing", replay.isPlaying());
    bar.classList.toggle("loading", replay.isLoading());

    const speed = replay.getSpeed();
    if (shown.speed !== speed) {
        shown.speed = speed;
        ui.speed.textContent = speed + "×";
    }

    const labelsOn = replay.getLabels();
    if (shown.labelsOn !== labelsOn) {
        shown.labelsOn = labelsOn;
        ui.labels.classList.toggle("on", labelsOn);
        ui.labels.setAttribute("aria-pressed", labelsOn);
        ui.labels.title = labelsOn ? "Hide ship labels" : "Show ship labels";
        ui.labels.setAttribute("aria-label", ui.labels.title);
    }

    // no history at all means nothing to start
    const hasHistory = end > start;
    ui.play.disabled = !hasHistory || replay.isLoading();
    ui.scrub.disabled = !hasHistory;

    const playTitle = replay.isLoading() ? "Loading" : replay.isPlaying() ? "Pause" : "Play";
    if (shown.playTitle !== playTitle) {
        shown.playTitle = playTitle;
        ui.play.title = playTitle;
    }

    const at = replay.isActive() ? replay.getInstant() : replayScrubTime(ui.scrub.value);

    if (replay.isActive() && hasHistory) {
        const v = Math.round(((at - start) / (end - start)) * 1000);
        if (Number(ui.scrub.value) !== v) ui.scrub.value = v;
    }

    const fill = (Number(ui.scrub.value) / 10) + "%";
    if (shown.fill !== fill) {
        shown.fill = fill;
        ui.fill.style.width = fill;
    }

    const atSec = hasHistory ? Math.floor(at) : -1;
    if (shown.atSec !== atSec || shown.start !== start || shown.end !== end) {
        shown.atSec = atSec;
        shown.start = start;
        shown.end = end;
        ui.elapsed.textContent = hasHistory ? replayStamp(at, end - start) : "--:--";
        ui.elapsed.title = hasHistory ? new Date(at * 1000).toLocaleString() : "";
        ui.remaining.textContent = hasHistory ? "-" + replayRemain(end - at) : "--:--";
    }
}

register({
    toggleReplaycard: () => toggleReplaycard(),
    replayToggle: () => replayToggle(),
    replaySeek: (e, d, el) => replaySeek(el),
});
