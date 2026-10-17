"""Research: prove a single simple boundary before the general coverage check."""

import numpy as np
import shapely

GENERAL_COVERAGE = shapely.coverage_is_valid


def boundary_cycle(edges, vertex_count):
    """Return the one directed cycle, or None when the disk witness is unavailable."""
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


def witness(xy, faces, polygons):
    """Sufficient planar-chain witness, never a negative coverage verdict.

    Opposite internal edges cancel in the oriented triangle boundary. If the
    remaining chain is one simple CCW ring, its winding number is one inside
    and zero outside. Every CCW nondegenerate triangle contributes +1 inside.
    Additivity then excludes positive-area overlap and holes. Full edge pairing
    also excludes unmatched interior edges. Other topologies use GEOS instead.
    """
    edges = faces[:, [0, 1, 1, 2, 2, 0]].reshape(-1, 2)
    low, high = np.min(edges, axis=1), np.max(edges, axis=1)
    codes = low.astype(np.uint64) * np.uint64(2**32) + high
    order = np.argsort(codes, kind="stable")
    ordered = codes[order]
    begin = np.r_[0, np.flatnonzero(ordered[1:] != ordered[:-1]) + 1]
    counts = np.diff(np.r_[begin, len(ordered)])
    if np.any(counts > 2):
        return {"proved": False, "reason": "edge_incidence"}
    paired = begin[counts == 2]
    a, b = edges[order[paired]], edges[order[paired + 1]]
    if not np.array_equal(a, b[:, ::-1]):
        return {"proved": False, "reason": "edge_orientation"}
    outer = edges[order[begin[counts == 1]]]
    cycle = boundary_cycle(outer, len(xy))
    if cycle is None:
        return {"proved": False, "reason": "not_one_boundary_cycle"}
    ring = shapely.LinearRing(xy[cycle])
    if not shapely.is_simple(ring) or not shapely.is_ccw(ring):
        return {"proved": False, "reason": "boundary_not_simple_ccw"}
    if not shapely.is_ccw(shapely.get_exterior_ring(polygons)).all():
        return {"proved": False, "reason": "triangle_orientation"}
    return {
        "proved": True,
        "reason": "positive_chain_simple_boundary",
        "triangles": len(faces),
        "paired_edges": len(paired),
        "boundary_edges": len(outer),
    }


def polygon_witness(polygons):
    """Conservative adapter; production code could reuse existing XY and indices."""
    polygons = np.asarray(polygons, dtype=object)
    if (
        polygons.ndim != 1
        or not 1 <= len(polygons) <= 65536
        or not np.all(shapely.get_type_id(polygons) == 3)
        or not np.all(shapely.get_num_coordinates(polygons) == 4)
    ):
        return {"proved": False, "reason": "not_bounded_triangles"}
    coords = shapely.get_coordinates(polygons).reshape(-1, 4, 2)
    if (
        not np.isfinite(coords).all()
        or np.max(abs(coords), initial=0) > 1e9
        or not np.array_equal(coords[:, 0], coords[:, -1])
    ):
        return {"proved": False, "reason": "not_finite_closed_triangles"}
    xy, inverse = np.unique(coords[:, :3].reshape(-1, 2), axis=0, return_inverse=True)
    if len(xy) > 65536:
        return {"proved": False, "reason": "vertex_budget"}
    faces = inverse.reshape(-1, 3)
    if np.any(np.diff(np.sort(faces, axis=1), axis=1) == 0):
        return {"proved": False, "reason": "repeated_triangle_vertex"}
    return witness(xy, faces, polygons)


def checked_coverage(polygons, *, records=None):
    proof = polygon_witness(polygons)
    result = proof["proved"] or bool(GENERAL_COVERAGE(polygons))
    if records is not None:
        records.append(dict(proof, valid=result, input_polygons=len(polygons)))
    return result
