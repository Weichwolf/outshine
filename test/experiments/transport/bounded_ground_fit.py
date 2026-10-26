"""Experimental joint ground fit with affine height inequalities, not a public API."""

import time

import numpy as np
from outshine_ref.transport.heights import _gradient_matrix, _gradient_weights
from surface_height_checks import normal_gradients


def fit(
    xy,
    faces,
    target,
    frames,
    bounds,
    weights,
    *,
    seconds=8,
    iterations=100,
    max_elements=8192,
):
    import clarabel
    from scipy import sparse

    began = time.perf_counter()
    count = len(xy)
    columns, coefficients, limits = bounds
    if (
        type(max_elements) is not int
        or not 1 <= max_elements <= 262144
        or not 1 <= count <= max_elements
        or not 1 <= len(faces) <= max_elements
        or not 1 <= len(limits) <= 65536
        or columns.shape != coefficients.shape
        or columns.shape != (len(limits), 3)
        or np.any(columns < 0)
        or np.any(columns >= count)
        or frames.shape != (len(faces), 2, 2)
        or target.shape != (count,)
        or weights.shape != (count,)
        or np.any(weights <= 0)
        or not all(
            np.isfinite(a).all()
            for a in (xy, target, frames, coefficients, limits, weights)
        )
        or not np.isfinite(seconds)
        or seconds < 0
        or type(iterations) is not int
        or not 0 <= iterations <= 100
    ):
        raise ValueError("bounded finite nonempty experimental height input required")
    if seconds == 0 or iterations == 0:
        return None, {"valid": False, "reason": "height_budget_exhausted"}
    centre = float(np.median(target))
    directed = np.einsum("fki,fij->fkj", frames, _gradient_weights(xy, faces))
    cone_rows = np.concatenate((np.zeros((len(faces), 1, 3)), -directed), axis=1)
    gradient = _gradient_matrix(faces, cone_rows, count)[:, :count]
    rows = np.repeat(np.arange(len(limits)), 3)
    linear = sparse.csc_matrix(
        (coefficients.ravel(), (rows, columns.ravel())), shape=(len(limits), count)
    )
    matrix = sparse.vstack((gradient, linear)).tocsc()
    rhs = np.r_[
        np.c_[np.ones(len(faces)), np.zeros((len(faces), 2))].ravel(),
        limits - centre * coefficients.sum(axis=1),
    ]
    cones = [clarabel.SecondOrderConeT(3) for _ in faces]
    cones.append(clarabel.NonnegativeConeT(len(limits)))
    settings = clarabel.DefaultSettings()
    settings.verbose = False
    settings.time_limit, settings.max_iter, settings.max_threads = (
        seconds,
        iterations,
        1,
    )
    settings.tol_feas = settings.tol_gap_abs = settings.tol_gap_rel = 1e-10
    hessian = sparse.diags(weights, format="csc")
    result = clarabel.DefaultSolver(
        hessian,
        -weights * (target - centre),
        matrix,
        rhs,
        cones,
        settings,
    ).solve()
    report = {
        "valid": False,
        "reason": "unchecked_candidate",
        "solver_status": str(result.status),
        "iterations": result.iterations,
        "solver_ms": result.solve_time * 1000,
        "matrix_shape": list(matrix.shape),
        "matrix_nonzeros": matrix.nnz,
        "matrix_storage_bytes": matrix.data.nbytes
        + matrix.indices.nbytes
        + matrix.indptr.nbytes,
        "hessian_storage_bytes": hessian.data.nbytes
        + hessian.indices.nbytes
        + hessian.indptr.nbytes,
        "face_cones": len(faces),
        "affine_bound_rows": len(limits),
        "total_ms": (time.perf_counter() - began) * 1000,
    }
    heights = None if result.x is None else np.asarray(result.x) + centre
    if heights is None or not np.isfinite(heights).all():
        return None, dict(report, reason="nonfinite_or_absent_candidate")
    grade = normal_gradients(xy, faces, heights)
    maximum = float(
        np.linalg.norm(np.einsum("fij,fj->fi", frames, grade), axis=1).max()
    )
    violation = float((np.sum(coefficients * heights[columns], axis=1) - limits).max())
    valid = maximum <= 1 + 1e-7 and violation <= 1e-7
    report.update(
        valid=valid,
        reason="" if valid else "whole_face_or_affine_bound_violation",
        maximum_normalized_grade=maximum,
        maximum_bound_violation_m=violation,
        objective_rms_m=float(
            np.sqrt(np.average((heights - target) ** 2, weights=weights))
        ),
        total_ms=(time.perf_counter() - began) * 1000,
    )
    return (heights if valid else None), report
