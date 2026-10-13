"""Conforming ground/traffic partition before a common bounded height plan."""

import time

import numpy as np
import shapely

from outshine_ref.contracts import Mesh
from outshine_ref.geometry import conform_triangles
from outshine_ref.transport import HeightBudget, fit_surface_heights


def ground_layout(xy, road_faces, domain, *, method="coverage", max_faces=65536):
    if type(max_faces) is not int or max_faces <= 0 or len(road_faces) > max_faces:
        raise ValueError("ground input exceeds face budget")
    road = Mesh(np.c_[xy, np.zeros(len(xy))], road_faces)
    xy, road_faces = road.vertices[:, :2], road.triangles
    if domain.is_empty or not domain.is_valid or domain.geom_type != "Polygon":
        raise ValueError("valid polygon ground domain required")
    began = time.perf_counter()
    polygons = shapely.polygons(xy[road_faces])
    if not shapely.coverage_is_valid(polygons):
        raise ValueError("road input must be an edge-matched nonoverlapping coverage")
    audit_ms = (time.perf_counter() - began) * 1000
    began = time.perf_counter()
    footprint = (shapely.coverage_union_all if method == "coverage" else shapely.union_all)(polygons)
    if not domain.covers(footprint):
        raise ValueError("ground domain must contain every retained road face")
    union_ms = (time.perf_counter() - began) * 1000
    began = time.perf_counter()
    outside = shapely.normalize(domain.difference(shapely.normalize(footprint)))
    triangles = shapely.get_parts(shapely.constrained_delaunay_triangles(outside))
    positions = shapely.get_coordinates(triangles).reshape(-1, 4, 2)[:, :3]
    vertices, labels = np.unique(np.vstack((xy, positions.reshape(-1, 2))), axis=0, return_inverse=True)
    road_vertex_map = labels[:len(xy)]
    terrain_faces, conformance = conform_triangles(vertices, labels[len(xy):].reshape(-1, 3))
    if len(road_faces) + len(terrain_faces) > max_faces:
        raise ValueError("ground output exceeds face budget")
    faces = np.vstack((road_vertex_map[road_faces], terrain_faces))
    p = vertices[faces]
    u, v = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
    signed = (u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0]) * .5
    if np.any(signed == 0):
        raise ValueError("ground triangulation has a degenerate projected face")
    faces[signed < 0] = faces[signed < 0][:, [0, 2, 1]]
    triangulation_ms = (time.perf_counter() - began) * 1000
    began = time.perf_counter()
    if not shapely.coverage_is_valid(shapely.polygons(vertices[faces])):
        raise ValueError("ground/road partition contains overlap or unmatched interfaces")
    edges, counts = np.unique(np.sort(np.vstack((faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]])), axis=1),
                             axis=0, return_counts=True)
    boundary = shapely.linestrings(vertices[edges[counts == 1]])
    if np.any(counts > 2) or not np.all(shapely.covered_by(boundary, domain.boundary)):
        raise ValueError("ground partition has an interior crack or nonmanifold edge")
    error = abs(float(abs(signed).sum()) - domain.area)
    if error > max(1e-7, domain.area * 1e-12):
        raise ValueError("ground partition does not cover its complete domain")
    report = dict(method=method, input_coverage_ms=audit_ms, union_ms=union_ms, triangulation_ms=triangulation_ms,
                  output_audit_ms=(time.perf_counter() - began) * 1000, road_faces=len(road_faces),
                  terrain_faces=len(terrain_faces), vertices=len(vertices), coverage_error_m2=error,
                  area_m2=domain.area, boundary_edges=len(boundary), interface_conformance=conformance)
    return dict(xy=vertices, faces=faces, road_vertex_map=road_vertex_map, road_faces=len(road_faces)), report


def fit_ground(layout, targets, road_frames, *, soil_grade=2 / 3, budget=HeightBudget()):
    if not np.isfinite(soil_grade) or soil_grade <= 0:
        raise ValueError("finite positive procedural soil grade required")
    count = layout["road_faces"]
    frames = np.concatenate((road_frames, np.broadcast_to(np.eye(2) / soil_grade, (len(layout["faces"]) - count, 2, 2))))
    mesh = Mesh(np.c_[layout["xy"], targets], layout["faces"])
    return fit_surface_heights(mesh, frames, budget=budget), frames
