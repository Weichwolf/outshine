"""Complete positive water contacts in a declared delivered cell cohort."""

from collections import defaultdict

import numpy as np
import shapely
from water_cell_edges import edges


def lines(geometry):
    if geometry.geom_type == "LineString":
        if geometry.length > 0:
            yield geometry
    elif geometry.geom_type in ("MultiLineString", "GeometryCollection"):
        for part in shapely.get_parts(geometry):
            yield from lines(part)


def adjacent_cells(key):
    z, axis, x, y = key
    return ((z, x - 1, y), (z, x, y)) if axis == 0 else ((z, x, y - 1), (z, x, y))


def shared_borders(records, knots, *, known_cells, coordinate_limit=8192):
    """Keep provenance and unknown edges; point-only contacts never become lines.

    The caller supplies one already normalized/semantically assembled body and
    the explicit complete source cohort. No acquisition or completeness inference
    happens here. Disjoint positive intervals stay separate, including islands.
    """
    if type(coordinate_limit) is not int or not 1 <= coordinate_limit <= 8192:
        raise ValueError("bounded border coordinate work required")
    known = set(known_cells)
    if not {r[0] for r in records} <= known:
        raise ValueError("declared cohort omits a source cell")
    grouped = defaultdict(lambda: defaultdict(list))
    provenance = defaultdict(list)
    for tile, piece, parent, _source in records:
        for key, axis, fixed, along in edges(tile):
            lo = tile[along + 1]
            a = [0.0, 0.0]
            b = [0.0, 0.0]
            a[axis] = b[axis] = fixed
            a[along], b[along] = lo, lo + 1
            for line in lines(piece.polygon.intersection(shapely.LineString([a, b]))):
                grouped[key][tile].append(line)
                provenance[key].append((parent, line))
    results, open_edges, coordinates = [], [], 0
    for key, sides in sorted(grouped.items()):
        owners = adjacent_cells(key)
        unknown = tuple(t for t in owners if t not in known)
        if unknown:
            open_edges.append({"edge": key, "unknown_cells": unknown})
            continue
        if not all(t in sides for t in owners):
            continue
        common = shapely.union_all(sides[owners[0]]).intersection(
            shapely.union_all(sides[owners[1]])
        )
        parts = list(lines(shapely.line_merge(common)))
        if not parts:
            continue
        polylines, source_ids = [], set()
        along = 1 - key[1]
        for line in sorted(parts, key=lambda p: p.bounds):
            raw = np.asarray(line.coords)
            low, high = float(raw[:, along].min()), float(raw[:, along].max())
            published = knots.get(key, ())
            if low not in published or high not in published:
                raise ValueError("shared interval endpoint absent from published knots")
            values = [v for v in published if low <= v <= high]
            coordinates += len(values)
            if coordinates > coordinate_limit:
                raise ValueError("shared border coordinate budget exhausted")
            xy = np.empty((len(values), 2))
            xy[:, key[1]] = key[2 + key[1]]
            xy[:, along] = values
            polylines.append(xy)
            source_ids.update(
                parent
                for parent, original in provenance[key]
                if original.intersection(line).length > 0
            )
        results.append(
            {
                "edge": key,
                "cells": owners,
                "polylines": tuple(polylines),
                "source_keys": tuple(sorted(source_ids)),
            }
        )
    return results, {
        "open_edges": open_edges,
        "shared_edges": len(results),
        "published_border_coordinates": coordinates,
    }
