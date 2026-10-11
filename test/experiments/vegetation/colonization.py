"""Bounded space-colonization form plans; coordinates and dimensions are in metres."""

from dataclasses import dataclass

import numpy as np
from nearest import brute_nearest, tree_nearest


@dataclass(frozen=True)
class GrowthBudget:
    nodes: int = 4096
    iterations: int = 128

    def __post_init__(self):
        if not all(
            isinstance(v, (int, np.integer)) for v in (self.nodes, self.iterations)
        ):
            raise ValueError("integer growth budgets required")
        if self.nodes < 2 or self.iterations < 1:
            raise ValueError("at least two nodes and one iteration required")


def crown_points(height, spread, bole, count, seed):
    if (
        not all(np.isfinite(v) and v > 0 for v in (height, spread))
        or not 0 < bole < 1
        or count < 1
    ):
        raise ValueError(
            "positive dimensions, count, and a bole fraction between zero and one required"
        )
    rng = np.random.default_rng(seed)
    directions = rng.normal(size=(count, 3))
    directions /= np.linalg.norm(directions, axis=1)[:, None]
    directions *= rng.random(count)[:, None] ** (1 / 3)
    centre = np.array([0.0, 0.0, height * (1 + bole) / 2])
    radii = np.array([spread / 2, spread / 2, height * (1 - bole) / 2])
    return centre + directions * radii


def grow(points, height, bole, step, influence, kill, method="tree", budget=None):
    if budget is None:
        budget = GrowthBudget()
    if not all(np.isfinite(v) and v > 0 for v in (height, step, influence, kill)):
        raise ValueError("positive finite dimensions and growth distances required")
    if not 0 < kill <= influence or not 0 < step <= influence:
        raise ValueError("positive step and kill distances cannot exceed influence")
    if budget.nodes < 2 or budget.iterations < 1 or not 0 < bole < 1:
        raise ValueError("valid growth budget and bole fraction required")
    if method not in ("tree", "brute"):
        raise ValueError("unknown nearest-node method")
    nearest = tree_nearest if method == "tree" else brute_nearest
    trunk_count = min(budget.nodes, max(2, int(np.ceil(height * bole / step)) + 1))
    positions = np.zeros((budget.nodes, 3))
    positions[:trunk_count, 2] = np.linspace(0, height * bole, trunk_count)
    parents = np.full(budget.nodes, -1, dtype=np.int32)
    parents[1:trunk_count] = np.arange(trunk_count - 1)
    active = np.array(points, dtype=np.float64, copy=True)
    count, queries, consumed = trunk_count, 0, 0
    reason = "iteration_budget"
    for iteration in range(budget.iterations):
        owner, distance = nearest(active, positions[:count], influence)
        queries += len(active)
        killed = distance <= kill
        consumed += int(np.count_nonzero(killed))
        active, owner = active[~killed], owner[~killed]
        if not len(active):
            reason = "all_attractors_consumed"
            break
        accepted = owner >= 0
        if not np.any(accepted):
            reason = "no_attractor_in_reach"
            break
        owners = owner[accepted]
        vectors = active[accepted] - positions[owners]
        vectors /= np.linalg.norm(vectors, axis=1)[:, None]
        summed = np.zeros((count, 3))
        np.add.at(summed, owners, vectors)
        lengths = np.linalg.norm(summed, axis=1)
        source = np.flatnonzero(lengths > 1e-12)
        if not len(source):
            reason = "cancelled_growth_directions"
            break
        available = budget.nodes - count
        source = source[:available]
        if not len(source):
            reason = "node_budget"
            break
        new_positions = (
            positions[source] + summed[source] / lengths[source, None] * step
        )
        end = count + len(source)
        positions[count:end] = new_positions
        parents[count:end] = source
        count = end
    return {
        "positions": positions[:count].copy(),
        "parents": parents[:count].copy(),
        "remaining": active,
        "consumed": consumed,
        "iterations": iteration + 1,
        "queries": queries,
        "terminal_reason": reason,
    }


def pipe_radii(parents, root_radius):
    if not np.isfinite(root_radius) or root_radius <= 0:
        raise ValueError("positive finite root radius required")
    if parents.ndim != 1 or not len(parents) or parents[0] != -1:
        raise ValueError("one rooted topologically ordered tree required")
    if np.any(parents[1:] < 0) or np.any(parents[1:] >= np.arange(1, len(parents))):
        raise ValueError("every child must follow its parent")
    children = np.bincount(parents[1:], minlength=len(parents))
    support = (children == 0).astype(np.int64)
    for child in range(len(parents) - 1, 0, -1):
        support[parents[child]] += support[child]
    return root_radius * np.sqrt(support / support[0])
