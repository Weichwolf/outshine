"""Fixed-footprint material-block candidate; no class/rock blend or NDF guarantee."""

from dataclasses import dataclass

import numpy as np
from terrain_material_grid import frozen, interpolate
from terrain_material_plane import checked_at


@dataclass(frozen=True, slots=True)
class MaterialBasis:
    values: np.ndarray
    origin_m: tuple[float, float]
    extent_m: float
    footprint_m: float

    @property
    def retained_bytes(self):
        return self.values.nbytes


def bake(evaluate, origin_m, extent_m, footprint_m, side, *, dtype="float32"):
    """Cache raw RGB/roughness and metric gradients, before clamp and lighting."""
    origin = np.asarray(origin_m, dtype=float)
    storage = np.dtype(dtype)
    if (
        origin.shape != (2,)
        or not np.isfinite(origin).all()
        or not np.isfinite((extent_m, footprint_m)).all()
        or min(extent_m, footprint_m) <= 0
        or type(side) is not int
        or not 2 <= side <= 257
        or storage not in (np.dtype("float16"), np.dtype("float32"))
    ):
        raise ValueError("bounded metric block and Float16/32 storage required")
    x, y = np.meshgrid(*[np.linspace(0, extent_m, side)] * 2)
    at = checked_at(origin + np.column_stack((x.ravel(), y.ravel())))
    dx, dy = (
        np.tile([footprint_m, 0], (len(at), 1)),
        np.tile([0, footprint_m], (len(at), 1)),
    )
    values = np.array(evaluate(at, dx, dy), dtype=float, copy=True)
    if values.shape != (len(at), 6) or not np.isfinite(values).all():
        raise ValueError("six finite raw material components required")
    values[:, 4:] /= footprint_m
    if np.any(np.abs(values) > np.finfo(storage).max):
        raise ValueError("raw material exceeds storage range")
    values = values.astype(storage).reshape(side, side, 6)
    return MaterialBasis(frozen(values), tuple(origin), extent_m, footprint_m)


def sample(basis, at, dx, dy):
    at = checked_at(at)
    dx, dy = np.asarray(dx, dtype=float), np.asarray(dy, dtype=float)
    if (
        not isinstance(basis, MaterialBasis)
        or dx.shape != at.shape
        or dy.shape != at.shape
        or not np.isfinite(dx).all()
        or not np.isfinite(dy).all()
        or not np.allclose(
            np.maximum(np.linalg.norm(dx, axis=-1), np.linalg.norm(dy, axis=-1)),
            basis.footprint_m,
            rtol=1e-12,
            atol=0,
        )
    ):
        raise ValueError(
            "matching bounded footprint level and metric derivatives required"
        )
    raw = interpolate(
        basis.values, (basis.extent_m,) * 2, at - np.asarray(basis.origin_m)
    )
    gradient = raw[:, 4:].copy()
    raw[:, 4] = np.sum(gradient * dx, axis=-1)
    raw[:, 5] = np.sum(gradient * dy, axis=-1)
    return raw
