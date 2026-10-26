"""Declared building vertex encodings in the original parent-offset frame."""

import numpy as np

RECIPES = ("float64", "snapped-f32")


def millimetres(values):
    """Model native round(value*1000)/1000, including ties away from zero."""
    values = np.asarray(values, dtype=np.float64)
    fractional, integral = np.modf(np.abs(values) * 1000.0)
    return np.copysign(integral + (fractional >= 0.5), values) / 1000.0


def building_positions(local, surface, *, recipe="float64"):
    origin, east, north, up = (
        np.asarray(surface[key]) for key in ("origin", "east", "north", "up")
    )
    if recipe == "float64":
        return origin + local @ np.asarray([east, north, up])
    if recipe != "snapped-f32":
        raise ValueError("explicit supported building vertex recipe required")
    values = millimetres(local)
    world = (
        origin
        + values[..., 0, None] * east
        + values[..., 1, None] * north
        + values[..., 2, None] * up
    )
    return world.astype(np.float32).astype(np.float64)
