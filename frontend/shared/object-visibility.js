// One visibility query for every object kind. Persisted setting names remain
// compatible with existing viewer profiles.
export function objectKindVisible(settings, kind) {
    if (kind === 'place') return settings.show_places !== false;
    if (kind === 'port') return settings.show_ports !== false;
    return !(settings.binary_exclude || []).includes(kind);
}
export function needsMapObjects(settings, binaryShown) {
    return objectKindVisible(settings, 'place') || objectKindVisible(settings, 'port') || binaryShown;
}
