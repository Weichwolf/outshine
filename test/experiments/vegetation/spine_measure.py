"""Prepared monotone arc-length coordinates, independent of surface tessellation."""

from dataclasses import dataclass
from functools import cached_property

import numpy as np
from numpy.polynomial.legendre import leggauss
from scipy.interpolate import CubicHermiteSpline, PchipInterpolator
from scipy.optimize import elementwise


@dataclass(frozen=True)
class ArcMeasure:
    spine: object
    intervals_per_segment: int = 16

    def __post_init__(self):
        if (
            type(self.intervals_per_segment) is not int
            or not 2 <= self.intervals_per_segment <= 128
        ):
            raise ValueError("two to 128 arc intervals per cubic segment required")
        intervals = len(self.spine.controls) * self.intervals_per_segment
        if intervals > 1024:
            raise ValueError("complete arc table exceeds its station budget")
        t = np.linspace(0, 1, intervals + 1)
        widths = np.diff(t)
        estimates = []
        for order in (16, 32):
            x, w = leggauss(order)
            query = t[:-1, None] + widths[:, None] * (x + 1) / 2
            velocity = self.spine.evaluate(query, 1)
            estimates.append(
                np.sum(np.linalg.norm(velocity, axis=-1) * w, axis=1) * widths / 2
            )
        error = abs(estimates[1] - estimates[0])
        if not np.isfinite(estimates).all() or np.any(
            error > 1e-11 * np.maximum(estimates[1], 1e-12)
        ):
            raise ValueError("arc quadrature needs a more resolved curve table")
        s = np.r_[0, np.cumsum(estimates[1])]
        speed = np.linalg.norm(self.spine.evaluate(t, 1), axis=-1)
        polynomial = CubicHermiteSpline(t, s, speed)
        a, b, c = 3 * polynomial.c[0], 2 * polynomial.c[1], polynomial.c[2]
        critical = np.clip(
            np.divide(-b, 2 * a, out=np.zeros_like(a), where=a > 0), 0, widths
        )
        if np.any(a * critical**2 + b * critical + c <= 0) or np.any(np.diff(s) <= 0):
            raise ValueError("strictly monotone prepared arc polynomial required")
        for value in (t, s):
            value.setflags(write=False)
        object.__setattr__(self, "parameter_nodes", t)
        object.__setattr__(self, "length_nodes", s)
        object.__setattr__(self, "polynomial", polynomial)
        object.__setattr__(self, "length", float(s[-1]))
        object.__setattr__(self, "quadrature_difference_m", float(error.sum()))

    @cached_property
    def inverse(self):
        return PchipInterpolator(self.length_nodes, self.parameter_nodes)

    def parameter(self, fraction):
        fraction = np.asarray(fraction, dtype=float)
        if not np.isfinite(fraction).all() or np.any((fraction < 0) | (fraction > 1)):
            raise ValueError("finite normalized arc locations in [0,1] required")
        target = fraction * self.length
        t = self.inverse(target)
        for _ in range(3):
            t = np.clip(
                t - (self.polynomial(t) - target) / self.polynomial(t, nu=1), 0, 1
            )
        residual = abs(self.polynomial(t) - target)
        if np.any(residual > 2e-13 * self.length):
            solve = elementwise.find_root(
                lambda value, goal: self.polynomial(value) - goal,
                (np.zeros_like(t), np.ones_like(t)),
                args=(target,),
                tolerances={"xatol": 1e-14, "fatol": 1e-14 * self.length},
                maxiter=64,
            )
            if not solve.success.all():
                raise ValueError("arc inverse did not converge within its budget")
            t = solve.x
        return np.where(fraction == 0, 0, np.where(fraction == 1, 1, t))

    def fraction(self, parameter):
        parameter = np.asarray(parameter, dtype=float)
        if not np.isfinite(parameter).all() or np.any(
            (parameter < 0) | (parameter > 1)
        ):
            raise ValueError("finite raw curve parameters in [0,1] required")
        return self.polynomial(parameter) / self.length
