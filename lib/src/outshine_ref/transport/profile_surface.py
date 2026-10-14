"""A sampled C1 scalar height profile on a complete bounded XY domain."""

import time
from dataclasses import replace

import numpy as np
import shapely

from ..geometry import ProjectedSurfaceBudget, projected_height_surface
from .heights import _face_gradients
from .vertical import VerticalTransition, sample_vertical_transition

DEFAULT_BUDGET = ProjectedSurfaceBudget()


def _profile_table(profile, station, first, error, budget):
    low, high = float(station.min()), float(station.max())
    last = first + profile.length_m
    if high <= first or low >= last:
        end = 0.0 if high <= first else profile.length_m
        height = float(profile.evaluate([end])[0])
        return np.array([[low - 1, height], [high + 1, height]]), {
            "maximum_height_chord_bound_m": 0.0,
            "vertices": 2,
            "constant_domain": True,
            "reason": "",
        }
    reserve = int(low < first) + int(high > last)
    if budget.max_stations - reserve < 2:
        return None, {"valid": False, "reason": "projected_station_budget"}
    table, sampling = sample_vertical_transition(
        profile, height_error_m=error, max_vertices=budget.max_stations - reserve
    )
    if table is None:
        return None, dict(sampling, valid=False, reason="projected_station_budget")
    table[:, 0] += first
    if low < first:
        table = np.vstack(([low - 1, float(table[0, 1])], table))
    if high > last:
        table = np.vstack((table, [high + 1, float(table[-1, 1])]))
    return table, sampling


def mesh_projected_profile(
    domain,
    profile,
    origin_xy_m,
    direction_xy,
    *,
    start_station_m=0.0,
    height_error_m=0.01,
    budget=DEFAULT_BUDGET,
):
    """Return a complete conforming C0 mesh and a whole-face C1-field error bound.

    The analytic field is z=h(dot(XY-origin,unit_direction)-start), clamped to its
    flat endpoint heights outside the VerticalTransition. Its gradient is h'*axis.
    Source identities, frame, surface kind and contacts remain caller-owned.
    No horizontal path, driving, terrain, collision or LOD certificate is implied.
    Budget failure yields no mesh with a reason; wrong types raise TypeError and
    invalid values raise ValueError.
    """
    began = time.monotonic()
    if not isinstance(profile, VerticalTransition) or not isinstance(
        budget, ProjectedSurfaceBudget
    ):
        raise TypeError("owned VerticalTransition and ProjectedSurfaceBudget required")
    if (
        not np.isfinite(start_station_m)
        or abs(start_station_m) > 4e7
        or not np.isfinite(height_error_m)
        or height_error_m <= 0
    ):
        raise ValueError("finite bounded station and positive height error required")
    if budget.time_limit_s == 0:
        return None, {"valid": False, "reason": "projected_time_budget"}
    if (
        not hasattr(domain, "geom_type")
        or domain.geom_type not in ("Polygon", "MultiPolygon")
        or domain.is_empty
        or domain.has_z
    ):
        raise ValueError("nonempty polygonal XY domain required")
    if shapely.get_num_coordinates(domain) > budget.max_boundary_vertices:
        return None, {"valid": False, "reason": "projected_boundary_vertex_budget"}
    if not domain.is_valid:
        raise ValueError("valid polygonal XY domain required")
    if (
        len(origin_xy_m) != 2
        or len(direction_xy) != 2
        or any(not np.isscalar(v) for v in (*origin_xy_m, *direction_xy))
    ):
        raise ValueError("two-dimensional scalar reference required")
    origin, direction = np.asarray(origin_xy_m, float), np.asarray(direction_xy, float)
    if (
        not np.isfinite(origin).all()
        or not np.isfinite(direction).all()
        or abs(np.linalg.norm(direction) - 1) > 1e-12
    ):
        raise ValueError("finite origin and unit direction required")
    station = (shapely.get_coordinates(domain) - origin) @ direction
    table, sampling = _profile_table(
        profile, station, start_station_m, height_error_m, budget
    )
    if table is None:
        return None, sampling
    remaining = budget.time_limit_s - (time.monotonic() - began)
    if remaining <= 0:
        return None, {"valid": False, "reason": "projected_time_budget"}
    mesh, report = projected_height_surface(
        domain, origin, direction, table, budget=replace(budget, time_limit_s=remaining)
    )
    if mesh is None:
        return None, report
    gradients = _face_gradients(
        mesh.vertices[:, :2], mesh.triangles, mesh.vertices[:, 2]
    )
    maximum = float(np.linalg.norm(gradients, axis=1).max())
    if maximum > profile.grade_bound * (1 + 1e-7) + 1e-10:
        return None, dict(
            report,
            valid=False,
            reason="projected_profile_mesh_grade",
            maximum_gradient_norm=maximum,
        )
    if time.monotonic() - began > budget.time_limit_s:
        return None, {"valid": False, "reason": "projected_time_budget"}
    numeric_guard = profile.grade_bound * 1e-8 + 1e-7
    return mesh, dict(
        report,
        maximum_gradient_norm=maximum,
        whole_face_height_error_bound_m=sampling["maximum_height_chord_bound_m"]
        + numeric_guard,
        sampling=sampling,
        numeric_height_guard_m=numeric_guard,
        analytic_c1_height_field=True,
        mesh_continuity="C0",
        phase_ms=(time.monotonic() - began) * 1000,
    )
