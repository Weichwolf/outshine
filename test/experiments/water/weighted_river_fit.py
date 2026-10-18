"""Small weighted directional QP comparison; existing shared fitters stay unchanged."""

import time

import clarabel
import numpy as np
from directional_river_fit import face_gradients
from outshine_ref.contracts import Mesh
from outshine_ref.transport.heights import HeightFit, _gradient_weights
from scipy import sparse


def directions(mesh, flow, directed_rows):
    nf = len(mesh.triangles)
    flow = np.asarray(flow, float)
    rows = (
        np.empty((0, 5)) if directed_rows is None else np.asarray(directed_rows, float)
    )
    if flow.shape != (nf, 2) or not np.isfinite(flow).all():
        raise ValueError("one finite XY direction per existing face required")
    if (
        rows.ndim != 2
        or rows.shape[1] != 5
        or not np.isfinite(rows).all()
        or np.any(rows[:, 0] != np.floor(rows[:, 0]))
        or np.any(rows[:, 0] < 0)
        or np.any(rows[:, 0] >= nf)
        or np.any(rows[:, 3] > rows[:, 4])
    ):
        raise ValueError(
            "existing face, finite direction and gradient interval required"
        )
    lengths = np.linalg.norm(flow, axis=1)
    extra = np.linalg.norm(rows[:, 1:3], axis=1)
    if (
        np.any(lengths == 0)
        or np.any(extra == 0)
        or not np.isfinite(np.r_[lengths, extra]).all()
    ):
        raise ValueError("nonzero representable direction vectors required")
    normalized = rows.copy()
    normalized[:, 1:3] /= extra[:, None]
    return flow / lengths[:, None], normalized


def constraints(mesh, flow, rows, down_grade, cross_grade):
    nv, nf = len(mesh.vertices), len(mesh.triangles)
    coefficients = _gradient_weights(mesh.vertices[:, :2], mesh.triangles)
    if not np.isfinite(coefficients).all():
        raise ValueError("finite representable gradient coefficients required")

    def matrix(directions, faces=mesh.triangles, weights=coefficients):
        values = np.einsum("fi,fij->fj", directions, weights)
        return sparse.coo_matrix(
            (values.ravel(), (np.repeat(np.arange(len(faces)), 3), faces.ravel())),
            shape=(len(faces), nv),
        ).tocsr()

    along = matrix(flow)
    across = matrix(np.c_[-flow[:, 1], flow[:, 0]])
    faces = rows[:, 0].astype(int)
    directed = matrix(rows[:, 1:3], mesh.triangles[faces], coefficients[faces])
    result = sparse.vstack(
        (along, -along, across, -across, directed, -directed), format="csc"
    )
    result.eliminate_zeros()
    rhs = np.r_[
        np.zeros(nf),
        np.full(nf, down_grade),
        np.full(nf, cross_grade),
        np.full(nf, cross_grade),
        rows[:, 4],
        -rows[:, 3],
    ]
    return result, rhs


def certificate(result, matrix, rhs, weights, linear, inequalities):
    x, dual = np.asarray(result.x), np.asarray(result.z)
    stationarity = weights * x + linear + matrix.T @ dual
    conjugate = linear + matrix.T @ dual
    primal = float(0.5 * np.dot(weights * x, x) + linear @ x)
    lower = float(-0.5 * np.dot(conjugate / weights, conjugate) - rhs @ dual)
    error = matrix @ x - rhs
    return {
        "dual_gap": abs(primal - lower),
        "stationarity_residual": float(np.abs(stationarity).max(initial=0)),
        "dual_sign_violation": float(
            np.maximum(-dual[:inequalities], 0).max(initial=0)
        ),
        "primal_row_violation": max(
            float(error[:inequalities].max(initial=0)),
            float(np.abs(error[inequalities:]).max(initial=0)),
        ),
    }


def checked(mesh, height, flow, rows, pins, down_grade, cross_grade):
    gradient = face_gradients(mesh, height)
    along = np.einsum("fi,fi->f", gradient, flow)
    across = np.einsum("fi,fi->f", gradient, np.c_[-flow[:, 1], flow[:, 0]])
    values = np.einsum("fi,fi->f", gradient[rows[:, 0].astype(int)], rows[:, 1:3])
    return {
        "whole_face_violation": max(
            float(along.max()),
            float((-along - down_grade).max()),
            float((np.abs(across) - cross_grade).max()),
            0.0,
        ),
        "source_face_violation": max(
            float((values - rows[:, 4]).max(initial=0)),
            float((rows[:, 3] - values).max(initial=0)),
        ),
        "fixed_height_error_m": max(
            (abs(height[i] - h) for i, h in pins.items()), default=0.0
        ),
        "maximum_uphill_grade": float(along.max()),
        "maximum_cross_grade": float(np.abs(across).max()),
    }


