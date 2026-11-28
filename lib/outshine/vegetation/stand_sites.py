"""Bounded rejection-free uniform sites within one prepared leaf habitat."""

from types import MappingProxyType

import numpy as np
import shapely

from .plan_arrays import immutable_array
from .stand_habitat import polygon_components


def habitat_sites(cell, random, max_triangles):
    count = sum(cell.counts)
    if count == 0:
        return MappingProxyType(
            {
                "positions_xy_m": immutable_array(np.empty((0, 2))),
                "ids": immutable_array(np.empty((0, 5), np.uint64)),
            }
        )
    parts = polygon_components(cell.domain)
    rings = sum(1 + len(p.interiors) for p in parts)
    holes = rings - len(parts)
    vertices = int(shapely.get_num_coordinates(cell.domain)) - rings
    # Euler bound for triangulated polygons with holes and no Steiner points.
    if vertices + 2 * holes - 2 * len(parts) > max_triangles:
        return None
    polygons = shapely.get_parts(shapely.constrained_delaunay_triangles(cell.domain))
    if not 0 < len(polygons) <= max_triangles:
        return None
    area = shapely.area(polygons)
    if abs(float(area.sum()) - cell.area_m2) > max(1e-8, cell.area_m2 * 1e-10):
        raise ValueError("site triangles changed complete habitat area")
    triangles = shapely.get_coordinates(polygons).reshape(-1, 4, 2)[:, :3]
    cumulative = np.cumsum(area / area.sum())
    cumulative[-1] = 1.0
    points, ids = [], []
    for kind, n in enumerate(cell.counts):
        if not n:
            continue
        values = random(cell.key, "sites:" + str(kind)).random((n, 3))
        selected = np.searchsorted(cumulative, values[:, 0], side="right")
        root = np.sqrt(values[:, 1])
        weights = np.c_[1 - root, root * (1 - values[:, 2]), root * values[:, 2]]
        xy = np.einsum("ni,nij->nj", weights, triangles[selected])
        points.append(xy)
        record = np.empty((n, 5), np.uint64)
        record[:, :3] = cell.key
        record[:, 3] = kind
        record[:, 4] = np.arange(n, dtype=np.uint64)
        ids.append(record)
    return MappingProxyType(
        {
            "positions_xy_m": immutable_array(np.concatenate(points)),
            "ids": immutable_array(np.concatenate(ids)),
        }
    )
