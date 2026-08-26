# Marine regions

Which sea, gulf, strait or lake a vessel is in. One `.geojson` file per region;
`scripts/regions/build_regions.py` turns the directory into
`Source/Tracking/RegionTree.cpp` (a ~150 KB lookup table) and
`frontend/src/core/regionNames.js` (the names the ship card shows).
`Region::find(lat, lon)` answers in one binary search.

## Adding or fixing a region

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
3. From the repo root: `python3 scripts/regions/build_regions.py` (needs
   `pip install shapely`; a slightly self-intersecting polygon is repaired and
   reported). Commit the geojson and both generated files.

## How the table is built

The world is split into quadrants where a region's edge crosses the cell. A
shoreline is followed only to 0.5 degrees (lakes 1/16), so a position well
inland - a river or canal without a polygon - reads as no region rather than
the nearest sea; where two regions meet, the edge is as sharp as the finest
`min_cell` of the regions involved. A cell is assigned the **highest id** among
the regions that cover at least 10% of it, so a specific waterway added later
wins over the sea around it without editing the sea. That is why contributed
regions start at 2000, and why a port that lies behind the coastline reads as
no region until someone adds its waterway.

## Layout and ids

| dir | ids | content |
|---|---|---|
| `seas/` | 0-999 | Natural Earth 5.2 `ne_10m_geography_marine_polys` |
| `lakes/` | 1000-1999 | Natural Earth 5.2 `ne_10m_lakes`, scalerank <= 3 |
| `contrib/` | 2000-4095 | everything added here; also straits Natural Earth 5.2 lost |

Natural Earth data is public domain. "Made with Natural Earth. Free vector and
raster map data @ naturalearthdata.com".
