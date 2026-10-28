"""Opt-in affine physical cover; original Unknown and routing remain external."""

import time

import numpy as np
from coincident_ground_geometry import layout
from coincident_ground_sources import prepare_sources
from outshine_ref.contracts import Mesh
from outshine_ref.transport.canonical_ports._affine import AffineGround
from outshine_ref.transport.canonical_ports._geodesy import (
    ground_geographic_at,
    positions_in_frame,
)
from outshine_ref.transport.canonical_ports._ground_scope_field import affine_surface
from outshine_ref.transport.canonical_ports._ground_terminal_quality import (
    qualify_terminal_surface,
)
from outshine_ref.transport.port_heights import PortHeightInput


def generate(index, selected, terrain, budget, *, shared=True, prior_limit=0.5):
    if not np.isfinite(prior_limit) or not 0 < prior_limit <= 0.5:
        raise ValueError("cover_prior_limit")
    deadline = time.monotonic() + budget.surface.time_limit_s
    geometry_deadline = min(
        deadline, time.monotonic() + budget.surface.geometry_time_limit_s
    )
    if time.monotonic() >= geometry_deadline:
        raise ValueError("cover_geometry_deadline")
    source = prepare_sources(index, selected, budget)
    nodes = index[0]
    anchor = nodes[source.node_ids[0]]
    axes = positions_in_frame(
        anchor, [nodes[n] for n in source.node_ids], np.zeros(len(source.node_ids))
    )[:, :2]
    raw, domain, sections, work = layout(source, axes, budget, shared=shared)
    observation_xy, inverse = np.unique(
        np.vstack((raw["xy"], sections.reshape(-1, 2), axes)),
        axis=0,
        return_inverse=True,
    )
    if len(observation_xy) > budget.terrain_samples:
        raise ValueError("cover_complete_observation_budget")
    geographic = ground_geographic_at(anchor, observation_xy)
    if time.monotonic() >= geometry_deadline:
        raise ValueError("cover_geometry_deadline")
    heights = np.asarray(terrain(geographic), float)
    if (
        heights.shape != (len(geographic),)
        or not np.isfinite(heights).all()
        or np.any(abs(heights) > 1e6)
    ):
        raise ValueError("cover_missing_or_invalid_terrain")
    lifted = positions_in_frame(anchor, geographic, heights)
    observations = np.c_[observation_xy, lifted[:, 2]]
    vertices = observations[inverse[: len(raw["xy"])]]
    inputs = PortHeightInput(
        Mesh(vertices, raw["faces"]),
        raw["owners"],
        raw["roles"],
        raw["ports"],
        raw["links"],
        raw["frames"],
    )
    field = AffineGround(inputs)
    original_errors = abs(
        field.intercept
        + (observation_xy - field.origin) @ field.gradient
        - observations[:, 2]
    )
    if float(original_errors.max()) > prior_limit:
        raise ValueError("cover_original_prior_limit")
    mesh = affine_surface(inputs, field)[0]
    mesh, parts, quality, copied = qualify_terminal_surface(
        field, mesh, {"source_input": inputs}, prior_limit
    )
    if time.monotonic() >= deadline:
        raise ValueError("cover_total_deadline")
    return {
        "source": source,
        "anchor": anchor,
        "axes": axes,
        "domain": domain,
        "sections": sections,
        "inputs": inputs,
        "observations": observations,
        "mesh": mesh,
        "field": field,
        "face_parts": parts,
        "quality": quality,
        "work": work
        | {
            "terrain_samples": len(heights),
            "maximum_all_original_observation_error_m": float(original_errors.max()),
            "maximum_discarded_horizontal_height_projection_m": float(
                np.linalg.norm(lifted[:, :2] - observation_xy, axis=1).max()
            ),
        },
        "known_plan_copy_bytes": copied,
        "total_internal_copied_bytes": None,
        "scope": "One affine physical ground patch with unchanged original members/directions and fixed source-frame XY. Bound geographic DEM observations contribute ENU-Up only, not geodetic-height horizontal displacement. No logical graph rewrite, original-end/Source/World/routing/controller/terrain/device admission.",
    }
