"""Shared bounded layout and full field qualification for explicit cyclic recipes."""

import time
from dataclasses import replace

import numpy as np
from outshine_ref.transport.canonical_ports._geodesy import positions_in_frame
from outshine_ref.transport.canonical_ports._ground import fit_ground
from outshine_ref.transport.canonical_ports._ground_network_geometry import layout
from outshine_ref.transport.canonical_ports._ground_scope_field import affine_surface
from outshine_ref.transport.canonical_ports._ground_terminal_quality import (
    qualify_terminal_surface,
)


def plan_source_surface(
    index,
    selected,
    budget,
    *,
    anchor_node,
    leaves,
    expected_holes,
    deadline,
    cut_method,
    maximum_holes=2,
):
    if min(budget.requests, budget.surface.max_queries) < len(leaves):
        raise ValueError("cycle_terminal_query_budget")
    nodes, spans, _ = index
    node_ids = sorted({n for e in selected for n in spans[e].nodes})
    local = {n: i for i, n in enumerate(node_ids)}
    anchor = nodes[anchor_node]
    xyz = positions_in_frame(
        anchor, [nodes[n] for n in node_ids], np.zeros(len(node_ids))
    )
    edges = np.array([[local[n] for n in spans[e].nodes] for e in selected])
    geometry_deadline = min(
        deadline, time.monotonic() + budget.surface.geometry_time_limit_s
    )
    raw, geometry = layout(
        xyz[:, :2],
        edges,
        np.array([spans[e].width_m for e in selected]),
        np.array([spans[e].maximum_gradient for e in selected]),
        seconds=geometry_deadline - time.monotonic(),
        cut_method=cut_method,
        expected_holes=expected_holes,
        maximum_holes=maximum_holes,
    )
    geometry.update(
        source_node_ids=tuple(node_ids),
        source_nodes_EN=xyz[:, :2],
        source_edges_local=edges,
    )
    return anchor, raw, geometry, geometry_deadline


def finish_surface(prepared, selected, budget, *, deadline, prior_limit, scope):
    anchor, inputs, caps, geometry = prepared
    remaining = deadline - time.monotonic()
    if remaining <= 0:
        raise ValueError("cycle_total_deadline")
    fitting_budget = replace(
        budget,
        surface=replace(
            budget.surface,
            time_limit_s=remaining,
            solver_time_limit_s=min(budget.surface.solver_time_limit_s, remaining),
            geometry_time_limit_s=min(budget.surface.geometry_time_limit_s, remaining),
        ),
    )
    field, fitting = fit_ground(inputs, prior_limit, "auto", fitting_budget)
    if field is None:
        raise ValueError(
            "cycle_field_unavailable:" + fitting.get("reason", "unresolved")
        )
    mesh = (
        field.plan.mesh if hasattr(field, "plan") else affine_surface(inputs, field)[0]
    )
    report = {"source_input": inputs}
    mesh, parts, quality, copied = qualify_terminal_surface(
        field, mesh, report, prior_limit
    )
    if time.monotonic() >= deadline:
        raise ValueError("cycle_total_deadline")
    return {
        "anchor": anchor,
        "inputs": inputs,
        "mesh": mesh,
        "field": field,
        "geometry": geometry,
        "source_terminal_cuts": caps,
        "fitting": fitting,
        "quality": quality,
        "face_parts": parts,
        "known_plan_copy_bytes": copied,
        "source_members": tuple(sorted(selected)),
        "scope": scope,
    }
