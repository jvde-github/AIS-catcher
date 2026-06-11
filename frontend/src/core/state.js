// Shared mutable settings. Mutated in place by loadSettings() /
// restoreDefaultSettings() in script.js — importers can't reassign the
// imported binding.

export let settings = {};

// URL parameters can leave these settings as the string "true".
export function isAndroid() {
    return settings.android === true || settings.android === "true";
}

export function isKiosk() {
    return settings.kiosk === true || settings.kiosk === "true";
}
