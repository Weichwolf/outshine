"""Shared closed-cell masks and finite root geometry for water consumers."""

import numpy as np
import shapely
from shapely.affinity import translate


def local_sources(cell, polygons, keys, coordinate_limit):
    if not 0 < len(polygons) <= 1024 or len(polygons) != len(keys):
        raise ValueError(
            "bounded polygons and corresponding source identities required"
        )
    if (
        shapely.get_num_coordinates(polygons).sum() > coordinate_limit
        or any(
            p.geom_type not in ("Polygon", "MultiPolygon")
            or not p.is_valid
            or p.is_empty
            or not 0 < p.area < np.inf
            for p in polygons
        )
        or not np.isfinite(shapely.get_coordinates(polygons)).all()
    ):
        raise ValueError("bounded finite valid source polygons required")
    local = [translate(p, xoff=-cell[1], yoff=-cell[2]) for p in polygons]
    mask = shapely.union_all(local)
    if not shapely.box(0, 0, 1, 1).covers(mask):
        raise ValueError("normalized sources must stay inside their canonical cell")
    return local, mask


def root_normal(root):
    root = np.asarray(root, dtype=float)
    if root.shape != (3, 3) or not np.isfinite(root).all():
        raise ValueError("three finite ECEF root vertices required")
    normal = np.cross(root[1] - root[0], root[2] - root[0])
    length = np.linalg.norm(normal)
    if not np.isfinite(length) or length == 0:
        raise ValueError("nondegenerate ECEF root required")
    return root, normal / length
