"""Independent continuous projected silhouette areas of closed mesh alternatives."""

import numpy as np
import shapely
from foliage_selection import directions
from outshine_ref.vegetation.shoot_plan import normal_frame

SNAP_GRID_M = 1e-9


def projected_areas(mesh):
    views = np.vstack((directions(64), np.eye(3), -np.eye(3)))
    areas = []
    for direction in views:
        hint = np.array((0, 0, 1)) if abs(direction[2]) < 0.99 else np.array((1, 0, 0))
        frame = normal_frame(direction, hint)
        projected = mesh["vertices"] @ frame[:, (0, 2)]
        triangles = projected[mesh["faces"]]
        polygons = shapely.polygons(triangles)
        polygons = polygons[shapely.area(polygons) > 1e-24]
        areas.append(
            float(shapely.area(shapely.union_all(polygons, grid_size=SNAP_GRID_M)))
        )
    result = np.array(areas)
    if not np.isfinite(result).all() or np.any(result <= 0):
        raise AssertionError("closed positive needle projection required")
    return result
