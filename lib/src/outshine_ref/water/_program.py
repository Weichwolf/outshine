"""Sparse physical water constraint programs and finite solver certificates (#760)."""

import time
from types import SimpleNamespace

import clarabel
import numpy as np
from scipy import sparse

from outshine_ref.transport.heights import _gradient_weights

from ._mass import area_mass


def solve_osqp(hessian, linear, matrix, bound, inequalities, seconds, iterations):
    import osqp

    lower = np.full(len(bound), -np.inf)
    lower[inequalities:] = bound[inequalities:]
    solver = osqp.OSQP()
    solver.setup(
        P=sparse.triu(hessian).tocsc(),
        q=linear,
        A=matrix,
        l=lower,
        u=bound,
        verbose=False,
        max_iter=iterations,
        time_limit=seconds,
        eps_abs=1e-11,
        eps_rel=1e-11,
        polishing=True,
        adaptive_rho_interval=50,
        warm_starting=False,
    )
    result = solver.solve(raise_error=False)
    status = {1: "Solved", 2: "AlmostSolved"}.get(
        result.info.status_val, result.info.status
    )
    return SimpleNamespace(
        x=result.x,
        z=result.y,
        status=status,
        iterations=result.info.iter,
        solve_time=result.info.solve_time + result.info.polish_time,
        native_setup_ms=result.info.setup_time * 1000,
        rho_updates=result.info.rho_updates,
        polish_status=result.info.status_polish,
    )


def condition_matrix(
    mesh, caps, row_faces, tangent, lower, upper, datum, normal_bound=None
):
    """Build prism inequalities with either norm SOCs or derived constant bounds."""
    nv, nf = len(mesh.vertices), len(mesh.triangles)
    auxiliary_count = nf if normal_bound is None else 0
    weights = _gradient_weights(mesh.vertices[:, :2], mesh.triangles)
    if not np.isfinite(weights).all():
        raise ValueError("finite nondegenerate face gradients required")

    def normal_rows(faces, vectors, auxiliary):
        values = -np.einsum("fi,fij->fj", vectors[:, :2], weights[faces])
        if normal_bound is not None:
            matrix = sparse.coo_matrix(
                (
                    values.ravel(),
                    (
                        np.repeat(np.arange(len(faces)), 3),
                        mesh.triangles[faces].ravel(),
                    ),
                ),
                shape=(len(faces), nv),
            ).tocsr()
            return matrix, -vectors[:, 2] - auxiliary * normal_bound[faces]
        rows = np.repeat(np.arange(len(faces)), 4)
        columns = np.c_[mesh.triangles[faces], nv + faces].ravel()
        return sparse.coo_matrix(
            (np.c_[values, auxiliary].ravel(), (rows, columns)),
            shape=(len(faces), nv + nf),
        ).tocsr(), -vectors[:, 2]

    matrices, bounds = [], []
    delta = caps["delta"][row_faces]
    k = np.sqrt(1 - delta**2)
    for vectors, limit in ((tangent, -lower), (-tangent, upper)):
        direction = vectors - (limit * k)[:, None] * caps["up"][row_faces]
        matrix, bound = normal_rows(row_faces, direction, delta * (1 + limit * k))
        matrices.append(matrix)
        bounds.append(bound)
    matrix, bound = normal_rows(np.arange(nf), -caps["up"], caps["delta"] + 0.5)
    matrices.append(matrix)
    bounds.append(bound)
    identity = sparse.hstack([sparse.eye(nv), sparse.csr_matrix((nv, auxiliary_count))])
    low, high = caps["up_bounds_m"] - datum
    matrices.extend([identity, -identity])
    bounds.extend([np.full(nv, high), np.full(nv, -low)])
    inequalities = sparse.vstack(matrices, format="csc")
    rhs = np.concatenate(bounds)
    if normal_bound is not None:
        return inequalities, rhs, sparse.csc_matrix((0, nv)), np.empty(0)
    soc_rows, soc_columns, soc_values = [], [], []
    for face in range(nf):
        soc_rows.append(4 * face)
        soc_columns.append(nv + face)
        soc_values.append(-1.0)
        for axis in range(2):
            soc_rows.extend([4 * face + 1 + axis] * 3)
            soc_columns.extend(mesh.triangles[face])
            soc_values.extend(-weights[face, axis])
    soc = sparse.coo_matrix(
        (soc_values, (soc_rows, soc_columns)), shape=(4 * nf, nv + nf)
    ).tocsc()
    return inequalities, rhs, soc, np.tile([0, 0, 0, 1.0], nf)


