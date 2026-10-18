"""Bounded space-colonization form plans; coordinates and dimensions are in metres."""

from dataclasses import dataclass

import numpy as np
from form_plan import validate_parents
from nearest import brute_nearest, free_endpoints, tree_nearest


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


def initial_skeleton(height, bole, step, budget, seed):
    if seed is None:
        count = min(budget.nodes, max(2, int(np.ceil(height * bole / step)) + 1))
        source = np.zeros((count, 3))
        source[:, 2] = np.linspace(0, height * bole, count)
        links = np.arange(-1, count - 1, dtype=np.int32)
    else:
        source, links = (np.asarray(a) for a in seed)
        if links.ndim != 1 or not 2 <= len(links) <= budget.nodes:
            raise ValueError("initial skeleton must fit node capacity before growth")
        validate_parents(links)
        if (
            source.shape != (len(links), 3)
            or source.dtype.kind not in "fiu"
            or not np.isfinite(source).all()
            or np.any(abs(source) > 100000)
            or np.any(np.linalg.norm(source[1:] - source[links[1:]], axis=1) <= 1e-8)
        ):
            raise ValueError("finite bounded noncollapsed initial skeleton required")
        count = len(links)
    positions = np.zeros((budget.nodes, 3))
    positions[:count] = source
    parents = np.full(budget.nodes, -1, dtype=np.int32)
    parents[:count] = links
    return positions, parents, count


def grow(
    points,
    height,
    bole,
    step,
    influence,
    kill,
    method="tree",
    budget=None,
    *,
    seed=None,
):
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
    positions, parents, count = initial_skeleton(height, bole, step, budget, seed)
    active = np.array(points, dtype=np.float64, copy=True)
    queries, consumed, rejected = 0, 0, 0
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
        if not available:
            reason = "node_budget"
            break
        new_positions = (
            positions[source] + summed[source] / lengths[source, None] * step
        )
        free = free_endpoints(new_positions, positions[:count], step * 1e-9, nearest)
        queries += len(new_positions)
        rejected += int(np.count_nonzero(~free))
        source = source[free][:available]
        new_positions = new_positions[free][:available]
        if not len(source):
            reason = "occupied_growth_endpoints"
            break
        end = count + len(source)
        positions[count:end] = new_positions
        parents[count:end] = source
        count = end
    if len(active):
        _, distance = nearest(active, positions[:count], kill)
        queries += len(active)
        killed = distance <= kill
        consumed += int(np.count_nonzero(killed))
        active = active[~killed]
    if not len(active):
        reason = "all_attractors_consumed"
    return {
        "positions": positions[:count].copy(),
        "parents": parents[:count].copy(),
        "remaining": active,
        "consumed": consumed,
        "iterations": iteration + 1,
        "queries": queries,
        "rejected_endpoints": rejected,
        "terminal_reason": reason,
    }


def pipe_radii(parents, root_radius):
    if not np.isfinite(root_radius) or root_radius <= 0:
        raise ValueError("positive finite root radius required")
    validate_parents(parents)
    children = np.bincount(parents[1:], minlength=len(parents))
    support = (children == 0).astype(np.int64)
    for child in range(len(parents) - 1, 0, -1):
        support[parents[child]] += support[child]
    return root_radius * np.sqrt(support / support[0])
