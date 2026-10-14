"""Complete compatible field extension with unchanged previously chosen surfaces."""

import time

import numpy as np
import shapely
from bridge_continuation_field import TOP_ROLES, extension_field, field_certificate
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import (
    ProjectedSurfaceBudget,
    projected_height_surface,
    vertical_clearance,
)
from retained_surface import (
    flat_domain,
    old_contacts,
    prior_heights,
    stitch,
    surface_domain,
)
from shared_bridge_geometry import corridor
from surface_height_methods import checked_fit, face_slopes

DEFAULT_BUDGET = ProjectedSurfaceBudget()


def unchanged_surface(existing, result):
    forward = vertical_clearance(existing, result)
    backward = vertical_clearance(result, existing)
    if not forward["valid"] or not backward["valid"]:
        raise ValueError("old_surface_comparison_budget")
    maximum = max(
        abs(forward["minimum_clearance_m"]), abs(backward["minimum_clearance_m"])
    )
    if maximum > 1e-7:
        raise ValueError("previously_chosen_surface_changed")
    return {"maximum_height_difference_m": maximum, "affine_overlap_verified": True}


def extend_ground(sources, existing, *, method, budget):
    ground = [s for s in sources if s["role"] == "ground"]
    if not 1 <= len(ground) <= 64:
        raise ValueError("ground_component_source_budget")
    domain = shapely.union_all([corridor(s) for s in ground])
    extra = domain.difference(surface_domain(existing))
    if extra.area <= 0:
        return existing, {"unchanged": True}
    addition = flat_domain(extra, budget=budget)
    planar, geometry = stitch(existing, addition, domain, budget=budget)
    xy, faces = planar.vertices[:, :2], planar.triangles
    target = prior_heights(ground, xy)
    mask, contacts = old_contacts(existing, xy)
    pins = {int(i): float(contacts[i]) for i in np.flatnonzero(mask)}
    heights, fit = checked_fit(
        method,
        xy,
        faces,
        target,
        maximum_grade=0.025,
        seconds=budget.time_limit_s,
        pins=pins,
    )
    if heights is None:
        raise ValueError("ground_" + fit["reason"])
    if fit["maximum_height_change_m"] > 2.0 + 1e-7:
        raise ValueError("ground_prior_change_budget")
    result = Mesh(np.c_[xy, heights], faces)
    preservation = unchanged_surface(existing, result)
    return result, {"geometry": geometry, "fit": fit, "preserved": preservation}


def extend_top(sources, existing, field, profile, *, geometry_method, budget):
    domain = shapely.union_all([corridor(s) for s in sources if s["role"] in TOP_ROLES])
    _old_domain, origin, axis, table, _curve = field
    if geometry_method == "rebuild":
        mesh, geometry = projected_height_surface(
            domain, origin, axis, table, budget=budget
        )
        if mesh is None:
            raise ValueError(geometry["reason"])
    elif geometry_method == "retain":
        extra = domain.difference(surface_domain(existing))
        if extra.area <= 0:
            return existing, {"unchanged": True}
        addition, sampling = projected_height_surface(
            extra, origin, axis, table, budget=budget
        )
        if addition is None:
            raise ValueError(sampling["reason"])
        planar, geometry = stitch(existing, addition, domain, budget=budget)
        xy = planar.vertices[:, :2]
        station = (xy - origin) @ axis
        heights = np.interp(station, table[:, 0], table[:, 1])
        mask, old = old_contacts(existing, xy)
        heights[mask] = old[mask]
        mesh = Mesh(np.c_[xy, heights], planar.triangles)
        geometry["addition_sampling"] = sampling
    else:
        raise ValueError("unknown complete-field mesh method")
    slopes = face_slopes(mesh.vertices[:, :2], mesh.triangles, mesh.vertices[:, 2])
    maximum = float(slopes.max())
    if maximum > 0.024 * (1 + 1e-7) + 1e-10:
        raise ValueError("extended_top_whole_face_grade")
    preservation = unchanged_surface(existing, mesh)
    return mesh, dict(
        geometry,
        maximum_face_grade=maximum,
        preserved=preservation,
        **field_certificate(mesh, field, profile),
    )


def build(
    sources,
    approaches,
    root,
    old,
    *,
    ground_method="cone",
    geometry_method="rebuild",
    budget=DEFAULT_BUDGET,
):
    began = time.monotonic()
    report = {
        "valid": False,
        "valid_partial_base": True,
        "reason": "",
        "geometry_method": geometry_method,
        "ground_method": ground_method,
    }
    try:
        if not isinstance(budget, ProjectedSurfaceBudget):
            raise TypeError("explicit ProjectedSurfaceBudget required")
        if not 1 <= len(sources) <= 128 or len(
            {s["source_id"] for s in sources}
        ) != len(sources):
            raise ValueError("bounded unique complete-field sources required")
        if budget.time_limit_s == 0:
            raise ValueError("continuation_time_budget")
        lower, low_report = extend_ground(
            sources, old[1], method=ground_method, budget=budget
        )
        report["ground"] = low_report
        field, profile = extension_field(sources, approaches, root, old, lower, budget)
        report["profile"] = profile
        top, top_report = extend_top(
            sources,
            old[0],
            field,
            profile,
            geometry_method=geometry_method,
            budget=budget,
        )
        report["top"] = top_report
        gap = vertical_clearance(
            lower, Mesh(top.vertices - [0, 0, 0.35], top.triangles)
        )
        report["clearance"] = gap
        if not gap["valid"] or gap["minimum_clearance_m"] < 4.5 - 1e-7:
            raise ValueError(gap["reason"] or "new_known_surface_clearance")
        if time.monotonic() - began > 4 * budget.time_limit_s + 1:
            raise ValueError("continuation_total_time_budget")
        report.update(
            valid=True,
            ground=low_report,
            top=top_report,
            clearance=gap,
            used_source_ids=[
                s["source_id"] for s in sources if s["role"] in (*TOP_ROLES, "ground")
            ],
            unresolved=[
                {"source_id": s["source_id"], "reason": s["omission_reason"]}
                for s in sources
                if s["role"] in ("deferred", "unresolved")
            ],
            phase_ms=(time.monotonic() - began) * 1000,
            scope="Complete compatible corridor field in three pinned tiles, old affine surfaces preserved and known outside-source clearances only. Two disjoint flat-ended C1 transitions share one projected height field; finite C0 faces. Indoor/tunnel/ground-subway contacts, outside continuations, traffic routes, terrain/water and structure remain uncertified; clipping endpoints are not DEM floor anchors.",
        )
        return (top, lower), report
    except ValueError as failure:
        return old, dict(
            report, reason=str(failure), phase_ms=(time.monotonic() - began) * 1000
        )
