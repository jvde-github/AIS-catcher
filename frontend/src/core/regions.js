import names from './regionNames.js';

export function regionName(id) {
    return id == null || id < 0 ? null : names[id] || null;
}
