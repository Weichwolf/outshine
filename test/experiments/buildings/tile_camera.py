"""Center complete source bounds with the fixed demand-model projection."""

import numpy as np
from demand import View


def camera(points):
    points = np.asarray(points, dtype=float)
    if (
        points.ndim != 2
        or points.shape[1] != 3
        or not len(points)
        or not np.isfinite(points).all()
    ):
        raise ValueError("Whole-tile camera requires finite XYZ bounds")
    target = points.min(axis=0) / 2 + points.max(axis=0) / 2
    direction = np.array([1.0, -1.25, 0.95])
    direction /= np.linalg.norm(direction)
    view = View(target + direction, target, width=3840, height=2160, fov_deg=48.0)
    projected = (points - target) @ view.basis.T
    limits = np.array([view.width * 0.46, view.height * 0.43]) / view.focal_px
    lower = (projected[:, :2] - projected[:, 2, None] * limits).max(axis=0)
    upper = (projected[:, :2] + projected[:, 2, None] * limits).min(axis=0)
    required = max(
        0.1,
        0.1 - float(projected[:, 2].min()),
        float(((lower - upper) / (2 * limits)).max()),
    )
    shift = ((lower + upper) / 2) @ view.basis[:2]
    target = target + shift
    return View(
        target + direction * (required + 10.0),
        target,
        width=view.width,
        height=view.height,
        fov_deg=48.0,
    )
