"""Axial ring schedules; profile schedules retain the radial maximum explicitly."""

import numpy as np


def axial_parameters(segments, taper_start, sampling="uniform"):
    if type(segments) is not int or not 2 <= segments <= 128:
        raise ValueError("two to 128 axial needle segments required")
    if not np.isfinite(taper_start) or not 0.25 <= taper_start <= 0.875:
        raise ValueError("bounded physical needle taper maximum required")
    if sampling == "uniform":
        return np.arange(segments) / segments
    if sampling != "profile":
        raise ValueError("uniform or profile needle ring schedule required")
    body = max(1, segments // 2)
    tip = segments - body
    return np.r_[
        np.arange(body) * (taper_start / body),
        taper_start + np.arange(tip) * ((1 - taper_start) / tip),
    ]
