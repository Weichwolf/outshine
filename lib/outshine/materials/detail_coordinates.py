"""Continuous, world-anchored sampling of tileable natural-material channels."""

from dataclasses import dataclass

import numpy as np

from outshine.materials.aeolian_sand import SAND_DETAIL, TREAD_DETAIL

METRES_PER_DEGREE = np.array([111319.49, 111132.0])
WAVES = np.array(
    [[0.117, 0.405, 0.141], [0.369, -0.155, 0.245], [-0.274, 0.204, 0.363]]
)
WAVE_STRETCH = (1.0, 7.79)
WARP_METRES = np.array([0.87, 1.04, 0.72])
VARIED_DETAILS = frozenset(
    (
        "soil",
        "sandy clay loam",
        "gravelly soil",
        "compacted",
        "compacted road",
        "granodiorite",
        "grass",
        "leaf litter",
        "redwood bark",
        SAND_DETAIL,
        TREAD_DETAIL,
    )
)


@dataclass(frozen=True)
class DetailFrame:
    origin: np.ndarray
    scale: np.ndarray

    @classmethod
    def from_source(cls, source):
        return cls(
            np.r_[METRES_PER_DEGREE * [source.lon, source.lat], source.datum],
            np.r_[METRES_PER_DEGREE / source.metres, 1.0],
        )

    def phase(self, stretch):
        return np.remainder(WAVES @ (self.origin / stretch), 2 * np.pi)

    def offset(self, span):
        return np.remainder(self.origin, span)
