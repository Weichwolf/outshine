"""Bounded complete triangle/cell incidences, not visible opacity or a renderer."""

import numpy as np


def overlap(triangle, centres):
    """Closed unit cubes, 13 SAT axes; Float64 allowance is not certification."""
    edges = np.roll(triangle, -1, axis=0) - triangle
    axes = np.vstack(
        (
            np.eye(3),
            np.cross(edges[0], edges[1]),
            np.cross(np.eye(3)[:, None], edges[None]).reshape(-1, 3),
        )
    )
    keep = np.ones(len(centres), bool)
    tests = 0
    for axis in axes:
        if not axis.any():
            continue
        values = triangle @ axis
        shift = centres @ axis
        radius = 0.5 * abs(axis).sum()
        guard = 64 * np.finfo(float).eps * (abs(values).max() + abs(shift) + radius)
        keep &= (values.min() - shift <= radius + guard) & (
            values.max() - shift >= -radius - guard
        )
        tests += len(centres)
        if not keep.any():
            break
    return keep, tests


def assemble(
    triangles,
    *,
    pitch,
    candidate_limit=4194304,
    face_limit=131072,
    incidence_limit=524288,
):
    """Atomic per-face fallback; affordable unrelated faces continue.

    Coordinates are explicit caller-frame metres. The enclosing grid origin is
    fixed by the whole input, never the eye. Face IDs address the immutable input.
    Grid, candidate and output limits precede corresponding large allocations.
    """
    triangles = np.asarray(triangles, float)
    if (
        triangles.ndim != 3
        or triangles.shape[1:] != (3, 3)
        or len(triangles) > 131072
        or not np.isfinite(triangles).all()
        or (abs(triangles) > 1e9).any()
    ):
        raise ValueError("bounded finite original triangles required")
    if not np.isfinite(pitch) or pitch < 1e-6:
        raise ValueError("finite pitch >= 1 micrometre required")
    for value, maximum in (
        (candidate_limit, 4194304),
        (face_limit, 131072),
        (incidence_limit, 524288),
    ):
        if type(value) is not int or not 0 <= value <= maximum:
            raise ValueError("bounded nonnegative integer limits required")
    count = len(triangles)
    reasons = np.zeros(count, np.uint8)
    work = {
        "input_faces": count,
        "candidate_cells": 0,
        "axis_projection_pairs": 0,
        "accepted_incidences": 0,
        "detected_incidences": 0,
        "max_candidate_batch": 0,
        "candidate_coordinate_bytes": 0,
    }
    origin = np.zeros(3)
    if count:
        origin = np.floor(triangles.min((0, 1)) / pitch) * pitch - pitch
        if ((triangles.max((0, 1)) - origin) / pitch > 65533).any():
            reasons[:] = 2
    grid = (triangles - origin) / pitch
    work["normalised_triangle_array_bytes"] = grid.nbytes
    keys, faces = [], []
    for face, triangle in enumerate(grid):
        if reasons[face]:
            continue
        if not np.cross(triangle[1] - triangle[0], triangle[2] - triangle[0]).any():
            reasons[face] = 1
            continue
        # Closed cells on both sides of an exactly integral source boundary.
        low = np.ceil(np.nextafter(triangle.min(0), -np.inf)).astype(np.int64) - 1
        high = np.floor(np.nextafter(triangle.max(0), np.inf)).astype(np.int64)
        shape = high - low + 1
        candidates = int(np.prod(shape))
        if candidates > face_limit:
            reasons[face] = 3
            continue
        if candidates > candidate_limit - work["candidate_cells"]:
            reasons[face] = 4
            continue
        work["candidate_cells"] += candidates
        hits = []
        for first in range(0, candidates, 32768):
            linear = np.arange(first, min(first + 32768, candidates))
            xyz = np.column_stack(np.unravel_index(linear, tuple(shape))) + low
            work["candidate_coordinate_bytes"] += xyz.nbytes
            work["max_candidate_batch"] = max(work["max_candidate_batch"], len(xyz))
            accepted, tests = overlap(triangle, xyz + 0.5)
            work["axis_projection_pairs"] += tests
            xyz = xyz[accepted].astype(np.uint64)
            hits.append(xyz[:, 0] | xyz[:, 1] << 16 | xyz[:, 2] << 32)
        hit = np.concatenate(hits)
        work["detected_incidences"] += len(hit)
        if len(hit) > incidence_limit - work["accepted_incidences"]:
            reasons[face] = 5
            continue
        if not len(hit):
            raise ValueError("nondegenerate face has no cell; retain original input")
        keys.append(hit)
        faces.append(np.full(len(hit), face, np.uint32))
        work["accepted_incidences"] += len(hit)
    raw_keys = np.concatenate(keys) if keys else np.empty(0, np.uint64)
    raw_faces = np.concatenate(faces) if faces else np.empty(0, np.uint32)
    order = np.lexsort((raw_faces, raw_keys))
    ordered = raw_keys[order]
    starts = (
        np.flatnonzero(np.r_[True, ordered[1:] != ordered[:-1]])
        if len(ordered)
        else np.empty(0, np.int64)
    )
    unique = ordered[starts]
    coordinates = np.column_stack(
        (unique & 65535, unique >> 16 & 65535, unique >> 32)
    ).astype(np.uint16)
    result = {
        "origin": origin,
        "pitch": np.array(pitch),
        "coordinates": coordinates,
        "offsets": np.r_[starts, len(ordered)].astype(np.uint32),
        "original_faces": raw_faces[order],
        "fallback": np.flatnonzero(reasons).astype(np.uint32),
        "reasons": reasons,
    }
    represented = np.unique(result["original_faces"])
    if not np.array_equal(
        np.sort(np.r_[represented, result["fallback"]]), np.arange(count)
    ):
        raise ValueError("missing or duplicated original face disposition")
    work.update(
        occupied_cells=len(coordinates),
        fallback_faces=len(result["fallback"]),
        output_array_bytes=sum(a.nbytes for a in result.values()),
        concatenated_incidence_bytes=raw_keys.nbytes + raw_faces.nbytes,
        sort_index_bytes=order.nbytes,
        scope="complete known surface occupancy plus original fallback; no opacity/material/image admission",
    )
    return result, work
