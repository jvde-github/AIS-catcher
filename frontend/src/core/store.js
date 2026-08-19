// Vessel data, tracks, counts and selection, shared by every surface that draws
// them. Exported as live bindings so an importer sees a reassignment made here
// but cannot make one itself — reassign through the setters, mutate in place
// directly.

export let ships = {};
export let planes = {};

export let paths = {};
export let pathsFrom = -1;

export let station = {};
export let counts = { total: 0, shown: 0, buckets: {} };

// cursors run a second behind `clock` so a record written mid-response is not
// stepped over; ages are measured against `clock`, never a cursor
export let shipsSince = 0;
export let planesSince = 0;
export let clock = 0;

export let cardMmsi = null;
export let cardType = null;
export let hoverMmsi = undefined;
export let hoverType = undefined;
export let markerTracks = new Set();

export function setCard(mmsi, type) { cardMmsi = mmsi; cardType = type; }
export function setHover(mmsi, type) { hoverMmsi = mmsi; hoverType = type; }
export function setMarkerTracks(v) { markerTracks = v; }

export function setShips(v) { ships = v; }
export function setPlanes(v) { planes = v; }
export function setPaths(v) { paths = v; }
export function setPathsFrom(v) { pathsFrom = v; }
export function setStation(v) { station = v; }
export function setCounts(v) { counts = v; }
export function setShipsSince(v) { shipsSince = v; }
export function setPlanesSince(v) { planesSince = v; }
export function setClock(v) { clock = v; }
