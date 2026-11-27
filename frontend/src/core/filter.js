import * as shared from '@aiscatcher/core/filter.js';
import { settings } from './state.js';

export { BUCKETS, bucketFor, bucketOf } from '@aiscatcher/core/filter.js';

let clock = 0;
export function setClock(serverTime) {
    if (serverTime > 0) clock = serverTime;
}

const instance = shared.create({
    state: () => {
        if (!settings.ship_filter || typeof settings.ship_filter !== "object") settings.ship_filter = {};
        return settings.ship_filter;
    },
    now: () => clock,
});

export const {
    get, set, LISTS, isHidden, toggle, setAll, reset, passesAppearance,
    isActive, describe, shipPasses, passesUntimed, passesSeen,
} = instance;

// whether a vessel passes the filter; the verdict on all but "seen within" stays on the
// entry until the vessel's data or the filter changes (both clear entry.show), that
// term goes by the server clock and is asked each time
export function visible(entry) {
    if (entry.show === undefined) entry.show = passesUntimed(entry.raw);
    return entry.show && passesSeen(entry.raw);
}
