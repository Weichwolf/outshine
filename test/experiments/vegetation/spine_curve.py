"""Immutable authored cubic spatial spines and conservative local regularity bounds."""

from dataclasses import dataclass
from functools import cached_property

import numpy as np
from scipy.interpolate import BPoly, CubicHermiteSpline
from spine_frames import unit


@dataclass(frozen=True)
class CubicSpine:
    controls: tuple

    def __post_init__(self):
        points = np.asarray(self.controls, dtype=np.float64)
        if (
            points.ndim != 3
            or points.shape[1:] != (4, 3)
            or not 1 <= len(points) <= 64
            or not np.isfinite(points).all()
        ):
            raise ValueError("one through 64 finite cubic spatial segments required")
        if not np.allclose(points[:-1, -1], points[1:, 0], rtol=0, atol=1e-12):
            raise ValueError("continuous segment positions required")
        if not np.allclose(
            points[:-1, -1] - points[:-1, -2],
            points[1:, 1] - points[1:, 0],
            rtol=1e-10,
            atol=1e-12,
        ):
            raise ValueError("continuous normalized-parameter tangents required")
        guide = unit(points[:, -1] - points[:, 0])
        velocity = 3 * len(points) * np.diff(points, axis=1)
        speed_lower = np.min(np.sum(velocity * guide[:, None], axis=-1), axis=1)
        if np.any(speed_lower <= 1e-12):
            raise ValueError(
                "regular segments with forward derivative control vectors required"
            )
        acceleration = 6 * len(points) ** 2 * np.diff(points, n=2, axis=1)
        upper = np.max(np.linalg.norm(acceleration, axis=-1), axis=1) / speed_lower**2
        if not np.isfinite(upper).all():
            raise ValueError("finite regularity bounds required")
        object.__setattr__(self, "_curvature_bound", float(np.max(upper)))
        object.__setattr__(
            self,
            "controls",
            tuple(tuple(tuple(p) for p in segment) for segment in points),
        )

    @classmethod
    def from_hermite(cls, positions, derivatives):
        positions = np.asarray(positions, dtype=np.float64)
        derivatives = np.asarray(derivatives, dtype=np.float64)
        if positions.shape != derivatives.shape or positions.ndim != 2:
            raise ValueError(
                "matching spatial Hermite anchors and derivatives required"
            )
        if positions.shape[1] != 3 or not 2 <= len(positions) <= 65:
            raise ValueError("two through 65 spatial Hermite anchors required")
        knots = np.linspace(0, 1, len(positions))
        polynomial = CubicHermiteSpline(knots, positions, derivatives, axis=0)
        return cls(BPoly.from_power_basis(polynomial).c.transpose(1, 0, 2))

    @cached_property
    def polynomial(self):
        controls = np.array(self.controls).transpose(1, 0, 2)
        controls.setflags(write=False)
        return BPoly(
            controls, np.linspace(0, 1, len(self.controls) + 1), extrapolate=False
        )

    @property
    def curvature_bound(self):
        return self._curvature_bound

    def evaluate(self, parameter, derivative=0):
        parameter = np.asarray(parameter, dtype=np.float64)
        if not np.isfinite(parameter).all() or np.any(
            (parameter < 0) | (parameter > 1)
        ):
            raise ValueError("finite normalized spine positions in [0, 1] required")
        if not isinstance(derivative, int) or not 0 <= derivative <= 2:
            raise ValueError("position, first or second derivative required")
        return self.polynomial(parameter, nu=derivative)

    def sample(self, parameter):
        centre = self.evaluate(parameter)
        velocity = self.evaluate(parameter, 1)
        speed = np.linalg.norm(velocity, axis=-1)
        tangent = velocity / speed[..., None]
        acceleration = self.evaluate(parameter, 2)
        curvature = (
            acceleration - np.sum(acceleration * tangent, axis=-1)[..., None] * tangent
        ) / speed[..., None] ** 2
        return centre, tangent, speed, curvature
