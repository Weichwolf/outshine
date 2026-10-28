"""Render/normal adapter for the explicit cubic-crossfall recipe (#1533)."""

import numpy as np
from road_surface_model import Curve, bezier


class BankedSurface(Curve):
    def __init__(self, source, crossfall_controls, *, height_controls=None):
        super().__init__(source)
        if height_controls is not None:
            height = np.array(height_controls, dtype=float, copy=True)
            if height.shape != self.height.shape or not np.isfinite(height).all():
                raise ValueError("One finite cubic height per original span required")
            self.height = height
        self.crossfall_controls = np.array(crossfall_controls, dtype=float, copy=True)
        if (
            self.crossfall_controls.shape != self.bank.shape
            or not np.isfinite(self.crossfall_controls).all()
        ):
            raise ValueError("One finite cubic crossfall per original span required")
        self.crossfall_derivative = (
            3 * np.diff(self.crossfall_controls, axis=1) / self.lengths[:, None]
        )
        second_q = np.max(
            abs(2 * np.diff(self.crossfall_derivative, axis=1) / self.lengths[:, None]),
            axis=1,
        )
        height_second = np.max(
            abs(6 * np.diff(self.height, n=2, axis=1) / self.lengths[:, None] ** 2),
            axis=1,
        )
        planar = np.hypot(
            self.width * abs(self.curvature_rate),
            self.curvature_max * (1 + self.width * self.curvature_max),
        )
        self.second = np.hypot(planar, height_second + self.width * second_q)

    def crossfall(self, indices, parameter, *, derivative=0):
        if derivative == 0:
            return bezier(self.crossfall_controls[indices], parameter)
        if derivative == 1:
            return bezier(self.crossfall_derivative[indices], parameter)
        raise ValueError("Crossfall value or first derivative required")
