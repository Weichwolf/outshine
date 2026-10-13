"""Cubic Bernstein profile construction with budgeted monotone inversion."""

from dataclasses import dataclass
from functools import cached_property

import numpy as np
from scipy.interpolate import BPoly
from scipy.optimize import brentq, elementwise

from .profiles import finite


@dataclass(frozen=True)
class BezierCurve:
    controls: tuple

    def __post_init__(self):
        points = finite(self.controls)
        if points.ndim != 3 or points.shape[1:] != (4, 2) or not 1 <= len(points) <= 64:
            raise ValueError("one to 64 cubic two-dimensional segments required")
        if not np.array_equal(points[:-1, -1], points[1:, 0]):
            raise ValueError("continuous curve endpoints required")
        object.__setattr__(
            self, "controls", tuple(tuple(tuple(p) for p in c) for c in points)
        )

    @cached_property
    def _polynomial(self):
        return BPoly(
            np.asarray(self.controls).transpose(1, 0, 2),
            np.linspace(0, 1, len(self.controls) + 1),
        )

    @property
    def polynomial(self):
        """Return an independent SciPy representation for external inspection."""
        return BPoly(self._polynomial.c.copy(), self._polynomial.x.copy())

    def raw(self, parameter):
        return self._polynomial(np.clip(finite(parameter), 0, 1))

    def coordinates(self, coordinate):
        points = np.asarray(self.controls)
        if points[0, 0, 0] != 0 or points[-1, -1, 0] != 1:
            raise ValueError("profile endpoints must span zero to one")
        derivative = 3 * np.diff(points[..., 0], axis=1)
        a = derivative[:, 0] - 2 * derivative[:, 1] + derivative[:, 2]
        u = np.clip(
            np.divide(
                derivative[:, 0] - derivative[:, 1],
                a,
                out=np.zeros_like(a),
                where=a > 0,
            ),
            0,
            1,
        )
        minimum = np.minimum(derivative[:, 0], derivative[:, 2])
        interior = (
            derivative[:, 0] * (1 - u) ** 2
            + 2 * derivative[:, 1] * u * (1 - u)
            + derivative[:, 2] * u**2
        )
        minimum = np.minimum(minimum, interior)
        if np.any(minimum < 0) or np.any(points[:, -1, 0] <= points[:, 0, 0]):
            raise ValueError("single-valued monotone profile required")
        return np.clip(finite(coordinate), 0, 1)

    def reference_value(self, coordinate):
        x = self.coordinates(coordinate)
        parameters = [
            brentq(
                lambda t, target=target: float(self._polynomial(t)[0] - target),
                0,
                1,
                xtol=1e-13,
            )
            for target in x.ravel()
        ]
        return self._polynomial(np.asarray(parameters).reshape(x.shape))[..., 1]

    def value(self, coordinate):
        x = self.coordinates(coordinate)
        result = elementwise.find_root(
            lambda t, targets: self._polynomial(t)[..., 0] - targets,
            (np.zeros_like(x), np.ones_like(x)),
            args=(x,),
            tolerances={"xatol": 1e-13, "xrtol": 0, "fatol": 0, "frtol": 0},
            maxiter=100,
        )
        if not np.all(result.success):
            raise ValueError("profile inverse did not converge within its budget")
        return self._polynomial(result.x)[..., 1]
