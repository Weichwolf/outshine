"""Fit free flat junction levels, then pin their exact Float32 heights."""

import time

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.transport import HeightBudget, HeightFit, fit_surface_heights
from outshine_ref.transport.heights import (
    _error_rows,
    _face_gradients,
    _gradient_matrix,
    _gradient_weights,
)


def level_groups(xy, plans):
    if not 1 <= len(plans) <= 20:
        raise ValueError("one to twenty explicit joint level regions required")
    used, groups = set(), []
    points = shapely.points(xy)
    for plan in plans:
        group = np.flatnonzero(shapely.dwithin(plan["shape"], points, 1e-8))
        if len(group) < 3 or used.intersection(group):
            raise ValueError("disjoint nonempty whole joint level groups required")
        used.update(group)
        groups.append(group)
    return groups


def bound_levels(plans, heights):
    heights = np.asarray(heights)
    if (
        heights.shape != (len(plans),)
        or not np.isfinite(heights).all()
        or not np.array_equal(heights, heights.astype("f4").astype(float))
    ):
        raise ValueError("one finite exact Float32 height per joint region required")
    return [
        {**plan, "prior_height_m": plan["height_m"], "height_m": float(h)}
        for plan, h in zip(plans, heights, strict=True)
    ]


def recipe_levels(plans, data, recipe):
    if recipe.get("junction_height_mode") == "joint":
        return bound_levels(plans, data.get("joint_level_heights_m"))
    if "joint_level_heights_m" in data:
        raise ValueError("undeclared free joint level heights")
    return plans


def solve_levels(surface, frames, groups, budget):
    import clarabel
    from scipy import sparse

    began = time.perf_counter()
    xy, faces, target = (
        surface.vertices[:, :2],
        surface.triangles,
        surface.vertices[:, 2],
    )
    count, centre = len(target), float(np.median(target))
    error, bounds = _error_rows(target - centre)
    directed = np.einsum("fki,fij->fkj", frames, _gradient_weights(xy, faces))
    matrix = _gradient_matrix(
        faces, np.concatenate((np.zeros((len(faces), 1, 3)), -directed), axis=1), count
    )
    vertices = np.concatenate([g[1:] for g in groups])
    anchors = np.concatenate([np.full(len(g) - 1, g[0]) for g in groups])
    equality = sparse.csc_matrix(
        (
            np.r_[np.ones(len(vertices)), -np.ones(len(vertices))],
            (np.tile(np.arange(len(vertices)), 2), np.r_[vertices, anchors]),
        ),
        shape=(len(vertices), count + 1),
    )
    matrix = sparse.vstack((error, matrix, equality)).tocsc()
    rhs = np.r_[
        bounds,
        np.c_[np.ones(len(faces)), np.zeros((len(faces), 2))].ravel(),
        np.zeros(len(vertices)),
    ]
    cones = [
        clarabel.NonnegativeConeT(len(bounds)),
        *[clarabel.SecondOrderConeT(3) for _ in faces],
        clarabel.ZeroConeT(len(vertices)),
    ]
    settings = clarabel.DefaultSettings()
    settings.verbose, settings.max_threads = False, 1
    settings.time_limit, settings.max_iter = budget.time_limit_s, budget.max_iterations
    settings.tol_feas = settings.tol_gap_abs = settings.tol_gap_rel = 1e-9
    result = clarabel.DefaultSolver(
        sparse.csc_matrix((count + 1, count + 1)),
        np.r_[np.zeros(count), 1.0],
        matrix,
        rhs,
        cones,
        settings,
    ).solve()
    heights = np.asarray(result.x[:-1]) + centre if result.x is not None else None
    return heights, {
        "solver_status": str(result.status),
        "solver_ms": result.solve_time * 1000,
        "total_ms": (time.perf_counter() - began) * 1000,
        "iterations": result.iterations,
        "matrix_shape": list(matrix.shape),
        "matrix_nonzeros": matrix.nnz,
        "flat_equality_rows": len(vertices),
    }


def fit_joint_levels(surface, frames, plans, budget):
    began = time.perf_counter()
    if not isinstance(surface, Mesh) or not isinstance(budget, HeightBudget):
        raise TypeError("owned surface and explicit joint height budget required")
    if len(surface.vertices) > min(budget.max_vertices, 4096) or len(
        surface.triangles
    ) > min(budget.max_faces, 8192):
        return HeightFit(
            None, {"reason": "input_budget_exhausted", "valid": False}
        ), None
    if budget.time_limit_s == 0:
        return HeightFit(
            None, {"reason": "time_budget_exhausted", "valid": False}
        ), None
    frames = np.asarray(frames, float)
    if (
        not len(surface.triangles)
        or frames.shape != (len(surface.triangles), 2, 2)
        or not np.isfinite(frames).all()
        or np.any(np.linalg.det(frames) == 0)
    ):
        raise ValueError("finite nonsingular joint whole-face frames required")
    groups = level_groups(surface.vertices[:, :2], plans)
    heights, first = solve_levels(surface, frames, groups, budget)
    report = {
        "free_levels": first,
        "valid": False,
        "reason": "invalid_free_joint_height_fit",
    }
    if heights is None or not np.isfinite(heights).all():
        return HeightFit(None, report), None
    gradients = _face_gradients(surface.vertices[:, :2], surface.triangles, heights)
    grade = float(
        np.linalg.norm(np.einsum("fij,fj->fi", frames, gradients), axis=1).max()
    )
    error = max(float(np.ptp(heights[g])) for g in groups)
    report.update(maximum_free_level_error_m=error, maximum_free_normalized_grade=grade)
    if grade > 1 + 1e-7 or error > 1e-7:
        return HeightFit(None, report), None
    seconds = budget.time_limit_s - (time.perf_counter() - began)
    iterations = budget.max_iterations - first["iterations"]
    if seconds <= 0 or iterations <= 0:
        return HeightFit(
            None, {**report, "reason": "joint_height_budget_exhausted"}
        ), None
    levels = np.array([np.mean(heights[g]) for g in groups], dtype="f4").astype(float)
    pins = {
        int(i): float(h) for group, h in zip(groups, levels, strict=True) for i in group
    }
    final = fit_surface_heights(
        surface,
        frames,
        fixed=pins,
        budget=HeightBudget(
            time_limit_s=seconds,
            max_iterations=iterations,
            max_vertices=budget.max_vertices,
            max_faces=budget.max_faces,
        ),
    )
    report.update(
        fixed_levels=dict(final.report),
        valid=final.mesh is not None,
        reason=final.report.get("reason", ""),
        total_ms=(time.perf_counter() - began) * 1000,
    )
    return HeightFit(final.mesh, report), levels
