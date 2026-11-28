"""Seeded, wind-advected cloud fields for the Vienna sky reference."""

from dataclasses import dataclass
from functools import cache

import numpy as np
from scipy.ndimage import gaussian_filter

RECIPE_VERSION = 1
WIND_KM_S = np.array((0.007, 0.0035))
KM_PER_DEGREE = (74.55, 111.18)
SUN_MAP_SIZE = 1024
SUN_MAP_RADIUS_KM = 32.0


@dataclass(frozen=True)
class CloudProfile:
    kind: int
    layer: tuple[float, float, float, float]
    origin_km: tuple[float, float]
    seed: int

    @property
    def archive_key(self):
        return (RECIPE_VERSION, self.kind, self.seed, *self.layer, *self.origin_km)

    def sun_map_origin(self, eye, datum_m):
        origin = np.array(eye, dtype=float)
        # The corners of the tangent plane also need to start below the layer.
        origin[2] = min(origin[2], self.layer[0] * 1000 - datum_m - 250)
        return origin


def cloud_profile(weather, longitude, latitude, eye_xy, elapsed_s, seed):
    settings = {
        "broken-cloud": (1, (1.45, 2.95, 0.58, 5.5)),
        "overcast": (2, (0.90, 1.36, 0.37, 18.0)),
    }.get(weather)
    if settings is None:
        return None
    site = np.asarray((longitude - 16, latitude - 48)) * KM_PER_DEGREE
    origin = site + np.asarray(eye_xy) * 0.001 - WIND_KM_S * elapsed_s
    return CloudProfile(*settings, tuple(origin), seed)


@cache
def cloud_noise(seed):
    """Create three repeatable, band-limited octave fields in a packed 3D texture."""
    rng = np.random.default_rng(seed)
    fields = []
    for deviation in (3.1, 2.0, 1.25):
        smooth = gaussian_filter(
            rng.standard_normal((64, 64, 64)), deviation, mode="wrap"
        )
        fields.append(np.clip(0.5 + 0.18 * smooth / smooth.std(), 0, 1))
    return np.stack(fields, axis=-1).astype("f4")
