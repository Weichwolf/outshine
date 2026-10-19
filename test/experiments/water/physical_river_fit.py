"""Conservative physical whole-facet water reference, not a native cooker (#760)."""

import time

import clarabel
import numpy as np
from native_profile_fit import area_mass
from outshine_ref.contracts import Mesh
from outshine_ref.transport.heights import HeightFit, _gradient_weights
from physical_river_frame import normal_caps, tangent_directions
from scipy import sparse


def grade_envelopes(normals, caps, faces, tangent):
    """Enclose physical geodetic grades in projected-constant directions.

    Horizontal metres are on the parallel surface at the query's own height.
    Caller supplies unit reference tangents orthogonal to each cap's centre Up.
    """
    normal = normals[faces]
    size = np.linalg.norm(normal, axis=1)
    delta = caps["delta"][faces]
    vertical = (normal * caps["up"][faces]).sum(axis=1)
    lower_denominator = (vertical - size * delta) * np.sqrt(1 - delta**2)
    upper_denominator = vertical + size * delta
    if np.any(lower_denominator <= 0):
        raise ValueError("physical facets require a uniformly positive vertical ray")
    centre = -(normal * tangent).sum(axis=1)
    low, high = centre - size * delta, centre + size * delta
    return np.c_[
        np.minimum(low / lower_denominator, low / upper_denominator),
        np.maximum(high / lower_denominator, high / upper_denominator),
    ]


def condition_matrix(mesh, caps, row_faces, tangent, lower, upper, datum):
    """Build sparse linear inequalities plus one norm SOC for each physical facet."""
    nv, nf = len(mesh.vertices), len(mesh.triangles)
    weights = _gradient_weights(mesh.vertices[:, :2], mesh.triangles)
    if not np.isfinite(weights).all():
        raise ValueError("finite nondegenerate face gradients required")

    def normal_rows(faces, vectors, auxiliary):
        values = -np.einsum("fi,fij->fj", vectors[:, :2], weights[faces])
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
    identity = sparse.hstack([sparse.eye(nv), sparse.csr_matrix((nv, nf))])
    low, high = caps["up_bounds_m"] - datum
    matrices.extend([identity, -identity])
    bounds.extend([np.full(nv, high), np.full(nv, -low)])
    inequalities = sparse.vstack(matrices, format="csc")
    rhs = np.concatenate(bounds)
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


