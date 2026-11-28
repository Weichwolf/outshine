"""Root broadleaf trees on the mapped verges of riverside park paths."""

import numpy as np
import shapely
from ground_surface import ROAD_WIDTH, at_grade_width, route_area
from park_avenue import shared_park_route
from plant_sites import patch_density, tile_sites
from woodland_sites import spaced_sites

RIVER_REACH_M = 65.0
VERGE_REACH_M = 11.0
BRIDGE_CLEARANCE_M = 7.0
PATH_KINDS = {"footway", "cycleway", "bridleway", "path"}
OPEN_PARK_COVER = {"allotments", "flowerbed", "garden", "grassland", "meadow"}


def riverside_verges(features, extent):
    parks = [
        shapely.make_valid(geom)
        for layer, geom, tags in features
        if layer == "landcover" and tags.get("subclass") == "park"
    ]
    rivers = [
        shapely.make_valid(geom)
        for layer, geom, tags in features
        if layer == "water" and tags.get("class") == "river"
    ]
    if not parks or not rivers:
        return shapely.Polygon()
    river_border = shapely.union_all(rivers).buffer(RIVER_REACH_M, quad_segs=2)
    bank_parks = (
        shapely.union_all(parks).intersection(extent).intersection(river_border)
    )
    if bank_parks.is_empty:
        return bank_parks
    nearby = bank_parks.buffer(32.0, quad_segs=2)
    shapely.prepare(nearby)

    reserved, paths = [], []
    for layer, geom, tags in features:
        if not nearby.intersects(geom):
            continue
        kind, detail = tags.get("class"), tags.get("subclass")
        if layer == "water":
            reserved.append(shapely.make_valid(geom).buffer(2.0, quad_segs=2))
        elif layer == "building" or (layer == "landcover" and kind == "wood"):
            clearance = 4.0 if kind == "wood" else 2.0
            reserved.append(shapely.make_valid(geom).buffer(clearance, quad_segs=2))
        elif layer == "landcover" and kind == "grass":
            if detail == "recreation_ground":
                reserved.append(shapely.make_valid(geom).buffer(-5.0, quad_segs=2))
            elif detail in OPEN_PARK_COVER:
                reserved.append(shapely.make_valid(geom))
        elif layer == "transportation":
            if kind == "bridge" or tags.get("brunnel") == "bridge":
                deck = route_area(geom, ROAD_WIDTH.get(kind, 6.0))
                reserved.append(deck.buffer(BRIDGE_CLEARANCE_M, quad_segs=2))
                continue
            width = at_grade_width(tags)
            if width is None:
                continue
            if shared_park_route(tags):
                reserved.append(geom.buffer(32.0, quad_segs=2))
            else:
                reserved.append(route_area(geom, width).buffer(1.35, quad_segs=2))
            if kind == "path" and detail in PATH_KINDS:
                paths.extend(
                    part
                    for part in shapely.get_parts(geom)
                    if part.geom_type == "LineString" and part.length >= 24.0
                )
    corridors = shapely.union_all(shapely.buffer(paths, VERGE_REACH_M, quad_segs=3))
    verge = bank_parks.intersection(corridors)
    blocked = [geom for geom in reserved if geom.intersects(verge)]
    return verge.difference(shapely.union_all(blocked))


def verge_tree_sites(source, domain, seed, receipts):
    if domain.is_empty:
        return np.empty((0, 2)), np.empty((0, 3))
    lo, hi = np.asarray(domain.bounds).reshape(2, 2)
    centre = (lo + hi) / 2
    candidates = tile_sites(
        source,
        centre,
        np.linalg.norm(hi - centre) + 0.1,
        seed ^ 0x4E91,
        density=0.036,
        cell_size=12,
        receipts=receipts,
    )
    points, marks = [], []
    for xy, values in candidates:
        inside = shapely.contains_xy(domain, *xy.T)
        points.append(xy[inside])
        marks.append(values[inside])
    if not points:
        return np.empty((0, 2)), np.empty((0, 3))
    xy, values = np.concatenate(points), np.concatenate(marks)
    ranks = patch_density(source, xy, seed ^ 0x863D, scale=0.125)
    keep = spaced_sites(xy, ranks, 5.5)
    values[:, 2] = 0.85 + 0.49 * values[:, 2]
    return xy[keep], values[keep]
