import * as shared from '../../shared/core/filter.js';
import { settings } from './state.js';

export { MOVING_KNOTS, BUCKETS, CLASSES, STATUSES, moving, bucketFor, bucketOf } from '../../shared/core/filter.js';

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
    get, set, LISTS, isHidden, toggle, setAll, reset, isActive, describe, passesAppearance, shipPasses,
} = instance;
