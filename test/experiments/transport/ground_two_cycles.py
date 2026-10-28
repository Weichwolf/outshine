"""Two bounded original cycles; finite geometry grants no Source authority."""

import time

import numpy as np
import shapely
from ground_cyclic_surface import finish_surface, plan_source_surface
from outshine_ref.transport.canonical_ports._ground_star_input import lift_star_input

TWO_CYCLE_RECIPE = "outshine/experimental-coarse-ground-bicyclic/1"


def source_two_cycles(index, selected):
    nodes, spans, incidence = index
    selected = tuple(sorted(selected))
    if not 5 <= len(selected) <= 12 or len(set(selected)) != len(selected):
        raise ValueError("bounded_two_cycle_source_required")
    if any(edge not in spans for edge in selected):
        raise ValueError("missing_two_cycle_source_span")
    adjacency, pairs = {}, set()
    for edge in selected:
        span = spans[edge]
        a, b = span.nodes
        pair = tuple(sorted((a, b)))
        if a == b or pair in pairs:
            raise ValueError("two_cycle_parallel_or_self_span")
        pairs.add(pair)
        adjacency.setdefault(a, []).append(b)
        adjacency.setdefault(b, []).append(a)
        if span.bridge or span.tunnel or span.layer != 0:
            raise ValueError("two_cycle_ground_profile_required")
        if (
            span.maximum_gradient is None
            or not np.isfinite([span.maximum_gradient, span.width_m]).all()
            or not 0 < span.maximum_gradient <= 1
            or not 0 < span.width_m <= 30
        ):
            raise ValueError("missing_source_profile_recipe")
    if len(selected) - len(adjacency) + 1 != 2 or max(map(len, adjacency.values())) > 6:
        raise ValueError("bounded_two_cycle_source_required")
    if any(n not in nodes or not set(incidence[n]) <= set(selected) for n in adjacency):
        raise ValueError("incomplete_two_cycle_component")
    visited, queue = {min(adjacency)}, [min(adjacency)]
    for node in queue:
        for neighbour in adjacency[node]:
            if neighbour not in visited:
                visited.add(neighbour)
                queue.append(neighbour)
    if len(visited) != len(adjacency):
        raise ValueError("disconnected_two_cycle_component")
    branches = sorted(n for n, neighbours in adjacency.items() if len(neighbours) >= 3)
    leaves = tuple(
        sorted(n for n, neighbours in adjacency.items() if len(neighbours) == 1)
    )
    return branches[0], leaves


def source_faces(geometry, vertices, triangles):
    points = np.asarray(geometry["source_nodes_EN"])
    edges = np.asarray(geometry["source_edges_local"])
    faces = shapely.polygonize(shapely.linestrings(points[edges]))
    if len(faces.geoms) != 2:
        raise ValueError("two_cycle_original_plane_faces_required")
    domain = shapely.coverage_union_all(shapely.polygons(vertices[triangles, :2]))
    if (
        domain.geom_type != "Polygon"
        or not domain.is_valid
        or len(domain.interiors) != 2
    ):
        raise ValueError("two_cycle_hole_topology")
    holes = [shapely.Polygon(ring) for ring in domain.interiors]
    assignment = np.array(
        [[face.covers(hole) for hole in holes] for face in faces.geoms]
    )
    if not np.all(assignment.sum(axis=0) == 1) or not np.all(
        assignment.sum(axis=1) == 1
    ):
        raise ValueError("two_cycle_source_holes_not_preserved")
    return tuple(face.wkb_hex for face in faces.geoms)


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
