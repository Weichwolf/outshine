"""Whole bounded multi-loop ground geometry with original Source facts retained."""

import time

import numpy as np
from ground_cycle_faces import source_faces
from ground_cyclic_source import source_component
from ground_cyclic_surface import finish_surface, plan_source_surface
from outshine_ref.transport.canonical_ports._ground_star_input import lift_star_input

MULTICYCLE_RECIPE = "outshine/experimental-coarse-ground-multicycle/1"


def generate(
    index, selected, terrain, budget, *, cut_method="adaptive", prior_limit=0.5
):
    if (
        cut_method not in ("adaptive", "linear")
        or not np.isfinite(prior_limit)
        or not 0 < prior_limit <= 1000
    ):
        raise ValueError("Known cycle cut procedure and positive prior bound required")
    deadline = time.monotonic() + budget.surface.time_limit_s
    selected = tuple(sorted(selected))
    anchor_node, leaves, cycles = source_component(
        index,
        selected,
        minimum_cycles=3,
        maximum_cycles=8,
        maximum_spans=32,
        prefix="multicycle",
    )
    if len(selected) > budget.selected_spans:
        raise ValueError("multicycle_selected_span_budget")
    anchor, raw, geometry, geometry_deadline = plan_source_surface(
        index,
        selected,
        budget,
        anchor_node=anchor_node,
        leaves=leaves,
        expected_holes=cycles,
        maximum_holes=8,
        deadline=deadline,
        cut_method=cut_method,
    )
    originals = source_faces(
        geometry,
        raw["xy"],
        raw["faces"],
        expected_holes=cycles,
        prefix="multicycle",
    )
    geometry.update(
        recipe=MULTICYCLE_RECIPE if cut_method == "adaptive" else None,
        source_cycle_faces_wkb=originals,
    )
    node_ids = geometry["source_node_ids"]
    prepared = lift_star_input(
        anchor,
        raw,
        geometry,
        terrain,
        budget,
        edges=selected,
        leaves=tuple(node_ids[n] for n in geometry["terminal_node_indices"]),
        cap_edges=tuple(selected[e] for e in geometry["terminal_edge_indices"]),
        deadline=deadline,
        geometry_deadline=geometry_deadline,
    )
    return finish_surface(
        prepared,
        selected,
        budget,
        deadline=deadline,
        prior_limit=prior_limit,
        scope="Finite experimental whole multi-loop geometry; no Source/terminal/World admission. Original Unknown, missing topology and end authority remain external facts.",
    )
