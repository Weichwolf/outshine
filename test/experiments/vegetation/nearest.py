"""Exact bounded nearest-node queries for the growth comparison."""

import numpy as np
from scipy.spatial import cKDTree


def validate(points, nodes, radius):
    if (
        points.ndim != 2
        or nodes.ndim != 2
        or points.shape[1] != 3
        or nodes.shape[1] != 3
    ):
        raise ValueError("positions must be N by 3 arrays")
    if (
        not len(nodes)
        or not np.all(np.isfinite(points))
        or not np.all(np.isfinite(nodes))
    ):
        raise ValueError("finite positions and at least one node required")
    if not np.isfinite(radius) or radius <= 0:
        raise ValueError("positive finite influence radius required")


def brute_nearest(points, nodes, radius, block=64):
    validate(points, nodes, radius)
    if not isinstance(block, (int, np.integer)) or block < 1:
        raise ValueError("positive integer block size required")
    indices = np.full(len(points), -1, dtype=np.int32)
    distances = np.full(len(points), np.inf)
    for begin in range(0, len(points), block):
        delta = points[begin : begin + block, None, :] - nodes[None, :, :]
        squared = np.einsum("pnc,pnc->pn", delta, delta)
        closest = np.argmin(squared, axis=1)
        values = squared[np.arange(len(closest)), closest]
        accepted = values <= radius * radius
        indices[begin : begin + block][accepted] = closest[accepted]
        distances[begin : begin + block][accepted] = np.sqrt(values[accepted])
    return indices, distances


def tree_nearest(points, nodes, radius):
    validate(points, nodes, radius)
    tree = cKDTree(nodes)
    distance, index = tree.query(points, k=2, workers=1)
    closest = index[:, 0].astype(np.int32)
    if len(nodes) > 1:
        tolerance = np.spacing(np.maximum(distance[:, 0], 1.0)) * 32
        tied = np.flatnonzero(distance[:, 1] - distance[:, 0] <= tolerance)
        for row in tied:
            candidates = np.array(
                tree.query_ball_point(points[row], distance[row, 0] + tolerance[row])
            )
            delta = nodes[candidates] - points[row]
            squared = np.einsum("nc,nc->n", delta, delta)
            order = np.lexsort((candidates, squared))
            closest[row] = candidates[order[0]]
    delta = points - nodes[closest]
    squared = np.einsum("pc,pc->p", delta, delta)
    accepted = squared <= radius * radius
    closest[~accepted] = -1
    distances = np.full(len(points), np.inf)
    distances[accepted] = np.sqrt(squared[accepted])
    return closest, distances


def free_endpoints(points, nodes, radius, nearest):
    occupied, _ = nearest(points, nodes, radius)
    free = occupied < 0
    candidates = np.flatnonzero(free)
    if len(candidates) < 2:
        return free
    tree = cKDTree(points[candidates])
    search_radius = radius + np.spacing(radius) * 32
    for index in candidates:
        if not free[index]:
            continue
        neighbors = np.asarray(
            tree.query_ball_point(points[index], search_radius), dtype=np.int64
        )
        indices = candidates[neighbors]
        indices = indices[indices > index]
        delta = points[indices] - points[index]
        close = np.einsum("pc,pc->p", delta, delta) <= radius * radius
        free[indices[close]] = False
    return free
