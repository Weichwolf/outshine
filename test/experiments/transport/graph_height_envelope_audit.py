"""Independent bounded-query and lazy inconsistency controls for the graph model."""

import heapq
import time

import numpy as np
from graph_height_envelopes import query


def all_queries(graph, values, lower, upper, guard):
    known = np.isfinite(values)
    minimum, maximum = float(values[known].min()), float(values[known].max())
    work, error = [], 0.0
    maximum_visits = 10 * sum(map(len, graph))
    began = time.perf_counter()
    for origin in range(len(values)):
        lo, hi, record = query(
            graph, values, origin, minimum, maximum, guard, maximum_visits
        )
        assert record["complete"]
        delta = max(abs(lo - lower[origin]), abs(hi - upper[origin]))
        assert delta <= guard
        error = max(error, delta)
        work.append(record)
    visits = [r["edge_visits"] for r in work]
    return {
        "queries": len(work),
        "maximum_error_m": error,
        "elapsed_ms": 1000 * (time.perf_counter() - began),
        "edge_visits": {
            "minimum": min(visits),
            "median": float(np.median(visits)),
            "maximum": max(visits),
            "sum": sum(visits),
        },
        "visits": visits,
    }


def lazy_witness(
    graph, sample, origin, epsilon, *, max_samples, max_visits, guard=1e-7
):
    """Partial constraints can prove inconsistency without any global extrema.

    A callback returns an observed scalar height or None. No absent value is zero.
    This grants no Source closure or geometric meaning to the graph constraints.
    """
    queue, distance, previous = [(0.0, origin)], {origin: 0.0}, {origin: None}
    upper, lower = np.inf, -np.inf
    upper_node = lower_node = None
    observations, visits, attempts = [], 0, 0
    state = "exhausted"
    while queue:
        d, node = heapq.heappop(queue)
        if d != distance[node]:
            continue
        if attempts >= max_samples:
            state = "budget"
            break
        attempts += 1
        z = sample(node)
        if z is not None:
            if not np.isfinite(z):
                raise ValueError("finite observation or explicit None required")
            observations.append(
                {"node": node, "height_m": float(z), "distance_budget_m": d}
            )
            if z + d < upper:
                upper, upper_node = z + d, node
            if z - d > lower:
                lower, lower_node = z - d, node
            if lower - upper > 2 * epsilon + guard:
                state = "infeasible"
                break
        for target, weight in graph[node]:
            if visits >= max_visits:
                state = "budget"
                break
            visits += 1
            candidate = d + weight
            if candidate < distance.get(target, np.inf):
                distance[target] = candidate
                previous[target] = (node, float(weight))
                heapq.heappush(queue, (candidate, target))
        if state == "budget":
            break

    def path(node):
        result = []
        while node is not None and previous[node] is not None:
            parent, weight = previous[node]
            result.append({"from": parent, "to": node, "weight_m": weight})
            node = parent
        return result[::-1]

    return {
        "state": state,
        "epsilon_m": epsilon,
        "partial_lower_m": lower - epsilon,
        "partial_upper_m": upper + epsilon,
        "empty_interval_gap_m": lower - upper - 2 * epsilon,
        "upper_node": upper_node,
        "lower_node": lower_node,
        "upper_path": path(upper_node),
        "lower_path": path(lower_node),
        "sample_attempts": attempts,
        "observations": observations,
        "edge_visits": visits,
        "global_extrema_used": False,
    }


def plot(output, positions, values, audit, witness):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    known = np.isfinite(values)
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.6), layout="constrained")
    axes[0].scatter(*positions[known, :2].T, c=values[known], s=8, cmap="viridis")
    axes[0].scatter(
        *positions[~known, :2].T, color=".7", s=8, label="No DEM observation"
    )
    axes[0].set_aspect("equal")
    axes[0].set(
        xlabel="Source east [m]",
        ylabel="Source north [m]",
        title="Actual captured model / explicit missing DEM",
    )
    axes[0].legend(fontsize=7)
    axes[1].hist(audit["visits"], bins=25)
    axes[1].set(
        xlabel="Adjacency visits",
        ylabel="Query count",
        title="All bounded scalar queries",
    )
    for i, obs in enumerate(witness["observations"]):
        if i >= 2:
            break
        d, z = obs["distance_budget_m"], obs["height_m"]
        epsilon = witness["epsilon_m"]
        axes[2].plot([i, i], [z - epsilon - d, z + epsilon + d], linewidth=8)
    axes[2].axhspan(
        witness["partial_upper_m"], witness["partial_lower_m"], alpha=0.15, color="red"
    )
    axes[2].set(
        xticks=[0, 1],
        xticklabels=["Own constraint", "Neighbor constraint"],
        ylabel="Source model height [m]",
        title="Lazy two-sample inconsistency witness",
    )
    fig.suptitle(
        "Scalar fixed-XY graph model only; original Source scope remains unqualified"
    )
    fig.savefig(output / "diagnostic.png", dpi=110)
    plt.close(fig)
