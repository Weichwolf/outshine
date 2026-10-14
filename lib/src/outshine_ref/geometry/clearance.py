"""Bounded signed vertical separation of explicitly selected affine surfaces."""

import math
import time

import numpy as np
import shapely

from ..contracts import Mesh


class _Limit(ValueError):
    pass


def _time_check(began, seconds):
    if time.monotonic() - began > seconds:
        raise _Limit("clearance_time_budget")


def _planes(surface, max_faces, began, seconds):
    if not isinstance(surface, Mesh):
        raise TypeError("explicit selected Mesh surface required")
    if not 1 <= len(surface.triangles) <= max_faces:
        raise _Limit("clearance_face_budget")
    _time_check(began, seconds)
    xyz = surface.vertices[surface.triangles]
    edges = xyz[:, 1:, :2] - xyz[:, :1, :2]
    determinant = edges[:, 0, 0] * edges[:, 1, 1] - edges[:, 0, 1] * edges[:, 1, 0]
    if np.any(determinant == 0):
        raise _Limit("clearance_degenerate_projection")
    try:
        gradient = np.linalg.solve(edges, (xyz[:, 1:, 2] - xyz[:, :1, 2])[..., None])[
            ..., 0
        ]
    except np.linalg.LinAlgError as failure:
        raise _Limit("clearance_singular_plane") from failure
    if not np.isfinite(gradient).all():
        raise _Limit("clearance_nonfinite_plane")
    shapes = shapely.polygons(xyz[:, :, :2])
    _time_check(began, seconds)
    return xyz, gradient, shapes


def vertical_clearance(
    lower, underside, *, max_pairs=65536, seconds=1.0, max_faces=8192
):
    """Minimum upper-minus-lower Z on positive-area XY overlaps, in caller's datum.

    Both inputs are explicitly chosen triangle height surfaces; winding is irrelevant.
    Frame, kind, source and physical clearance requirements remain caller-owned.
    A negative result is valid signed overlap, not an automatic rejection or floor
    selection. Edge/point-only contact yields no numeric result. Time is cooperative;
    input face and actual candidate-pair caps bound allocation/work separately.
    """
    if type(max_pairs) is not int or not 0 <= max_pairs <= 1048576:
        raise ValueError("bounded nonnegative candidate-pair count required")
    if type(max_faces) is not int or not 1 <= max_faces <= 65536:
        raise ValueError("bounded positive input face count required")
    if not math.isfinite(seconds) or seconds < 0:
        raise ValueError("finite nonnegative cooperative time required")
    began = time.monotonic()
    checks, overlaps = 0, 0
    minimum, witness, pair = math.inf, None, None
    try:
        low, low_gradient, low_shapes = _planes(lower, max_faces, began, seconds)
        high, high_gradient, high_shapes = _planes(underside, max_faces, began, seconds)
        tree = shapely.STRtree(high_shapes)
        for first, shape in enumerate(low_shapes):
            _time_check(began, seconds)
            # A query holds at most max_faces indices; independent of pair budget.
            candidates = tree.query(shape, predicate="intersects")
            if checks + len(candidates) > max_pairs:
                raise _Limit("clearance_pair_budget")
            checks += len(candidates)
            for second in np.sort(candidates):
                _time_check(began, seconds)
                overlap = shape.intersection(high_shapes[second])
                if overlap.area <= 0:
                    continue
                overlaps += 1
                xy = shapely.get_coordinates(overlap)
                high_height = (
                    high[second, 0, 2]
                    + (xy - high[second, 0, :2]) @ high_gradient[second]
                )
                low_height = (
                    low[first, 0, 2] + (xy - low[first, 0, :2]) @ low_gradient[first]
                )
                gaps = high_height - low_height
                index = int(np.argmin(gaps))
                if gaps[index] < minimum:
                    minimum = float(gaps[index])
                    witness = xy[index].tolist()
                    pair = [first, int(second)]
        _time_check(began, seconds)
        if witness is None:
            raise _Limit("no_positive_area_overlap")
    except (_Limit, shapely.errors.GEOSException) as failure:
        return {
            "valid": False,
            "reason": str(failure),
            "checks": checks,
            "positive_area_pairs": overlaps,
            "elapsed_ms": (time.monotonic() - began) * 1000,
        }
    return {
        "valid": True,
        "reason": "",
        "minimum_clearance_m": minimum,
        "witness_xy": witness,
        "face_pair": pair,
        "checks": checks,
        "positive_area_pairs": overlaps,
        "elapsed_ms": (time.monotonic() - began) * 1000,
    }
