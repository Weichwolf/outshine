"""Compare identical whole-face/contact constraints with two DEM objectives."""

import time

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.transport.heights import (
    HeightBudget,
    _gradient_matrix,
    _gradient_weights,
    fit_surface_heights,
)


def audit(xy, faces, targets, frames, heights, fixed, datum, weights):
    vertices = np.c_[xy, heights][faces]
    normal = np.cross(vertices[:, 1] - vertices[:, 0], vertices[:, 2] - vertices[:, 0])
    gradient = -normal[:, :2] / normal[:, 2, None]
    normalized = np.linalg.norm(np.einsum("fij,fj->fi", frames, gradient), axis=1)
    error = abs(heights - targets)
    pins = abs(heights[fixed] - datum)
    return {
        "valid": bool(
            np.isfinite(heights).all()
            and normalized.max() <= 1 + 1e-7
            and pins.max(initial=0) <= 1e-7
        ),
        "maximum_normalized_grade": float(normalized.max()),
        "maximum_pin_error_m": float(pins.max(initial=0)),
        "maximum_dem_vertex_error_m": float(error.max()),
        "weighted_dem_rms_m": float(np.sqrt(np.average(error**2, weights=weights))),
        "dem_vertex_rms_m": float(np.sqrt(np.mean(error**2))),
        "dem_vertex_p95_m": float(np.percentile(error, 95)),
    }


def quadratic_fit(xy, faces, target, frames, fixed, datum, weights, seconds):
    import clarabel
    from scipy import sparse

    began = time.perf_counter()
    count = len(xy)
    centre = float(np.median(target))
    coefficients = np.einsum("fki,fij->fkj", frames, _gradient_weights(xy, faces))
    cone_rows = np.concatenate((np.zeros((len(faces), 1, 3)), -coefficients), axis=1)
    # Existing helper reserves the last minimax variable; quadratic fitting has
    # only the actual shared vertex heights, so drop that unused zero column.
    gradient = _gradient_matrix(faces, cone_rows, count)[:, :count]
    constraints = sparse.csc_matrix(
        (np.ones(len(fixed)), (np.arange(len(fixed)), fixed)), shape=(len(fixed), count)
    )
    matrix = sparse.vstack((gradient, constraints)).tocsc()
    rhs = np.r_[
        np.c_[np.ones(len(faces)), np.zeros((len(faces), 2))].ravel(),
        np.full(len(fixed), datum - centre),
    ]
    cones = [clarabel.SecondOrderConeT(3) for _ in faces]
    if len(fixed):
        cones.append(clarabel.ZeroConeT(len(fixed)))
    settings = clarabel.DefaultSettings()
    settings.verbose = False
    settings.time_limit, settings.max_iter, settings.max_threads = seconds, 100, 1
    settings.tol_feas = settings.tol_gap_abs = settings.tol_gap_rel = 1e-9
    setup_ms = (time.perf_counter() - began) * 1000
    solver = clarabel.DefaultSolver(
        sparse.diags(weights, format="csc"),
        -weights * (target - centre),
        matrix,
        rhs,
        cones,
        settings,
    )
    result = solver.solve()
    heights = None if result.x is None else np.asarray(result.x) + centre
    report = {
        "solver_status": str(result.status),
        "iterations": result.iterations,
        "setup_ms": setup_ms,
        "solver_ms": result.solve_time * 1000,
        "total_ms": (time.perf_counter() - began) * 1000,
        "matrix_shape": list(matrix.shape),
        "matrix_nonzeros": matrix.nnz,
        "hessian_nonzeros": count,
        "objective": "lumped-area weighted vertex least squares",
    }
    return heights, report


def conservative_candidate(arrays, datum, heights):
    """A global convex contraction preserves shared topology and fixed contacts.

    The flat datum is feasible for every homogeneous gradient constraint. Pin
    projection happens before measuring gradients. This is a feasible candidate,
    not the optimum or a reason to loosen the final independent acceptance gate.
    """
    z = np.array(heights, dtype=float, copy=True)
    if z.shape != arrays["targets"].shape or not np.isfinite(z).all():
        return None, {"reason": "nonfinite_or_incomplete_solver_candidate"}
    fixed = arrays["fixed"]
    pin_change = float(abs(z[fixed] - datum).max(initial=0))
    z[fixed] = datum
    check = audit(
        *(arrays[k] for k in ("xy", "faces", "targets", "frames")),
        z,
        fixed,
        datum,
        arrays["weights"],
    )
    maximum = check["maximum_normalized_grade"]
    if not np.isfinite(maximum):
        return None, {"reason": "nonfinite_solver_gradient"}
    factor = min(1.0, (1 - 1e-5) / max(maximum, 1e-30))
    z = datum + factor * (z - datum)
    return z, {
        "feasible_flat_contraction": factor,
        "maximum_pin_projection_m": pin_change,
        "before_contraction": check,
        "reason": "",
        "scope": "Global homogeneous-gradient contraction toward the valid flat datum; all fixed contacts exact before contraction. Independent final tolerance remains unchanged; no optimality claim.",
    }


def fit(arrays, datum, mode, seconds=8):
    xy, faces, target, frames, fixed, weights = (
        arrays[k] for k in ("xy", "faces", "targets", "frames", "fixed", "weights")
    )
    if mode == "minimax":
        result = fit_surface_heights(
            Mesh(np.c_[xy, target], faces),
            frames,
            fixed={int(i): datum for i in fixed},
            budget=HeightBudget(
                time_limit_s=seconds, max_vertices=131072, max_faces=160000
            ),
        )
        heights = result.mesh.vertices[:, 2] if result.mesh else None
        report = dict(result.report)
    elif mode == "least_squares":
        heights, report = quadratic_fit(
            xy, faces, target, frames, fixed, datum, weights, seconds
        )
        if heights is not None:
            heights, repair = conservative_candidate(arrays, datum, heights)
            report["conservative_candidate"] = repair
    else:
        raise ValueError("known height objective required")
    report.update(valid=False, mode=mode, solver_time_limit_s=seconds)
    report.setdefault("reason", "no_valid_whole_surface")
    if heights is not None:
        check = audit(xy, faces, target, frames, heights, fixed, datum, weights)
        report["independent_check"] = check
        report["valid"] = check["valid"]
        if report["valid"]:
            report["reason"] = ""
    return heights if report["valid"] else None, report
