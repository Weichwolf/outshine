"""One bounded noded arrangement owns every critical profile intersection."""

import time

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import conform_triangles
from scipy.spatial import cKDTree
from transport_vertex_pool import welded_vertices


def surface(domain, origin, axis, table):
    began = time.monotonic()
    if len(table) > 512 or shapely.get_num_coordinates(domain) > 1024:
        raise ValueError("bridge arrangement input budget")
    normal = np.array([-axis[1], axis[0]])
    xy = shapely.get_coordinates(domain)
    stations, cross = (xy - origin) @ axis, (xy - origin) @ normal
    cuts = table[
        (table[:, 0] > stations.min() + 1e-8) & (table[:, 0] < stations.max() - 1e-8), 0
    ]
    endpoints = (
        origin
        + cuts[:, None, None] * axis
        + np.array([cross.min() - 1, cross.max() + 1])[None, :, None] * normal
    )
    lines = np.r_[shapely.get_parts(domain.boundary), shapely.linestrings(endpoints)]
    noded = shapely.get_parts(shapely.union_all(lines))
    if len(noded) > 8192:
        raise ValueError("bridge arrangement segment budget")
    cells = shapely.get_parts(shapely.polygonize(noded))
    cells = cells[shapely.covers(domain, shapely.point_on_surface(cells))]
    triangles = shapely.get_parts(shapely.constrained_delaunay_triangles(cells))
    if not 1 <= len(triangles) <= 8192:
        raise ValueError("bridge arrangement face budget")
    raw = shapely.get_coordinates(triangles).reshape(-1, 4, 2)[:, :3]
    points, inverse = np.unique(raw.reshape(-1, 2), axis=0, return_inverse=True)
    if len(points) > 8192:
        raise ValueError("bridge arrangement vertex budget")
    tree = cKDTree(points)
    if (tree.count_neighbors(tree, 1e-8) - len(points)) // 2 > 65536:
        raise ValueError("bridge arrangement weld pair budget")
    points, labels, displacement = welded_vertices(points, 1e-8)
    faces = labels[inverse].reshape(-1, 3)
    a, b = (
        points[faces[:, 1]] - points[faces[:, 0]],
        points[faces[:, 2]] - points[faces[:, 0]],
    )
    determinant = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]
    collapsed = determinant == 0
    removed_area = float(shapely.area(triangles[collapsed]).sum())
    faces, determinant = faces[~collapsed], determinant[~collapsed]
    faces[determinant < 0] = faces[determinant < 0, ::-1]
    faces, conformance = conform_triangles(
        points, faces, max_faces=8192, max_candidates=65536
    )
    shapes = shapely.polygons(points[faces])
    if not shapely.coverage_is_valid(shapes):
        raise ValueError(
            "bridge arrangement is not a conforming nonoverlapping coverage"
        )
    error = float(shapely.coverage_union_all(shapes).symmetric_difference(domain).area)
    if max(error, removed_area) > 1e-7:
        raise ValueError("bridge arrangement changes its actual source footprint")
    projected = (points - origin) @ axis
    ranges = projected[faces]
    interval = np.searchsorted(table[:, 0], ranges.min(axis=1) + 1e-8, side="right")
    if np.any(
        ranges.max(axis=1) > table[np.minimum(interval, len(table) - 1), 0] + 1e-8
    ):
        raise ValueError("bridge arrangement face crosses a profile kink")
    if time.monotonic() - began > 4:
        raise ValueError("bridge arrangement time budget")
    return Mesh(np.c_[points, np.interp(projected, table[:, 0], table[:, 1])], faces), {
        "vertices": len(points),
        "faces": len(faces),
        "segments": len(noded),
        "collapsed_triangles": int(collapsed.sum()),
        "removed_triangle_area_m2": removed_area,
        "numeric_weld_m": displacement,
        "coverage_error_m2": error,
        "conformance": conformance,
        "seconds": time.monotonic() - began,
    }
