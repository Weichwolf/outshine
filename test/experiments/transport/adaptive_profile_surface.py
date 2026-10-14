"""Station-range midpoint refinement with a whole-face interpolation bound."""

import time

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import conform_triangles
from projected_profile_surface import projected_mesh
from surface_height_checks import normal_gradients


def exact_heights(stations, curve, first):
    values = np.clip(np.asarray(stations) - first, 0, curve.length_m)
    return curve.evaluate(values)


def seed(domain, origin, direction, curve, first):
    station = (shapely.get_coordinates(domain) - origin) @ direction
    start, end = curve.evaluate([0, curve.length_m])
    table = np.array(
        [
            [station.min() - 1, start],
            [first, start],
            [first + curve.length_m, end],
            [station.max() + 1, end],
        ]
    )
    # Contact regions are exact constraints, so their boundaries belong in both seeds.
    mesh, note = projected_mesh(domain, origin, direction, table)
    return mesh.vertices[:, :2].copy(), mesh.triangles.copy(), note["numeric_weld_m"]


def error_bounds(stations, faces, curve, first):
    values = stations[faces]
    minimum, maximum = values.min(axis=1), values.max(axis=1)
    constant = (maximum <= first) | (minimum >= first + curve.length_m)
    bound = curve.second_derivative_bound * (maximum - minimum) ** 2 / 8
    return np.where(constant, 0.0, bound)


def adaptive_mesh(
    domain,
    origin,
    direction,
    curve,
    first,
    *,
    height_error=0.01,
    grade_limit=0.025,
    seconds=2.0,
    max_faces=8192,
):
    began = time.monotonic()
    if (
        not np.isfinite(height_error)
        or height_error <= 0
        or not np.isfinite(grade_limit)
        or grade_limit <= 0
        or not np.isfinite(seconds)
        or seconds <= 0
        or type(max_faces) is not int
        or not 1 <= max_faces <= 8192
    ):
        raise ValueError("positive bounded interpolation and work limits required")
    origin, direction = np.asarray(origin), np.asarray(direction)
    if (
        origin.shape != (2,)
        or direction.shape != (2,)
        or not np.isfinite(origin).all()
        or not np.isfinite(direction).all()
        or abs(np.linalg.norm(direction) - 1) > 1e-12
    ):
        raise ValueError("finite origin and unit projection direction required")
    xy, faces, displacement = seed(domain, origin, direction, curve, first)
    for iteration in range(16):
        if len(xy) > 8192 or len(faces) > max_faces:
            raise ValueError("adaptive conforming face/vertex budget")
        if time.monotonic() - began > seconds:
            raise ValueError("adaptive surface time budget")
        stations = (xy - origin) @ direction
        bounds = error_bounds(stations, faces, curve, first)
        heights = exact_heights(stations, curve, first)
        gradients = normal_gradients(xy, faces, heights)
        grades = np.linalg.norm(gradients, axis=1)
        marked = np.flatnonzero(
            (bounds > height_error) | (grades > grade_limit * (1 + 1e-7))
        )
        if not len(marked):
            break
        triangles = faces[marked]
        positions = stations[triangles]
        edges = np.sort(
            np.c_[
                triangles[np.arange(len(triangles)), positions.argmin(axis=1)],
                triangles[np.arange(len(triangles)), positions.argmax(axis=1)],
            ],
            axis=1,
        )
        edges = np.unique(edges, axis=0)
        if len(xy) + len(edges) > 8192:
            raise ValueError("adaptive midpoint vertex budget")
        xy = np.vstack((xy, xy[edges].mean(axis=1)))
        # Shared-edge splits apply to every neighbor, including unmarked faces.
        faces, _ = conform_triangles(xy, faces)
    else:
        raise ValueError("adaptive refinement iteration budget")
    shapes = shapely.polygons(xy[faces])
    if not shapely.coverage_is_valid(shapes):
        raise ValueError("adaptive triangles overlap or do not conform")
    area_error = shapely.coverage_union_all(shapes).symmetric_difference(domain).area
    if area_error > max(1e-7, domain.area * 1e-10):
        raise ValueError("adaptive refinement changed complete domain")
    heights = exact_heights(stations, curve, first)
    return Mesh(np.c_[xy, heights], faces), {
        "coverage_error_m2": float(area_error),
        "numeric_weld_m": displacement,
        "whole_face_height_error_bound_m": float(bounds.max(initial=0)),
        "faces": len(faces),
        "vertices": len(xy),
        "iterations": iteration,
        "maximum_gradient_norm": float(grades.max()),
        "geometry_ms": (time.monotonic() - began) * 1000,
        "certificate": "For each barycentric interpolant, |E[h(t)]-h(E[t])| <= max|h''|*Var(t)/2 <= max|h''|*(max(t)-min(t))^2/8. Flat outside regions have zero error.",
    }
