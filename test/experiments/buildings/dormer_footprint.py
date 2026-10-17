"""Robust audit-only XY projection; physical vertices are never snapped or changed."""

import numpy as np
import shapely

GRID_M = 1e-12


def projected(mesh):
    xyz = mesh["vertices"][mesh["faces"]]
    normal = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
    triangles = shapely.polygons(xyz[normal[:, 2] > 0, :, :2])
    return shapely.union_all(triangles, grid_size=GRID_M)
