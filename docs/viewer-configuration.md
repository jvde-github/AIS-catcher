# Viewer configuration ownership

`ViewerConfiguration.h` defines the shared three-stage rule: parse values and
paths, prepare resources, then apply effects. Parsing opens no tile or plugin
files. `prepare()` resolves resources before `applySettings()` changes databases,
workers or listener options. Startup, live configuration and receiver attachment
use the same effects path. `configuration.values` holds requested values;
`settings` holds the effective values, including restart restrictions.

Receiver attachment reads the request and resolves its receiver zone mask. It
does not overwrite effective settings. Immutable tile handles may be reused
between configurations; place files reload when the configured directory changes.

The managed service loop owns configuration application and receiver attachment.
HTTP handlers save files under the control file mutex, then set change flags.
They do not call receiver or viewer reconfiguration while holding control locks.
The loop consumes those flags and applies changes. The embedded host must also
serialize its attachment calls through the viewer API.

`WebViewer::state_mtx` protects configuration, effective settings, receiver
trackers and published frontend state. Applying changes acquires that lock,
then each DB's `configuration_mtx`, which excludes incoming decoding while
tracking configuration changes. The DB's own `mtx` protects ships, port membership
index, history and events. DB callbacks execute under `mtx`: they must remain
short and must not re-enter DB or acquire viewer/control locks.

Place snapshots are immutable and published by shared pointer. Control edits
produce a new snapshot; the managed loop installs it in each DB, rematches
stationary ships and updates the map-object sequence. Place markers use kind
10 in the delta feed. Full geometry remains available for editing and is fetched
on hover for map display.

The place generation token remains in the map and vessel feeds solely to guard
against matching compact place IDs from different snapshots. It no longer
triggers a separate geometry download. Removing that guard would make a stale
vessel feed temporarily match unrelated polygons after a reload.

Measured locally with the nationwide dataset: 470 loaded, 0 skipped, 3,143 parts,
39,050 vertices; point buffers 312,400 bytes, retained canonical definitions
1,585,322 bytes. Whole process memory was 16,285,696 bytes on this macOS build;
this is not an isolated PlaceCatalogue allocation measurement or a Pi measurement.

Compatibility and review decisions:

- Control `/api/places` and viewer `/api/places.json` return summaries.
  Both hosts use the version-checked `/api/place.json` GET for a full Feature.
- Per-part bounds and compilation during validation were already implemented.
  The exact boundary check stays in the single ring pass: outer boundaries count
  as inside and hole boundaries as outside.
- Voyage entries already share metadata. Keeping it preserves historical names
  after an area edit/deletion; an eight-byte entry cannot also retain the current
  timestamp and crossing coordinates.
- The hub private PortIndex is removed. Its query adapter uses the DB index
  when available, and a read-only scan with its currently pinned older upstream.
  Updating that dependency activates the indexed path without another migration.
- A blanket data licence and automatic shipment of mixed-source area datasets
  are intentionally not added without verified provenance.

Control assigns its shared area store explicitly through `setPlaceCatalogue()` during
startup and from the managed update loop. Directory changes and disabling places
replace that assignment; no viewer callback reads ControlCore on demand.
