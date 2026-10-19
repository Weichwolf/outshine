"""CPU novel-view diagnostic for bounded, piecewise planar layered texels.

No source triangles are used by the layered query. Each hit is extended only
over its capture cell, using its stored normal, and clipped to capture bounds.
This sample-and-hold reconstruction is a proposal, never an opaque-coverage
certificate. Work counters describe this CPU reference, not a GPU implementation.
"""

import numpy as np
import trimesh
from packed_normals import unpack


def field_bounds(data):
    normal = unpack(data["normal"]) @ data["basis"].T
    valid = np.isfinite(data["depth"]) & (abs(normal[:, 2]) > 1e-8)
    low, high = data["low"].copy(), data["high"].copy()
    # Crop reconstruction to original capture bounds. A numeric guard accounts
    # for depth storage and the established Oct16x2 normal quantisation angle.
    # This defines the proposal's crop, not its source/coverage error bound.
    angle = np.arcsin(np.sqrt(18) / 65535)
    rounding = np.spacing(abs(data["depth"][valid])).max(initial=0)
    guard = float(rounding + np.linalg.norm(data["spacing"]) / 2 * np.tan(angle))
    low[2] -= guard
    high[2] += guard
    return low, high, normal, valid


def intersect_field(data, origins, directions):
    basis = data["basis"]
    origin = (origins - data["centre"]) @ basis.T
    direction = directions @ basis.T
    low, high, normal, supported = field_bounds(data)
    n = round(np.sqrt(len(data["counts"])))
    layers = len(data["depth"]) // (n * n)
    parallel = abs(direction) < 1e-14
    inverse = np.divide(1.0, direction, out=np.zeros_like(direction), where=~parallel)
    a, b = (low - origin) * inverse, (high - origin) * inverse
    near = np.where(parallel, -np.inf, np.minimum(a, b)).max(1)
    far = np.where(parallel, np.inf, np.maximum(a, b)).min(1)
    outside = (parallel & ((origin < low) | (origin > high))).any(1)
    entry = np.maximum(near, 0.1)
    active = (entry <= far) & ~outside
    cell = np.floor(
        (
            origin[:, :2]
            + np.where(active, entry, 0)[:, None] * direction[:, :2]
            - low[:2]
        )
        / data["spacing"]
    )
    cell = np.clip(cell, 0, n - 1).astype(np.int64)
    step = np.sign(direction[:, :2]).astype(np.int64)
    delta = np.divide(
        data["spacing"],
        abs(direction[:, :2]),
        out=np.full((len(origin), 2), np.inf),
        where=~parallel[:, :2],
    )
    boundary = low[:2] + (cell + (step > 0)) * data["spacing"]
    next_t = np.where(
        parallel[:, :2], np.inf, (boundary - origin[:, :2]) * inverse[:, :2]
    )
    answer = np.full(len(origin), np.inf)
    slots = np.full(len(origin), -1, np.int64)
    tested, visited, lookups = 0, 0, 0
    for _ in range(2 * n + 2):
        ids = np.flatnonzero(active)
        if not len(ids):
            break
        visited += len(ids)
        sample = cell[ids, 1] * n + cell[ids, 0]
        sample = sample[:, None] + np.arange(layers)[None, :] * n * n
        lookups += sample.size
        valid = supported[sample]
        normals = normal[sample]
        point_xy = low[:2] + (cell[ids] + 0.5) * data["spacing"]
        point = np.empty((len(ids), layers, 3))
        point[:, :, :2] = point_xy[:, None, :]
        point[:, :, 2] = data["low"][2] - 1 + data["depth"][sample]
        denominator = np.einsum("nli,ni->nl", normals, direction[ids])
        tested += int(valid.sum())
        front = valid & (denominator < -1e-12)
        numerator = np.einsum("nli,nli->nl", normals, point - origin[ids, None, :])
        t = np.divide(
            numerator, denominator, out=np.full_like(numerator, np.inf), where=front
        )
        crossing = np.minimum(next_t[ids, 0], next_t[ids, 1])
        valid = front & (t >= entry[ids, None] - 1e-8) & (t <= far[ids, None] + 1e-8)
        valid &= t <= crossing[:, None] + 1e-8
        t = np.where(valid, t, np.inf)
        chosen = np.argmin(t, axis=1)
        hit = t[np.arange(len(ids)), chosen]
        found = np.isfinite(hit)
        answer[ids[found]] = hit[found]
        slots[ids[found]] = sample[np.arange(len(ids)), chosen][found]
        active[ids[found]] = False
        # Corner crossings advance both axes. Avoid inf <= inf for axis-parallel rays.
        advance = np.isfinite(next_t[ids]) & (next_t[ids] <= crossing[:, None] + 1e-10)
        cell[ids] += advance * step[ids]
        next_t[ids] += np.where(advance, delta[ids], 0)
        entry[ids] = crossing
        active[ids] &= (crossing <= far[ids]) & np.isfinite(crossing)
        active[ids] &= ((cell[ids] >= 0) & (cell[ids] < n)).all(1)
    if active.any():
        raise AssertionError("bounded capture-cell traversal unfinished")
    return (
        answer,
        slots,
        {
            "query_rays": len(origin),
            "visited_ray_cells": visited,
            "layer_depth_lookups": lookups,
            "valid_plane_tests": tested,
            "field_bytes_examined_estimate": lookups * 4 + tested * 4,
            "unsupported_grazing_samples": int(
                (np.isfinite(data["depth"]) & ~supported).sum()
            ),
            "byte_scope": "Depth and normal operands counted per visit, not cache traffic or material fetches.",
        },
    )


