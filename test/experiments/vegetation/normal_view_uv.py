"""Move existing quad atlas UVs to independent view tiles without moving geometry."""

import numpy as np


def isolated_uv(uv, views):
    uv, views = np.asarray(uv, float), np.asarray(views)
    if (
        views.ndim != 1
        or views.dtype.kind not in "iu"
        or not 1 <= len(views) <= 80000
        or np.any(views < 0)
        or np.any(views >= 64)
        or uv.shape != (len(views) * 4, 2)
        or not np.isfinite(uv).all()
    ):
        raise ValueError("bounded complete quad UVs and64-view indices required")
    old_lower = np.c_[views % 8 * 40 + 4, views // 8 * 40 + 4]
    local = (uv.reshape(-1, 4, 2) * 320 - old_lower[:, None]) / 32
    if np.any(local < -1e-12) or np.any(local > 1 + 1e-12):
        raise ValueError("UVs must belong to the declared original chart")
    return (local + np.c_[views % 10, views // 10][:, None]).reshape(-1, 2)
