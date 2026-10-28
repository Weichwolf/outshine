"""Two bounded original cycles; finite geometry grants no Source authority."""

import time

import numpy as np
from ground_cycle_faces import source_faces as cycle_faces
from ground_cyclic_source import source_component
from ground_cyclic_surface import finish_surface, plan_source_surface
from outshine_ref.transport.canonical_ports._ground_star_input import lift_star_input

TWO_CYCLE_RECIPE = "outshine/experimental-coarse-ground-bicyclic/1"


def source_two_cycles(index, selected):
    anchor, leaves, _ = source_component(
        index,
        selected,
        minimum_cycles=2,
        maximum_cycles=2,
        maximum_spans=12,
        prefix="two_cycle",
    )
    return anchor, leaves


def source_faces(geometry, vertices, triangles):
    return cycle_faces(
        geometry, vertices, triangles, expected_holes=2, prefix="two_cycle"
    )


def prepare_two_cycles(
    index, selected, terrain, budget, *, deadline, cut_method="adaptive"
):
    selected = tuple(sorted(selected))
    anchor_node, leaves = source_two_cycles(index, selected)
    if len(selected) > budget.selected_spans:
        raise ValueError("two_cycle_selected_span_budget")
    anchor, raw, geometry, geometry_deadline = plan_source_surface(
        index,
        selected,
        budget,
        anchor_node=anchor_node,
        leaves=leaves,
        expected_holes=2,
        deadline=deadline,
        cut_method=cut_method,
    )
    original_faces = source_faces(geometry, raw["xy"], raw["faces"])
    geometry.update(
        recipe=TWO_CYCLE_RECIPE if cut_method == "adaptive" else None,
        source_cycle_faces_wkb=original_faces,
    )
    node_ids = geometry["source_node_ids"]
    return lift_star_input(
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
    prepared = prepare_two_cycles(
        index, selected, terrain, budget, deadline=deadline, cut_method=cut_method
    )
    return finish_surface(
        prepared,
        selected,
        budget,
        deadline=deadline,
        prior_limit=prior_limit,
        scope="Finite experimental two-cycle geometry; no canonical Source/terminal/World admission. Original Unknown, missing topology and end authority remain external facts.",
    )
