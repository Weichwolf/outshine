"""Own bounded bifurcation sites from local turning on a stable spatial spine."""

from dataclasses import asdict, dataclass

import numpy as np
from placement_patterns import EMPTY_SITES
from scipy.signal import find_peaks
from spatial_axis import SpatialAxis


@dataclass(frozen=True)
class BendPattern:
    mode: str = "bifurcation"
    minimum_angle: float = 0.16
    window_m: float = 0.18
    minimum_spacing_m: float = 0.16
    minimum_prominence: float = 0.02
    limit: int = 32
    samples: int = 257

    def __post_init__(self):
        if (
            self.mode != "bifurcation"
            or not np.isfinite(
                (
                    self.minimum_angle,
                    self.window_m,
                    self.minimum_spacing_m,
                    self.minimum_prominence,
                )
            ).all()
        ):
            raise ValueError("finite declared bifurcation controls required")
        if (
            not 0 <= self.minimum_angle < np.pi
            or self.window_m <= 0
            or self.minimum_spacing_m < 0
            or self.minimum_prominence < 0
        ):
            raise ValueError(
                "bounded turning angle and positive physical window required"
            )
        if (
            type(self.limit) is not int
            or not 0 <= self.limit <= 128
            or type(self.samples) is not int
            or not 17 <= self.samples <= 1025
        ):
            raise ValueError(
                "bounded marker and stable generation sampling counts required"
            )

    def recipe(self):
        return asdict(self)

    @classmethod
    def from_recipe(cls, record):
        return cls(**record)

    def capacity(self, length, first, last):
        if not np.isfinite(length) or length < 0 or not 0 <= first <= last <= 1:
            raise ValueError(
                "finite parent length and ordered bifurcation interval required"
            )
        return self.limit if length > 0 and first < last else 0

    def sites(self, shape, length, first, last):
        if not isinstance(shape, SpatialAxis):
            raise TypeError("bifurcation sites require a spatial parent curve")
        if self.capacity(length, first, last) == 0:
            return EMPTY_SITES
        u = np.linspace(0, 1, self.samples)
        half = self.window_m / (2 * length)
        before = shape.spine.sample(shape.measure.parameter(np.maximum(u - half, 0)))[1]
        after = shape.spine.sample(shape.measure.parameter(np.minimum(u + half, 1)))[1]
        dot = np.clip(np.sum(before * after, axis=-1), -1, 1)
        angle = np.arccos(np.where(dot > 1 - 1e-14, 1, dot))
        distance = max(
            1, int(np.ceil(self.minimum_spacing_m / length * (self.samples - 1)))
        )
        minimum = np.where((u >= first) & (u <= last), self.minimum_angle, np.inf)
        locality = max(
            distance, int(np.ceil(self.window_m / length * (self.samples - 1)))
        )
        peaks, _properties = find_peaks(
            angle,
            height=minimum,
            distance=distance,
            prominence=max(self.minimum_prominence, 1e-8),
            wlen=2 * locality + 1,
        )
        if len(peaks) > self.limit:
            order = np.lexsort((peaks, -angle[peaks]))[: self.limit]
            peaks = np.sort(peaks[order])
        # Slots progress from the tip toward the base, as other physical patterns do.
        fraction = u[peaks[::-1]]
        sample = shape.field.sample(fraction)
        outward = before[peaks[::-1]] - after[peaks[::-1]]
        roll = np.arctan2(
            np.sum(outward * sample["normal"], axis=-1),
            np.sum(outward * sample["lateral"], axis=-1),
        )
        result = np.column_stack((fraction, roll))
        result.setflags(write=False)
        return result
