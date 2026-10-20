"""Conforming water-surface experiments; source semantics remain the caller's responsibility."""

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from shapely.geometry import Polygon


def source_polygons(data):
    polygons = []
    for first, count, _ in data["surfaces"]:
        rings = [
            data["ring_points"][begin : begin + size]
            for begin, size in data["rings"][int(first) : int(first + count)]
        ]
        polygon = Polygon(rings[0], rings[1:])
        if not polygon.is_valid or polygon.is_empty:
            raise ValueError("valid source rings and holes required")
        polygons.append(polygon)
    return polygons


def source_body(data):
    return shapely.union_all(source_polygons(data))


def surface_mesh(body, spacing_m=20.0, vertex_limit=8192, face_limit=16384):
    """None keeps original boundary vertices without adding render-spacing detail."""
    if spacing_m is not None and (not np.isfinite(spacing_m) or spacing_m <= 0):
        raise ValueError("positive finite boundary spacing required")
    parts = shapely.get_parts(body)
    if (
        body.is_empty
        or not body.is_valid
        or any(part.geom_type != "Polygon" for part in parts)
    ):
        raise ValueError("a valid polygonal water body required")
    holes = sum(len(part.interiors) for part in parts)
    planned_vertices = 0
    for part in parts:
        for ring in [part.exterior, *part.interiors]:
            coordinates = np.asarray(ring.coords)
            lengths = np.linalg.norm(np.diff(coordinates, axis=0), axis=1)
            planned_vertices += (
                len(lengths)
                if spacing_m is None
                else sum(int(np.ceil(length / spacing_m)) for length in lengths)
            )
    planned_faces = planned_vertices + 2 * holes - 2 * len(parts)
    if planned_vertices > vertex_limit or planned_faces > face_limit:
        raise ValueError("boundary refinement exceeds the declared mesh budget")
    boundary = body if spacing_m is None else shapely.segmentize(body, spacing_m)
    triangles = shapely.get_parts(shapely.constrained_delaunay_triangles(boundary))
    coordinates = shapely.get_coordinates(triangles).reshape(-1, 4, 2)[:, :3]
    xy, indices = np.unique(coordinates.reshape(-1, 2), axis=0, return_inverse=True)
    faces = indices.reshape(-1, 3)
    if len(xy) > vertex_limit or len(faces) > face_limit:
        raise ValueError("triangulation exceeds the declared mesh budget")
    u, v = xy[faces[:, 1]] - xy[faces[:, 0]], xy[faces[:, 2]] - xy[faces[:, 0]]
    determinant = u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0]
    if np.any(determinant == 0):
        raise ValueError("degenerate water triangles are not usable")
    faces[determinant < 0] = faces[determinant < 0][:, [0, 2, 1]]
    covered = shapely.union_all(triangles)
    missing_or_excess = float(body.symmetric_difference(covered).area)
    overlap = float(shapely.area(triangles).sum() - covered.area)
    tolerance = max(1e-8, body.area * 1e-12)
    if missing_or_excess > tolerance or overlap > tolerance:
        raise ValueError("water triangulation changes coverage or overlaps")
    edges = np.sort(
        np.stack(
            [faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]], axis=1
        ).reshape(-1, 2),
        axis=1,
    )
    _, counts = np.unique(edges, axis=0, return_counts=True)
    if counts.max(initial=0) > 2:
        raise ValueError("water triangles have a nonmanifold edge")
    report = {
        "planned_vertices_most": planned_vertices,
        "planned_faces_most": planned_faces,
        "vertices": len(xy),
        "faces": len(faces),
        "holes": holes,
        "components": len(parts),
        "area_m2": float(body.area),
        "missing_or_excess_area_m2": missing_or_excess,
        "overlap_area_m2": overlap,
        "shared_edges": int((counts == 2).sum()),
        "boundary_edges": int((counts == 1).sum()),
    }
    return Mesh(np.c_[xy, np.zeros(len(xy))], faces), report
