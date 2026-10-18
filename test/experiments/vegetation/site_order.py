"""Proposal priority only; no site is admitted or removed by XY distance."""

import numpy as np


def envelope_radius(bounds):
    bounds = np.asarray(bounds, dtype=float)
    if (
        bounds.shape != (2, 3)
        or not np.isfinite(bounds).all()
        or np.any(bounds[0] > bounds[1])
    ):
        raise ValueError("complete finite ordered prototype bounds required")
    xy = np.max(abs(bounds[:, :2]), axis=0)
    return float(np.nextafter(np.hypot(*xy), np.inf))


def priority(pool, bounds, mode):
    if mode not in ("nearest", "boundary"):
        raise ValueError("declared proposal order required")
    radius = envelope_radius(bounds)
    if mode == "nearest":
        return sorted(pool, key=lambda row: row[:3])
    return sorted(
        pool,
        key=lambda row: (
            row[0] < radius,
            abs(row[0] - radius),
            row[1],
            row[2],
        ),
    )
