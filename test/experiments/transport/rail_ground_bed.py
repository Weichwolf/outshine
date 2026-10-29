"""Construct a bounded rail bed and earthworks without granting Source authority."""

import time

import numpy as np
from coincident_ground_geometry import layout
from coincident_ground_sources import prepare_sources
from constructive_earthwork_plane import affine_prior_conflict, fit_plane
from ground_contact import ContactLimits, build_contact
from outshine_ref.contracts import Mesh
from outshine_ref.transport.canonical_ports._geodesy import (
    ground_geographic_at,
    positions_in_frame,
)
from rail_ground_profile import RailGroundProfile


def geometry(index, profile, budget, *, shared):
    if not isinstance(profile, RailGroundProfile):
        raise TypeError("Bound constructive rail profile required")
    profile.check_index(index)
    selected = tuple(m.identifier for m in profile.members)
    source = prepare_sources(
        index, selected, budget, construction_grades=profile.grades()
    )
    anchor = index[0][source.node_ids[0]]
    axes = positions_in_frame(
        anchor, [index[0][n] for n in source.node_ids], np.zeros(len(source.node_ids))
    )[:, :2]
    raw, domain, sections, work = layout(
        source, axes, budget, shared=shared, physical_overlap=True, maximum_holes=8
    )
    return source, anchor, axes, raw, domain, sections, work


def oriented_mesh(xy, faces, plane):
    vertices = np.c_[xy, plane.evaluate(xy)]
    faces = np.array(faces, copy=True)
    points = vertices[faces]
    signed = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])[:, 2]
    if not np.isfinite(vertices).all() or np.any(signed == 0):
        raise ValueError("rail_constructed_degenerate_or_nonfinite_bed")
    negative = signed < 0
    faces[negative] = faces[negative][:, ::-1]
    return Mesh(vertices, faces)


def construct(index, profile, terrain, budget, *, shared=True, contact_limits=None):
    started = time.monotonic()
    deadline = started + budget.surface.time_limit_s
    limits = ContactLimits() if contact_limits is None else contact_limits
    source, anchor, axes, raw, domain, sections, work = geometry(
        index, profile, budget, shared=shared
    )
    xy = np.unique(np.vstack((raw["xy"], sections.reshape(-1, 2), axes)), axis=0)
    if len(xy) > min(512, budget.terrain_samples):
        raise ValueError("rail_complete_observation_budget")
    counters = {
        "dem_calls": 0,
        "dem_points": 0,
        "maximum_ignored_horizontal_projection_m": 0.0,
    }

    def sample_z(points):
        geographic = ground_geographic_at(anchor, np.asarray(points, dtype=float))
        heights = np.asarray(terrain(geographic), dtype=float)
        counters["dem_calls"] += 1
        counters["dem_points"] += len(points)
        if heights.shape != (len(points),) or not np.isfinite(heights).all():
            raise ValueError("rail_missing_or_nonfinite_bound_dem")
        lifted = positions_in_frame(anchor, geographic, heights)
        counters["maximum_ignored_horizontal_projection_m"] = max(
            counters["maximum_ignored_horizontal_projection_m"],
            float(np.linalg.norm(lifted[:, :2] - points, axis=1).max(initial=0)),
        )
        return lifted[:, 2]

    heights = sample_z(xy)
    original_conflict = affine_prior_conflict(
        xy, heights, profile.maximum_gradient, 0.5
    )
    remaining = deadline - time.monotonic()
    if remaining <= 0:
        raise ValueError("rail_construction_deadline")
    # The earthwork reach comes from the existing first collar margin and soil
    # grade. It does not replace the original DEM-field prior of0.5m.
    earthwork_reach = limits.margins_m[0] * limits.soil_grade
    plane, fitting = fit_plane(
        xy,
        heights,
        maximum_gradient=profile.maximum_gradient,
        maximum_earthwork_m=earthwork_reach,
        time_limit_s=min(remaining, budget.surface.solver_time_limit_s),
    )
    if plane is None:
        return {"state": "pending", "reason": fitting["reason"], "fitting": fitting}
    bed = oriented_mesh(raw["xy"], raw["faces"], plane)
    original_bed_heights = sample_z(bed.vertices[:, :2])
    if time.monotonic() >= deadline:
        raise ValueError("rail_construction_deadline")
    contact = build_contact(
        bed, original_bed_heights, sample_z, limits=limits, whole_profiles=True
    )
    if contact["status"] != "candidate":
        return {"state": "pending", "reason": contact["reason"], "terrain": contact}
    if time.monotonic() >= deadline:
        raise ValueError("rail_construction_deadline")
    return {
        "state": "constructed",
        "reason": "",
        "source": source,
        "profile": profile,
        "anchor": anchor,
        "axes": axes,
        "domain": domain,
        "sections": sections,
        "bed": bed,
        "plane": plane,
        "observations": np.c_[xy, heights],
        "fitting": fitting,
        "terrain": contact,
        "original_affine_prior_conflict": original_conflict,
        "construction_earthwork_reach_m": earthwork_reach,
        "work": work | counters,
        "known_owned_bed_array_bytes": bed.vertices.nbytes + bed.triangles.nbytes,
        "total_internal_copied_bytes": None,
        "scope": "Explicit constructed bed and slope-bounded terrain. Original source profiles/Unknown/logical topology/Source/EndContact/World authority remain unchanged. No original DEM-fit, driving or native/device admission.",
    }
