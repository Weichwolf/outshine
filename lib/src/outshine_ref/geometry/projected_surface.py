"""Bounded complete polygon coverage under a sampled scalar height coordinate."""

import math
import time
from dataclasses import dataclass

import numpy as np
import shapely
from shapely.geometry.base import BaseGeometry

from ..contracts import Mesh, geometry
from .triangulation import conform_triangles


@dataclass(frozen=True, slots=True)
class ProjectedSurfaceBudget:
    """Offline cooperative time and preallocation caps; not a frame deadline."""

    time_limit_s: float = 2.0
    max_boundary_vertices: int = 16384
    max_stations: int = 4096
    max_vertices: int = 8192
    max_faces: int = 8192
    max_weld_pairs: int = 65536
    max_interface_candidates: int = 1048576

    def __post_init__(self):
        if not math.isfinite(self.time_limit_s) or self.time_limit_s < 0:
            raise ValueError("finite nonnegative surface phase time required")
        for name, minimum, maximum in (
            ("max_boundary_vertices", 3, 65536),
            ("max_stations", 2, 4096),
            ("max_vertices", 3, 65536),
            ("max_faces", 1, 65536),
            ("max_weld_pairs", 0, 1048576),
            ("max_interface_candidates", 0, 1048576),
        ):
            value = getattr(self, name)
            if type(value) is not int or not minimum <= value <= maximum:
                raise ValueError("bounded integer " + name + " required")


class _Limit(ValueError):
    pass


DEFAULT_BUDGET = ProjectedSurfaceBudget()


def _time_check(began, budget):
    if time.monotonic() - began > budget.time_limit_s:
        raise _Limit("projected_time_budget")


def _inputs(domain, origin, direction, table, budget):
    if not isinstance(budget, ProjectedSurfaceBudget):
        raise TypeError("explicit ProjectedSurfaceBudget required")
    if not isinstance(domain, BaseGeometry):
        raise TypeError("polygonal XY domain required")
    if shapely.get_num_coordinates(domain) > budget.max_boundary_vertices:
        raise _Limit("projected_boundary_vertex_budget")
    if len(table) > budget.max_stations:
        raise _Limit("projected_station_budget")
    if (
        len(origin) != 2
        or len(direction) != 2
        or any(not np.isscalar(v) for v in (*origin, *direction))
        or any(len(row) != 2 or any(not np.isscalar(v) for v in row) for row in table)
    ):
        raise ValueError("two-dimensional reference and two-column stations required")
    geometry(domain, polygonal=True)
    origin, direction, table = (
        np.array(origin, float),
        np.array(direction, float),
        np.array(table, float),
    )
    if (
        origin.shape != (2,)
        or direction.shape != (2,)
        or not np.isfinite(origin).all()
        or not np.isfinite(direction).all()
        or abs(np.linalg.norm(direction) - 1) > 1e-12
    ):
        raise ValueError("finite origin and unit projection direction required")
    if (
        table.shape != (len(table), 2)
        or len(table) < 2
        or not np.isfinite(table).all()
        or np.any(np.diff(table[:, 0]) <= 0)
    ):
        raise ValueError("finite increasing full station table required")
    coordinates = shapely.get_coordinates(domain)
    if (
        np.abs(coordinates).max() > 1e7
        or np.abs(origin).max() > 1e7
        or np.abs(table[:, 1]).max() > 1e7
        or np.abs(table[:, 0]).max() > 4e7
    ):
        raise ValueError("projected surface exceeds metric numeric domain")
    station = (coordinates - origin) @ direction
    if station.min() < table[0, 0] - 1e-8 or station.max() > table[-1, 0] + 1e-8:
        raise ValueError("height table must cover the complete domain")
    return origin, direction, table, coordinates


def _slices(domain, origin, direction, table, coordinates, budget, began):
    normal = np.array([-direction[1], direction[0]])
    cross = (coordinates - origin) @ normal
    edges = (
        origin
        + table[:, 0, None] * direction
        + np.array([cross.min() - 10, cross.max() + 10])[:, None, None] * normal
    )
    pieces, counted = [], 0
    for i in range(len(table) - 1):
        _time_check(began, budget)
        region = shapely.Polygon(
            [edges[0, i], edges[0, i + 1], edges[1, i + 1], edges[1, i]]
        )
        part = domain.intersection(region)
        if part.area <= 0:
            continue
        triangles = shapely.get_parts(shapely.constrained_delaunay_triangles(part))
        counted += len(triangles)
        if counted > budget.max_faces:
            raise _Limit("projected_face_budget")
        pieces.append(shapely.get_coordinates(triangles).reshape(-1, 4, 2)[:, :3])
    _time_check(began, budget)
    return np.concatenate(pieces).reshape(-1, 2)


