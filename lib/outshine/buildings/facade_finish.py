"""Finite metric facade finishes, integrated over the same opening repeat."""

import math
from dataclasses import dataclass

import numpy as np

from .facades import interval_area, rectangle_coverage

CHANNELS = ("wall", "frame", "glass", "band")


def boundary_band_area(low, high, fraction, count):
    """Integrate bands centred at integer floor boundaries in [0, count]."""
    low = np.clip(low, 0.0, count) + 0.5
    high = np.clip(high, 0.0, count) + 0.5
    origin = np.floor(low)

    def primitive(value):
        whole = np.floor(value)
        return whole * fraction + np.clip(
            value - whole - (1.0 - fraction) / 2.0, 0.0, fraction
        )

    return primitive(high - origin) - primitive(low - origin)


@dataclass(frozen=True, slots=True)
class FacadeFinish:
    frame_m: float = 0.06
    band_m: float = 0.16

    def __post_init__(self):
        if any(
            isinstance(value, (bool, np.bool_))
            or not math.isfinite(value)
            or value < 0.0
            for value in (self.frame_m, self.band_m)
        ):
            raise ValueError("Facade finish widths require finite nonnegative metres")

    def coverage(self, repeat, centres_m, footprints_m):
        """Return wall/frame/glass/band area fractions for an axis-aligned box.

        Frames occupy the inside of the original opening. Floor bands are
        limited to the opaque gap between openings. This describes surface
        material coverage, without changing silhouette, openings or collision.
        """
        opening = repeat.coverage(centres_m, footprints_m)
        size = np.asarray(repeat.cell_size_m)
        centres, footprints = np.broadcast_arrays(
            np.asarray(centres_m, dtype=float) / size,
            np.asarray(footprints_m, dtype=float) / size,
        )
        fractions = np.array([repeat.opening_width, repeat.opening_height])
        inner = np.maximum(fractions - 2.0 * self.frame_m / size, 0.0)
        counts = np.array([repeat.columns, repeat.rows])
        glass = np.minimum(
            opening, rectangle_coverage(centres, footprints, inner, counts)
        )
        low, high = centres - footprints / 2.0, centres + footprints / 2.0
        band_fraction = min(self.band_m / size[1], 1.0 - fractions[1])
        band = (
            interval_area(low[..., 0], high[..., 0], 1.0, repeat.columns)
            / footprints[..., 0]
            * boundary_band_area(low[..., 1], high[..., 1], band_fraction, repeat.rows)
            / footprints[..., 1]
        )
        band = np.clip(band, 0.0, 1.0 - opening)
        return np.stack((1.0 - opening - band, opening - glass, glass, band), axis=-1)
