"""Bounded owned surface fitting; sources and complete support selection stay external."""

import time
from numbers import Real

import numpy as np

from ...contracts import Mesh
from ..heights import HeightBudget, HeightFit, _gradient_weights
from ._checks import conservative_candidate, inspect
from ._solve import solve

DEFAULT_BUDGET = HeightBudget()


def absent(report, began, reason):
    return HeightFit(
        None,
        {
            **report,
            "valid": False,
            "reason": reason,
            "total_ms": (time.perf_counter() - began) * 1000,
        },
    )


def checked_inputs(surface, grade_frames, weights, fixed_vertices):
    count, face_count = len(surface.vertices), len(surface.triangles)
    frames = np.array(grade_frames, dtype=float, copy=True)
    if (
        not face_count
        or frames.shape != (face_count, 2, 2)
        or not np.isfinite(frames).all()
    ):
        raise ValueError("finite 2x2 grade frames per nonempty face required")
    with np.errstate(over="ignore", invalid="ignore"):
        determinant = np.linalg.det(frames)
    if not np.isfinite(determinant).all() or np.any(determinant == 0):
        raise ValueError("nonsingular finite grade frames required")
    values = (
        np.ones(count) if weights is None else np.array(weights, dtype=float, copy=True)
    )
    if values.shape != (count,) or not np.isfinite(values).all() or np.any(values <= 0):
        raise ValueError("one finite positive objective weight per vertex required")
    with np.errstate(over="ignore", invalid="ignore"):
        total = values.sum()
        objective = values * (
            surface.vertices[:, 2] - np.median(surface.vertices[:, 2])
        )
    if not np.isfinite(total) or not np.isfinite(objective).all():
        raise ValueError("finite representable weighted objective required")
    fixed = np.asarray(fixed_vertices)
    if fixed.ndim != 1 or (fixed.size and fixed.dtype.kind not in "iu"):
        raise ValueError("one-dimensional integer contact vertex IDs required")
    if fixed.size and (fixed.min() < 0 or fixed.max() >= count):
        raise ValueError("contact vertices must belong to the chosen surface")
    fixed = np.unique(fixed).astype(np.intp)
    with np.errstate(over="ignore", invalid="ignore", divide="ignore"):
        coefficients = _gradient_weights(surface.vertices[:, :2], surface.triangles)
    if not np.isfinite(coefficients).all():
        raise ValueError(
            "finite nondegenerate projected gradient coefficients required"
        )
    return frames, values, fixed, coefficients


def fit_weighted_heights(
    surface: Mesh,
    grade_frames,
    *,
    reference_datum_m: float,
    weights=None,
    fixed_vertices=(),
    budget: HeightBudget = DEFAULT_BUDGET,
) -> HeightFit:
    """Fit priors with whole-face bounds and contacts on an explicit flat reference.

    Surface XYZ stores local metric XY and height priors; topology remains exact.
    Fixed vertex heights equal reference_datum_m, not their possibly different
    prior heights. The caller owns frame/source/kind and complete support choice.
    Weights are positive vertex quadrature weights (default uniform). The result
    is an owned immutable C0 Mesh, or explained absence. A feasible prior is
    returned directly. Solver status never replaces whole-face/contact checks.
    """
    began = time.perf_counter()
    if not isinstance(surface, Mesh) or not isinstance(budget, HeightBudget):
        raise TypeError("owned Mesh and HeightBudget required")
    if (
        not isinstance(reference_datum_m, Real)
        or isinstance(reference_datum_m, bool)
        or not np.isfinite(reference_datum_m)
    ):
        raise ValueError("explicit finite scalar flat reference datum required")
    count, faces = len(surface.vertices), surface.triangles
    if count > budget.max_vertices or len(faces) > budget.max_faces:
        return absent({}, began, "input_budget_exhausted")
    if budget.time_limit_s == 0:
        return absent({}, began, "time_budget_exhausted")
    frames, values, fixed, coefficients = checked_inputs(
        surface, grade_frames, weights, fixed_vertices
    )
    target = surface.vertices[:, 2]
    with np.errstate(over="ignore", invalid="ignore"):
        squared_span = (abs(target) + abs(reference_datum_m)) ** 2
        weighted_span = np.sum(values * squared_span)
    if not np.isfinite(squared_span).all() or not np.isfinite(weighted_span):
        raise ValueError("finite representable height-error scale required")
    prior = inspect(
        coefficients, faces, frames, target, fixed, reference_datum_m, target, values
    )
    if not all(np.isfinite(v) for v in prior.values()):
        raise ValueError("finite representable normalized surface gradients required")
    if prior["maximum_normalized_grade"] <= 1 + 1e-7 and np.all(
        target[fixed] == reference_datum_m
    ):
        return HeightFit(
            surface,
            {
                **prior,
                "valid": True,
                "reason": "",
                "solver_status": "prior_feasible",
                "iterations": 0,
                "total_ms": (time.perf_counter() - began) * 1000,
            },
        )
    heights, report = solve(
        faces, target, frames, coefficients, fixed, reference_datum_m, values, budget
    )
    report.update(
        valid=False,
        reason="no_valid_whole_surface",
        solver_time_limit_s=budget.time_limit_s,
    )
    reference_rms = float(
        np.sqrt(np.average((target - reference_datum_m) ** 2, weights=values))
    )
    report["reference_weighted_rms_m"] = reference_rms
    if heights is not None and heights.shape == target.shape:
        heights, repair = conservative_candidate(
            heights, coefficients, faces, frames, fixed, reference_datum_m
        )
        report["conservative_candidate"] = repair
        if heights is None:
            report["reason"] = repair["reason"]
        if heights is not None:
            check = inspect(
                coefficients,
                faces,
                frames,
                heights,
                fixed,
                reference_datum_m,
                target,
                values,
            )
            if not all(np.isfinite(v) for v in check.values()):
                return absent(report, began, "nonfinite_whole_face_check")
            report["whole_face_check"] = check
            if (
                check["maximum_normalized_grade"] <= 1 + 1e-7
                and check["maximum_pin_error_m"] <= 1e-7
            ):
                if check["weighted_dem_rms_m"] > reference_rms * (1 + 1e-12) + 1e-12:
                    return absent(report, began, "objective_worse_than_flat_reference")
                vertices = surface.vertices.copy()
                vertices[:, 2] = heights
                mesh = Mesh(vertices, faces)
                report.update(
                    valid=True, reason="", total_ms=(time.perf_counter() - began) * 1000
                )
                return HeightFit(mesh, report)
    return absent(report, began, report["reason"])
