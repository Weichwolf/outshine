"""Conforming extension that keeps the original vertex positions and affine faces."""

import time

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import (
    ProjectedSurfaceBudget,
    SurfaceSampler,
    conform_triangles,
)
from outshine_ref.geometry.projected_surface import _time_check, _weld_xy

DEFAULT_BUDGET = ProjectedSurfaceBudget()


def surface_domain(mesh):
    faces = shapely.polygons(mesh.vertices[mesh.triangles, :2])
    if not shapely.coverage_is_valid(faces):
        raise ValueError("valid original affine coverage required")
    return shapely.coverage_union_all(faces)


def flat_domain(domain, *, budget=DEFAULT_BUDGET):
    began = time.monotonic()
    _time_check(began, budget)
    if shapely.get_num_coordinates(domain) > budget.max_boundary_vertices:
        raise ValueError("retained_boundary_vertex_budget")
    parts = shapely.get_parts(shapely.constrained_delaunay_triangles(domain))
    if not 1 <= len(parts) <= budget.max_faces:
        raise ValueError("retained_face_budget")
    raw = shapely.get_coordinates(parts).reshape(-1, 4, 2)[:, :3]
    xy, inverse = np.unique(raw.reshape(-1, 2), axis=0, return_inverse=True)
    if len(xy) > budget.max_vertices:
        raise ValueError("retained_vertex_budget")
    xy, labels, _displacement = _weld_xy(xy, budget)
    faces = labels[inverse].reshape(-1, 3)
    a, b = xy[faces[:, 1]] - xy[faces[:, 0]], xy[faces[:, 2]] - xy[faces[:, 0]]
    determinant = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]
    faces = faces[determinant != 0]
    determinant = determinant[determinant != 0]
    faces[determinant < 0] = faces[determinant < 0, ::-1]
    _time_check(began, budget)
    return Mesh(np.c_[xy, np.zeros(len(xy))], faces)


def stitch(existing, addition, domain, *, budget=DEFAULT_BUDGET):
    began = time.monotonic()
    _time_check(began, budget)
    if (
        len(existing.vertices) + len(addition.vertices) > budget.max_vertices
        or len(existing.triangles) + len(addition.triangles) > budget.max_faces
    ):
        raise ValueError("retained_surface_input_budget")
    raw = np.vstack((existing.vertices[:, :2], addition.vertices[:, :2]))
    xy, first, inverse = np.unique(raw, axis=0, return_index=True, return_inverse=True)
    order = np.argsort(first)
    reordered = np.empty(len(order), int)
    reordered[order] = np.arange(len(order))
    xy, labels, displacement = _weld_xy(xy[order], budget)
    remap = labels[reordered[inverse]]
    old = remap[: len(existing.vertices)]
    new = remap[len(existing.vertices) :]
    if np.any(xy[old] != existing.vertices[:, :2]):
        raise ValueError("retained_surface_changed_original_vertex")
    triangles = np.vstack((old[existing.triangles], new[addition.triangles]))
    triangles, conformance = conform_triangles(
        xy,
        triangles,
        max_candidates=budget.max_interface_candidates,
        max_faces=budget.max_faces,
    )
    shapes = shapely.polygons(xy[triangles])
    keep = shapely.area(shapes) > 0
    removed = int(np.count_nonzero(~keep))
    triangles, shapes = triangles[keep], shapes[keep]
    if not len(shapes) or not shapely.coverage_is_valid(shapes):
        raise ValueError("retained_surface_invalid_coverage")
    error = float(shapely.coverage_union_all(shapes).symmetric_difference(domain).area)
    if error > max(1e-7, domain.area * 1e-10):
        raise ValueError("retained_surface_changed_domain")
    boundary = float(shapely.coverage_union_all(shapes).hausdorff_distance(domain))
    if boundary > 1e-8:
        raise ValueError("retained_surface_changed_boundary")
    _time_check(began, budget)
    return Mesh(np.c_[xy, np.zeros(len(xy))], triangles), {
        "coverage_error_m2": error,
        "boundary_hausdorff_m": boundary,
        "numeric_weld_m": displacement,
        "original_vertices_unchanged": True,
        "conformance": conformance,
        "removed_exact_zero_faces": removed,
        "vertices": len(xy),
        "faces": len(triangles),
    }


def old_contacts(existing, xy):
    contacts = SurfaceSampler(existing).sample(xy)
    mask = np.isfinite(contacts.points_enu_m[:, 2])
    if any(reason not in ("", "outside_surface") for reason in contacts.reasons):
        raise ValueError("old chosen surface contact unresolved")
    return mask, contacts.points_enu_m[:, 2]


def prior_heights(sources, xy):
    total, weights = np.zeros(len(xy)), np.zeros(len(xy))
    points = shapely.points(xy)
    from shared_bridge_geometry import corridor

    for source in sources:
        touched = shapely.dwithin(corridor(source), points, 1e-8)
        line = shapely.LineString(source["xy"])
        station = shapely.line_locate_point(line, points[touched])
        total[touched] += np.interp(station, [0, line.length], source["end_heights_m"])
        weights[touched] += 1
    if np.any(weights == 0):
        raise ValueError("extended ground vertex lost its source")
    return total / weights
