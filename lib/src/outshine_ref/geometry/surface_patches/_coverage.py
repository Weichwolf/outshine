"""Sufficient exact-edge witness before general projected coverage validation."""

import numpy as np
import shapely


def _cycle(edges, vertex_count):
    if len(edges) < 3:
        return None
    starts, counts = np.unique(edges[:, 0], return_counts=True)
    if np.any(counts != 1) or not np.array_equal(starts, np.sort(edges[:, 1])):
        return None
    following = np.full(vertex_count, -1, dtype=np.int64)
    following[edges[:, 0]] = edges[:, 1]
    cycle = np.empty(len(edges) + 1, dtype=np.int64)
    cycle[0] = starts[0]
    for i in range(len(edges)):
        cycle[i + 1] = following[cycle[i]]
        if cycle[i + 1] == cycle[0] and i + 1 != len(edges):
            return None
    return cycle if cycle[-1] == cycle[0] else None


def _outer_ring(xy, faces):
    edges = faces[:, [0, 1, 1, 2, 2, 0]].reshape(-1, 2)
    low, high = np.min(edges, axis=1), np.max(edges, axis=1)
    codes = low.astype(np.uint64) * np.uint64(2**32) + high
    order = np.argsort(codes, kind="stable")
    ordered = codes[order]
    begin = np.r_[0, np.flatnonzero(ordered[1:] != ordered[:-1]) + 1]
    counts = np.diff(np.r_[begin, len(ordered)])
    if np.any(counts > 2):
        return None
    paired = begin[counts == 2]
    a, b = edges[order[paired]], edges[order[paired + 1]]
    if not np.array_equal(a, b[:, ::-1]):
        return None
    cycle = _cycle(edges[order[begin[counts == 1]]], len(xy))
    return None if cycle is None else shapely.LinearRing(xy[cycle])


def proves_single_boundary(xy, faces, polygons, *, ring_batch=1024):
    """Prove an already bounded mesh's coverage, or require the general fallback.

    The caller has checked finite coordinates, nondegenerate upward triangles,
    exact single-valued XY and valid indices; faces use canonical XY indices.
    Internal directed edges must cancel in pairs. The remaining simple CCW
    cycle has winding number one inside and zero outside. Each robustly CCW
    triangle contributes +1, so winding additivity excludes area overlap/holes.
    Other boundary topologies receive no verdict here. This is not an unchecked
    validity flag or a manifold/physics/3D-solid certificate.
    """
    ring = _outer_ring(xy, faces)
    if ring is None or not shapely.is_simple(ring) or not shapely.is_ccw(ring):
        return False
    batch = len(polygons) if ring_batch is None else ring_batch
    if type(batch) is not int or not 1 <= batch <= 65536:
        raise ValueError("bounded positive orientation batch required")
    for begin in range(0, len(polygons), batch):
        rings = shapely.get_exterior_ring(polygons[begin : begin + batch])
        if not shapely.is_ccw(rings).all():
            return False
        del rings
    return True
