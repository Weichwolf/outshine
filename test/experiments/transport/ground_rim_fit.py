"""Same bounded quadratic rim problem, with or without fixed-row elimination."""

import time

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.transport.heights import (
    HeightBudget,
    HeightFit,
    _face_gradients,
    _gradient_weights,
)


def fit_rim_heights(
    surface,
    frames,
    fixed,
    *,
    budget=None,
    maximum_change_m=2.0,
    weights=None,
    eliminate_fixed=True,
):
    budget = HeightBudget() if budget is None else budget
    if not isinstance(surface, Mesh) or not isinstance(budget, HeightBudget):
        raise TypeError("Owned Mesh and explicit HeightBudget required")
    began = time.perf_counter()
    n, f = len(surface.vertices), len(surface.triangles)
    method = "fixed-eliminated-quadratic" if eliminate_fixed else "full-quadratic"

    def pending(reason):
        return HeightFit(
            None,
            {
                "valid": False,
                "reason": reason,
                "method": method,
                "total_ms": (time.perf_counter() - began) * 1000,
            },
        )

    if n > budget.max_vertices or f > budget.max_faces:
        return pending("input_budget_exhausted")
    if budget.time_limit_s == 0:
        return pending("time_budget_exhausted")
    frames = np.asarray(frames, dtype=float)
    weights = np.ones(n) if weights is None else np.asarray(weights, dtype=float)
    if (
        not f
        or frames.shape != (f, 2, 2)
        or not np.isfinite(frames).all()
        or np.any(np.linalg.det(frames) == 0)
        or weights.shape != (n,)
        or not np.isfinite(weights).all()
        or np.any(weights <= 0)
        or not np.isfinite(maximum_change_m)
        or not 0 < maximum_change_m <= 8
    ):
        raise ValueError(
            "Finite face frames, positive weights and bounded observation change required"
        )
    if any(
        not isinstance(i, (int, np.integer))
        or isinstance(i, bool)
        or not 0 <= i < n
        or not np.isfinite(z)
        or z != surface.vertices[i, 2]
        for i, z in fixed.items()
    ):
        raise ValueError("Existing fixed coordinates must exactly match target heights")
    free = np.setdiff1d(np.arange(n), list(fixed))
    gradients = np.einsum(
        "fki,fi->fk",
        frames,
        _face_gradients(
            surface.vertices[:, :2], surface.triangles, surface.vertices[:, 2]
        ),
    )
    maximum = float(np.linalg.norm(gradients, axis=1).max())
    if maximum <= 1.0000001:
        return HeightFit(
            surface,
            {
                "valid": True,
                "reason": "",
                "method": method,
                "solver_status": "prior_feasible",
                "free_vertices": len(free),
                "maximum_height_change_m": 0.0,
                "weighted_squared_error_m2": 0.0,
                "maximum_normalized_grade": maximum,
                "total_ms": (time.perf_counter() - began) * 1000,
            },
        )
    if not len(free):
        return pending("fixed_face_grade_conflict")
    variables = free if eliminate_fixed else np.arange(n)
    local = np.full(n, -1, dtype=np.int64)
    local[variables] = np.arange(len(variables))
    face_nodes = local[surface.triangles]
    active = np.any(face_nodes >= 0, axis=1)
    if np.any(np.linalg.norm(gradients[~active], axis=1) > 1.0000001):
        return pending("fixed_face_grade_conflict")
    return solve_rim(
        surface,
        frames,
        fixed,
        variables,
        face_nodes,
        active,
        weights,
        maximum_change_m,
        budget,
        began,
        method,
    )


