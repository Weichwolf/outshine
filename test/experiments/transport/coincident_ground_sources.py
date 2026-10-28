"""Physical-band reuse with unchanged original source members and directions."""

from collections import defaultdict
from dataclasses import dataclass

import numpy as np


@dataclass(frozen=True)
class CoverSources:
    members: tuple
    node_ids: tuple
    bands: tuple
    bindings: tuple
    width_m: float
    maximum_gradient: float


def prepare_sources(index, selected, budget):
    nodes, spans, incidence = index
    selected = tuple(sorted(selected))
    if not 1 <= len(selected) <= min(32, budget.selected_spans):
        raise ValueError("cover_selected_span_budget")
    if len(set(selected)) != len(selected) or any(e not in spans for e in selected):
        raise ValueError("cover_missing_or_repeated_source")
    members = tuple(spans[e] for e in selected)
    used = sorted({n for s in members for n in s.nodes})
    if len(used) > 32 or any(n not in nodes for n in used):
        raise ValueError("cover_node_budget_or_missing_node")
    chosen = set(selected)
    if any(not set(incidence[n]) <= chosen for n in used):
        raise ValueError("cover_incomplete_source_component")
    profiles = set()
    adjacency, grouped = defaultdict(set), defaultdict(list)
    for member in members:
        if (
            member.bridge
            or member.tunnel
            or member.layer != 0
            or member.attributes.get("ground_height_recipe_qualified") is not True
            or member.maximum_gradient is None
            or not np.isfinite([member.width_m, member.maximum_gradient]).all()
            or not 0 < member.width_m <= 30
            or not 0 < member.maximum_gradient <= 1
        ):
            raise ValueError("cover_ground_profile_required")
        profiles.add((member.width_m, member.maximum_gradient))
        a, b = member.nodes
        if a == b:
            raise ValueError("cover_collapsed_source")
        adjacency[a].add(b)
        adjacency[b].add(a)
        grouped[tuple(sorted((a, b)))].append(member)
    if len(profiles) != 1:
        raise ValueError("cover_mixed_profiles")
    if max(map(len, adjacency.values())) > 6:
        raise ValueError("cover_physical_degree_budget")
    visited, pending = {used[0]}, [used[0]]
    for node in pending:
        for other in sorted(adjacency[node]):
            if other not in visited:
                visited.add(other)
                pending.append(other)
    if len(visited) != len(used):
        raise ValueError("cover_disconnected_source")
    bands = tuple(sorted(grouped))
    binding = {pair: i for i, pair in enumerate(bands)}
    bindings = tuple(
        (
            member.identifier,
            binding[tuple(sorted(member.nodes))],
            1 if member.nodes == tuple(sorted(member.nodes)) else -1,
        )
        for member in members
    )
    width, grade = next(iter(profiles))
    return CoverSources(members, tuple(used), bands, bindings, width, grade)
