"""WHAT THE GROUND IS, WHERE OSM SAYS SO -- and it is not grass everywhere.

A city twin whose every gap between the buildings is a green field is a twin that has thrown away
half the extract. OSM states the ground: a market square is paved, a car park is asphalt with
bays, a park is grass with trees in it, a river is water at a level, a rail corridor is ballast.
All of it is AREA data -- `landuse`, `natural`, `leisure`, `amenity=parking`, `place=square`,
`highway=pedestrian` with `area=yes` -- and none of it is invention.

The rules are two, and both are about what covers what:

    a PRIORITY orders them, because OSM polygons overlap freely: a square inside a park inside a
    residential landuse is three polygons over one piece of ground and only the top one is seen
    the STREET always wins, because a road drawn over a car park and a car park drawn over the
    same road are two surfaces in one place, and the road is the one that was solved

What comes out is a list of (role, polygon) with nothing overlapping, which `ground.py` cuts its
one welded sheet along -- so the terrain, the square and the water are one surface with several
materials and no crack between them.
"""
import math

import numpy as np
from shapely.geometry import Polygon
from shapely.ops import unary_union

# WHAT A TAG MEANS ON THE GROUND, most specific first. The priority is the ORDER: an entry
# earlier in the list covers one later, which is what a surveyor means by drawing one inside
# the other. Every role names a material in `materials.py`.
KINDS = (
    ("water",    (("natural", "water"), ("waterway", "riverbank"), ("waterway", "dock"),
                  ("landuse", "reservoir"), ("landuse", "basin"))),
    ("paving",   (("place", "square"), ("highway", "pedestrian"), ("highway", "footway"),
                  ("amenity", "marketplace"), ("man_made", "pier"))),
    ("asphalt",  (("amenity", "parking"), ("amenity", "bicycle_parking"),
                  ("landuse", "garages"))),
    ("ballast",  (("landuse", "railway"),)),
    ("sand",     (("natural", "sand"), ("natural", "beach"))),
    ("gravel",   (("surface", "gravel"), ("natural", "scree"), ("natural", "bare_rock"))),
    ("soil",     (("landuse", "farmland"), ("landuse", "allotments"),
                  ("landuse", "orchard"), ("landuse", "vineyard"))),
    ("crop",     (("landuse", "meadow"), ("landuse", "grass"), ("landuse", "village_green"),
                  ("leisure", "park"), ("leisure", "garden"), ("leisure", "pitch"),
                  ("landuse", "forest"), ("natural", "wood"), ("natural", "grassland"),
                  ("landuse", "cemetery"), ("amenity", "grave_yard"))),
)
AREA_ONLY = ("highway", "man_made")   # these need `area=yes` before they are a surface at all
LEAST_M2 = 20.0           # [SET] below this a polygon is a mapping artefact, not a surface


def _closed(nodes, refs, frame):
    if len(refs) < 4 or refs[0] != refs[-1]:
        return None
    pts = []
    for r in refs[:-1]:
        got = nodes.get(r)
        if got is None:
            return None
        pts.append(frame.xy(*got))
    if len(pts) < 3:
        return None
    poly = Polygon(pts)
    if not poly.is_valid:
        poly = poly.buffer(0)
    return poly if (not poly.is_empty and poly.area >= LEAST_M2) else None


def role_of(tags):
    """Which surface a way's tags name, or None. `building` is never a surface: a building has
    a body and a body has its own ground."""
    if "building" in tags or "building:part" in tags:
        return None
    for role, pairs in KINDS:
        for key, value in pairs:
            if tags.get(key) != value:
                continue
            if key in AREA_ONLY and str(tags.get("area", "")).lower() not in ("yes", "true", "1"):
                continue
            return role
    return None


def regions(doc, frame, reach_m=1e9, taken=None):
    """The ground's surfaces as (role, polygon), by priority, nothing overlapping anything.

    `taken` is what is already spoken for -- the street's own footprint -- and it is subtracted
    from every one of them, because a road drawn over a car park puts two surfaces in one place."""
    nodes = {e["id"]: (e["lat"], e["lon"]) for e in doc["elements"] if e["type"] == "node"}
    order = {role: k for k, (role, _) in enumerate(KINDS)}
    found = {}
    for e in doc["elements"]:
        if e.get("type") != "way" or "nodes" not in e:
            continue
        role = role_of(e.get("tags") or {})
        if role is None:
            continue
        poly = _closed(nodes, e["nodes"], frame)
        if poly is None:
            continue
        x0, y0, x1, y1 = poly.bounds
        if min(abs(x0), abs(x1)) > reach_m or min(abs(y0), abs(y1)) > reach_m:
            continue
        found.setdefault(role, []).append(poly)
    out = []
    covered = taken if (taken is not None and not taken.is_empty) else None
    for role in sorted(found, key=lambda r: order[r]):
        got = unary_union(found[role])
        if covered is not None:
            got = got.difference(covered)
        if got.is_empty or got.area < LEAST_M2:
            continue
        out.append((role, got))
        covered = got if covered is None else unary_union([covered, got])
    return tuple(out)


def check_no_overlap(patches, street=None):
    """I22: NO TWO SURFACES STAND IN ONE PLACE. The overlapping area, in square metres.

    The control is the same set unresolved -- OSM's polygons as they come, where a square inside
    a park inside a residential landuse is three surfaces over one piece of ground."""
    worst = 0.0
    for i, (_, a) in enumerate(patches):
        for (_, b) in patches[i + 1:]:
            worst += float(a.intersection(b).area)
        if street is not None and not street.is_empty:
            worst += float(a.intersection(street).area)
    return worst
