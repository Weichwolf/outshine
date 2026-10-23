"""Candidate-only packet traversal; no source forms, meshes or source queries."""

import numpy as np
from trilinear_support import (
    CORNERS,
    LEAF,
    PAYLOAD,
    SOLID,
    UNKNOWN,
    first_surface,
    intervals,
)


def validate(arrays):
    tags, values = arrays["tags"], arrays["values"]
    if tags.ndim != 1 or not 1 <= len(tags) <= 65535 or tags.dtype != np.uint32:
        raise ValueError("bounded UInt32 field topology required")
    if (
        values.ndim != 2
        or values.shape[1] != 8
        or values.dtype != np.float32
        or not np.isfinite(values).all()
    ):
        raise ValueError("finite Float32 scalar corners required")
    if (
        arrays["root_low"].shape != (3,)
        or not np.isfinite(arrays["root_low"]).all()
        or not np.isfinite(arrays["root_side"])
        or arrays["root_side"] <= 0
    ):
        raise ValueError("finite cubic root required")
    parents = np.zeros(len(tags), np.uint32)
    for node, raw in enumerate(tags):
        tag = int(raw)
        if tag == 0 or tag == SOLID or tag & UNKNOWN:
            continue
        if tag & LEAF:
            if tag & SOLID or (tag & PAYLOAD) >= len(values):
                raise ValueError("field leaf payload mismatch")
            continue
        if tag <= node or tag + 8 > len(tags):
            raise ValueError("forward bounded octree children required")
        parents[tag : tag + 8] += 1
    if parents[0] != 0 or np.any(parents[1:] != 1):
        raise ValueError("each nonroot node must have one parent")


def active_bounds(arrays):
    low, high = np.full(3, np.inf), np.full(3, -np.inf)
    stack = [(0, arrays["root_low"], float(arrays["root_side"]))]
    while stack:
        node, origin, side = stack.pop()
        tag = int(arrays["tags"][node])
        if tag == 0:
            continue
        if tag == SOLID or tag & (LEAF | UNKNOWN):
            low, high = np.minimum(low, origin), np.maximum(high, origin + side)
        else:
            stack.extend(
                (tag + i, origin + CORNERS[i] * (side / 2), side / 2) for i in range(8)
            )
    if not np.isfinite(low).all():
        raise ValueError("no active field domain")
    return np.stack((low, high))


def packet(arrays, origins, directions, stop, minimum):
    depth = stop.copy()
    normals = np.zeros_like(origins)
    unknown = np.full(len(origins), np.inf)
    hits = np.zeros(len(origins), bool)
    work = {
        "node_ray_tests": 0,
        "leaf_ray_queries": 0,
        "polynomial_ray_queries": 0,
        "polynomial_roots": 0,
        "box_cap_hits": 0,
        "unknown_ray_box_visits": 0,
        "max_pending_ray_indices": 0,
    }
    stack = [
        (0, arrays["root_low"], float(arrays["root_side"]), np.arange(len(origins)))
    ]
    while stack:
        work["max_pending_ray_indices"] = max(
            work["max_pending_ray_indices"], sum(len(row[3]) for row in stack)
        )
        node, low, side, ids = stack.pop()
        work["node_ray_tests"] += len(ids)
        if work["node_ray_tests"] > len(arrays["tags"]) * len(origins):
            raise ValueError("field traversal exceeded tree/ray work bound")
        tag = int(arrays["tags"][node])
        if tag == 0:
            continue
        hit, near, far, axes, box_near = intervals(
            low, low + side, origins[ids], directions[ids], depth[ids], minimum
        )
        ids, near, far, axes, box_near = (
            a[hit] for a in (ids, near, far, axes, box_near)
        )
        if not len(ids):
            continue
        if tag & UNKNOWN:
            unknown[ids] = np.minimum(unknown[ids], near)
            work["unknown_ray_box_visits"] += len(ids)
            continue
        if tag & LEAF:
            candidate, normal, interior, current = first_surface(
                arrays["values"][tag & PAYLOAD],
                low,
                low + side,
                origins[ids],
                directions[ids],
                near,
                far,
                axes,
                box_near,
            )
            for key, value in current.items():
                work[key] += value
            work["leaf_ray_queries"] += len(ids)
        elif tag == SOLID:
            candidate = near.copy()
            normal = np.zeros((len(ids), 3))
            normal[np.arange(len(ids)), axes] = -np.sign(directions[ids, axes])
            interior = box_near < minimum
            candidate[interior] = np.inf
            work["box_cap_hits"] += int((~interior).sum())
        else:
            for child in range(7, -1, -1):
                stack.append(
                    (tag + child, low + CORNERS[child] * (side / 2), side / 2, ids)
                )
            continue
        unknown[ids[interior]] = np.minimum(unknown[ids[interior]], near[interior])
        bad_normal = np.isfinite(candidate) & (np.linalg.norm(normal, axis=1) < 0.5)
        unknown[ids[bad_normal]] = np.minimum(
            unknown[ids[bad_normal]], candidate[bad_normal]
        )
        better = candidate < depth[ids]
        chosen = ids[better]
        depth[chosen], normals[chosen], hits[chosen] = (
            candidate[better],
            normal[better],
            True,
        )
    depth[~hits] = np.inf
    return depth, normals, unknown, work


def trace(arrays, origins, directions, stop=None, *, minimum=0.1):
    origins, directions = np.asarray(origins, float), np.asarray(directions, float)
    if (
        origins.shape != directions.shape
        or origins.ndim != 2
        or origins.shape[1] != 3
        or not np.isfinite(origins).all()
        or not np.isfinite(directions).all()
        or np.any(np.linalg.norm(directions, axis=1) == 0)
    ):
        raise ValueError("finite nonzero field ray packet required")
    if not np.isfinite(minimum) or minimum <= 0 or len(origins) > 4194304:
        raise ValueError("positive finite near distance and bounded ray count required")
    stop = np.full(len(origins), np.inf) if stop is None else np.asarray(stop, float)
    if stop.shape != (len(origins),) or np.isnan(stop).any():
        raise ValueError("one finite/infinite stop per ray required")
    depth, unknown = np.full(len(origins), np.inf), np.full(len(origins), np.inf)
    normals = np.zeros_like(origins)
    work = {}
    for begin in range(0, len(origins), 128):
        part = slice(begin, begin + 128)
        d, n, u, current = packet(
            arrays, origins[part], directions[part], stop[part], minimum
        )
        depth[part], normals[part], unknown[part] = d, n, u
        for key, value in current.items():
            work[key] = (
                max(work.get(key, 0), value)
                if key.startswith("max_")
                else work.get(key, 0) + value
            )
    return depth, normals, unknown, work
