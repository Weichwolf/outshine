"""Lift the complete small source tree with bounded, actual DEM observations."""

import time

import numpy as np

from ._geodesy import positions_in_frame
from ._ground_network_geometry import layout
from ._ground_recipe import source_network
from ._ground_star_input import lift_star_input


def prepare_network_ground(
    index, selected, terrain, budget, *, deadline, cut_method="adaptive"
):
    selected = tuple(sorted(selected))
    started = time.monotonic()
    geometry_deadline = min(deadline, started + budget.surface.geometry_time_limit_s)
    shape, reason = source_network(index, selected)
    if reason:
        raise ValueError(reason)
    anchor_node, leaves = shape
    if min(budget.requests, budget.surface.max_queries) < len(leaves):
        raise ValueError("ground_terminal_query_budget")
    nodes, spans, _ = index
    node_ids = sorted({node for edge in selected for node in spans[edge].nodes})
    local = {node: i for i, node in enumerate(node_ids)}
    anchor = nodes[anchor_node]
    xyz = positions_in_frame(
        anchor, [nodes[node] for node in node_ids], np.zeros(len(node_ids))
    )
    edges = np.array([[local[n] for n in spans[e].nodes] for e in selected])
    raw, geometry = layout(
        xyz[:, :2],
        edges,
        np.array([spans[e].width_m for e in selected]),
        np.array([spans[e].maximum_gradient for e in selected]),
        seconds=geometry_deadline - time.monotonic(),
        cut_method=cut_method,
    )
    ends = tuple(node_ids[n] for n in geometry["terminal_node_indices"])
    cap_edges = tuple(selected[e] for e in geometry["terminal_edge_indices"])
    geometry.update(
        source_node_ids=tuple(node_ids),
        source_nodes_EN=xyz[:, :2],
        source_edges_local=edges,
    )
    return lift_star_input(
        anchor,
        raw,
        geometry,
        terrain,
        budget,
        edges=selected,
        leaves=ends,
        cap_edges=cap_edges,
        deadline=deadline,
        geometry_deadline=geometry_deadline,
    )
