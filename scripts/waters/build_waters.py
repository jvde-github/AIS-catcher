#!/usr/bin/env python3
"""Turns data/regions (one geojson per sea, gulf, strait or lake) into water place files.

Each water becomes a place of type `water`, its kind in attributes.area_subtype. The outlines
are simplified as a coverage: where two waters overlap the smaller wins, shared edges are
simplified once for both sides so borders between seas stay consistent, and along a coast an
outline may go wider than the shoreline, never into a neighbour's water. Ids are stable
(uuid5 of the source id), so a rebuild updates a place rather than creating a new one.

    build_waters.py <out dir> [--src data/regions]

Needs shapely >= 2.0.
"""
import argparse, glob, json, os, re, sys, uuid

import numpy as np
import shapely
from shapely import make_valid
from shapely.geometry import shape, mapping, Polygon, MultiPoint
from shapely.ops import unary_union
from shapely.strtree import STRtree

SHARED_TOL = 0.01  # simplification of the edges between waters, about 1 km
MAX_VERTICES = 2000  # the catalogue's limit per place
NAMESPACE = "https://aiscatcher.org/water/"
LAKES = {"lake", "alkaline lake", "reservoir"}
KIND_WORDS = ("ocean", "sea", "gulf", "bay", "strait", "channel", "sound", "passage", "lagoon",
              "fjord", "inlet", "reef", "lake", "river", "canal", "bight", "basin", "estuary")


def subtype(props):
    kind = (props.get("featurecla") or "").strip().lower()
    if kind in LAKES:
        return "lake"
    if kind and kind != "generic":
        return kind
    words = re.findall(r"[a-z]+", (props.get("name") or "").lower())
    for w in KIND_WORDS:
        if w in words:
            return w
    return "sea"


def tolerance(area):
    """Simplification in degrees by size: 2 km for a bay, 60 km for an ocean."""
    for limit, tol in ((1, 0.02), (20, 0.05), (200, 0.1), (2000, 0.2)):
        if area < limit:
            return tol
    return 0.6


def parts_of(geom):
    return [geom] if geom.geom_type == "Polygon" else [g for g in geom.geoms if g.geom_type == "Polygon"]


def vertices(geom):
    return sum(len(g.exterior.coords) + sum(len(r.coords) for r in g.interiors) for g in parts_of(geom))


def solid(geom):
    parts = parts_of(geom)
    return unary_union([Polygon(g.exterior) for g in parts]) if parts else geom


def fill(geom, tol, holes=True):
    parts = [Polygon(g.exterior) if holes else g for g in parts_of(geom) if g.area >= tol * tol]
    return unary_union(parts) if parts else geom


def widen(geom, exact, tol):
    boundary = exact.boundary if exact is not None else None
    out = []
    for part in parts_of(geom):
        coords = list(part.exterior.coords)[:-1]
        if len(coords) < 3:
            continue
        if boundary is None:
            out.append(part.convex_hull)
            continue
        locked = shapely.distance(shapely.points(coords), boundary) < 1e-7
        if not locked.any():
            out.append(part.convex_hull)
            continue
        first = int(locked.argmax())
        coords = coords[first:] + coords[:first]
        locked = list(locked[first:]) + list(locked[:first])
        bulges = [part]
        run = [coords[0]]
        for c, l in zip(coords[1:] + coords[:1], locked[1:] + locked[:1]):
            run.append(c)
            if l:
                if len(run) > 2:
                    bulges.append(MultiPoint(run).convex_hull)
                run = [c]
        widened = make_valid(unary_union(bulges))
        out.append(widened.difference(exact) if exact is not None else widened)
    return unary_union(out) if out else geom


