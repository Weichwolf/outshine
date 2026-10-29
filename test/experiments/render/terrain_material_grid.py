"""Bounded plane diagnostic for raw noise interpolation, not a terrain asset format."""

from dataclasses import dataclass

import numpy as np
from terrain_material_noise import raw_noise


def frozen(value):
    return np.frombuffer(value.tobytes(), dtype=value.dtype).reshape(value.shape)


@dataclass(frozen=True, slots=True)
class NoiseGrid:
    values: np.ndarray
    extent_uv_m: tuple[float, float]

    @property
    def retained_bytes(self):
        return self.values.nbytes


def bake(origin_m, tangent_u, tangent_v, extent_uv_m, shape, matrix, wavelength_m):
    """Four Float32 raw components on an explicit diagnostic plane.

    Geometry/recipe binding and spatial admission are outside this experiment.
    Grid bounds include their endpoints; there is no wrap/clamp/hidden fallback.
    """
    frame = np.asarray((origin_m, tangent_u, tangent_v), dtype=np.float64)
    if (
        len(shape) != 2
        or any(type(v) is not int or not 2 <= v <= 512 for v in shape)
        or frame.shape != (3, 3)
        or not np.isfinite(frame).all()
        or len(extent_uv_m) != 2
        or not all(np.isfinite(v) and v > 0 for v in extent_uv_m)
        or np.linalg.norm(np.cross(frame[1], frame[2])) <= 1e-12
    ):
        raise ValueError(
            "bounded independent plane frame, metric extents and grid required"
        )
    u, v = np.meshgrid(
        np.linspace(0, extent_uv_m[0], shape[1]),
        np.linspace(0, extent_uv_m[1], shape[0]),
    )
    world = frame[0] + u[..., None] * frame[1] + v[..., None] * frame[2]
    values = raw_noise(world, matrix, wavelength_m).astype(np.float32)
    return NoiseGrid(frozen(values), tuple(extent_uv_m))


def sample(grid, uv_m):
    uv = np.asarray(uv_m, dtype=np.float64)
    if (
        not isinstance(grid, NoiseGrid)
        or uv.ndim < 2
        or uv.shape[-1] != 2
        or uv.size > 2 * 262144
        or not np.isfinite(uv).all()
        or np.any(uv < 0)
        or np.any(uv > grid.extent_uv_m)
    ):
        raise ValueError("bounded in-grid metric queries required")
    size = np.asarray(grid.values.shape[1::-1])
    at = uv / grid.extent_uv_m * (size - 1)
    low = np.minimum(np.floor(at).astype(np.int64), size - 2)
    fraction = at - low
    a = grid.values[low[..., 1], low[..., 0]].astype(np.float64)
    b = grid.values[low[..., 1], low[..., 0] + 1].astype(np.float64)
    c = grid.values[low[..., 1] + 1, low[..., 0]].astype(np.float64)
    d = grid.values[low[..., 1] + 1, low[..., 0] + 1].astype(np.float64)
    horizontal = fraction[..., :1]
    return (a + (b - a) * horizontal) * (1 - fraction[..., 1:]) + (
        c + (d - c) * horizontal
    ) * fraction[..., 1:]
