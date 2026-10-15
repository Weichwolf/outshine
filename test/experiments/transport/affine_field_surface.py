"""Build actual terrain/traffic facets within their shared piecewise affine height field."""

import time

import numpy as np
import shapely
from ground_surface_showcase import sample_vertices
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler, conform_triangles
from outshine_ref.transport.surface import _sorted_boundary
from retained_surface import surface_domain
from surface_height_checks import normal_gradients


def mesh_field(field, traffic, domain, *, max_faces=262144):
    began = time.monotonic()
    if (
        type(max_faces) is not int
        or not 1 <= max_faces <= 262144
        or not 1 <= len(field.triangles) <= 8192
        or max(
            shapely.get_num_coordinates(traffic), shapely.get_num_coordinates(domain)
        )
        > 262144
        or not traffic.is_valid
        or not domain.is_valid
        or not domain.covers(traffic)
    ):
        raise ValueError("bounded covered terrain/traffic domains required")
    if domain.difference(surface_domain(field)).area > 1e-7:
        raise ValueError("height field must cover the complete actual world domain")
    edges = field.triangles[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
    edges = np.unique(np.sort(edges, axis=1), axis=0)
    lines = np.r_[
        shapely.linestrings(field.vertices[edges, :2]),
        shapely.get_parts(traffic.boundary),
        shapely.get_parts(domain.boundary),
    ]
    if shapely.get_num_coordinates(lines).sum() > 1048576:
        raise ValueError("field arrangement input coordinate budget")
    # One noded arrangement owns every shared intersection. Cutting neighbouring
    # triangles independently did not give a valid coverage on the real input.
    noded = shapely.get_parts(shapely.union_all(lines))
    if len(noded) > 1048576 or shapely.get_num_coordinates(noded).sum() > 1048576:
        raise ValueError("field arrangement segment budget")
    parts = shapely.get_parts(shapely.polygonize(noded))
    parts = parts[shapely.covers(domain, shapely.point_on_surface(parts))]
    triangles = shapely.get_parts(shapely.constrained_delaunay_triangles(parts))
    if not 1 <= len(triangles) <= max_faces:
        raise ValueError("field projection face budget")
    points = shapely.get_coordinates(triangles).reshape(-1, 4, 2)[:, :3]
    xy, labels = np.unique(points.reshape(-1, 2), axis=0, return_inverse=True)
    if len(xy) > max_faces:
        raise ValueError("field projection vertex budget")
    faces = labels.reshape(-1, 3)
    a, b = xy[faces[:, 1]] - xy[faces[:, 0]], xy[faces[:, 2]] - xy[faces[:, 0]]
    determinant = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]
    faces = faces[determinant != 0]
    determinant = determinant[determinant != 0]
    faces[determinant < 0] = faces[determinant < 0][:, [0, 2, 1]]
    faces, conformance = conform_triangles(
        xy, faces, max_faces=max_faces, max_candidates=2097152
    )
    # Height queries must not round-trip and perturb the owned XY triangulation.
    xyz = np.c_[xy, sample_vertices(SurfaceSampler(field), xy)[:, 2]]
    mesh = Mesh(xyz, faces)
    boundary = shapely.linestrings(xy[_sorted_boundary(faces)])
    if not np.all(shapely.covered_by(boundary, domain.boundary.buffer(1e-8))):
        raise ValueError("field arrangement contains an interior crack")
    centres = shapely.points(xy[faces].mean(axis=1))
    road = shapely.covers(traffic, centres)
    grade = np.linalg.norm(normal_gradients(xy, faces, xyz[:, 2]), axis=1)
    maximum = np.max(grade / np.where(road, 0.025, 2 / 3))
    actual_road = surface_domain(Mesh(xyz, faces[road]))
    error = surface_domain(mesh).symmetric_difference(domain).area
    road_error = actual_road.symmetric_difference(traffic).area
    if not np.isfinite(maximum) or maximum > 1 + 1e-7 or max(error, road_error) > 1e-7:
        raise ValueError(
            f"actual lifted field geometry violates grade or coverage: {maximum}, {error}, {road_error}"
        )
    return (
        mesh,
        road,
        {
            "faces": len(faces),
            "vertices": len(xy),
            "road_faces": int(road.sum()),
            "maximum_normalized_grade": float(maximum),
            "domain_error_m2": error,
            "traffic_error_m2": road_error,
            "noded_segments": len(noded),
            "conformance": conformance,
            "total_ms": (time.monotonic() - began) * 1000,
        },
    )
