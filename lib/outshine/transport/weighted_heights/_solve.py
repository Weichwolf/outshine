"""Sparse quadratic objective under existing shared whole-face cone constraints."""

import time

import numpy as np

from ..heights import _gradient_matrix


def solve(
    faces,
    target,
    frames,
    coefficients,
    fixed,
    fixed_heights,
    weights,
    budget,
    *,
    excess_limits=None,
):
    import clarabel
    from scipy import sparse

    began = time.perf_counter()
    count = len(target)
    centre = float(np.median(target))
    directed = np.einsum("fki,fij->fkj", frames, coefficients)
    cone_rows = np.concatenate((np.zeros((len(faces), 1, 3)), -directed), axis=1)
    matrix = _gradient_matrix(faces, cone_rows, count)[:, :count]
    contacts = sparse.csc_matrix(
        (np.ones(len(fixed)), (np.arange(len(fixed)), fixed)), shape=(len(fixed), count)
    )
    matrix = sparse.vstack((matrix, contacts)).tocsc()
    rhs = np.r_[
        np.c_[np.ones(len(faces)), np.zeros((len(faces), 2))].ravel(),
        np.asarray(fixed_heights) - centre,
    ]
    cones = [clarabel.SecondOrderConeT(3) for _ in faces]
    if len(fixed):
        cones.append(clarabel.ZeroConeT(len(fixed)))
    diagonal = np.asarray(weights)
    objective = -weights * (target - centre)
    if excess_limits is not None:
        softened = np.flatnonzero(excess_limits > 0)
        slack_count = len(softened)
        slack_columns = sparse.csc_matrix(
            (-np.ones(slack_count), (3 * softened, np.arange(slack_count))),
            shape=(matrix.shape[0], slack_count),
        )
        matrix = sparse.hstack((matrix, slack_columns))
        identity = sparse.eye(slack_count, format="csc")
        bounds = sparse.vstack((-identity, identity))
        matrix = sparse.vstack(
            (
                matrix,
                sparse.hstack((sparse.csc_matrix((2 * slack_count, count)), bounds)),
            )
        ).tocsc()
        rhs = np.r_[rhs, np.zeros(slack_count), excess_limits[softened]]
        if slack_count:
            cones.append(clarabel.NonnegativeConeT(2 * slack_count))
        diagonal = np.r_[diagonal, np.zeros(slack_count)]
        objective = np.r_[objective, np.ones(slack_count)]
    settings = clarabel.DefaultSettings()
    settings.verbose = False
    settings.time_limit = budget.time_limit_s
    settings.max_iter = budget.max_iterations
    settings.max_threads = 1
    settings.tol_feas = settings.tol_gap_abs = settings.tol_gap_rel = 1e-9
    setup_ms = (time.perf_counter() - began) * 1000
    solver = clarabel.DefaultSolver(
        sparse.diags(diagonal, format="csc"),
        objective,
        matrix,
        rhs,
        cones,
        settings,
    )
    result = solver.solve()
    heights = None if result.x is None else np.asarray(result.x[:count]) + centre
    return heights, {
        "solver_status": str(result.status),
        "iterations": result.iterations,
        "setup_ms": setup_ms,
        "solver_ms": result.solve_time * 1000,
        "matrix_shape": list(matrix.shape),
        "matrix_nonzeros": matrix.nnz,
        "hessian_nonzeros": count,
        "objective": "weighted vertex least squares",
    }
