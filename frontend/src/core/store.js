// The vessel data every surface reads: the ship and plane records, the tracks
// drawn for them, the counts derived from them, and the poll cursors that keep
// them current.
//
// It lives here rather than in script.js because the map, the ship table, the
// replay engine, the ticker and the range overlay all need it, and until now the
// only way for a module to reach it was to be handed an accessor at init(). That
// is why replay takes thirteen injected callbacks and the ticker three: not
// because those modules are entangled with the shell, but because the data they
// work on had no address of its own.
//
// Exported as live bindings rather than a container object: an importer sees a
// reassignment made in here, but cannot make one itself. That keeps a single
// writer for each field while leaving `store.ships[mmsi]` reading as `ships[mmsi]`
// at every call site. Reassignment goes through the setters below; mutation in
// place - adding, updating and deleting records - does not.

export let ships = {};
export let planes = {};

// mmsi -> array of track points, and the epoch the current window starts at
export let paths = {};
export let pathsFrom = -1;

// the receiver's own position, when it publishes one
export let station = {};

// totals and per-bucket tallies, recomputed from `ships` on every refresh
export let counts = { total: 0, shown: 0, buckets: {} };

// Poll cursors, deliberately a second behind the server clock so a record
// written while the response was serialised is not stepped over.
export let shipsSince = 0;
export let planesSince = 0;

// The server's actual clock, which is what an age is measured against. Separate
// from the cursor because they are a second apart: a vessel heard in that second
// would otherwise have a negative age, and every "x ago" in the app is derived
// from this one number.
export let clock = 0;

// What the user has picked out of the fleet: the vessel whose card is open, the
// one under the cursor, and the set with tracks drawn. Not derived from the data
// and not settings - it is the third kind of state the viewer has, and replay,
// kiosk and the shipcard all read it.
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