def solve_rim(
    surface,
    frames,
    fixed,
    variables,
    face_nodes,
    active,
    weights,
    maximum_change,
    budget,
    began,
    method,
):
    import clarabel
    from scipy import sparse

    datum = float(np.median(surface.vertices[:, 2]))
    target = surface.vertices[variables, 2] - datum
    coefficients = np.einsum(
        "fki,fij->fkj",
        frames[active],
        _gradient_weights(surface.vertices[:, :2], surface.triangles[active]),
    )
    base = surface.vertices[:, 2] - datum
    base[variables] = 0
    offset = np.einsum("fij,fj->fi", coefficients, base[surface.triangles[active]])
    rows = np.repeat(np.arange(2 * sum(active)), 3)
    columns = np.broadcast_to(face_nodes[active, None, :], coefficients.shape).ravel()
    used = columns >= 0
    cone_rows = 3 * (rows[used] // 2) + 1 + rows[used] % 2
    cone_matrix = sparse.csc_matrix(
        (-coefficients.ravel()[used], (cone_rows, columns[used])),
        shape=(3 * sum(active), len(variables)),
    )
    identity = sparse.eye(len(variables), format="csc")
    blocks = [sparse.vstack((identity, -identity)), cone_matrix]
    rhs = [
        np.r_[target + maximum_change, -target + maximum_change],
        np.c_[np.ones(sum(active)), offset].ravel(),
    ]
    cones = [clarabel.NonnegativeConeT(2 * len(variables))] + [
        clarabel.SecondOrderConeT(3) for _ in range(sum(active))
    ]
    if len(variables) == len(surface.vertices) and fixed:
        ids = np.array(list(fixed), dtype=int)
        blocks.append(
            sparse.csc_matrix(
                (np.ones(len(ids)), (np.arange(len(ids)), ids)),
                shape=(len(ids), len(variables)),
            )
        )
        rhs.append(surface.vertices[ids, 2] - datum)
        cones.append(clarabel.ZeroConeT(len(ids)))
    settings = clarabel.DefaultSettings()
    settings.verbose = False
    settings.time_limit = budget.time_limit_s
    settings.max_iter = budget.max_iterations
    settings.max_threads = 1
    settings.tol_feas = settings.tol_gap_abs = settings.tol_gap_rel = 1e-9
    matrix = sparse.vstack(blocks).tocsc()
    w = weights[variables]
    solver = clarabel.DefaultSolver(
        sparse.diags(2 * w, format="csc"),
        -2 * w * target,
        matrix,
        np.concatenate(rhs),
        cones,
        settings,
    )
    solution = solver.solve()
    status = str(solution.status)
    report = {
        "valid": False,
        "reason": "no_valid_solver_result",
        "method": method,
        "solver_status": status,
        "iterations": solution.iterations,
        "free_vertices": len(surface.vertices) - len(fixed),
        "solver_variables": len(variables),
        "active_cones": int(sum(active)),
        "omitted_fixed_faces": int(sum(~active)),
        "matrix_rows": matrix.shape[0],
        "matrix_nonzeros": matrix.nnz,
        "constraint_matrix_payload_bytes": matrix.data.nbytes
        + matrix.indices.nbytes
        + matrix.indptr.nbytes,
        "solver_ms": solution.solve_time * 1000,
    }
    mesh = None
    if status in ("Solved", "AlmostSolved"):
        vertices = surface.vertices.copy()
        vertices[variables, 2] = np.asarray(solution.x) + datum
        lower = surface.vertices[variables, 2] - maximum_change
        upper = surface.vertices[variables, 2] + maximum_change
        lower = np.where(
            surface.vertices[variables, 2] - lower > maximum_change,
            np.nextafter(lower, np.inf),
            lower,
        )
        upper = np.where(
            upper - surface.vertices[variables, 2] > maximum_change,
            np.nextafter(upper, -np.inf),
            upper,
        )
        bounded = np.clip(vertices[variables, 2], lower, upper)
        report["observation_bound_projections"] = int(
            np.count_nonzero(bounded != vertices[variables, 2])
        )
        report["maximum_bound_projection_m"] = float(
            abs(bounded - vertices[variables, 2]).max(initial=0)
        )
        vertices[variables, 2] = bounded
        for at, z in fixed.items():
            vertices[at, 2] = z
        if np.isfinite(vertices).all():
            gradients = np.einsum(
                "fki,fi->fk",
                frames,
                _face_gradients(vertices[:, :2], surface.triangles, vertices[:, 2]),
            )
            maximum = float(np.linalg.norm(gradients, axis=1).max())
            delta = vertices[:, 2] - surface.vertices[:, 2]
            report.update(
                maximum_normalized_grade=maximum,
                maximum_height_change_m=float(abs(delta).max()),
                weighted_squared_error_m2=float(np.dot(weights, delta * delta)),
            )
            if maximum <= 1.0000001 and abs(delta).max() <= maximum_change:
                mesh = Mesh(vertices, surface.triangles)
                report.update(valid=True, reason="")
            else:
                report["reason"] = "whole_face_or_observation_check_failed"
    report["total_ms"] = (time.perf_counter() - began) * 1000
    return HeightFit(mesh, report)
