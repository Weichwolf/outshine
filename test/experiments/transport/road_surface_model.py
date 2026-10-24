"""Analytic road upper surfaces from recipe poses, without emitted mesh inputs."""

import numpy as np
from outshine_ref.transport.clothoid_turns import quadrature


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
        height = bezier(self.height[i], t)
        bank = bezier(self.bank[i], t)
        heading = (
            self.poses[i, 4]
            + self.curvature_start[i] * distance
            + self.curvature_rate[i] * distance**2 / 2
        )
        direction = np.c_[-np.sin(heading), np.cos(heading), -np.tan(bank)]
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
