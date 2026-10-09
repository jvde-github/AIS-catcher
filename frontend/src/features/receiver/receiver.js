// The receiver the viewer shows, when the station has several: the dialog to
// pick one, opened from the map's context menu, and what a switch resets.

import { config } from '../../core/config.js';
import { sanitizeString } from '@aiscatcher/core/text.js';
import { receiver, switchReceiver, refresh } from '../../data.js';
import { showDialog, closeDialog } from '../../dialog.js';
import { replaycardVisible } from '../../panels.js';
import * as replaybar from '../replay/replaybar.js';
import * as mapObjects from '../mapobjects/mapobjects.js';
import * as range from '../range/range.js';
import { register } from '../../actions.js';

// { closeSearch() }
let deps = { closeSearch: () => {} };

export function init(d) {
    deps = d;
}

// the receiver shown, as the menu names it
export function receiverLabel() {
    const r = (config.receivers || []).find((x) => x.idx === receiver);
    return r ? r.label : "Receiver " + receiver;
}

// the menu offers a choice only when there is one
export function hasReceiverChoice() {
    return (config.receivers || []).length >= 2;
}

export function showReceiverDialog() {
    const receivers = config.receivers || [];
    let html = '<div class="receiver-list">';
    for (const r of receivers) {
        html += `<div class="receiver-option${r.idx === receiver ? " active" : ""}"` +
            ` data-action="selectReceiver" data-idx="${r.idx}">${sanitizeString(r.label)}</div>`;
    }
    showDialog("Select Receiver", html + "</div>");
}

function selectReceiver(idx) {
    closeDialog();
    onReceiverChange(idx);
}

export function onReceiverChange(idx) {
    deps.closeSearch();
    if (replaycardVisible()) replaybar.toggleReplaycard();
    switchReceiver(idx);
    mapObjects.resetSince();
    range.resetUpdateTime();
    refresh();
}

register({
    showReceiverDialog: () => showReceiverDialog(),
    selectReceiver: (e, d) => selectReceiver(d.idx),
});