def conic_certificate(result, hessian, linear, matrix, bound, inequalities, contacts):
    """Reconstruct primal/dual cone residuals and the finite convex QP gap."""
    x, dual = np.asarray(result.x), np.asarray(result.z)
    slack = bound - matrix @ x
    first = inequalities + contacts
    soc_slack, soc_dual = slack[first:].reshape(-1, 4), dual[first:].reshape(-1, 4)
    return {
        "primal_cone_violation": max(
            float(-slack[:inequalities].min(initial=0)),
            float(abs(slack[inequalities:first]).max(initial=0)),
            float(
                (np.linalg.norm(soc_slack[:, 1:], axis=1) - soc_slack[:, 0]).max(
                    initial=0
                )
            ),
        ),
        "dual_cone_violation": max(
            float(-dual[:inequalities].min(initial=0)),
            float(
                (np.linalg.norm(soc_dual[:, 1:], axis=1) - soc_dual[:, 0]).max(
                    initial=0
                )
            ),
        ),
        "stationarity_residual": float(
            abs(hessian @ x + linear + matrix.T @ dual).max()
        ),
        "finite_qp_dual_gap": abs(float(x @ hessian @ x + linear @ x + bound @ dual)),
        "complementarity_residual": abs(float(slack @ dual)),
    }


def solve_program(
    mesh,
    caps,
    faces,
    tangent,
    rows,
    pins,
    seconds,
    iterations,
    start,
    *,
    normal_bound=None,
    solver_name="clarabel",
):
    nv, nf = len(mesh.vertices), len(mesh.triangles)
    auxiliary_count = nf if normal_bound is None else 0
    datum = float(np.median(mesh.vertices[:, 2]))
    inequality, rhs, soc, soc_rhs = condition_matrix(
        mesh, caps, faces, tangent, rows[:, 4], rows[:, 5], datum, normal_bound
    )
    contact = sparse.coo_matrix(
        (np.ones(len(pins)), (np.arange(len(pins)), list(pins))),
        shape=(len(pins), nv + auxiliary_count),
    ).tocsc()
    matrix = sparse.vstack([inequality, contact, soc], format="csc")
    bound = np.r_[rhs, np.array(list(pins.values())) - datum, soc_rhs]
    mass = area_mass(mesh)
    mass /= mass.sum()
    hessian = sparse.block_diag(
        [mass, sparse.csc_matrix((auxiliary_count, auxiliary_count))], format="csc"
    )
    linear = np.r_[-mass @ (mesh.vertices[:, 2] - datum), np.zeros(auxiliary_count)]
    cones = [clarabel.NonnegativeConeT(len(rhs))]
    if pins:
        cones.append(clarabel.ZeroConeT(len(pins)))
    cones.extend(clarabel.SecondOrderConeT(4) for _ in range(auxiliary_count))
    settings = clarabel.DefaultSettings()
    settings.verbose = False
    settings.max_threads = 1
    settings.max_iter = iterations
    settings.tol_feas = settings.tol_gap_abs = settings.tol_gap_rel = 1e-11
    settings.time_limit = max(0, seconds - (time.perf_counter() - start))
    if settings.time_limit == 0:
        return None, None, {"reason": "solver_budget_exhausted"}
    began = time.perf_counter()
    cpu_began = time.process_time()
    if solver_name == "clarabel":
        result = clarabel.DefaultSolver(
            sparse.triu(hessian).tocsc(), linear, matrix, bound, cones, settings
        ).solve()
    elif solver_name == "osqp" and normal_bound is not None:
        result = solve_osqp(
            hessian, linear, matrix, bound, len(rhs), settings.time_limit, iterations
        )
    else:
        raise ValueError(
            "OSQP requires a linear-bound program; unknown solver rejected"
        )
    report = {
        "solver_api_wall_ms": 1000 * (time.perf_counter() - began),
        "solver_api_cpu_ms": 1000 * (time.process_time() - cpu_began),
        "solver_status": str(result.status),
        "iterations": result.iterations,
        "solve_ms": result.solve_time * 1000,
        "vertices": nv,
        "solver": solver_name,
        "unknowns": matrix.shape[1],
        "normal_auxiliaries": auxiliary_count,
        "normal_norm_bound_max": None
        if normal_bound is None
        else float(normal_bound.max()),
        "faces": nf,
        "directed_rows": len(rows),
        "matrix_rows": matrix.shape[0],
        "matrix_nonzeros": matrix.nnz,
        "matrix_bytes": matrix.data.nbytes
        + matrix.indices.nbytes
        + matrix.indptr.nbytes,
        "hessian_nonzeros": hessian.nnz,
        "hessian_bytes": hessian.data.nbytes
        + hessian.indices.nbytes
        + hessian.indptr.nbytes,
        "requested_solver_vector_bytes": bound.nbytes + linear.nbytes,
        "cap_delta_max": float(caps["delta"].max()),
    }
    if solver_name == "osqp":
        report.update(
            native_setup_ms=result.native_setup_ms,
            rho_updates=result.rho_updates,
            polish_status=result.polish_status,
        )
    if str(result.status) not in ("Solved", "AlmostSolved"):
        return None, None, dict(report, reason="solver_not_solved")
    report.update(
        conic_certificate(result, hessian, linear, matrix, bound, len(rhs), len(pins))
    )
    report["finite_qp_optimal"] = (
        max(
            report[k]
            for k in (
                "primal_cone_violation",
                "dual_cone_violation",
                "stationarity_residual",
                "finite_qp_dual_gap",
                "complementarity_residual",
            )
        )
        <= 1e-7
    )
    return np.asarray(result.x)[:nv] + datum, mass, report
