"""Analytic road upper surfaces from recipe poses, without emitted mesh inputs."""

from fractions import Fraction
from math import factorial, inf, isfinite, nextafter

import numpy as np
from outshine_ref.transport.clothoid_turns import quadrature

GAUSS16_CONSTANT = nextafter(
    float(Fraction(factorial(16) ** 4, 33 * factorial(32) ** 3)), inf
)


def positive_product(a, b):
    return 0.0 if a == 0 or b == 0 else nextafter(a * b, inf)


def quadrature_remainder(length, curvature, rate):
    """Gauss16 remainder for an Euler-spiral unit tangent on the whole source span.

    With a=K*L and b=abs(kappa')*L^2, derivative majorants obey
    B_0=1, B_1=a, B_(m+1)=a*B_m+m*b*B_(m-1). The integral remainder
    is at most L*(16!)^4/(33*(32!)^3)*B_32. This is truncation, not
    a universal libm/IEEE floating-point certificate.
    """
    if curvature == 0 and rate == 0:
        return 0.0
    a = positive_product(curvature, length)
    b = positive_product(positive_product(abs(rate), length), length)
    previous, current = 1.0, a
    for order in range(1, 32):
        first = positive_product(a, current)
        second = positive_product(positive_product(float(order), b), previous)
        value = first + second
        previous, current = current, nextafter(value, inf) if value else 0.0
        if not isfinite(current):
            return inf
    return positive_product(positive_product(length, GAUSS16_CONSTANT), current)


def controls(first, last, first_rate, last_rate, length):
    return np.array(
        [first, first + length * first_rate / 3, last - length * last_rate / 3, last]
    )


def bezier(values, parameter):
    t = np.asarray(parameter)[..., None]
    values = np.broadcast_to(values, t.shape[:-1] + values.shape[-1:]).copy()
    while values.shape[-1] > 1:
        values = values[..., :-1] * (1 - t) + values[..., 1:] * t
    return values[..., 0]


def derivative_limits(control, lengths):
    first = 3 * np.diff(control, axis=1) / lengths[:, None]
    second = 2 * np.diff(first, axis=1) / lengths[:, None]
    return np.max(abs(first), axis=1), np.max(abs(second), axis=1)


