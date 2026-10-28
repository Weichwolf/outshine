"""Check emitted top triangles against absolute source-volume height intervals."""

import numpy as np
import shapely


def floor_clearance(floor, mesh, volumes, base_z_m, limits):
    triangles = mesh["vertices"][mesh["faces"]]
    top = triangles[np.all(triangles[:, :, 2] == floor.upper_z_m, axis=1)]
    if not len(top):
        raise ValueError("floor_clearance_missing_top_triangles")
    supports, intrusions = [], []
    ceiling = floor.upper_z_m + limits.headroom_m
    for part in volumes:
        bottom = base_z_m + part.lower_m
        roof = base_z_m + part.upper_m - limits.slab_m
        if bottom <= floor.lower_z_m and roof >= ceiling:
            supports.append(part.polygon)
        if floor.upper_z_m <= bottom < ceiling or floor.upper_z_m <= roof < ceiling:
            intrusions.append(part.polygon)
    emitted = shapely.polygons(top[:, :, :2])
    unsupported = shapely.area(shapely.difference(emitted, shapely.union_all(supports)))
    blocked = shapely.area(shapely.intersection(emitted, shapely.union_all(intrusions)))
    scale = max(1.0, float(abs(top[:, :, :2]).max()))
    arithmetic_area = 128 * np.spacing(scale) * floor.polygon.length
    if (
        float(unsupported.sum()) > arithmetic_area
        or float(blocked.sum()) > arithmetic_area
    ):
        raise ValueError("emitted_floor_source_height_clearance_failed")
    return {
        "top_triangles_checked": len(top),
        "unsupported_top_area_m2": float(unsupported.sum()),
        "obstructed_top_area_m2": float(blocked.sum()),
        "arithmetic_area_bound_m2": float(arithmetic_area),
        "headroom_m": limits.headroom_m,
        "scope": "Actual top triangles against chosen source-envelope absolute height intervals, not rooms, doors, World obstacles or terrain/statics.",
    }
