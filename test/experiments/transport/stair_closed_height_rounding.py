"""Independent budget/grade witness for the existing closed seed contraction."""

import numpy as np
from stair_height_contraction import MAX_REPAIR_M, normalized_grade


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
