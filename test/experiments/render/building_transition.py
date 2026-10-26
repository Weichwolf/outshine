"""Convex complete-layer C/T transition; caller proves same source/view/Ready role."""

import numpy as np


def mix_layers(first, second, weight):
    if not np.isfinite(weight) or not 0 <= weight <= 1:
        raise ValueError("finite transition weight in [0,1] required")
    for endpoint in (first, second):
        c, t = endpoint["C"], endpoint["T"]
        if c.shape[:-1] != t.shape[:-1] or c.shape[-1] != 3 or t.shape[-1] != 1:
            raise ValueError("complete premultiplied C/T layer shapes required")
        if (
            not np.isfinite(c).all()
            or not np.isfinite(t).all()
            or np.any((t < 0) | (t > 1))
        ):
            raise ValueError("finite C and bounded T required")
    if first["C"].shape != second["C"].shape:
        raise ValueError("matching complete source/view domain required")
    return {key: (1 - weight) * first[key] + weight * second[key] for key in ("C", "T")}


def independent_fade_over(first, second, weight):
    """Negative control: independently attenuated overlapping complete layers."""
    a, b = 1 - weight, weight
    transmission = 1 - a * (1 - first["T"])
    return {
        "C": a * first["C"] + transmission * b * second["C"],
        "T": transmission * (1 - b * (1 - second["T"])),
    }
