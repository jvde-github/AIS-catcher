// The viewer's one dialog, for messages and tables, and its notifications.

import * as components from '@aiscatcher/ui/components.js';
import { register } from './actions.js';
import { sanitizeString } from '@aiscatcher/core/text.js';

let dialogModal = null;

export function showDialog(title, message, hideTitle) {
    if (!dialogModal) {
        dialogModal = components.modal({
            id: "dialog-box", cardClass: "modal-fit", bodyClass: "dialog-message",
            // some callers widen the card for their content; every close path resets it
            onClose: () => { dialogModal.card.style.maxWidth = ""; },
        });
        dialogModal.root.classList.add("scrim-strong");
    }
    dialogModal.card.classList.toggle("dialog-hide-title", !!hideTitle);
    dialogModal.setTitle(title || "");
    dialogModal.body.innerHTML = message;
    dialogModal.open();
}

export function showDialogPlain(message) {
    showDialog("", message, true);
}

export function closeDialog() {
    if (dialogModal) dialogModal.close();
}

export function isDialogOpen() {
    return !!(dialogModal && dialogModal.isOpen());
}

// an object as rows of key and value; `copyContext` lets a value be copied from its context menu
export function objectToTableHtml(obj, copyContext) {
    let tableHtml = '<table class="kv-table">';
    for (let key in obj) {
        let value = obj[key];
        if (Array.isArray(value) || (value !== null && typeof value === 'object')) value = JSON.stringify(value);
        if (value == null) value = '';
        const safeKey = sanitizeString(String(key));
        const safeVal = sanitizeString(String(value));
        const empty = safeVal === '';
        const copyAttrs = copyContext && !empty ? ` data-on-contextmenu="showNMEAContextCopy" data-copy="${safeVal}"` : '';
        const cell = empty ? '<span class="kv-empty">-</span>' : safeVal;
        tableHtml += `<tr><td class="kv-key">${safeKey}</td><td class="kv-value"${copyAttrs}>${cell}</td></tr>`;
    }
    return tableHtml + "</table>";
}

export function showNotification(message, type = "info", duration) {
    (type === "error" ? console.error : console.log)("[notification] " + message);
    return components.toast(type, message, { duration, container: "notification-container" });
}

register({
    closeDialog: () => closeDialog(),
});
