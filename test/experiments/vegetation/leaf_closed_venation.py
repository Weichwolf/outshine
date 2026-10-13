"""Bounded closed-vein growth from a cached primary graph and fixed sources."""

from dataclasses import dataclass

import numpy as np
from leaf_closed_state import NetworkBudget, NetworkBuilder, SourceState
from leaf_domain import contains_segment
from leaf_relative_neighbors import relative_neighbors


@dataclass(frozen=True)
class ClosedVeins:
    positions: np.ndarray
    edges: np.ndarray
    base_edges: int
    sources: np.ndarray
    hubs: np.ndarray
    first_reach: np.ndarray
    completed: np.ndarray
    remaining_sources: int
    stop_reason: str
    rejected_segments: int


def tag_signature(states):
    return tuple(None if s.tags is None else tuple(sorted(s.tags)) for s in states)


def grow_closed(
    blade, positions, edges, sources, method="sectors", steps=60, max_nodes=2048
):
    if type(steps) is not int or type(max_nodes) is not int:
        raise ValueError("integer growth budgets required")
    if method not in ("dense", "sectors"):
        raise ValueError("dense or sectors neighborhood search required")
    positions, sources = np.asarray(positions, float), np.asarray(sources, float)
    edges = np.asarray(edges)
    if not np.issubdtype(edges.dtype, np.integer):
        raise ValueError("integer edge indices required")
    if not 1 <= len(positions) <= max_nodes <= 4096 or not 0 <= steps <= 160:
        raise ValueError("bounded initial graph and growth request required")
    for points in (positions, sources):
        if points.ndim != 2 or points.shape[1] != 2 or not np.isfinite(points).all():
            raise ValueError("finite physical vein nodes/sources required")
        u = points[:, 1] / blade.length
        widths, _ = blade.width_profile(np.clip(u, 0, 1))
        if np.any((u < 0) | (u > 1) | (np.abs(points[:, 0]) > widths)):
            raise ValueError("vein nodes and sources must lie inside the blade")
    if len(sources) > 96 or len(np.unique(sources, axis=0)) != len(sources):
        raise ValueError("at most 96 distinct refinement sources required")
    if edges.ndim != 2 or edges.shape[1] != 2 or len(edges) > 4096:
        raise ValueError("bounded edge pairs required")
    if (
        np.any(edges < 0)
        or np.any(edges >= len(positions))
        or np.any(edges[:, 0] == edges[:, 1])
    ):
        raise ValueError("valid distinct edge endpoints required")
    if len(np.unique(positions, axis=0)) != len(positions):
        raise ValueError("distinct initial vein nodes required")
    if len(np.unique(np.sort(edges, axis=1), axis=0)) != len(edges):
        raise ValueError("distinct initial edges required")
    edges = edges.astype(np.int32)
    if any(not contains_segment(blade, positions[a], positions[b]) for a, b in edges):
        raise ValueError("initial vein segments must have certified blade containment")
    builder = NetworkBuilder(blade, positions, edges, max_nodes, 4096)
    states = [SourceState(p) for p in sources]
    step = min(blade.width, blade.length) / 100
    kill = step * 0.8
    reason = "step_budget"
    try:
        for iteration in range(steps):
            active = [s for s in states if s.active]
            if not active:
                reason = "complete"
                break
            before = (len(builder.positions), len(builder.edges), len(active))
            before_tags = tag_signature(states)
            owners, _ = relative_neighbors(
                np.array(builder.positions), np.array([s.point for s in active]), method
            )
            influences = [
                s.influence(o, builder, kill, iteration) for s, o in zip(active, owners)
            ]
            nodes = np.array(builder.positions)
            directions = np.zeros_like(nodes)
            for state, ids in zip(active, influences):
                if not ids:
                    continue
                delta = state.point - nodes[ids]
                directions[ids] += delta / np.linalg.norm(delta, axis=1)[:, None]
            magnitude = np.linalg.norm(directions, axis=1)
            children = {}
            for parent in np.flatnonzero(magnitude > 1e-12):
                point = nodes[parent] + step * directions[parent] / magnitude[parent]
                child = builder.connect(int(parent), point)
                if child is not None:
                    children[int(parent)] = child
            for state in active:
                state.inherit(children)
            after = (
                len(builder.positions),
                len(builder.edges),
                sum(s.active for s in states),
            )
            if before == after and before_tags == tag_signature(states):
                reason = "stationary"
                break
    except NetworkBudget as error:
        reason = str(error)
    remaining = sum(s.active for s in states)
    return ClosedVeins(
        np.array(builder.positions),
        np.array(builder.edges, dtype=np.int32).reshape(-1, 2),
        len(edges),
        sources,
        np.array([s.hub for s in states]),
        np.array([s.first_reach for s in states]),
        np.array([s.completed for s in states]),
        remaining,
        reason if remaining else "complete",
        builder.rejected_segments,
    )
