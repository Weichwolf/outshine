"""Invertible blade expansion with optional transverse longitudinal shear."""

from dataclasses import dataclass

import numpy as np


@dataclass(frozen=True)
class LeafGrowthMap:
    scale: float
    shear: float
    length: float
    width: float

    def forward(self, points):
        out = np.array(points, dtype=float, copy=True)
        out[..., 1] -= self.shear * self.length * (out[..., 0] / self.width) ** 2
        return out * self.scale

    def inverse(self, points):
        out = np.array(points, dtype=float, copy=True) / self.scale
        out[..., 1] += self.shear * self.length * (out[..., 0] / self.width) ** 2
        return out

    def chord_error(self, physical_delta):
        dx = physical_delta[..., 0] / self.scale
        return abs(self.shear) * self.length * (dx / self.width) ** 2 / 4