def load(src):
    waters = []
    for path in sorted(glob.glob(os.path.join(src, "**", "*.geojson"), recursive=True)):
        j = json.load(open(path, encoding="utf-8"))
        for f in (j["features"] if j.get("type") == "FeatureCollection" else [j]):
            p = f["properties"]
            geom = shape(f["geometry"])
            if not geom.is_valid:
                pieces = [g for g in getattr(make_valid(geom), "geoms", [make_valid(geom)]) if g.geom_type in ("Polygon", "MultiPolygon")]
                if not pieces:
                    sys.exit("%s: geometry cannot be repaired" % path)
                geom = unary_union(pieces)
            if geom.is_empty:
                continue
            if not (p.get("name") or "").strip():
                print("warning: %s has no name, skipped" % os.path.relpath(path, src))
                continue
            waters.append(dict(id=int(p["id"]), name=p.get("name", ""), kind=subtype(p), orig=geom,
                               rank_area=geom.area, tol=float(p.get("tolerance", tolerance(geom.area))),
                               source=p.get("source", ""), wikidata=p.get("wikidataid", "")))
    if not waters:
        sys.exit("no waters under " + src)
    return waters


def coverage(waters):
    """The stashed zone builder's hull pass, unchanged in substance."""
    regions = sorted(waters, key=lambda z: z["rank_area"])
    tree = STRtree([z["orig"] for z in regions])
    pieces = []
    for i, z in enumerate(regions):
        smaller = [regions[j]["orig"] for j in tree.query(z["orig"]) if j != i and regions[j]["rank_area"] < z["rank_area"]]
        g = z["orig"].difference(unary_union(smaller)) if smaller else z["orig"]
        pieces.append(g if g.is_valid else make_valid(g))
    pieces = list(shapely.coverage_simplify(pieces, SHARED_TOL, simplify_boundary=True))
    ptree = STRtree(pieces)
    for i, z in enumerate(regions):
        piece = pieces[i]
        if piece.is_empty:
            z["geom"] = None
            continue
        tol = z["tol"]
        larger = [pieces[j] for j in ptree.query(piece.convex_hull) if j != i and regions[j]["rank_area"] >= z["rank_area"]]
        exact = make_valid(solid(unary_union(larger)).difference(piece)) if larger else None

        def outline(base):
            if exact is not None:
                base = make_valid(base.difference(exact))
            base = widen(base, exact, tol)
            return fill(base, tol, holes=False)

        hull = outline(unary_union([g.convex_hull for g in parts_of(piece)]))
        coast = outline(fill(piece.buffer(tol / 2, join_style=2).simplify(tol, preserve_topology=True), tol))
        spans_globe = hull.bounds[2] - hull.bounds[0] > 180 if not hull.is_empty else True
        best = hull if not spans_globe and vertices(hull) < vertices(coast) else coast
        coarse = fill(make_valid(best.buffer(tol / 4, join_style=2).simplify(tol / 4, preserve_topology=True)), tol, holes=False)
        if exact is not None:
            coarse = fill(make_valid(coarse.difference(exact)), tol, holes=False)
        geom = coarse if not coarse.is_empty and vertices(coarse) < vertices(best) else best
        # the catalogue takes at most MAX_VERTICES per place: coarsen until it fits
        t = tol
        while vertices(geom) > MAX_VERTICES:
            t *= 1.5
            geom = fill(make_valid(geom.simplify(t, preserve_topology=True)), tol, holes=False)
        geom = clean(geom, tol)
        if geom is None:
            print("warning: %s (%d) could not be made valid for the catalogue, skipped" % (z["name"], z["id"]))
        z["geom"] = geom if geom is not None and not geom.is_empty else None


WORLD = shapely.box(-180, -90, 180, 90)
GAP = 1e-4  # about ten metres between parts that would otherwise touch
QUARTERS = [shapely.box(x + GAP, -90, x + 90 - GAP, 90) for x in (-180, -90, 0, 90)]


