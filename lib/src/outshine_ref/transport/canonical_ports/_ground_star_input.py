"""Bounded original DEM observations for an already source-qualified Ground star."""

import time

import numpy as np

from ...contracts import Mesh
from ..port_heights import PortHeightInput
from ._geodesy import ground_geographic_at, positions_in_frame
from ._ground_recipe import source_star
from ._ground_star_geometry import layout


def prepare_star_ground(
    index,
    edges,
    terrain,
    budget,
    *,
    deadline=None,
):
    started = time.monotonic()
    deadline = started + budget.surface.time_limit_s if deadline is None else deadline
    geometry_deadline = min(deadline, started + budget.surface.geometry_time_limit_s)
    if started >= deadline:
        raise ValueError("three_arm_ground_total_deadline")
    if budget.requests < 3 or budget.surface.max_queries < 3:
        raise ValueError("three_arm_ground_query_budget")
    if (
        budget.surface.time_limit_s <= 0
        or budget.surface.geometry_time_limit_s <= 0
        or budget.surface.solver_time_limit_s <= 0
    ):
        raise ValueError("three_arm_ground_time_budget")
    star, reason = source_star(index, edges)
    if reason:
        raise ValueError(reason)
    centre, leaves = star
    nodes, spans, _ = index
    anchor = nodes[centre]
    ends = positions_in_frame(anchor, [nodes[n] for n in leaves], np.zeros(3))[:, :2]
    widths = [spans[e].width_m for e in edges]
    grades = [spans[e].maximum_gradient for e in edges]
    raw, geometry = layout(
        ends,
        widths,
        grades,
        proximal=True,
        cut_method="linear",
        seconds=geometry_deadline - time.monotonic(),
    )
    geometry.update(
        source_anchor=anchor,
        source_spans=edges,
        source_end_nodes=leaves,
        widths_m=widths,
        maximum_gradients=grades,
    )
    return lift_star_input(
        anchor,
        raw,
        geometry,
        terrain,
        budget,
        edges=edges,
        leaves=leaves,
        deadline=deadline,
        geometry_deadline=geometry_deadline,
    )


def lift_star_input(
    anchor,
    raw,
    geometry,
    terrain,
    budget,
    *,
    edges,
    leaves,
    deadline,
    geometry_deadline,
):
    """Observe a generated finite input; this helper grants no scope authority."""
    sizes = [len(raw["xy"]), len(raw["faces"]), len(raw["roles"]), len(raw["ports"])]
    caps = [
        budget.surface.max_vertices,
        budget.surface.max_faces,
        budget.surface.max_parts,
        budget.surface.max_ports,
    ]
    if (
        any(n > cap for n, cap in zip(sizes, caps))
        or len(raw["xy"]) > budget.terrain_samples
    ):
        raise ValueError("three_arm_ground_input_budget")
    if (
        sizes[0] > budget.surface.max_output_vertices
        or sizes[1] > budget.surface.max_output_faces
    ):
        raise ValueError("three_arm_ground_output_budget")
    if time.monotonic() >= geometry_deadline:
        raise ValueError("three_arm_ground_geometry_deadline")
    geographic = ground_geographic_at(anchor, raw["xy"])
    if time.monotonic() >= geometry_deadline:
        raise ValueError("three_arm_ground_geometry_deadline")
    heights = np.asarray(terrain(geographic), float)
    if time.monotonic() >= deadline:
        raise ValueError("three_arm_ground_total_deadline")
    if (
        heights.shape != (sizes[0],)
        or not np.isfinite(heights).all()
        or np.any(abs(heights) > 1e6)
    ):
        raise ValueError("missing_or_invalid_bound_terrain_samples")
    vertices = positions_in_frame(anchor, geographic, heights)
    inputs = PortHeightInput(
        Mesh(vertices, raw["faces"]),
        raw["owners"],
        raw["roles"],
        raw["ports"],
        raw["links"],
        raw["frames"],
    )
    original_caps = [
        {
            "source_node": int(node),
            "ground_edge": edge,
            "original_vertices": tuple(map(int, cap)),
        }
        for node, edge, cap in zip(leaves, edges, raw["caps"])
    ]
    geometry.update(
        source_anchor=anchor,
        source_spans=edges,
        source_end_nodes=leaves,
        terrain_samples=sizes[0],
        original_caps=original_caps,
        part_source_edges=[
            edges if a == -1 else (edges[a],) for a in geometry["part_source_arms"]
        ],
    )
    return anchor, inputs, original_caps, geometry
