"""Optional second LP: preserve minimax error, then reduce path-gradient variation."""

import time
from collections import defaultdict

import numpy as np
from scipy import sparse
from scipy.optimize import linprog
from stair_junction_solve import gradient_rows


def _variation_matrix(plan, projection, deadline, max_edges):
    directions = np.c_[
        np.cos(np.arange(16) * np.pi / 8), np.sin(np.arange(16) * np.pi / 8)
    ]
    stairs = {s[0] for s in plan["stairs"]}
    rows, columns, values, weights = [], [], [], []
    visits = 0
    for owner, faces in enumerate(plan["parts"]):
        if owner in stairs:
            continue
        edges = defaultdict(list)
        for index, face in enumerate(faces):
            for a, b in zip(face, np.roll(face, -1), strict=True):
                edges[tuple(sorted((int(a), int(b))))].append(index)
                visits += 1
        for edge, incident in sorted(edges.items()):
            if len(incident) > 2:
                raise ValueError("variation selection needs manifold source caps")
            if len(incident) != 2:
                continue
            if len(weights) >= max_edges:
                return (
                    None,
                    None,
                    {
                        "status": "edge_budget",
                        "edge_visits": visits,
                        "max_edges": max_edges,
                    },
                )
            if len(weights) % 32 == 0 and time.perf_counter() >= deadline:
                return None, None, {"status": "time_budget", "edge_visits": visits}
            first, second = (faces[i] for i in incident)
            difference = np.c_[
                gradient_rows(plan["xy"], first), -gradient_rows(plan["xy"], second)
            ]
            coefficients = directions @ difference
            offset = 16 * len(weights)
            rows.extend(np.repeat(np.arange(16) + offset, 6))
            columns.extend(np.tile(np.r_[first, second], 16))
            values.extend(coefficients.ravel())
            weights.append(
                float(np.linalg.norm(plan["xy"][edge[1]] - plan["xy"][edge[0]]))
            )
    if not weights:
        return None, None, {"status": "no_path_edges", "edge_visits": visits}
    matrix = (
        sparse.csc_matrix(
            (values, (rows, columns)), shape=(16 * len(weights), projection.shape[0])
        )
        @ projection
    )
    matrix.eliminate_zeros()
    return (
        matrix,
        np.array(weights),
        {
            "edge_visits": visits,
            "edges": len(weights),
            "direction_rows": matrix.shape[0],
            "unreduced_coefficient_entries": 16 * 6 * len(weights),
        },
    )


def _score(matrix, weights, solution):
    return float(
        weights @ np.maximum(0, (matrix @ solution).reshape(-1, 16).max(axis=1))
    )


def _storage_note(primary, variation, epigraph, secondary, target, objective, weights):
    def storage(matrix):
        return sum(a.nbytes for a in (matrix.data, matrix.indices, matrix.indptr))

    return {
        "sparse_array_bytes": storage(secondary),
        "variation_sparse_array_bytes": storage(variation),
        "epigraph_sparse_array_bytes": storage(epigraph),
        "primary_sparse_bytes_reassembled": storage(primary),
        "secondary_dense_array_bytes": target.nbytes
        + objective.nbytes
        + weights.nbytes,
    }


def select_variation(
    plan,
    matrix,
    rhs,
    projection,
    original,
    primary_error,
    bounds,
    solver,
    deadline,
    *,
    max_edges=1024,
):
    began = time.perf_counter()
    note = {
        "selected": False,
        "primary_error_limit_m": float(primary_error),
        "scope": "Same original inequalities/bounds; minimax error is pinned before minimizing edge-length-weighted 16-direction gradient jumps. No changed geometric admissibility.",
    }
    if type(max_edges) is not int or not 0 <= max_edges <= 1024:
        raise ValueError("bounded integer variation edge budget required")
    if began >= deadline:
        return original, dict(
            note, status="time_budget", elapsed_s=time.perf_counter() - began
        )
    variation, weights, work = _variation_matrix(plan, projection, deadline, max_edges)
    note.update(work)
    if variation is None:
        return original, dict(note, elapsed_s=time.perf_counter() - began)
    before = _score(variation, weights, original)
    note["variation_before_m"] = before
    if before == 0 or time.perf_counter() >= deadline:
        return original, dict(
            note,
            status="zero_variation" if before == 0 else "time_budget",
            elapsed_s=time.perf_counter() - began,
        )
    count, edges = matrix.shape[1], len(weights)
    epigraph = sparse.csc_matrix(
        (np.ones(16 * edges), (np.arange(16 * edges), np.repeat(np.arange(edges), 16))),
        shape=(16 * edges, edges),
    )
    secondary = sparse.bmat([[matrix, None], [variation, -epigraph]], format="csc")
    target = np.r_[rhs, np.zeros(16 * edges)]
    objective = np.r_[np.zeros(count), weights]
    selected_bounds = list(bounds)
    selected_bounds[-1] = (bounds[-1][0], min(bounds[-1][1], primary_error))
    selected_bounds += [(0, None)] * edges
    remaining = deadline - time.perf_counter()
    if remaining <= 0:
        return original, dict(
            note, status="time_budget", elapsed_s=time.perf_counter() - began
        )
    prepared = time.perf_counter()
    answer = linprog(
        objective,
        A_ub=secondary,
        b_ub=target,
        bounds=selected_bounds,
        method=solver,
        options={
            "time_limit": remaining,
            "presolve": False,
            "primal_feasibility_tolerance": 1e-9,
        },
    )
    note.update(
        status=answer.message,
        variables=secondary.shape[1],
        constraints=secondary.shape[0],
        nonzeros=secondary.nnz,
        **_storage_note(
            matrix, variation, epigraph, secondary, target, objective, weights
        ),
        preparation_s=prepared - began,
        solver_s=time.perf_counter() - prepared,
        elapsed_s=time.perf_counter() - began,
    )
    if not answer.success:
        return original, note
    candidate = answer.x[:count]
    if not np.isfinite(candidate).all():
        return original, dict(note, status="nonfinite_candidate")
    residual = float(np.max(matrix @ candidate - rhs))
    after = _score(variation, weights, candidate)
    note.update(
        variation_after_m=after,
        original_maximum_residual=residual,
        selected_error_variable_m=float(candidate[-1]),
    )
    if (
        residual > 1e-7
        or any(
            (low is not None and value < low) or (high is not None and value > high)
            for value, (low, high) in zip(
                candidate, selected_bounds[:count], strict=True
            )
        )
        or after >= before
    ):
        return original, dict(note, status="candidate_not_better_or_not_feasible")
    return candidate, dict(note, selected=True)