def objective_weights(weights, count):
    weights = np.ones(count) if weights is None else np.asarray(weights, float)
    if (
        weights.shape != (count,)
        or not np.isfinite(weights).all()
        or np.any(weights <= 0)
    ):
        raise ValueError("one positive finite objective weight per vertex required")
    weights = weights / weights.max()
    weights = weights / weights.sum()
    if np.any(weights == 0):
        raise ValueError("representable positive normalized weights required")
    return weights


def fit_weighted_surface(
    mesh,
    flow,
    *,
    weights=None,
    directed_rows=None,
    fixed=None,
    down_grade=0.1,
    cross_grade=0.005,
    seconds=2.0,
    iterations=200,
):
    start = time.perf_counter()
    report = {
        "valid": False,
        "solver_optimal": False,
        "objective": "positive weighted vertex least squares",
    }

    def absent(reason):
        return HeightFit(
            None,
            dict(report, reason=reason, total_ms=(time.perf_counter() - start) * 1000),
        )

    if not isinstance(mesh, Mesh):
        raise TypeError("owned shared Mesh required")
    if not all(np.isfinite(x) and x >= 0 for x in (down_grade, cross_grade, seconds)):
        raise ValueError("finite nonnegative grades/time required")
    nv, nf = len(mesh.vertices), len(mesh.triangles)
    if not nf:
        return absent("empty_surface")
    if nv > 8192 or nf > 16384:
        return absent("input_budget_exhausted")
    if seconds == 0 or type(iterations) is not int or iterations <= 0:
        return absent("solver_budget_exhausted")
    if directed_rows is not None and len(directed_rows) > 32768:
        return absent("directed_row_budget_exhausted")
    flow, rows = directions(mesh, flow, directed_rows)
    pins = dict(fixed or {})
    if any(
        type(i) is not int or not 0 <= i < nv or not np.isfinite(h)
        for i, h in pins.items()
    ):
        raise ValueError("finite separate hard heights at existing vertices required")
    weights = objective_weights(weights, nv)
    try:
        matrix, rhs = constraints(mesh, flow, rows, down_grade, cross_grade)
    except ValueError:
        return absent("degenerate_surface")
    inequality_count = len(rhs)
    centre = float(np.median(mesh.vertices[:, 2]))
    contact = sparse.coo_matrix(
        (np.ones(len(pins)), (np.arange(len(pins)), list(pins))), shape=(len(pins), nv)
    ).tocsc()
    matrix = sparse.vstack((matrix, contact), format="csc")
    rhs = np.r_[rhs, np.array(list(pins.values())) - centre]
    linear = -weights * (mesh.vertices[:, 2] - centre)
    cones = [clarabel.NonnegativeConeT(inequality_count)]
    if pins:
        cones.append(clarabel.ZeroConeT(len(pins)))
    settings = clarabel.DefaultSettings()
    settings.verbose = False
    settings.max_threads = 1
    settings.max_iter = iterations
    settings.time_limit = max(0.0, seconds - (time.perf_counter() - start))
    settings.tol_feas = settings.tol_gap_abs = settings.tol_gap_rel = 1e-11
    if settings.time_limit == 0:
        return absent("solver_budget_exhausted")
    prepared = time.perf_counter()
    solver = clarabel.DefaultSolver(
        sparse.diags(weights, format="csc"), linear, matrix, rhs, cones, settings
    )
    result = solver.solve()
    report.update(
        solver_status=str(result.status),
        iterations=result.iterations,
        setup_ms=(prepared - start) * 1000,
        solve_ms=result.solve_time * 1000,
        rows=matrix.shape[0],
        nonzeros=matrix.nnz,
        hessian_nonzeros=nv,
        matrix_bytes=matrix.data.nbytes + matrix.indices.nbytes + matrix.indptr.nbytes,
    )
    if str(result.status) not in ("Solved", "AlmostSolved"):
        return absent("solver_not_optimal")
    height = np.asarray(result.x) + centre
    if not np.isfinite(height).all():
        return absent("nonfinite_solution")
    report.update(certificate(result, matrix, rhs, weights, linear, inequality_count))
    report.update(checked(mesh, height, flow, rows, pins, down_grade, cross_grade))
    errors = tuple(
        report[k]
        for k in (
            "dual_gap",
            "stationarity_residual",
            "dual_sign_violation",
            "primal_row_violation",
            "whole_face_violation",
            "source_face_violation",
            "fixed_height_error_m",
        )
    )
    if not np.isfinite(errors).all() or max(errors) > 1e-7:
        return absent("independent_certificate_failed")
    vertices = mesh.vertices.copy()
    vertices[:, 2] = height
    residual = height - mesh.vertices[:, 2]
    report.update(
        valid=True,
        solver_optimal=True,
        reason="",
        total_ms=(time.perf_counter() - start) * 1000,
        weighted_rms_m=float(np.sqrt(np.dot(weights, residual**2))),
        maximum_prior_error_m=float(np.abs(residual).max()),
    )
    return HeightFit(Mesh(vertices, mesh.triangles), report)
