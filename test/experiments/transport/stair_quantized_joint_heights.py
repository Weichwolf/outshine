"""Bounded Float32 height lattice with conservative polygonal grade constraints."""

import time

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.transport import HeightFit
from outshine_ref.transport.heights import (
    _error_rows,
    _gradient_matrix,
    _gradient_weights,
)
from stair_closed_height_rounding import normalized_grade
from stair_joint_height_fit import level_groups


def lattice(target):
    centre = float(np.float32(np.median(target)))
    span = float(np.ptp(target))
    low, high = float(target.min() - span), float(target.max() + span)
    step = float(abs(np.spacing(np.float32(centre))))
    if (
        not np.isfinite([centre, low, high, step]).all()
        or low * high <= 0
        or min(abs(low), abs(high)) < np.finfo("f4").tiny
        or float(abs(np.spacing(np.float32(low)))) != step
        or float(abs(np.spacing(np.float32(high)))) != step
    ):
        raise ValueError("one normal Float32 height binade required by this lattice")
    return centre, step, low, high, span


def matrix_for(surface, frames, groups, centre, step):
    from scipy import sparse

    xy, faces, target = (
        surface.vertices[:, :2],
        surface.triangles,
        surface.vertices[:, 2],
    )
    count = len(target)
    error, bound = _error_rows((target - centre) / step)
    angle = np.arange(16) * 2 * np.pi / 16
    normals = np.c_[np.cos(angle), np.sin(angle)]
    coefficients = (
        np.einsum("ki,fij,fjl->fkl", normals, frames, _gradient_weights(xy, faces))
        * step
    )
    grade = _gradient_matrix(faces, coefficients, count)
    indices = np.concatenate([g[1:] for g in groups])
    anchors = np.concatenate([np.full(len(g) - 1, g[0]) for g in groups])
    equality = sparse.csc_matrix(
        (
            np.r_[np.ones(len(indices)), -np.ones(len(indices))],
            (np.tile(np.arange(len(indices)), 2), np.r_[indices, anchors]),
        ),
        shape=(len(indices), count + 1),
    )
    matrix = sparse.vstack((error, grade, equality)).tocsc()
    upper = np.r_[
        bound, np.full(len(faces) * 16, np.cos(np.pi / 16)), np.zeros(len(indices))
    ]
    lower = np.r_[
        np.full(len(bound) + len(faces) * 16, -np.inf), np.zeros(len(indices))
    ]
    return matrix, lower, upper


def fit_quantized_levels(surface, frames, plans, *, time_limit_s=8.0, max_nodes=100):
    from scipy.optimize import Bounds, LinearConstraint, milp

    began = time.perf_counter()
    if not isinstance(surface, Mesh):
        raise TypeError("owned surface required for quantized joint levels")
    if (
        not np.isfinite(time_limit_s)
        or not 0 <= time_limit_s <= 8
        or type(max_nodes) is not int
        or not 1 <= max_nodes <= 100
    ):
        raise ValueError("bounded quantized height time and node limits required")
    count, faces = len(surface.vertices), surface.triangles
    report = {
        "method": "Float32 height lattice MILP",
        "valid": False,
        "time_limit_s": time_limit_s,
        "max_nodes": max_nodes,
        "directions": 16,
    }
    if count > 4096 or len(faces) > 8192 or time_limit_s == 0:
        return HeightFit(
            None, {**report, "reason": "input_or_time_budget_exhausted"}
        ), None
    frames = np.asarray(frames, float)
    if (
        not len(faces)
        or frames.shape != (len(faces), 2, 2)
        or not np.isfinite(frames).all()
        or np.any(np.linalg.det(frames) == 0)
    ):
        raise ValueError("finite nonsingular quantized height frames required")
    if not np.array_equal(
        surface.vertices[:, :2], surface.vertices[:, :2].astype("f4").astype(float)
    ):
        raise ValueError("exact Float32 source-model XY required for height lattice")
    groups = level_groups(surface.vertices[:, :2], plans)
    target = surface.vertices[:, 2]
    try:
        centre, step, low, high, span = lattice(target)
    except ValueError as error:
        return HeightFit(None, {**report, "reason": str(error)}), None
    matrix, lower, upper = matrix_for(surface, frames, groups, centre, step)
    bounds = Bounds(
        np.r_[np.full(count, np.ceil((low - centre) / step)), 0],
        np.r_[np.full(count, np.floor((high - centre) / step)), span / step],
    )
    solve_start = time.perf_counter()
    result = milp(
        np.r_[np.zeros(count), 1.0],
        integrality=np.r_[np.ones(count), 0],
        bounds=bounds,
        constraints=LinearConstraint(matrix, lower, upper),
        options={
            "time_limit": time_limit_s,
            "node_limit": max_nodes,
            "mip_rel_gap": 1e-5,
        },
    )
    report.update(
        solver_status=int(result.status),
        solver_message=str(result.message),
        solver_ms=(time.perf_counter() - solve_start) * 1000,
        total_ms=(time.perf_counter() - began) * 1000,
        nodes=getattr(result, "mip_node_count", None),
        matrix_shape=list(matrix.shape),
        matrix_nonzeros=matrix.nnz,
        sparse_array_bytes=matrix.data.nbytes
        + matrix.indices.nbytes
        + matrix.indptr.nbytes,
        centre_m=centre,
        step_m=step,
        reason="no_valid_quantized_candidate",
    )
    if result.x is None or not np.isfinite(result.x).all():
        return HeightFit(None, report), None
    vertices = surface.vertices.copy()
    integers = np.rint(result.x[:count])
    if (
        np.max(abs(result.x[:count] - integers)) > 1e-6
        or np.any(integers < bounds.lb[:count])
        or np.any(integers > bounds.ub[:count])
    ):
        return HeightFit(None, {**report, "reason": "invalid_integer_candidate"}), None
    vertices[:, 2] = centre + integers * step
    if not np.array_equal(vertices, vertices.astype("f4").astype(float)):
        return HeightFit(
            None, {**report, "reason": "candidate_not_exact_Float32"}
        ), None
    grades = [normalized_grade(vertices, faces, frames, cast) for cast in (False, True)]
    error = max(float(np.ptp(vertices[g, 2])) for g in groups)
    report.update(
        maximum_normalized_grades=grades,
        maximum_level_error_m=error,
        maximum_height_change_m=float(abs(vertices[:, 2] - target).max()),
    )
    if not all(np.isfinite(g) and g <= 1 for g in grades) or error != 0:
        return HeightFit(None, report), None
    report.update(valid=True, reason="", optimality_claimed=False)
    return HeightFit(Mesh(vertices, faces), report), np.array(
        [vertices[g[0], 2] for g in groups]
    )
