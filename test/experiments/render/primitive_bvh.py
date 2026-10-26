"""Bounded median-split packet BVH for closed convex procedural primitives."""

from dataclasses import dataclass

import numpy as np
from roof_prisms import intersect_planes


@dataclass
class Hierarchy:
    bounds: np.ndarray
    children: np.ndarray


def build(bounds):
    bounds = np.asarray(bounds, float)
    if (
        bounds.ndim != 3
        or bounds.shape[1:] != (2, 3)
        or not 0 < len(bounds) <= 65536
        or not np.isfinite(bounds).all()
        or np.any(bounds[:, 0] > bounds[:, 1])
    ):
        raise ValueError("1..65536 finite ordered primitive boxes required")
    nodes, children = [], []
    centres = bounds.mean(1)

    def split(indices):
        node = len(nodes)
        box = np.array([bounds[indices, 0].min(0), bounds[indices, 1].max(0)])
        margin = 64 * np.finfo(float).eps * np.maximum(np.abs(box).max(0), 1)
        nodes.append(box + np.array([-1, 1])[:, None] * margin)
        children.append([-1, -1])
        if len(indices) == 1:
            children[node] = [-1, int(indices[0])]
        else:
            axis = np.ptp(centres[indices], axis=0).argmax()
            order = indices[np.argsort(centres[indices, axis], kind="stable")]
            middle = len(order) // 2
            children[node] = [split(order[:middle]), split(order[middle:])]
        return node

    split(np.arange(len(bounds)))
    return Hierarchy(np.array(nodes), np.array(children, np.int32))


def box_hits(box, origins, directions, maximum):
    parallel = directions == 0
    offset = box[None] - origins[:, None]
    t = np.divide(
        offset, directions[:, None], out=np.zeros_like(offset), where=~parallel[:, None]
    )
    low = np.min(t, axis=1)
    high = np.max(t, axis=1)
    low = np.where(parallel, -np.inf, low)
    high = np.where(parallel, np.inf, high)
    missed = np.any(parallel & ((origins < box[0]) | (origins > box[1])), axis=1)
    near = np.maximum(0.1, np.max(low, axis=1))
    far = np.minimum(maximum, np.min(high, axis=1))
    return ~missed & (near <= far), near


def trace(prisms, hierarchy, origins, directions):
    origins, directions = np.asarray(origins, float), np.asarray(directions, float)
    if (
        origins.shape != directions.shape
        or origins.ndim != 2
        or origins.shape[1] != 3
        or len(origins) > 4194304
        or not np.isfinite(origins).all()
        or not np.isfinite(directions).all()
        or np.any(np.linalg.norm(directions, axis=1) == 0)
    ):
        raise ValueError("bounded finite nonzero ray packet required")
    depth = np.full(len(origins), np.inf)
    owner = np.full(len(origins), -1, np.int32)
    face = np.full(len(origins), -1, np.int32)
    work = {
        "node_ray_tests": 0,
        "primitive_ray_tests": 0,
        "plane_ray_tests": 0,
        "max_pending_ray_indices": 0,
    }
    for start in range(0, len(origins), 256):
        stack = [(0, np.arange(start, min(start + 256, len(origins))))]
        while stack:
            work["max_pending_ray_indices"] = max(
                work["max_pending_ray_indices"], sum(len(ids) for _, ids in stack)
            )
            node, ids = stack.pop()
            work["node_ray_tests"] += len(ids)
            hit, _ = box_hits(
                hierarchy.bounds[node], origins[ids], directions[ids], depth[ids]
            )
            ids = ids[hit]
            if not len(ids):
                continue
            left, right = hierarchy.children[node]
            if left >= 0:
                # Shared packet order; per-ray nearest-depth pruning remains valid.
                a, b = left, right
                _, ta = box_hits(
                    hierarchy.bounds[a], origins[ids], directions[ids], depth[ids]
                )
                _, tb = box_hits(
                    hierarchy.bounds[b], origins[ids], directions[ids], depth[ids]
                )
                work["node_ray_tests"] += 2 * len(ids)
                if np.min(ta) > np.min(tb):
                    a, b = b, a
                stack.extend([(b, ids), (a, ids)])
                continue
            begin, end = prisms.offsets[right : right + 2]
            planes = prisms.planes[begin:end]
            work["primitive_ray_tests"] += len(ids)
            work["plane_ray_tests"] += len(ids) * len(planes)
            t, f = intersect_planes(planes, origins[ids], directions[ids])
            keep = np.isfinite(t) & (
                (t < depth[ids]) | ((t == depth[ids]) & (right < owner[ids]))
            )
            ids, t, f = ids[keep], t[keep], f[keep]
            depth[ids], owner[ids], face[ids] = t, right, begin + f
    return depth, owner, face, work