def solve_program(mesh, caps, faces, tangent, rows, pins, seconds, iterations, start):
    nv, nf = len(mesh.vertices), len(mesh.triangles)
    datum = float(np.median(mesh.vertices[:, 2]))
    inequality, rhs, soc, soc_rhs = condition_matrix(
        mesh, caps, faces, tangent, rows[:, 4], rows[:, 5], datum
    )
    contact = sparse.coo_matrix(
        (np.ones(len(pins)), (np.arange(len(pins)), list(pins))),
        shape=(len(pins), nv + nf),
    ).tocsc()
    matrix = sparse.vstack([inequality, contact, soc], format="csc")
    bound = np.r_[rhs, np.array(list(pins.values())) - datum, soc_rhs]
    mass = area_mass(mesh)
    mass /= mass.sum()
    hessian = sparse.block_diag([mass, sparse.csc_matrix((nf, nf))], format="csc")
    linear = np.r_[-mass @ (mesh.vertices[:, 2] - datum), np.zeros(nf)]
    cones = [clarabel.NonnegativeConeT(len(rhs))]
    if pins:
        cones.append(clarabel.ZeroConeT(len(pins)))
    cones.extend(clarabel.SecondOrderConeT(4) for _ in range(nf))
    settings = clarabel.DefaultSettings()
    settings.verbose = False
    settings.max_threads = 1
    settings.max_iter = iterations
    settings.tol_feas = settings.tol_gap_abs = settings.tol_gap_rel = 1e-11
    settings.time_limit = max(0, seconds - (time.perf_counter() - start))
    if settings.time_limit == 0:
        return None, None, {"reason": "solver_budget_exhausted"}
    result = clarabel.DefaultSolver(
        sparse.triu(hessian).tocsc(), linear, matrix, bound, cones, settings
    ).solve()
    report = {
        "solver_status": str(result.status),
        "iterations": result.iterations,
        "solve_ms": result.solve_time * 1000,
        "vertices": nv,
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


def fit_physical_surface(
    mesh,
    anchor,
    rows,
    *,
    up_bounds_m,
    fixed=None,
    seconds=5.0,
    iterations=200,
    normal_model="whole-prism",
):
    """Rows are (face, ENU dx,dy,dz,lower<=0,upper>=0), not old planar rows.

    Fixed contacts are explicit ENU-Up values at existing shared vertices. A
    rejected solve returns no geometry, never an unvalidated/relaxed water field.
    """
    start = time.perf_counter()
    report = {"valid": False, "scope": "finite conservative physical SOCP reference"}

    def absent(reason):
        return HeightFit(
            None,
            dict(report, reason=reason, total_ms=1000 * (time.perf_counter() - start)),
        )

    if not isinstance(mesh, Mesh):
        raise TypeError("owned shared Mesh required")
    nv, nf = len(mesh.vertices), len(mesh.triangles)
    rows = np.asarray(rows, float)
    if not np.isfinite(seconds) or seconds < 0 or type(iterations) is not int:
        raise ValueError(
            "finite nonnegative time and integer iteration budget required"
        )
    if nv > 8192 or nf > 16384 or len(rows) > 65536:
        return absent("input_budget_exhausted")
    if seconds == 0 or iterations <= 0:
        return absent("solver_budget_exhausted")
    triangle = mesh.vertices[mesh.triangles, :2]
    edge = triangle[:, 1:] - triangle[:, :1]
    determinant = edge[:, 0, 0] * edge[:, 1, 1] - edge[:, 0, 1] * edge[:, 1, 0]
    if np.any(determinant == 0) or not np.isfinite(determinant).all():
        return absent("degenerate_surface")
    if (
        not nf
        or rows.ndim != 2
        or rows.shape[1] != 6
        or not np.isfinite(rows).all()
        or np.any(rows[:, 0] != np.floor(rows[:, 0]))
        or np.any(rows[:, 0] < 0)
        or np.any(rows[:, 0] >= nf)
        or np.any(rows[:, 4] > 0)
        or np.any(rows[:, 5] < 0)
    ):
        raise ValueError(
            "existing faces, finite 3D directions and intervals containing zero required"
        )
    pins = dict(fixed or {})
    if any(
        type(i) is not int or not 0 <= i < nv or not np.isfinite(h)
        for i, h in pins.items()
    ):
        raise ValueError("finite ENU-Up contacts at existing vertices required")
    caps = normal_caps(anchor, mesh.vertices[:, :2], mesh.triangles, up_bounds_m)
    if normal_model not in ("whole-prism", "centre", "enu-control"):
        raise ValueError("unknown explicitly declared normal model")
    report["normal_model"] = normal_model
    report["scope"] = (
        "finite physical whole-prism envelope"
        if normal_model == "whole-prism"
        else "declared approximation control; no physical whole-face admission"
    )
    if normal_model != "whole-prism":
        caps["delta"] = np.zeros(nf)
    if normal_model == "enu-control":
        caps["up"] = np.tile([0.0, 0.0, 1.0], (nf, 1))
    faces = rows[:, 0].astype(int)
    tangent = tangent_directions(caps["up"][faces], rows[:, 1:4])
    height, mass, solver_report = solve_program(
        mesh, caps, faces, tangent, rows, pins, seconds, iterations, start
    )
    report.update(solver_report)
    if height is None:
        return absent(report["reason"])
    if not np.isfinite(height).all():
        return absent("nonfinite_solution")
    vertices = np.c_[mesh.vertices[:, :2], height]
    edges = vertices[mesh.triangles][:, 1:] - vertices[mesh.triangles][:, :1]
    normal = np.cross(edges[:, 0], edges[:, 1])
    normal /= normal[:, 2, None]
    envelope = grade_envelopes(normal, caps, faces, tangent)
    violation = max(
        float((rows[:, 4] - envelope[:, 0]).max(initial=0)),
        float((envelope[:, 1] - rows[:, 5]).max(initial=0)),
        0.0,
    )
    pin_error = max((abs(height[i] - h) for i, h in pins.items()), default=0.0)
    box_error = max(
        float(up_bounds_m[0] - height.min()), float(height.max() - up_bounds_m[1]), 0.0
    )
    report.update(
        whole_face_envelope_violation=violation,
        fixed_up_error_m=pin_error,
        height_box_violation_m=box_error,
    )
    if not np.isfinite(height).all() or max(violation, pin_error, box_error) > 1e-9:
        return absent("physical_envelope_check_failed")
    residual = height - mesh.vertices[:, 2]
    report.update(
        valid=True,
        physical_whole_face_admitted=normal_model == "whole-prism",
        reason="",
        integrated_enu_up_rms_m=float(np.sqrt(residual @ mass @ residual)),
        maximum_enu_up_target_error_m=float(abs(residual).max()),
        total_ms=1000 * (time.perf_counter() - start),
    )
    return HeightFit(Mesh(vertices, mesh.triangles), report)
