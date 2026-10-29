"""Original 2D UInt32 noise words for substrate, grains and litter queries."""

from dataclasses import dataclass

import numpy as np
from asphalt_field import cell_hash
from terrain_material_grid import frozen


def hash_words(cells):
    return cell_hash(cells) & np.uint32(0xFFFFFF)


def checked_at(at):
    points = np.asarray(at, dtype=np.float64)
    if (
        points.ndim != 2
        or points.shape[1] != 2
        or len(points) > 262144
        or not np.isfinite(points).all()
        or np.any(points < -(2**31) + 1)
        or np.any(points >= 2**31 - 2)
    ):
        raise ValueError("bounded finite normalized 2D material points required")
    return points


@dataclass(frozen=True, slots=True)
class WordPlane:
    first_xy: tuple[int, int]
    words_yx: np.ndarray

    @property
    def retained_bytes(self):
        return self.words_yx.nbytes

    def get(self, cells):
        cells = np.asarray(cells)
        if (
            cells.ndim != 2
            or cells.shape[1] != 2
            or len(cells) > 262144
            or cells.dtype.kind not in "iu"
            or np.any(cells < -(2**31))
            or np.any(cells >= 2**31)
        ):
            raise ValueError("bounded original integer sites required")
        local = cells.astype(np.int64, copy=False) - self.first_xy
        if np.any(local < 0) or np.any(local >= self.words_yx.shape[::-1]):
            raise ValueError("material query outside the prepared original plane")
        return self.words_yx[local[:, 1], local[:, 0]]


def prepare(bounds, *, word_limit=262144, halo=1):
    points = checked_at(bounds)
    if points.shape != (2, 2) or np.any(points[0] > points[1]):
        raise ValueError("ordered original material plane bounds required")
    if type(word_limit) is not int or not 1 <= word_limit <= 262144:
        raise ValueError("bounded original material word budget required")
    if type(halo) is not int or not 1 <= halo <= 3:
        raise ValueError("bounded explicit material kernel halo required")
    low, high = np.floor(points).astype(np.int64)
    first, size = low - halo, high - low + 2 * halo + 1
    if np.any(first < -(2**31)) or np.any(high + halo >= 2**31):
        raise ValueError(
            "kernel halo must remain in the original native integer lattice"
        )
    if np.prod(size, dtype=object) > word_limit:
        raise ValueError("original material plane word budget")
    y, x = np.indices(tuple(size[::-1]), dtype=np.int64)
    cells = np.column_stack((x.ravel(), y.ravel())) + first
    words = hash_words(cells).reshape(tuple(size[::-1]))
    return WordPlane(tuple(int(v) for v in first), frozen(words))


def noise(at, *, words=hash_words):
    points = checked_at(at)
    cell = np.floor(points).astype(np.int64)
    fraction = points - cell
    slope = 6 * fraction * (1 - fraction)
    f = fraction * fraction * (3 - 2 * fraction)
    a0, a1, b0, b1 = (
        words(cell + [x, y]).astype(np.float64) / 16777215
        for y in (0, 1)
        for x in (0, 1)
    )
    a, b = a0 * (1 - f[:, 0]) + a1 * f[:, 0], b0 * (1 - f[:, 0]) + b1 * f[:, 0]
    gradient = (
        np.column_stack(((a1 - a0) * (1 - f[:, 1]) + (b1 - b0) * f[:, 1], b - a))
        * slope
    )
    return a * (1 - f[:, 1]) + b * f[:, 1], gradient
