"""Conforming polygon slices under one chord-sampled C1 scalar height field."""

import time

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import conform_triangles
from transport_vertex_pool import welded_vertices


def projected_mesh(domain, origin, direction, table, *, seconds=2.0, max_faces=8192):
    began = time.monotonic()
    origin, direction, table = (
        np.asarray(origin, float),
        np.asarray(direction, float),
        np.asarray(table, float),
    )
    if (
        origin.shape != (2,)
        or direction.shape != (2,)
        or not np.isfinite(origin).all()
        or not np.isfinite(direction).all()
        or abs(np.linalg.norm(direction) - 1) > 1e-12
        or not 2 <= len(table) <= 4096
        or table.shape != (len(table), 2)
        or not np.isfinite(table).all()
        or np.any(np.diff(table[:, 0]) <= 0)
        or not np.isfinite(seconds)
        or seconds <= 0
        or type(max_faces) is not int
        or not 1 <= max_faces <= 8192
        or domain.is_empty
        or not domain.is_valid
        or domain.geom_type not in ("Polygon", "MultiPolygon")
    ):
        raise ValueError(
            "finite bounded domain, unit axis, full increasing station table and budgets required"
        )
    coordinates = shapely.get_coordinates(domain)
    if len(coordinates) > 16384:
        raise ValueError("height-domain coordinate budget")
    normal = np.array([-direction[1], direction[0]])
    station = (coordinates - origin) @ direction
    if station.min() < table[0, 0] - 1e-8 or station.max() > table[-1, 0] + 1e-8:
        raise ValueError("height table must cover the complete domain")
    cross = (coordinates - origin) @ normal
    edges = (
        origin
        + table[:, 0, None] * direction
        + np.array([cross.min() - 10, cross.max() + 10])[:, None, None] * normal
    )
    pieces = []
    for i in range(len(table) - 1):
        if time.monotonic() - began > seconds:
            raise ValueError("projected-surface time budget")
        region = shapely.Polygon(
            [edges[0, i], edges[0, i + 1], edges[1, i + 1], edges[1, i]]
        )
        part = domain.intersection(region)
        if part.area <= 0:
            continue
        triangles = shapely.get_parts(shapely.constrained_delaunay_triangles(part))
        if sum(len(p) for p in pieces) + len(triangles) > max_faces:
            raise ValueError("projected-surface face budget")
        pieces.append(shapely.get_coordinates(triangles).reshape(-1, 4, 2)[:, :3])
    raw = np.concatenate(pieces).reshape(-1, 2)
    xy, inverse = np.unique(raw, axis=0, return_inverse=True)
    xy, labels, displacement = welded_vertices(xy, 1e-8)
    faces = labels[inverse].reshape(-1, 3)
    u, v = xy[faces[:, 1]] - xy[faces[:, 0]], xy[faces[:, 2]] - xy[faces[:, 0]]
    determinant = u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0]
    if np.any(determinant == 0):
        raise ValueError("collapsed projected-surface triangle")
    faces[determinant < 0] = faces[determinant < 0, ::-1]
    faces, conformance = conform_triangles(xy, faces)
    if len(faces) > max_faces or len(xy) > 8192:
        raise ValueError("conforming projected-surface budget")
    shapes = shapely.polygons(xy[faces])
    if not shapely.coverage_is_valid(shapes):
        raise ValueError("projected slices overlap or do not conform")
    error = shapely.coverage_union_all(shapes).symmetric_difference(domain).area
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
    return Mesh(np.c_[xy, z], faces), {
        "coverage_error_m2": float(error),
        "numeric_weld_m": displacement,
        "conformance": conformance,
        "faces": len(faces),
        "vertices": len(xy),
        "each_face_within_one_height_chord": True,
        "geometry_ms": (time.monotonic() - began) * 1000,
    }
