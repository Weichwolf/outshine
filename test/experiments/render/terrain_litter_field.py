"""Immutable scalar litter field; original material composition and lighting remain."""

from dataclasses import dataclass

import numpy as np
from terrain_material_grid import frozen, interpolate
from terrain_material_plane import checked_at, noise


@dataclass(frozen=True, slots=True)
class LitterField:
    origin_m: tuple[float, float]
    extent_m: float
    scale_m: float
    levels: tuple[np.ndarray, ...]

    @property
    def retained_bytes(self):
        return sum(level.nbytes for level in self.levels)


def bake(origin_m, scale_m, *, cells=32, side=512, dtype="float16", evaluate=noise):
    origin, storage = np.asarray(origin_m, dtype=float), np.dtype(dtype)
    if (
        origin.shape != (2,)
        or not np.isfinite(origin).all()
        or not np.isfinite(scale_m)
        or scale_m <= 0
        or type(cells) is not int
        or not 4 <= cells <= 64
        or type(side) is not int
        or not 16 <= side <= 512
        or side & (side - 1)
        or storage not in (np.dtype("uint8"), np.dtype("float16"), np.dtype("float32"))
    ):
        raise ValueError("bounded metric scalar tile and dyadic resolution required")
    extent = cells * scale_m
    axis = (np.arange(side) + 0.5) * extent / side
    x, y = np.meshgrid(axis, axis)
    at = checked_at(origin + np.column_stack((x.ravel(), y.ravel())))
    values = np.asarray(evaluate(at / scale_m)[0], dtype=float) - 0.5
    if (
        values.shape != (side * side,)
        or not np.isfinite(values).all()
        or np.any(
            np.abs(values)
            > (0.5 if storage == np.dtype("uint8") else np.finfo(storage).max)
        )
    ):
        raise ValueError("finite scalar source field required")
    values = values.reshape(side, side)

    def encode(scalar):
        packed = (
            np.rint((scalar + 0.5) * 255) if storage == np.dtype("uint8") else scalar
        )
        return frozen(packed.astype(storage))

    levels = [encode(values)]
    width = side
    while width > 1:
        width //= 2
        values = values.reshape(width, 2, width, 2).mean(axis=(1, 3))
        levels.append(encode(values))
    return LitterField(tuple(origin), extent, scale_m, tuple(levels))


def _trilinear(field, at, lod):
    first = np.floor(lod).astype(int)
    fraction = lod - first
    result = np.zeros(len(at))
    for delta in (0, 1):
        levels = np.minimum(first + delta, len(field.levels) - 1)
        amount = 1 - fraction if delta == 0 else fraction
        for index in np.unique(levels[amount > 0]):
            selected = (levels == index) & (amount > 0)
            values = field.levels[index]
            width, cell = len(values), field.extent_m / len(values)
            if width == 1:
                scalar = values[0, 0]
            else:
                scalar = interpolate(
                    values[..., None],
                    ((width - 1) * cell,) * 2,
                    at[selected] - field.origin_m - cell / 2,
                )[:, 0]
            if values.dtype == np.dtype("uint8"):
                scalar = scalar / 255 - 0.5
            result[selected] += scalar * amount[selected]
    return result


def sample(field, at_m, dx_m, dy_m, *, taps=8, lod_bias=-0.5):
    """Bounded ellipse-axis approximation of the original nine-point mask filter."""
    at = checked_at(at_m)
    dx, dy = np.asarray(dx_m, dtype=float), np.asarray(dy_m, dtype=float)
    if (
        not isinstance(field, LitterField)
        or dx.shape != at.shape
        or dy.shape != at.shape
        or not np.isfinite((dx, dy)).all()
        or type(taps) is not int
        or not 1 <= taps <= 16
        or not np.isfinite(lod_bias)
    ):
        raise ValueError(
            "bounded finite material queries and anisotropic taps required"
        )
    footprint = np.maximum(np.linalg.norm(dx, axis=-1), np.linalg.norm(dy, axis=-1))
    result = np.zeros(len(at))
    active = footprint < 2 * field.scale_m
    if not np.any(active):
        return result
    local, horizontal, vertical = at[active], dx[active], dy[active]
    reach = (np.abs(horizontal) + np.abs(vertical)) / 2
    if np.any(local - reach < field.origin_m) or np.any(
        local + reach > np.asarray(field.origin_m) + field.extent_m
    ):
        raise ValueError("active footprint exceeds resident scalar tile")
    axes, lengths, _ = np.linalg.svd(np.stack((horizontal, vertical), axis=-1))
    aspect = np.divide(
        lengths[:, 0],
        lengths[:, 1],
        out=np.full(len(local), float(taps)),
        where=lengths[:, 1] > 0,
    )
    count = np.ceil(np.clip(aspect, 1, taps)).astype(int)
    count[lengths[:, 0] <= field.scale_m * 0.01] = 1
    cell = field.extent_m / len(field.levels[0])
    minor = np.maximum(lengths[:, 1], lengths[:, 0] / count)
    span = np.sqrt(np.maximum(lengths[:, 0] ** 2 - minor**2, 0))
    lod = np.clip(
        np.log2(np.maximum(minor / cell, 1)) + lod_bias, 0, len(field.levels) - 1
    )
    filtered = np.zeros(len(local))
    for index in range(int(count.max())):
        selected = index < count
        offset = (index + 0.5) / count[selected] - 0.5
        point = local[selected] + offset[:, None] * (
            axes[selected, :, 0] * span[selected, None]
        )
        filtered[selected] += _trilinear(field, point, lod[selected]) / count[selected]
    t = np.clip(footprint[active] / field.scale_m - 1, 0, 1)
    result[active] = filtered * (1 - t * t * (3 - 2 * t))
    return result
