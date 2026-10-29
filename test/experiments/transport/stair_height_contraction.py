"""Finite free-height contractions with independent Double/Float32 grade checks."""

import time

import numpy as np
from surface_height_checks import normal_gradients

MAX_REPAIR_M = 0.005
SCALES = (1.0, 0.99, 0.95, 0.9, 0.75, 0.5, 0.0)


def normalized_grade(vertices, faces, frames, cast):
    xyz = vertices.astype("f4").astype(float) if cast else vertices
    gradient = normal_gradients(xyz[:, :2], faces, xyz[:, 2])
    return float(
        np.linalg.norm(np.einsum("fij,fj->fi", frames, gradient), axis=1).max()
    )


def correct(vertices, faces, frames, *, seconds=None):
    if not np.isfinite(vertices).all() or np.max(abs(vertices)) > np.finfo("f4").max:
        raise ValueError("finite Float32-range height contraction required")
    if seconds is not None and (not np.isfinite(seconds) or not 0 <= seconds <= 8):
        raise ValueError(
            "finite remaining contraction budget within eight seconds required"
        )
    deadline = None if seconds is None else time.perf_counter() + seconds
    heights = vertices[:, 2]
    level = float(np.float32((float(heights.min()) + float(heights.max())) / 2))
    trials = []
    for scale in SCALES:
        if deadline is not None and time.perf_counter() >= deadline:
            raise ValueError("free height contraction time budget exhausted")
        candidate = vertices.copy()
        candidate[:, 2] = level + scale * (heights - level) if scale != 1 else heights
        change = float(abs(candidate[:, 2] - heights).max())
        if change > MAX_REPAIR_M:
            trials.append({"scale": scale, "reason": "height_repair_budget"})
            continue
        grades = [
            normalized_grade(candidate, faces, frames, cast) for cast in (False, True)
        ]
        trials.append({"scale": scale, "maximum_normalized_grades": grades})
        if all(np.isfinite(g) and g <= 1 for g in grades):
            return candidate, {
                "scale": scale,
                "level_m": level,
                "maximum_change_m": change,
                "maximum_change_budget_m": MAX_REPAIR_M,
                "trials": trials,
                "scope": "First valid of seven finite contractions, not an optimal quantized-height solution. No XY, width, profile limit or topology change.",
            }
    raise ValueError("free height fit has no tested D/Q-valid contraction within 5 mm")
