"""Whole source-component guards shared by explicit bounded cyclic recipes."""

import numpy as np


def source_component(
    index, selected, *, minimum_cycles, maximum_cycles, maximum_spans, prefix
):
    nodes, spans, incidence = index
    selected = tuple(sorted(selected))
    if not 5 <= len(selected) <= maximum_spans or len(set(selected)) != len(selected):
        raise ValueError("bounded_" + prefix + "_source_required")
    if any(edge not in spans for edge in selected):
        raise ValueError("missing_" + prefix + "_source_span")
    adjacency, pairs = {}, set()
    for edge in selected:
        span = spans[edge]
        a, b = span.nodes
        pair = tuple(sorted((a, b)))
        if a == b or pair in pairs:
            raise ValueError(prefix + "_parallel_or_self_span")
        pairs.add(pair)
        adjacency.setdefault(a, []).append(b)
        adjacency.setdefault(b, []).append(a)
        if span.bridge or span.tunnel or span.layer != 0:
            raise ValueError(prefix + "_ground_profile_required")
        if (
            span.maximum_gradient is None
            or not np.isfinite([span.maximum_gradient, span.width_m]).all()
            or not 0 < span.maximum_gradient <= 1
            or not 0 < span.width_m <= 30
        ):
            raise ValueError("missing_source_profile_recipe")
    cycles = len(selected) - len(adjacency) + 1
    if (
        not minimum_cycles <= cycles <= maximum_cycles
        or max(map(len, adjacency.values())) > 6
    ):
        raise ValueError("bounded_" + prefix + "_source_required")
    if any(n not in nodes or not set(incidence[n]) <= set(selected) for n in adjacency):
        raise ValueError("incomplete_" + prefix + "_component")
    visited, queue = {min(adjacency)}, [min(adjacency)]
    for node in queue:
        for neighbour in adjacency[node]:
            if neighbour not in visited:
                visited.add(neighbour)
                queue.append(neighbour)
    if len(visited) != len(adjacency):
        raise ValueError("disconnected_" + prefix + "_component")
    branches = sorted(n for n, neighbours in adjacency.items() if len(neighbours) >= 3)
    leaves = tuple(
        sorted(n for n, neighbours in adjacency.items() if len(neighbours) == 1)
    )
    return branches[0], leaves, cycles
