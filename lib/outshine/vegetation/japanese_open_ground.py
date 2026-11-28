"""Open plant communities from the Japanese Actual Vegetation Map."""

import numpy as np
import shapely

from outshine.vegetation.atlas import CELLS_PER_DEGREE
from outshine.vegetation.plant_sites import patch_density

BOG = frozenset(("JP-MOE-2024:470100", "JP-MOE-2024:470200"))
SILVERGRASS = frozenset(("JP-MOE-2024:250200",))
OPEN_COMMUNITIES = BOG | SILVERGRASS


def community_types(atlas, source, xy):
    bog = np.zeros(len(xy), dtype=bool)
    grass = bog.copy()
    if OPEN_COMMUNITIES.intersection(getattr(atlas, "observation_indices", {})):
        lon, lat = (np.array([source.lon, source.lat]) + xy / source.metres).T
        indices, _ = atlas.observed_cells(lat, lon)
        bog = atlas.observation_mask(indices, BOG)
        grass = atlas.observation_mask(indices, SILVERGRASS)
    return bog, grass


def open_ground_geometry(atlas, source, reach):
    """Merge the atlas cells into a local exclusion for woodland placement."""
    origin = np.array([source.lon, source.lat])
    pitch = source.metres / CELLS_PER_DEGREE
    lower = np.floor((origin - reach / source.metres) * CELLS_PER_DEGREE)
    shape = np.ceil((origin + reach / source.metres) * CELLS_PER_DEGREE - lower)
    corner = (lower / CELLS_PER_DEGREE - origin) * source.metres
    width, height = shape.astype(int)
    centre = corner + pitch / 2
    east, north = np.meshgrid(
        centre[0] + np.arange(width) * pitch[0],
        centre[1] + np.arange(height) * pitch[1],
    )
    bog, grass = community_types(
        atlas, source, np.column_stack((east.ravel(), north.ravel()))
    )
    changes = np.diff(
        (bog | grass).reshape(height, width), axis=1, prepend=False, append=False
    )
    row, edge = np.nonzero(changes)
    x0 = corner[0] + edge[::2] * pitch[0]
    x1 = corner[0] + edge[1::2] * pitch[0]
    y0 = corner[1] + row[::2] * pitch[1]
    y1 = corner[1] + (row[::2] + 1) * pitch[1]
    return shapely.union_all(shapely.box(x0, y0, x1, y1))


def open_ground_colour(source, xy, bog, seed):
    carpet = patch_density(source, xy, seed ^ 0x1ACF, scale=2.2)
    hollows = patch_density(source, xy, seed ^ 0x8A72, scale=9.1)
    broad = patch_density(source, xy, seed ^ 0x652C, scale=32.7)
    moss = np.array([0.117, 0.148, 0.046])
    peat = np.array([0.041, 0.030, 0.020])
    roots = np.array([0.132, 0.120, 0.061])
    grass = np.array([0.112, 0.179, 0.059])
    damp = np.clip(0.65 * carpet + 0.55 * hollows, 0, 1)
    bog_rgb = peat + (moss - peat) * (1 - damp[:, None] ** 1.4)
    grass_rgb = roots + (grass - roots) * (0.25 + 0.75 * carpet[:, None])
    return np.where(bog[:, None], bog_rgb, grass_rgb) * (0.84 + 0.30 * broad[:, None])
