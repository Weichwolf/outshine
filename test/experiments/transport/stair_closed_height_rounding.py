"""Bounded height contraction for standalone rings; actual D/Q limits still decide."""

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


def correct(vertices, faces, frames):
    heights = vertices[:, 2]
    level = float(np.float32((float(heights.min()) + float(heights.max())) / 2))
    trials = []
    for scale in SCALES:
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
    raise ValueError(
        "closed axis has no tested D/Q-valid height contraction within 5 mm"
    )


def verify(data, frames):
    raw = data["unrounded_closed_heights"]
    heights = data["surface_vertices"][:, 2]
    if (
        raw.dtype != np.dtype("f8")
        or raw.shape != heights.shape
        or not np.isfinite(raw).all()
    ):
        raise ValueError("complete original closed fit heights required")
    if float(abs(raw - heights).max()) > MAX_REPAIR_M:
        raise ValueError("closed height correction exceeds its 5 mm budget")
    before = data["surface_vertices"].copy()
    before[:, 2] = raw
    if normalized_grade(before, data["surface_faces"], frames, False) > 1:
        raise ValueError("original closed height fit was not valid in Double")
    return {
        "maximum_change_m": float(abs(raw - heights).max()),
        "budget_m": MAX_REPAIR_M,
    }
