"""Bounded matrix comparison against the installed prepared-mixture procedure."""

import numpy as np
from outshine_ref.vegetation.species_mixture import (
    SpeciesHeightMixture,
)
from outshine_ref.vegetation.species_mixture import (
    species_site_uniforms as site_uniforms,
)

__all__ = ["HeightMixture", "site_uniforms"]


class HeightMixture(SpeciesHeightMixture):
    def choose(self, height_m, uniform, *, method="binary"):
        if method == "binary":
            return super().choose(height_m, uniform)
        if method != "linear":
            raise ValueError("declared comparison method required")
        h, u = np.broadcast_arrays(height_m, uniform)
        if h.size * len(self.taxa) > 4000000:
            raise ValueError(
                "linear reference scratch budget exceeded before matrix allocation"
            )
        h = np.clip(h.ravel(), self.heights[0], self.heights[-1])
        row = np.minimum(
            np.searchsorted(self.heights, h, side="right") - 1, len(self.heights) - 2
        )
        t = (h - self.heights[row]) / (self.heights[row + 1] - self.heights[row])
        cdf = (1 - t[:, None]) * self.cumulative[row] + t[:, None] * self.cumulative[
            row + 1
        ]
        total = cdf[:, -1]
        threshold = np.minimum(u.ravel() * total, np.nextafter(total, 0))
        selected = np.sum(threshold[:, None] >= cdf, axis=1)
        return np.where(total > 0, selected, -1).reshape(u.shape)
