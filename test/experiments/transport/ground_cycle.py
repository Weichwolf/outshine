"""Explicit single-cycle source geometry, without replacing original admission."""

import time
from collections import Counter, deque
from dataclasses import replace

import numpy as np
from outshine_ref.transport.canonical_ports._geodesy import positions_in_frame
from outshine_ref.transport.canonical_ports._ground import fit_ground
from outshine_ref.transport.canonical_ports._ground_network_geometry import layout
from outshine_ref.transport.canonical_ports._ground_scope_field import affine_surface
from outshine_ref.transport.canonical_ports._ground_star_input import lift_star_input
from outshine_ref.transport.canonical_ports._ground_terminal_quality import (
    qualify_terminal_surface,
)

CYCLE_RECIPE = "outshine/experimental-coarse-ground-unicyclic/1"


def source_cycle(index, selected):
    nodes, spans, incidence = index
    selected = tuple(sorted(selected))
    if not 3 <= len(selected) <= 8 or len(set(selected)) != len(selected):
        raise ValueError("bounded_single_cycle_source_required")
    if any(e not in spans for e in selected):
        raise ValueError("missing_cycle_source_span")
    adjacency = {}
    pairs = set()
    for edge in selected:
        span = spans[edge]
        a, b = span.nodes
        pair = tuple(sorted((a, b)))
        if a == b or pair in pairs:
            raise ValueError("cycle_parallel_or_self_span")
        pairs.add(pair)
        adjacency.setdefault(a, []).append(b)
        adjacency.setdefault(b, []).append(a)
        if span.bridge or span.tunnel or span.layer != 0:
            raise ValueError("cycle_ground_profile_required")
        if (
            span.maximum_gradient is None
            or not 0 < span.maximum_gradient <= 1
            or not 0 < span.width_m <= 30
        ):
            raise ValueError("missing_source_profile_recipe")
    if len(adjacency) != len(selected) or max(map(len, adjacency.values())) > 6:
        raise ValueError("bounded_single_cycle_source_required")
    if any(n not in nodes or not set(incidence[n]) <= set(selected) for n in adjacency):
        raise ValueError("incomplete_cycle_component")
    visited, queue = {min(adjacency)}, [min(adjacency)]
    for node in queue:
        for neighbour in adjacency[node]:
            if neighbour not in visited:
                visited.add(neighbour)
                queue.append(neighbour)
    if len(visited) != len(adjacency):
        raise ValueError("disconnected_cycle_component")
    degree = Counter({n: len(a) for n, a in adjacency.items()})
    leaves = tuple(sorted(n for n, d in degree.items() if d == 1))
    prune = deque(leaves)
    while prune:
        node = prune.popleft()
        degree[node] = 0
        for neighbour in adjacency[node]:
            if degree[neighbour] > 0:
                degree[neighbour] -= 1
                if degree[neighbour] == 1:
                    prune.append(neighbour)
    core = {n for n, d in degree.items() if d == 2}
    if len(core) < 3 or any(d not in (0, 2) for d in degree.values()):
        raise ValueError("simple_original_cycle_required")
    order = [min(core)]
    previous = None
    for _ in range(len(core)):
        following = sorted(
            n for n in adjacency[order[-1]] if n in core and n != previous
        )
        node = following[0]
        if node == order[0]:
            break
        if node in order:
            raise ValueError("ambiguous_original_cycle")
        previous = order[-1]
        order.append(node)
    if len(order) != len(core):
        raise ValueError("incomplete_original_cycle")
    branches = sorted(n for n, a in adjacency.items() if len(a) >= 3)
    return (branches[0] if branches else min(core)), leaves, tuple(order)


def prepare_cycle(index, selected, terrain, budget, *, deadline, cut_method="adaptive"):
    selected = tuple(sorted(selected))
    anchor_node, leaves, cycle = source_cycle(index, selected)
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
        expected_holes=1,
    )
    geometry.update(
        recipe=CYCLE_RECIPE if cut_method == "adaptive" else None,
        source_node_ids=tuple(node_ids),
        source_nodes_EN=xyz[:, :2],
        source_edges_local=edges,
        original_cycle_nodes=cycle,
        original_cycle_local_nodes=tuple(local[n] for n in cycle),
    )
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
    anchor, inputs, caps, geometry = prepare_cycle(
        index, selected, terrain, budget, deadline=deadline, cut_method=cut_method
    )
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
        "scope": "Finite experimental single-cycle geometry; no shared Source/terminal/World admission. Original Unknown, missing topology and end authority remain external facts.",
    }
