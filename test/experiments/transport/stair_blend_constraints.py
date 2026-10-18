"""Whole affine bridge bounds and conforming refinement of offending soil cells."""

import numpy as np
import shapely
from scipy import sparse
from stair_terrain_solid import edges_of


def intersections(xy, faces, upper, footprint, *, reserve_m=0.1):
    if type(reserve_m) not in (float, int) or reserve_m not in (0, 0.1):
        raise ValueError("explicit zero or original bridge reserve required")
    if len(faces) > 32768 or len(upper.triangles) > 8192:
        raise ValueError("bridge-bound input face cap exhausted")
    triangles = xy[faces]
    high = upper.vertices[upper.triangles]
    shapes = shapely.polygons(high[:, :, :2])
    tree = shapely.STRtree(shapes)
    gradients = np.linalg.solve(
        high[:, 1:, :2] - high[:, :1, :2],
        (high[:, 1:, 2] - high[:, :1, 2])[..., None],
    )[..., 0]
    pairs = 0
    for i, shape in enumerate(shapely.polygons(triangles)):
        for j in tree.query(shape, predicate="intersects"):
            pairs += 1
            if pairs > 1000000:
                raise ValueError("bridge-bound candidate cap exhausted")
            region = shape.intersection(shapes[j])
            if region.area <= 1e-12:
                continue
            points = shapely.get_coordinates(region)[:-1]
            uv = np.linalg.solve(
                (triangles[i, 1:] - triangles[i, :1]).T,
                (points - triangles[i, 0]).T,
            ).T
            weights = np.c_[1 - uv.sum(axis=1), uv]
            ceiling = high[j, 0, 2] + (points - high[j, 0, :2]) @ gradients[j]
            offset = reserve_m * np.minimum(
                1, shapely.distance(shapely.points(points), footprint)
            )
            yield i, weights, ceiling - offset


def upper_rows(xy, faces, upper, footprint, *, reserve_m=0.1):
    rows, columns, values, bounds = [], [], [], []
    regions = 0
    for i, weights, ceiling in intersections(
        xy, faces, upper, footprint, reserve_m=reserve_m
    ):
        regions += 1
        if len(bounds) + len(ceiling) > 65536:
            raise ValueError("bridge-bound row cap exhausted")
        for w, height in zip(weights, ceiling, strict=True):
            rows.extend([len(bounds)] * 3)
            columns.extend(faces[i])
            values.extend(w)
            bounds.append(height)
    matrix = sparse.csc_matrix((values, (rows, columns)), shape=(len(bounds), len(xy)))
    note = {"regions": regions, "rows": len(bounds)}
    if reserve_m != 0.1:
        note["additional_distance_reserve_m"] = reserve_m
    return matrix, np.array(bounds), note


def violating_faces(layout, heights, upper, footprint):
    ids = np.flatnonzero(~layout["ground"])
    faces = layout["faces"][ids]
    result = set()
    for i, weights, ceiling in intersections(layout["xy"], faces, upper, footprint):
        if np.max(weights @ heights[faces[i]] - ceiling) > 1e-7:
            result.add(int(ids[i]))
    return result


def refine(layout, bad, sample):
    xy, faces, kinds, targets, bed = (
        layout[k] for k in ("xy", "faces", "ground", "targets", "bed")
    )
    edges = edges_of(faces)
    selected = set()
    for i in sorted(bad):
        for a, b in zip(faces[i], np.roll(faces[i], -1), strict=True):
            key = tuple(sorted((int(a), int(b))))
            if len(edges[key]) == 2 and not any(kinds[r[0]] for r in edges[key]):
                selected.add(key)
    if len(xy) + len(selected) + len(bad) > 16384:
        raise ValueError("refined soil vertex cap exhausted")
    points, mids, centres = xy.tolist(), {}, {}
    for a, b in sorted(selected):
        mids[a, b] = len(points)
        points.append(((xy[a] + xy[b]) / 2).tolist())
    for i in sorted(bad):
        centres[i] = len(points)
        points.append(xy[faces[i]].mean(axis=0).tolist())
    result, owners, parents = [], [], []
    for i, (original, kind) in enumerate(zip(faces, kinds, strict=True)):
        chain, children = [], [tuple(map(int, original))]
        for a, b in zip(original, np.roll(original, -1), strict=True):
            key = tuple(sorted((int(a), int(b))))
            chain.append(int(a))
            if key not in mids:
                continue
            chain.append(mids[key])
            at = next(k for k, t in enumerate(children) if a in t and b in t)
            other = next(v for v in children[at] if v not in (a, b))
            children[at : at + 1] = [(a, mids[key], other), (mids[key], b, other)]
        if i in centres:
            children = [
                (a, b, centres[i])
                for a, b in zip(chain, chain[1:] + chain[:1], strict=True)
            ]
        result.extend(children)
        owners.extend([kind] * len(children))
        parents.extend([i] * len(children))
    if len(result) > 32768:
        raise ValueError("refined soil face cap exhausted")
    positions = np.array(points)
    return {
        "xy": positions,
        "faces": np.array(result),
        "ground": np.array(owners),
        "targets": np.r_[targets, sample(positions[len(xy) :])],
        "bed": np.r_[bed, np.full(len(points) - len(xy), np.nan)],
        "parent_faces": np.array(parents),
    }, {"refined_cells": len(bad), "edge_midpoints": len(mids)}
