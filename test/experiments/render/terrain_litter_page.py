"""Direct canonical R8 pages; one bilinear gutter, doubling world coverage per mip."""

import numpy as np
from terrain_litter_mip import unorm8
from terrain_material_plane import hash_words


def bake_page(tile, level, *, core_texels=64, words=hash_words):
    tile = np.asarray(tile)
    if (
        tile.shape != (2,)
        or not np.issubdtype(tile.dtype, np.integer)
        or np.any(tile < -(2**31))
        or np.any(tile >= 2**31)
        or core_texels not in (64, 128)
        or type(level) is not int
        or not 0 <= level <= 5
    ):
        raise ValueError("native page tile, core and sampler mip range required")
    tile = tile.astype(np.int64)
    root = np.floor_divide(tile, 1 << (5 - level))
    if np.any(root * core_texels * 2 - 2 < -(2**31)) or np.any(
        root * core_texels * 2 + core_texels * 2 + 2 >= 2**31
    ):
        raise ValueError("complete spatial parent chain exceeds the noise lattice")
    side = core_texels + 2
    if level <= 4:
        count = 1 << level
        first = tile * core_texels * count // 16 - 1
        lattice_side = core_texels * count // 16 + 3
    else:
        count = 2
        first = (tile * core_texels - 1) * count
        lattice_side = side * count + 1
    x, y = np.meshgrid(np.arange(lattice_side), np.arange(lattice_side))
    sites = first + np.column_stack((x.ravel(), y.ravel()))
    lattice = np.asarray(words(sites), dtype=np.uint64).reshape(
        lattice_side, lattice_side
    )
    if level <= 4:
        axis = (np.arange(side) - 1) * count
        cells = np.floor_divide(axis, 16) + 1
        smooth = []
        for start in axis % 16:
            odd = 2 * np.arange(start, start + count) + 1
            smooth.append(int(np.sum(48 * odd**2 - odd**3)) // count)
        smooth = np.asarray(smooth, dtype=np.uint64)
        inverse = 16384 - smooth
        numerator = (
            lattice[cells[:, None], cells] * inverse[:, None] * inverse
            + lattice[cells[:, None], cells + 1] * inverse[:, None] * smooth
            + lattice[cells[:, None] + 1, cells] * smooth[:, None] * inverse
            + lattice[cells[:, None] + 1, cells + 1] * smooth[:, None] * smooth
        )
        return unorm8(numerator, 16384**2 * 65793)
    cells = lattice[:-1, :-1] + lattice[1:, :-1] + lattice[:-1, 1:] + lattice[1:, 1:]
    prefix = np.pad(cells.cumsum(axis=0).cumsum(axis=1), ((1, 0), (1, 0)))
    lo = np.arange(side) * count
    hi = lo + count
    numerator = (prefix[hi[:, None], hi] - prefix[lo[:, None], hi]) - (
        prefix[hi[:, None], lo] - prefix[lo[:, None], lo]
    )
    return unorm8(numerator, 4 * count**2 * 65793)
