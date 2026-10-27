"""Diagnostic original-source guards for one bounded three-arm Ground star."""

from collections import Counter

import numpy as np
from branched_ground_layout import layout
from outshine_ref.contracts import Mesh
from outshine_ref.transport.canonical_ports._geodesy import (
    ground_geographic_at,
    positions_in_frame,
)
from outshine_ref.transport.canonical_ports._ground_scope import (
    inside_coverage,
    qualify_groups,
    segment_covered,
    source_key,
)
from outshine_ref.transport.canonical_ports._ground_scope_index import (
    prepare_ground_scope_index,
)
from outshine_ref.transport.canonical_ports._source import component
from outshine_ref.transport.port_heights import PortHeightInput


def checked_star(source, seed, groups, ways, binding, budget):
    index, reason, index_work = prepare_ground_scope_index(
        source,
        groups,
        ways,
        source_binding=source.binding,
        candidate_binding=binding,
        budget=budget,
    )
    if reason:
        raise ValueError(reason)
    edges, reason, work = component(index.index, seed, index.unknown, budget)
    if reason not in ("", "unknown_source_frontier"):
        raise ValueError(reason)
    nodes, spans, _ = index.index
    degree = Counter(n for edge in edges for n in spans[edge].nodes)
    centres = [n for n, count in degree.items() if count == 3]
    if len(edges) != 3 or len(degree) != 4 or len(centres) != 1:
        raise ValueError("diagnostic_three_arm_topology_required")
    centre = centres[0]
    for edge in edges:
        span = spans[edge]
        if span.bridge or span.tunnel or span.layer != 0:
            raise ValueError("diagnostic_ground_star_required")
        if span.maximum_gradient is None or span.width_m <= 0:
            raise ValueError("missing_source_profile_recipe")
    coverage = source.binding.get("coverage", ())
    if (
        not coverage
        or any(not inside_coverage(nodes[n], coverage) for n in degree)
        or any(
            not segment_covered(
                nodes[spans[e].nodes[0]], nodes[spans[e].nodes[1]], coverage
            )
            for e in edges
        )
    ):
        raise ValueError("ground_scope_coverage_gap")
    explained = {
        n
        for e in edges
        if 4 in index.omissions.get(source_key(spans[e]), ())
        for n in spans[e].nodes
    }
    if not index.unknown.intersection(degree) <= explained:
        raise ValueError("unqualified_ground_scope_unknown")
    if any(
        n not in index.groups_by_node or index.groups_by_node[n] is None for n in degree
    ):
        raise ValueError("missing_or_ambiguous_ground_scope_original_group")
    original_groups = {n: index.groups_by_node[n] for n in degree}
    work["qualified_member_visits"] = 0
    remaining = budget.incidence_visits - work["incidence_visits"] - index.member_count
    qualified, reason = qualify_groups(
        index.index,
        edges,
        original_groups,
        index.ways_by_id,
        index.omissions,
        work,
        remaining,
    )
    if reason:
        raise ValueError(reason)
    leaves = tuple(next(n for n in spans[e].nodes if n != centre) for e in edges)
    return (
        index,
        edges,
        centre,
        leaves,
        {
            "state": "DiagnosticSourceGuardsOnly",
            "index_work": index_work,
            "source_work": work,
            "typed_exits": qualified[0],
            "original_unknown_count": len(source.unknown_nodes),
            "scope": "Independent original source guards; the shared producer still rejects branches. No new source/World authority.",
        },
    )


def source_input(
    source,
    index,
    edges,
    centre,
    leaves,
    terrain,
    budget,
    *,
    proximal=True,
    cut_method="linear",
):
    if index.source is not source:
        raise ValueError("diagnostic_ground_index_source_mismatch")
    if budget.requests < 3 or budget.surface.max_queries < 3:
        raise ValueError("diagnostic_ground_query_budget")
    if (
        budget.surface.time_limit_s <= 0
        or budget.surface.geometry_time_limit_s <= 0
        or budget.surface.solver_time_limit_s <= 0
    ):
        raise ValueError("diagnostic_ground_time_budget")
    nodes, spans, _ = index.index
    anchor = nodes[centre]
    ends = positions_in_frame(anchor, [nodes[n] for n in leaves], np.zeros(3))[:, :2]
    widths = [spans[e].width_m for e in edges]
    grades = [spans[e].maximum_gradient for e in edges]
    raw, geometry = layout(
        ends, widths, grades, proximal=proximal, cut_method=cut_method
    )
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
        raise ValueError("diagnostic_ground_input_budget")
    if (
        sizes[0] > budget.surface.max_output_vertices
        or sizes[1] > budget.surface.max_output_faces
    ):
        raise ValueError("diagnostic_ground_output_budget")
    if (
        budget.surface.time_limit_s <= 0
        or budget.surface.geometry_time_limit_s <= 0
        or budget.surface.solver_time_limit_s <= 0
    ):
        raise ValueError("diagnostic_ground_time_budget")
    geographic = ground_geographic_at(anchor, raw["xy"])
    heights = np.asarray(terrain(geographic), float)
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
        widths_m=widths,
        maximum_gradients=grades,
        terrain_samples=sizes[0],
        original_caps=original_caps,
        part_source_edges=[
            edges if a == -1 else (edges[a],) for a in geometry["part_source_arms"]
        ],
    )
    return anchor, inputs, original_caps, geometry
