"""Scalar graph-only distance envelopes; no source or geometry admission.

Inputs are finite undirected positive-weight graphs; NaN means no observation.
Bounded queries require independently valid extrema for every possible observation.
The replay validates those conditions before timing the numerical routines.
"""

import heapq
import time

import numpy as np
from scipy import optimize, sparse


def envelope(graph, values):
    dist = np.where(np.isfinite(values), values, np.inf)
    queue = [(float(v), i) for i, v in enumerate(dist) if np.isfinite(v)]
    heapq.heapify(queue)
    stats = {
        "edge_visits": 0,
        "heap_pops": 0,
        "relaxations": 0,
        "heap_peak": len(queue),
    }
    while queue:
        value, node = heapq.heappop(queue)
        stats["heap_pops"] += 1
        if value != dist[node]:
            continue
        for target, weight in graph[node]:
            stats["edge_visits"] += 1
            candidate = value + weight
            if candidate < dist[target]:
                dist[target] = candidate
                heapq.heappush(queue, (candidate, target))
                stats["relaxations"] += 1
        stats["heap_peak"] = max(stats["heap_peak"], len(queue))
    return dist, stats


def query(graph, z, origin, minimum, maximum, guard, max_visits):
    distance = np.full(len(z), np.inf)
    distance[origin] = 0
    queue = [(0.0, origin)]
    upper, lower = np.inf, -np.inf
    visits = settled = 0
    peak = 1
    while queue:
        d, node = heapq.heappop(queue)
        if d != distance[node]:
            continue
        if np.isfinite(upper) and d > max(upper - minimum, maximum - lower) + guard:
            return (
                lower,
                upper,
                {
                    "complete": True,
                    "settled_nodes": settled,
                    "edge_visits": visits,
                    "heap_peak": peak,
                    "frontier_distance_m": d,
                },
            )
        settled += 1
        if np.isfinite(z[node]):
            upper = min(upper, float(z[node] + d))
            lower = max(lower, float(z[node] - d))
        for target, weight in graph[node]:
            if visits >= max_visits:
                return (
                    lower,
                    upper,
                    {
                        "complete": False,
                        "settled_nodes": settled,
                        "edge_visits": visits,
                        "heap_peak": peak,
                    },
                )
            visits += 1
            candidate = d + weight
            if candidate < distance[target]:
                distance[target] = candidate
                heapq.heappush(queue, (candidate, target))
        peak = max(peak, len(queue))
    return (
        lower,
        upper,
        {
            "complete": True,
            "settled_nodes": settled,
            "edge_visits": visits,
            "heap_peak": peak,
        },
    )


def lp_reference(z, edges, weights):
    n = len(z)
    rows = []
    cols = []
    data = []
    bounds = []
    for i, (a, b) in enumerate(edges):
        for sign in (1.0, -1.0):
            r = len(bounds)
            rows.extend((r, r))
            cols.extend((a, b))
            data.extend((sign, -sign))
            bounds.append(weights[i])
    for i, value in enumerate(z):
        if not np.isfinite(value):
            continue
        for sign in (1.0, -1.0):
            r = len(bounds)
            rows.extend((r, r))
            cols.extend((i, n))
            data.extend((sign, -1.0))
            bounds.append(sign * value)
    matrix = sparse.coo_matrix((data, (rows, cols)), shape=(len(bounds), n + 1)).tocsc()
    start = time.perf_counter()
    r = optimize.linprog(
        np.r_[np.zeros(n), 1.0],
        A_ub=matrix,
        b_ub=bounds,
        bounds=[(None, None)] * n + [(0, None)],
        method="highs",
        options={"time_limit": 5.0},
    )
    assert r.success, r.message
    return r.x, {
        "solve_ms": 1000 * (time.perf_counter() - start),
        "iterations": r.nit,
        "status": r.message,
        "rows": matrix.shape[0],
        "nonzeros": matrix.nnz,
        "sparse_bytes": sum(
            a.nbytes for a in (matrix.data, matrix.indices, matrix.indptr)
        ),
    }