def strict_ok(geom):
    """The catalogue's test is stricter than OGC validity and runs in single precision:
    every ring simple, parts never touching, no edge spanning the antimeridian, no repeated
    vertex. Checked on the coordinates as the catalogue will see them, with a margin."""
    parts = parts_of(geom)
    if not parts or not geom.is_valid:
        return False
    rounded = []
    for g in parts:
        if g.interiors:
            return False
        coords = np.asarray(g.exterior.coords, dtype=np.float32).astype(np.float64)
        if len(coords) < 4 or np.abs(np.diff(coords[:, 0])).max() > 180:
            return False
        if (np.abs(np.diff(coords, axis=0)).max(axis=1) < 1e-6).any():
            return False
        ring = shapely.LinearRing(coords)
        if not ring.is_simple or not ring.is_valid:
            return False
        rounded.append(Polygon(coords))
    for i in range(len(rounded)):
        for j in range(i + 1, len(rounded)):
            if rounded[i].distance(rounded[j]) < 5e-5:
                return False
    return True


def settle(geom, tol):
    """Pulls touching parts apart and opens pinched rings, by a metre and then by more."""
    for eps in (1e-5, 3e-5, 1e-4, 3e-4, 1e-3, 3e-3):
        candidate = make_valid(geom.buffer(-eps, join_style="mitre").buffer(eps, join_style="mitre"))
        candidate = make_valid(candidate.simplify(eps / 2, preserve_topology=True))
        candidate = shapely.remove_repeated_points(fill(candidate, tol, holes=True), 1e-7)
        if strict_ok(candidate):
            return candidate
    hulls = unary_union([g.convex_hull for g in parts_of(geom)])
    candidate = shapely.remove_repeated_points(make_valid(hulls.buffer(-GAP, join_style="mitre").buffer(GAP, join_style="mitre")), 1e-7)
    return candidate if strict_ok(candidate) else None


def clean(geom, tol):
    """Nothing beyond the world box (an outline buffered across the antimeridian or over a
    pole is cut, and comes back as parts with their own bounding boxes). A water that reaches
    the antimeridian is cut into longitude quarters, kept apart by a small gap, so no edge
    runs round the globe. No overlapping or touching parts, no ring touching itself."""
    geom = make_valid(geom).intersection(WORLD)
    if geom.is_empty:
        return geom
    b = geom.bounds
    if b[0] <= -180 or b[2] >= 180 or b[2] - b[0] > 180:
        pieces = [settle(unary_union(parts_of(geom.intersection(q))), tol) for q in QUARTERS if not geom.intersection(q).is_empty]
        pieces = [p for p in pieces if p is not None and not p.is_empty]
        result = shapely.geometry.MultiPolygon([g for p in pieces for g in parts_of(p)]) if pieces else geom
        return result if strict_ok(result) else None
    return settle(unary_union(parts_of(geom)) if parts_of(geom) else geom, tol)


def feature(z):
    uid = str(uuid.uuid5(uuid.NAMESPACE_URL, NAMESPACE + str(z["id"])))
    props = {"schema_version": 2, "revision": 1, "place_type": "water", "name": z["name"],
             "attributes": {"area_subtype": z["kind"]}}
    source = {}
    if z["source"]:
        source["source"] = z["source"]
    if z["wikidata"]:
        source["url"] = "https://www.wikidata.org/wiki/" + z["wikidata"]
    if source:
        props["geometry_source"] = source
    return uid, {"type": "Feature", "id": uid, "properties": props, "geometry": mapping(z["geom"])}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--src", default=os.path.join(os.path.dirname(__file__), "..", "..", "data", "regions"))
    a = ap.parse_args()
    waters = load(a.src)
    coverage(waters)
    os.makedirs(a.out, exist_ok=True)
    total = 0
    for z in waters:
        if z["geom"] is None:
            print("warning: %s (%d) has no area left, skipped" % (z["name"], z["id"]))
            continue
        uid, f = feature(z)
        with open(os.path.join(a.out, uid + ".geojson"), "w", encoding="utf-8") as fh:
            json.dump(f, fh, separators=(",", ":"))
        total += vertices(z["geom"])
    print("%d waters, %d vertices" % (sum(1 for z in waters if z["geom"] is not None), total))


if __name__ == "__main__":
    main()
