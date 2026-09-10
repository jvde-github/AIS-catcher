# Places

Places are the local catalogue of ports, berths, anchorages and custom geographic
features. Configure a directory with `control.viewer.places`, or use the standalone
viewer setting `-N 8100 PLACES /path/to/places`. Managed installations create a
`places` directory beside the configuration. The top-level Places tab edits this catalogue.
There is no separate port file, PORTS setting, or port editor.

## Files and editing

A `.geojson` file contains a Feature or a FeatureCollection. Each Feature has a
unique lowercase UUID, a name, schema_version 1, revision, and place_type. Geometry
is WGS84 Point, Polygon or MultiPolygon. Coordinates are longitude, latitude.

```json
{"type":"Feature","id":"b2345678-1234-4234-8234-123456789abc",
 "properties":{"schema_version":1,"revision":1,"name":"Rotterdam",
 "place_type":"port","unlocode":"NLRTM","size":3},
 "geometry":{"type":"Point","coordinates":[4.48,51.9]}}
```

Ports require a unique UN/LOCODE. Berths and anchorages refer to a port through
port_unlocode; that port need not be installed. Custom places have a category.
Ports can specify parent_unlocode and size (0 unknown, 1 small, 2 medium, 3 large).
Several polygon parts of one port belong in one MultiPolygon Feature. Parts may
touch; overlapping interiors are rejected. Holes, self-intersections, duplicate
vertices and unsplit antimeridian crossings are validated.

Files are authoritative. Editing or deleting a Feature atomically rewrites its
owning file, preserving the other Features in a collection. New Features get their
own UUID-named file. Duplicate UUIDs and duplicate port codes are load errors:
the offending file is skipped with a diagnostic, rather than merged or overridden.
There are no override files or tombstones. Regeneration replaces manual changes;
put lasting corrections in the generator's input.

New edits use revision 0. A successful save increments revision. Saves/deletes
check UUID and revision under the same lock as the file rewrite, preventing lost
updates to collection siblings. External file changes require a restart or a
configured-directory change; there is no file watcher. Back up files before
editing them externally while the server is running.

The editor supports drawing points and polygons, moving points, reshaping polygon
rings, import/export of individual Features, and search. Lists show up to 200
matches; narrow the search for larger catalogues. The list transfers representative
points; selecting a polygon fetches its complete definition. The existing managed
session and origin checks protect mutations.

## Runtime and map delivery

Runtime place IDs are uint32_t, with UINT32_MAX reserved for missing membership.
IDs remain stable through managed edits/deletions; reloading may renumber them.
Consumers associate IDs with place_version. UUIDs remain in files; startup uses
temporary identity sets and admin edits scan source records.

The catalogue publishes immutable snapshots with ID, port-code and normalized-name
lookups. A separate polygon-only candidate list is sorted smallest-first. Queries
scan its bounding boxes, then part bounds and rings. Point Features never enter
containment queries. A spatial grid is deliberately not part of the base backend.

Each place has a representative point. Point Features use their coordinates;
polygons use an interior point outside holes. The map-object feed sends kind 10
markers and deletion IDs, without polygon payloads. Hover fetches a boundary from
`api/place.json?id=<runtime-id>&version=<place-version>` and displays it temporarily.
A stale version returns an empty object. The client retains at most 64 boundary
responses and invalidates them on catalogue version changes.

`/api/places.json` on the viewer and `/api/places` on control return the same
summary rows as the map feed, under `objects`, with `place_version`. The summary
writer uses compiled metadata directly; it never reparses saved GeoJSON. Both
hosts expose `/api/place.json?id=<runtime-id>&version=<place-version>` for one
complete Feature. The editor uses that GET for selection and export; there is no
POST read, duplicate viewer collection alias, or full-collection read endpoint.
Saves and deletes continue to check the Feature's UUID and revision.

Source records, snapshots and visit history share one immutable metadata object
per definition. Historical metadata does not retain geometry. Feed sequences are
indexed by runtime ID; deletions retain numeric ID/sequence pairs. Destination
matching uses the catalogue's code/name maps, with a transient port metadata view
only when serializing a ship card. No parallel port vector or provider remains.
Validation canonicalizes a save once, including its incremented revision.

Polygon storage is limited to 4 million compiled vertices and 256 MiB of retained
polygon definitions plus point buffers. Point records do not consume those budgets.
Each Feature is limited to 2,000 vertices and 128 KiB; each file is limited to
256 MiB. There is no 1,000-object limit. Point metadata still consumes memory.
Status reports loaded/skipped definitions, polygon parts, vertices and storage bytes.

## Ships and destination matching

`VisitTracker` owns all membership, confirmation and visit state, outside `Ship`.
Each ship slot has exactly five visit records. Repeat visits use separate records.
A visit stores two uint32 timestamps and inside/pending flags; the pending
crossing's timestamp is tentative until confirmed. Zero means unknown. Accepted
visit timestamps range from Unix second 1 through UINT32_MAX (February 2106);
out-of-range reports cannot wrap the tracker clock, and such backup times are
rejected. Shared historical metadata is retained across renames, deletions and
restarts. On the 64-bit build a visit is 32 bytes and a five-visit record is 168
bytes, down from 56 and 288 respectively. At 500,000 slots the record array is
84 MB, excluding relationship edges, reverse buckets and shared metadata.

