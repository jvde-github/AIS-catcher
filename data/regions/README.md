# Marine waters

Which sea, gulf, strait or lake a vessel is in. One `.geojson` file per water;
`scripts/waters/build_waters.py <out dir>` turns the directory into place files
of type `water` (one per water, the kind in `attributes.area_subtype`) for a
place catalogue: the hub's `data/places`, or any viewer's `places` directory.
The receiver then tracks a water like any other place: a vessel's current water
is the smallest one it is in, and crossing from one into the next is an event.

## Adding or fixing a water

1. Draw the polygon (geojson.io, QGIS, by hand) in WGS84 lat/lon. It need not be
   precise: only the edge where it meets another region is followed closely.
2. Save it as one file, `NNNN-name.geojson`, under `contrib/` with these properties:
   - `id` - next free number from 2000 up, below 4096. **Never change an existing
     id**: it is what a database stores.
   - `name` - what people call it.
   - `featurecla` - `sea`, `strait`, `bay`, `lake`, `river`, `canal`, ...
   - `source` - where the shape comes from and its licence (public domain or
     CC0/CC-BY; say which).
   - `min_cell` (optional, degrees) - how sharp its edge against other regions
     is. Defaults: seas 0.5, lakes/rivers/canals 1/32 (~3 km).
3. From the repo root: `python3 scripts/waters/build_waters.py <out dir>` (needs
   `pip install shapely`; a slightly self-intersecting polygon is repaired and
   reported). Commit the geojson here and the place files where they are used.

## How the outlines are built

The waters are simplified as one coverage. Where two overlap the smaller wins,
so a strait or bay added later takes its water from the sea around it without
the sea being edited. The shared edges are simplified once for both sides, so
borders between seas stay consistent. Along a coast nothing competes, so an
outline may run wider than the shoreline: onto land, never into a neighbour's
water. The tolerance grows with the water's size, from about 2 km for a bay to
60 km for an ocean, and an outline is coarsened further until it fits the
catalogue's 2,000 vertices per place. Inland positions with no water drawn, a
river or canal without a polygon, are in no water.

## Layout and ids

| dir | ids | content |
|---|---|---|
| `seas/` | 0-999 | Natural Earth 5.2 `ne_10m_geography_marine_polys` |
| `lakes/` | 1000-1999 | Natural Earth 5.2 `ne_10m_lakes`, scalerank <= 3 |
| `contrib/` | 2000-4095 | everything added here; also straits Natural Earth 5.2 lost |

Natural Earth data is public domain. "Made with Natural Earth. Free vector and
raster map data @ naturalearthdata.com".
