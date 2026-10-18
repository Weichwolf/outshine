"""Reproduce the exact area QP through installed OSQP's Python interface."""

import time

import numpy as np
import osqp
from native_profile_fit import area_mass, inspect_solution
from scipy import sparse
from weighted_river_fit import constraints, directions


def solve(mesh, flow, rows, *, solver_eps=1e-9):
    if not np.isfinite(solver_eps) or solver_eps <= 0:
        raise ValueError("positive finite numerical solver tolerance required")
    start = time.perf_counter()
    flow, rows = directions(mesh, flow, rows)
    matrix, bounds = constraints(mesh, flow, rows, 0.1, 0.005)
    mass = area_mass(mesh).tocsc()
    datum = float(np.median(mesh.vertices[:, 2]))
    cost = -mass @ (mesh.vertices[:, 2] - datum)
    solver = osqp.OSQP()
    solver.setup(
        P=sparse.triu(mass, format="csc"),
        q=cost,
        A=matrix,
        l=np.full(len(bounds), -np.inf),
        u=bounds,
        eps_abs=solver_eps,
        eps_rel=solver_eps,
        max_iter=100000,
        polishing=True,
        verbose=False,
        time_limit=5.0,
    )
    result = solver.solve(raise_error=False)
    if result.info.status_val != 1:
        raise RuntimeError(
            f"no solved reference: {result.info.status}, "
            f"iterations={result.info.iter}, residuals="
            f"{result.info.prim_res}/{result.info.dual_res}"
        )
    unbounded = np.full(len(cost), np.inf)
    certificate = inspect_solution(
        (matrix, bounds, cost, mass, -unbounded, unbounded),
        result.x,
        np.zeros(len(cost)),
        -result.y,
    )
    if max(certificate.values()) > 1e-6:
        raise RuntimeError(f"independent reference certificate failed: {certificate}")
    report = dict(
        certificate,
        osqp_version=osqp.__version__,
        status=result.info.status,
        solver_iterations=result.info.iter,
        solver_setup_solve_polish_ms=result.info.run_time * 1000,
        assembly_solve_check_ms=(time.perf_counter() - start) * 1000,
        matrix_rows=matrix.shape[0],
        matrix_nonzeros=matrix.nnz,
        hessian_nonzeros=mass.nnz,
        solver_eps_abs=solver_eps,
        solver_eps_rel=solver_eps,
        solver_time_limit_s=5.0,
        solver_max_iterations=100000,
    )
    return result.x + datum, flow, rows, report