Five records and their forward edges are allocated for every configured ship slot;
reverse buckets grow with retained relationships. There is no per-ship history
container. Hub memory and throughput measurements remain a separate step.
Current containment updates immediately; a subsequent accepted position at least
10 seconds later confirms a crossing at its first observation time. Stale and
copied reports cannot confirm crossings. Startup, position expiry, operator edits,
changed boundaries and reception gaps over 10 minutes establish a baseline without
inventing entry or exit times. A reception gap closes the previous observation
without an exit and starts a fresh baseline if still inside.

Empty records are used first, then the oldest completed record. If all five are
active or pending, current containment takes priority over pending exits, then
smaller places win (runtime ID breaks size ties). Dropping a record for capacity
never produces an exit event. Crossing detection is therefore limited to those
five records; it does not promise detection for every overlapping polygon.
Ticker events are emitted for ports only. Berth, anchorage and custom visits still
appear in the visit list and vessel history.

The compact ship row retains place_ids at index 14. Each retained visit is encoded
as `2 * runtime_id + inside_now`, including repeated IDs. JavaScript decodes using
arithmetic, avoiding signed bitwise truncation. The viewer derives distinct current
IDs for its place filter. Responses carry place_version. Vessel details expose
one `visits` list with runtime ID (null for a deleted place), historical name,
inside/pending status and confirmed entry/exit timestamps. There is no separate
current `places` list. The summary derives the current location name from visits without a visit preview. The History tab puts Visits and
Reported changes before the Speed and Draught charts, in separate sections, showing the latest item with an ellipsis
pill to reveal the rest. Empty sections are hidden. Visits are ordered
by newest known entry, with unknown entries last; Entry, Exit and Duration use three columns
with labels above their values; visits under a minute show seconds. The client computes duration from timestamps
and the response's server `time`, never its local clock. Unknown times/durations
remain unknown; observation gaps show “Observation ended; exit unknown”.
Crossing history is derived from the same five records, not a separate event deque.

Both forward and reverse relationships use `RelationshipIndex<Key, N>`: fixed
slot-indexed edges, compact reverse buckets and swap-removal with back-references.
Reverse place membership includes retained completed visits, deduplicated by ship.
Deduplication belongs to the relationship primitive. Visit IDs are reindexed only
on allocation, clearing or remapping; flag/time changes need no bucket changes.
Reindexing and compact-row packing use fixed arrays without temporary heap storage.
Deletion and slot recycling clear these relationships together. Catalogue changes
remap all retained IDs by UUID; deleted places retain historical metadata and times
but lose their runtime ID and reverse membership. Only ships in changed polygon
bounds need new containment tests; metadata-only edits keep crossing state.
Historical records share immutable metadata, never polygon geometry.

DB backup version 2 adds a sparse visit section keyed by MMSI with one shared
metadata/UUID table and small table references per visit. Entry/exit times survive
restart; current containment is recomputed from restored positions. Unconfirmed
crossings restart from a baseline. Backups omit pending entries and inactive
records without confirmed times; established inside baselines with unknown entry
times are retained. The same rule selects metadata, ship counts and visit rows.
Version 1 backups still restore ship fields.
The entire section is validated before applying ships or visits to the DB.

Local destination matching uses port places only: exact normalized code, then a
unique normalized name. Ambiguous and partial names are not guessed. Builds with
libport keep their broader matcher, including matches without a local place.
Being inside a place does not set the reported destination. `DestinationIndex`
interns destination codes and owns slot-to-code and reverse membership, matching, and a single retry when an
unmatched ship first gets a valid position. Catalogue changes rematch local-only
builds; libport matches remain independent of local catalogue edits. Hub port
queries and CSV exports read this same index.

Matched codes are derived state, absent from `Ship` and new ship backups (version
6). Loading recomputes them from destination text and restored position. Version 5
ship records remain readable: their old nine-byte matched code is discarded.
Versions 3 and 4 also remain readable. Old binaries cannot read version 6 records.

## Verification

Run `npm test` in frontend/shared; browser tests accept PUPPETEER_MODULE and
CHROME_PATH.

`PlaceMarkers` owns the immutable catalogue snapshot, per-marker feed sequences,
and deletion records. It adopts catalogue changes, emits marker rows and removals,
and returns old/new polygon bounds for containment refresh. DB coordinates visit
remapping, destination rematching and affected ships; the feed sequence remains
shared with binary objects and stations. `full_refresh_at` remains DB-owned because
imports, restores, station changes and expiry also invalidate ship deltas.

Place markers open the shared ship-list dialog. Inside shows current containment;
Arrived and Left show one latest confirmed crossing per ship;
Visits keeps repeat visit rows. Ports also offer Expected: matched destinations
excluding current containment in that port. Point ports cannot exclude containment
and the dialog says so. History includes all records available in the five-visit tracker.
The dialog shares the binary message dialog’s fixed top position across tabs. Each tab shows its total and the ten
newest rows; Inside and Expected sort by last report. Runtime-ID requests include
the catalogue version and fail clearly if the marker has become stale.
The viewer's place filter and side-table selector are removed; place activity is
explored through the marker dialog. Pending crossing times display Confirming…,
with a dash for duration until confirmation.