def _weld_xy(xy, budget):
    from scipy.sparse import coo_matrix
    from scipy.sparse.csgraph import connected_components
    from scipy.spatial import cKDTree

    tree = cKDTree(xy)
    count = (int(tree.count_neighbors(tree, 1e-8)) - len(xy)) // 2
    if count > budget.max_weld_pairs:
        raise _Limit("projected_weld_pair_budget")
    if not count:
        return xy, np.arange(len(xy)), 0.0
    pairs = tree.query_pairs(1e-8, output_type="ndarray")
    links = coo_matrix((np.ones(len(pairs)), pairs.T), shape=(len(xy), len(xy))).tocsr()
    _, labels = connected_components(links, directed=False)
    first = np.full(labels.max() + 1, len(xy), dtype=int)
    np.minimum.at(first, labels, np.arange(len(xy)))
    result = xy[first]
    displacement = float(np.linalg.norm(xy - result[labels], axis=1).max())
    if displacement > 1e-8:
        raise ValueError("numeric welding exceeds displacement contract")
    return result, labels, displacement


def _mesh(raw, domain, origin, direction, table, budget, began):
    xy, inverse = np.unique(raw, axis=0, return_inverse=True)
    if len(xy) > budget.max_vertices:
        raise _Limit("projected_vertex_budget")
    xy, labels, displacement = _weld_xy(xy, budget)
    faces = labels[inverse].reshape(-1, 3)
    u, v = xy[faces[:, 1]] - xy[faces[:, 0]], xy[faces[:, 2]] - xy[faces[:, 0]]
    determinant = u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0]
    if np.any(determinant == 0):
        raise ValueError("collapsed projected-surface triangle")
    faces[determinant < 0] = faces[determinant < 0, ::-1]
    try:
        faces, conformance = conform_triangles(
            xy,
            faces,
            max_candidates=budget.max_interface_candidates,
            max_faces=budget.max_faces,
        )
    except ValueError as failure:
        if str(failure) == "interface candidate budget exhausted":
            raise _Limit("projected_interface_candidate_budget") from failure
        if str(failure) == "interface face budget exhausted":
            raise _Limit("projected_face_budget") from failure
        raise
    _time_check(began, budget)
    shapes = shapely.polygons(xy[faces])
    if np.any(shapely.area(shapes) <= 0):
        raise ValueError("conformance produced a zero-area face")
    if not shapely.coverage_is_valid(shapes):
        raise ValueError("projected slices overlap or do not conform")
    error = float(shapely.coverage_union_all(shapes).symmetric_difference(domain).area)
    if error > max(1e-7, domain.area * 1e-10):
        raise ValueError("projected slices changed complete domain")
    projected = (xy - origin) @ direction
    z = np.interp(projected, table[:, 0], table[:, 1])
    face_stations = projected[faces]
    cuts = np.searchsorted(table[:, 0], face_stations.min(axis=1) + 1e-8, side="right")
    if np.any(
        face_stations.max(axis=1) > table[np.minimum(cuts, len(table) - 1), 0] + 1e-8
    ):
        raise ValueError("triangle straddles more than one height chord")
    _time_check(began, budget)
    return Mesh(np.c_[xy, z], faces), {
        "coverage_error_m2": error,
        "numeric_weld_m": displacement,
        "conformance": conformance,
        "faces": len(faces),
        "vertices": len(xy),
        "each_face_within_one_height_chord": True,
    }


def projected_height_surface(
    domain, origin_xy_m, direction_xy, height_table, *, budget=DEFAULT_BUDGET
):
    """Build complete polygon coverage under a piecewise-linear scalar height table.

    Stations are dot(XY-origin,unit_direction), heights use the caller's Z datum.
    Every face belongs to one height chord. A curve approximation certificate is
    supplied by transport.mesh_projected_profile; an arbitrary table is only C0.
    Limits return no mesh with a reason. Wrong types raise TypeError, bad values
    raise ValueError; numeric
    geometry failures return no mesh. GEOS steps can overrun cooperative time.
    """
    began = time.monotonic()
    try:
        origin, direction, table, coordinates = _inputs(
            domain, origin_xy_m, direction_xy, height_table, budget
        )
        _time_check(began, budget)
    except _Limit as failure:
        return None, {"valid": False, "reason": str(failure)}
    try:
        raw = _slices(domain, origin, direction, table, coordinates, budget, began)
        mesh, report = _mesh(raw, domain, origin, direction, table, budget, began)
        return mesh, dict(
            report, valid=True, reason="", geometry_ms=(time.monotonic() - began) * 1000
        )
    except _Limit as failure:
        return None, {"valid": False, "reason": str(failure)}
    except (ValueError, shapely.errors.GEOSException) as failure:
        return None, {
            "valid": False,
            "reason": "projected_geometry_invalid",
            "detail": str(failure),
        }