def intersect_source(source, origins, directions):
    xyz, ids = source["positions"].astype(float), source["indices"]
    triangle = xyz[ids]
    normals = np.cross(triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0])
    # Camera rays differ per pixel; do not replace actual per-ray facing with centre facing.
    mesh = trimesh.Trimesh(xyz, ids, process=False)
    points, rays, faces = mesh.ray.intersects_location(
        origins, directions, multiple_hits=True
    )
    front = np.einsum("ij,ij->i", normals[faces], directions[rays]) < -1e-12
    points, rays, faces = points[front], rays[front], faces[front]
    t = np.einsum("ij,ij->i", points - origins[rays], directions[rays])
    t /= np.einsum("ij,ij->i", directions[rays], directions[rays])
    keep = t > 0.1
    order = np.lexsort((faces[keep], t[keep], rays[keep]))
    points, rays, faces, t = (a[keep][order] for a in (points, rays, faces, t))
    first = np.r_[True, rays[1:] != rays[:-1]] if len(rays) else np.empty(0, bool)
    result = np.full(len(origins), np.inf)
    owner = np.full(len(origins), -1, np.int64)
    result[rays[first]], owner[rays[first]] = t[first], faces[first]
    return result, owner


def view_rays(view, positions):
    eye, basis, focal, width, height = view
    low, high = positions.min(0), positions.max(0)
    corners = np.array(
        [
            [x, y, z]
            for x in (low[0], high[0])
            for y in (low[1], high[1])
            for z in (low[2], high[2])
        ]
    )
    local = (corners - eye) @ basis.T
    if (local[:, 2] <= 0.1).any():
        raise ValueError("diagnostic currently requires bounds in front of near plane")
    xy = [width / 2, height / 2] + focal * local[:, :2] / local[:, 2, None] * [1, -1]
    lo = np.maximum(np.floor(xy.min(0)).astype(int) - 2, 0)
    hi = np.minimum(np.ceil(xy.max(0)).astype(int) + 2, [width, height])
    y, x = np.mgrid[lo[1] : hi[1], lo[0] : hi[0]]
    rays = (
        np.column_stack(
            (
                (x.ravel() + 0.5 - width / 2) / focal,
                -(y.ravel() + 0.5 - height / 2) / focal,
                np.ones(x.size),
            )
        )
        @ basis
    )
    return np.broadcast_to(eye, rays.shape), rays, (y.ravel(), x.ravel())
