"""Robust audit-only XY projection; physical vertices are never snapped or changed."""

import numpy as np
import shapely

GRID_M = 2.0**-36


def projected(mesh):
    xyz = mesh["vertices"][mesh["faces"]]
    normal = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
    xy = xyz[normal[:, 2] != 0, :, :2]
    origin = np.floor(xy.min(axis=(0, 1)))
    lattice = np.rint((xy - origin) / GRID_M)
    if not np.isfinite(lattice).all() or abs(lattice).max() >= 2**50:
        raise ValueError("Audit projection exceeds its exact integer lattice")
    region = shapely.union_all(shapely.polygons(lattice), grid_size=1)
    return shapely.transform(region, lambda coordinates: coordinates * GRID_M + origin)
