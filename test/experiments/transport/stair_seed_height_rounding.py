"""Repair only previously invalid stored heights of one unpinned open seed."""

import numpy as np
from stair_height_contraction import MAX_REPAIR_M, correct, normalized_grade


def free_seed(rows, plans, fixed):
    return (
        len(rows) == 1
        and not plans
        and not fixed.any()
        and not np.array_equal(rows[0]["xy"][0], rows[0]["xy"][-1])
    )


def repair(vertices, faces, frames, *, seconds):
    if not np.isfinite(seconds) or not 0 <= seconds <= 8:
        raise ValueError(
            "finite remaining free seed budget within eight seconds required"
        )
    if not np.isfinite(vertices).all() or np.max(abs(vertices)) > np.finfo("f4").max:
        raise ValueError("finite Float32-range free seed required")
    stored_grade = normalized_grade(vertices, faces, frames, True)
    if np.isfinite(stored_grade) and stored_grade <= 1:
        return vertices, None
    if not np.array_equal(vertices[:, :2], vertices[:, :2].astype("f4").astype(float)):
        raise ValueError("stored height repair requires exact Float32 XY")
    raw_grade = normalized_grade(vertices, faces, frames, False)
    if not np.isfinite(raw_grade) or raw_grade > 1:
        raise ValueError("stored height repair requires a valid original Double fit")
    return correct(vertices, faces, frames, seconds=seconds)


def verify(data, frames, rows, plans):
    if not free_seed(rows, plans, data["fixed"]):
        raise ValueError("height witness belongs only to one unpinned open seed")
    vertices = data["surface_vertices"]
    raw = data["unrounded_seed_heights"]
    if (
        raw.dtype != np.dtype("f8")
        or raw.shape != (len(vertices),)
        or not np.isfinite(raw).all()
    ):
        raise ValueError("complete original Double seed heights required")
    if not np.array_equal(vertices[:, :2], vertices[:, :2].astype("f4").astype(float)):
        raise ValueError("height witness requires unchanged exact Float32 XY")
    original = vertices.copy()
    original[:, 2] = raw
    original_grade = normalized_grade(original, data["surface_faces"], frames, False)
    stored_grade = normalized_grade(original, data["surface_faces"], frames, True)
    if (
        not np.isfinite(original_grade)
        or original_grade > 1
        or not np.isfinite(stored_grade)
        or stored_grade <= 1
    ):
        raise ValueError(
            "witness must be a valid Double fit with invalid stored heights"
        )
    change = float(abs(vertices[:, 2] - raw).max())
    if not np.isfinite(change) or change > MAX_REPAIR_M:
        raise ValueError("free seed height contraction exceeds its 5 mm budget")
    grades = [
        normalized_grade(vertices, data["surface_faces"], frames, cast)
        for cast in (False, True)
    ]
    if not all(np.isfinite(g) and g <= 1 for g in grades):
        raise ValueError("repaired free seed violates actual D/Q profile bounds")
    return {
        "maximum_change_m": change,
        "budget_m": MAX_REPAIR_M,
        "original_normalized_grades": [original_grade, stored_grade],
        "maximum_normalized_grades": grades,
    }