class Curve:
    def __init__(self, recipe):
        self.poses = np.array(recipe["seams"], dtype=float)
        if (
            self.poses.ndim != 2
            or self.poses.shape[1:] != (11,)
            or not 2 <= len(self.poses) <= 4096
        ):
            raise ValueError("Bounded eleven-column source poses required")
        self.stations = self.poses[:, 0]
        self.lengths = np.diff(self.stations)
        if not np.isfinite(self.poses).all() or np.any(self.lengths <= 0):
            raise ValueError("Finite ordered analytic seams required")
        if list(recipe["range_m"]) != [self.stations[0], self.stations[-1]]:
            raise ValueError("Recipe seams must cover the declared range")
        intervals = recipe["intervals"]
        if len(intervals) != len(self.lengths):
            raise ValueError("Each source span needs its own analytic midpoint")
        self.middle = np.array([i["middle"] for i in intervals])
        self.curvature_max = np.array([i["max_abs_curvature_per_m"] for i in intervals])
        ranges = np.array([i["range_m"] for i in intervals])
        if not np.array_equal(
            ranges, np.c_[self.stations[:-1], self.stations[1:]]
        ) or self.middle.shape != (len(self.lengths), 11):
            raise ValueError(
                "Source spans and analytic midpoints must cover the same seams"
            )
        if (
            not np.isfinite(self.middle).all()
            or not np.isfinite(self.curvature_max).all()
            or np.any(self.curvature_max < 0)
        ):
            raise ValueError(
                "Finite midpoint states and nonnegative native curvature bounds required"
            )
        self.curvature_rate = self.middle[:, 6]
        self.curvature_start = self.middle[:, 5] + self.curvature_rate * (
            self.stations[:-1] - self.middle[:, 0]
        )
        endpoints = np.maximum(
            abs(self.curvature_start),
            abs(self.curvature_start + self.curvature_rate * self.lengths),
        )
        if np.any(endpoints > self.curvature_max + 1e-12):
            raise ValueError("Native curvature envelope excludes its declared interval")
        self.curvature_max = np.maximum(self.curvature_max, endpoints)
        self.height = controls(
            self.poses[:-1, 3],
            self.poses[1:, 3],
            self.poses[:-1, 7],
            self.poses[1:, 7],
            self.lengths,
        ).T
        self.bank = controls(
            self.poses[:-1, 9],
            self.poses[1:, 9],
            self.poses[:-1, 10],
            self.poses[1:, 10],
            self.lengths,
        ).T
        self.half_width, self.shoulder, self.thickness = map(float, recipe["section_m"])
        self.width = self.half_width + self.shoulder
        if (
            not np.isfinite([self.half_width, self.shoulder, self.thickness]).all()
            or self.half_width <= 0
            or self.shoulder < 0
        ):
            raise ValueError(
                "Finite positive carriageway and nonnegative shoulder required"
            )
        self.across = np.array(
            [-self.width, -self.half_width, self.half_width, self.width]
        )
        bank_max = np.max(abs(self.bank), axis=1)
        if np.any(bank_max >= np.pi / 2):
            raise ValueError("Bank convex hull reaches a tangent pole")
        _, h2 = derivative_limits(self.height, self.lengths)
        b1, b2 = derivative_limits(self.bank, self.lengths)
        sec2 = 1 / np.cos(bank_max) ** 2
        q2 = sec2 * (b2 + 2 * np.tan(bank_max) * b1**2)
        k = self.curvature_max
        planar = np.hypot(
            self.width * abs(self.curvature_rate), k * (1 + self.width * k)
        )
        self.second = np.hypot(planar, h2 + self.width * q2)
        if not np.isfinite(self.second).all():
            raise ValueError("Finite swept-surface derivative bounds required")
        self.evaluations = 0
        self._evaluation_bound()

    def _planar(self, distance, i):
        points, weights = quadrature(16)
        integration = distance[:, None] * points
        phase = (
            self.poses[i, 4, None]
            + self.curvature_start[i, None] * integration
            + self.curvature_rate[i, None] * integration**2 / 2
        )
        planar = (
            self.poses[i, 1:3]
            + distance[:, None]
            * np.c_[np.cos(phase) @ weights, np.sin(phase) @ weights]
        )
        return planar

    def _evaluation_bound(self):
        self.integration_bounds_m = np.array(
            [
                quadrature_remainder(float(length), float(curvature), float(rate))
                for length, curvature, rate in zip(
                    self.lengths, self.curvature_max, self.curvature_rate, strict=True
                )
            ]
        )
        if not np.isfinite(self.integration_bounds_m).all():
            self.endpoint_disagreement_m = self.evaluation_error_bound_m = inf
            return
        end = self._planar(self.lengths, np.arange(len(self.lengths)))
        heading = (
            self.poses[:-1, 4]
            + self.curvature_start * self.lengths
            + self.curvature_rate * self.lengths**2 / 2
        )
        directions = np.c_[-np.sin(heading), np.cos(heading)]
        stored = np.c_[-np.sin(self.poses[1:, 4]), np.cos(self.poses[1:, 4])]
        defects = np.linalg.norm(end - self.poses[1:, 1:3], axis=1) + self.width * (
            np.linalg.norm(directions - stored, axis=1)
        )
        self.endpoint_disagreement_m = float(defects.max())
        self.evaluation_error_bound_m = max(
            nextafter(float(bound) + float(defect), inf) if bound or defect else 0.0
            for bound, defect in zip(self.integration_bounds_m, defects, strict=True)
        )

    def crossfall(self, indices, parameter, *, derivative=0):
        bank = bezier(self.bank[indices], parameter)
        if derivative == 0:
            return np.tan(bank)
        if derivative == 1:
            rates = (
                3 * np.diff(self.bank[indices], axis=1) / self.lengths[indices, None]
            )
            return bezier(rates, parameter) / np.cos(bank) ** 2
        raise ValueError("Crossfall value or first derivative required")

    def at(self, stations):
        stations = np.asarray(stations, dtype=float)
        if (
            stations.ndim != 1
            or not np.isfinite(stations).all()
            or np.any(stations < self.stations[0])
            or np.any(stations > self.stations[-1])
        ):
            raise ValueError(
                "Finite stations inside the analytic source domain required"
            )
        i = np.minimum(
            np.searchsorted(self.stations, stations, side="right") - 1,
            len(self.lengths) - 1,
        )
        distance = stations - self.stations[i]
        t = distance / self.lengths[i]
        planar = self._planar(distance, i)
        height = bezier(self.height[i], t)
        crossfall = self.crossfall(i, t)
        heading = (
            self.poses[i, 4]
            + self.curvature_start[i] * distance
            + self.curvature_rate[i] * distance**2 / 2
        )
        direction = np.c_[-np.sin(heading), np.cos(heading), -crossfall]
        center = np.c_[planar, height]
        exact = stations == self.stations[np.minimum(i + 1, len(self.stations) - 1)]
        if np.any(exact):
            center[exact] = self.poses[i[exact] + 1, 1:4]
            heading[exact] = self.poses[i[exact] + 1, 4]
            direction[exact] = np.c_[
                -np.sin(heading[exact]),
                np.cos(heading[exact]),
                -np.tan(self.poses[i[exact] + 1, 9]),
            ]
        self.evaluations += len(stations)
        return center[:, None, :] + self.across[None, :, None] * direction[:, None, :]
