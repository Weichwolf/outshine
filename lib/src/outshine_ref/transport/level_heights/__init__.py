"""Bounded shared flat levels with actual Float32 position/gradient checks."""

import time
from collections.abc import Sequence

import numpy as np

from ...contracts import Mesh
from ..heights import HeightBudget, HeightFit, _face_gradients
from ._continuous import fit_groups
from ._input import checked_frames, checked_groups
from ._rounding import repair

_DEFAULT_BUDGET = HeightBudget()


def fit_level_heights(
    surface: Mesh,
    grade_frames: np.ndarray,
    level_groups: Sequence[Sequence[int]],
    *,
    budget: HeightBudget = _DEFAULT_BUDGET,
) -> HeightFit:
    """Fit shared flat groups and return Float32-exact positions in an owned Mesh.

    Vertex groups are disjoint explicit lists/tuples of at least three integer IDs;
    the function does not infer geometric/source contacts. XY must already be
    exactly representable in Float32. The caller owns the common coordinate frame,
    conforming nonoverlapping topology and complete physical/source validation.

    Limits: 20 groups, 4096 vertices/8192 faces, eight solver seconds and 100 cone
    iterations shared by two fits. Remaining time and at most 100 adjacent-height
    proposals bound repair within 5 mm of the continuous result. No hidden fallback
    or general convergence/optimality guarantee. Budget/unsupported representation
    or missing valid candidate returns absence. Invalid typed contracts raise.
    Timing is checked between bounded solver/search steps, not a hard frame limit.
    Existing HeightBudget/HeightFit and root transport imports are unchanged.
    """
    began = time.perf_counter()
    if not isinstance(surface, Mesh) or not isinstance(budget, HeightBudget):
        raise TypeError("owned Mesh and explicit HeightBudget required")
    if budget.time_limit_s > 8 or budget.max_iterations > 100:
        raise ValueError("level fit supports at most eight seconds/100 iterations")
    reason = ""
    if len(surface.vertices) > min(budget.max_vertices, 4096) or len(
        surface.triangles
    ) > min(budget.max_faces, 8192):
        reason = "input_budget_exhausted"
    elif budget.time_limit_s == 0:
        reason = "time_budget_exhausted"
    elif np.max(abs(surface.vertices[:, 2]), initial=0) > np.finfo("f4").max:
        reason = "height_outside_Float32"
    elif not np.array_equal(
        surface.vertices[:, :2], surface.vertices[:, :2].astype("f4").astype(float)
    ):
        reason = "source_XY_not_exact_Float32"
    if reason:
        return HeightFit(None, {"valid": False, "reason": reason})
    groups = checked_groups(level_groups, len(surface.vertices))
    frames = checked_frames(grade_frames, len(surface.triangles))
    target = surface.vertices[:, 2]
    if np.array_equal(target, target.astype("f4").astype(float)) and all(
        np.ptp(target[group]) == 0 for group in groups
    ):
        gradient = _face_gradients(surface.vertices[:, :2], surface.triangles, target)
        maximum = float(
            np.linalg.norm(np.einsum("fij,fj->fi", frames, gradient), axis=1).max()
        )
        if np.isfinite(maximum) and maximum <= 1:
            return HeightFit(
                surface,
                {
                    "valid": True,
                    "reason": "",
                    "solver_status": "prior_feasible",
                    "iterations": 0,
                    "maximum_normalized_grade": maximum,
                    "rounding": {
                        "valid": True,
                        "reason": "",
                        "candidates": 0,
                        "maximum_change_m": 0.0,
                    },
                    "total_ms": (time.perf_counter() - began) * 1000,
                },
            )
    result, levels = fit_groups(surface, frames, groups, budget)
    if result.mesh is None:
        return result
    remaining = max(0.0, budget.time_limit_s - (time.perf_counter() - began))
    mesh, proof = repair(result.mesh, frames, groups, levels, seconds=remaining)
    return HeightFit(
        mesh,
        {
            "method": "Joint cone with finite Float32 repair",
            "continuous": dict(result.report),
            "rounding": proof,
            "valid": mesh is not None,
            "reason": proof["reason"],
            "total_ms": (time.perf_counter() - began) * 1000,
        },
    )
