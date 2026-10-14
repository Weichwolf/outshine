"""Minimum affine-height gap over complete triangle intersection polygons."""

import time

import numpy as np
import shapely


def vertical_clearance(lower, underside, *, max_pairs=65536, seconds=1.0):
    if (
        type(max_pairs) is not int
        or max_pairs < 0
        or not np.isfinite(seconds)
        or seconds < 0
    ):
        raise ValueError("finite nonnegative pair/time budgets required")
    if len(lower.triangles) > 8192 or len(underside.triangles) > 8192:
        return {"valid": False, "reason": "clearance_face_budget"}
    began = time.monotonic()
    low = lower.vertices[lower.triangles]
    high = underside.vertices[underside.triangles]
    low_shapes = shapely.polygons(low[:, :, :2])
    high_shapes = shapely.polygons(high[:, :, :2])
    tree = shapely.STRtree(high_shapes)
    checks, minimum, witness = 0, np.inf, None
    for first, shape in enumerate(low_shapes):
        if time.monotonic() - began > seconds:
            return {"valid": False, "reason": "clearance_time_budget", "checks": checks}
        # Count potential output before the GEOS index query allocates its array.
        if checks + len(high_shapes) > max_pairs:
            return {"valid": False, "reason": "clearance_pair_budget", "checks": checks}
        candidates = tree.query(shape, predicate="intersects")
        checks += len(candidates)
        for second in candidates:
            overlap = shape.intersection(high_shapes[second])
            if overlap.area <= 0:
                continue
            xy = shapely.get_coordinates(overlap)
            heights = []
            for triangle in (low[first], high[second]):
                gradient = np.linalg.solve(
                    triangle[1:, :2] - triangle[0, :2], triangle[1:, 2] - triangle[0, 2]
                )
                heights.append(triangle[0, 2] + (xy - triangle[0, :2]) @ gradient)
            gaps = heights[1] - heights[0]
            i = int(np.argmin(gaps))
            if gaps[i] < minimum:
                minimum, witness = float(gaps[i]), xy[i].tolist()
    if witness is None:
        return {"valid": False, "reason": "no_positive_area_overlap", "checks": checks}
    return {
        "valid": True,
        "reason": "",
        "minimum_clearance_m": minimum,
        "witness_xy": witness,
        "checks": checks,
        "elapsed_ms": (time.monotonic() - began) * 1000,
    }
