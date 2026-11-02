"""Generate only a requested R8 mip using exact discrete cubic moments."""

import numpy as np
from terrain_material_grid import frozen
from terrain_material_plane import hash_words


def unorm8(numerator, denominator):
    """Exact nearest-even quantization; the 24-bit lattice denominator includes 255."""
    quotient, remainder = np.divmod(numerator, np.uint64(denominator))
    up = (2 * remainder > denominator) | (
        (2 * remainder == denominator) & ((quotient & 1) != 0)
    )
    return frozen((quotient + up).astype(np.uint8))


def bake_mip(tile, level, *, words=hash_words):
    address = np.asarray(tile)
    if (
        address.shape != (2,)
        or not np.issubdtype(address.dtype, np.integer)
        or np.any(address < -(2**27) + 1)
        or np.any(address > 2**27 - 2)
        or type(level) is not int
        or not 0 <= level <= 9
    ):
        raise ValueError("bounded integer tile and resident mip level required")
    x, y = np.meshgrid(np.arange(33), np.arange(33))
    sites = address.astype(np.int64) * 16 - 8 + np.column_stack((x.ravel(), y.ravel()))
    lattice = np.asarray(words(sites), dtype=np.uint64).reshape(33, 33)
    side = 512 >> level
    if level <= 4:
        count = 1 << level
        weights = []
        for first in range(0, 16, count):
            odd = 2 * np.arange(first, first + count) + 1
            weights.append(int(np.sum(48 * odd**2 - odd**3)) // count)
        axis = np.arange(side) * count
        cell = axis // 16
        smooth = np.asarray(weights, dtype=np.uint64)[(axis % 16) // count]
        inverse = 16384 - smooth
        numerator = (
            lattice[cell[:, None], cell] * inverse[:, None] * inverse
            + lattice[cell[:, None], cell + 1] * inverse[:, None] * smooth
            + lattice[cell[:, None] + 1, cell] * smooth[:, None] * inverse
            + lattice[cell[:, None] + 1, cell + 1] * smooth[:, None] * smooth
        )
        return unorm8(numerator, 16384**2 * 65793)
    cells = lattice[:-1, :-1] + lattice[1:, :-1] + lattice[:-1, 1:] + lattice[1:, 1:]
    prefix = np.pad(cells.cumsum(axis=0).cumsum(axis=1), ((1, 0), (1, 0)))
    count = 1 << (level - 4)
    low = np.arange(side) * count
    high = low + count
    numerator = (prefix[high[:, None], high] - prefix[low[:, None], high]) - (
        prefix[high[:, None], low] - prefix[low[:, None], low]
    )
    return unorm8(numerator, 4 * count * count * 65793)
