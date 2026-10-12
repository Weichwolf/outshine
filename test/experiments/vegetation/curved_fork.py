"""Independent spatial branch plans, bounded chord approximations and shared envelope fields."""

from dataclasses import dataclass
from functools import cached_property

import numpy as np
from fork_field import smooth_maximum
from spine_frames import unit
from tapered_fork import TaperedFork


@dataclass(frozen=True)
class CurveBranch:
    points: tuple
    radii: tuple
    source_error_bound_m: float = 0.0

    def __post_init__(self):
        points, radii = (
            np.asarray(self.points, dtype=float),
            np.asarray(self.radii, dtype=float),
        )
        if (
            points.ndim != 2
            or points.shape[1] != 3
            or not 2 <= len(points) <= 257
            or radii.shape != (len(points),)
            or not np.isfinite(points).all()
            or not np.isfinite(radii).all()
            or np.any(radii <= 0)
            or not np.isfinite(self.source_error_bound_m)
            or self.source_error_bound_m < 0
        ):
            raise ValueError(
                "two through 257 finite branch stations with positive radii required"
            )
        object.__setattr__(self, "points", tuple(tuple(p) for p in points))
        object.__setattr__(self, "radii", tuple(float(r) for r in radii))
        _ = self.segments

    @cached_property
    def segments(self):
        return tuple(
            TaperedFork((np.asarray(b) - a,), (ra,), 0, a, (rb,))
            for a, b, ra, rb in zip(
                self.points[:-1],
                self.points[1:],
                self.radii[:-1],
                self.radii[1:],
                strict=True,
            )
        )

    @classmethod
    def from_spine(
        cls, spine, profile, radius_scale=1.0, tolerance=0.001, max_segments=256
    ):
        if (
            not np.isfinite(tolerance)
            or tolerance <= 0
            or not np.isfinite(radius_scale)
            or radius_scale <= 0
            or type(max_segments) is not int
            or not 1 <= max_segments <= 256
        ):
            raise ValueError(
                "positive finite approximation tolerance and bounded segment budget required"
            )
        controls = np.asarray(spine.controls)
        acceleration = 6 * len(controls) ** 2 * np.diff(controls, n=2, axis=1)
        upper = float(np.max(np.linalg.norm(acceleration, axis=-1)))
        count = max(1, int(np.ceil(np.sqrt(upper / (4 * tolerance)))))
        source_t = np.linspace(0, 1, len(profile.table))
        source_r = radius_scale * profile.evaluate(source_t)
        while count <= max_segments:
            parameter = np.linspace(0, 1, count + 1)
            radii = radius_scale * profile.evaluate(parameter)
            radial_error = float(
                np.max(abs(np.interp(source_t, parameter, radii) - source_r))
            )
            bound = upper / (8 * count**2) + radial_error
            if bound <= tolerance:
                return cls(spine.evaluate(parameter), radii, bound)
            count *= 2
        raise ValueError("curve approximation exceeds its segment budget")

    def evaluate(self, points, with_gradient=False):
        values = np.full(len(points), -np.inf)
        gradient = np.zeros_like(points) if with_gradient else None
        for segment in self.segments:
            local = points - segment.origin
            scores, parameter = segment.segment_scores(local)
            candidate = scores[:, 0]
            chosen = candidate > values
            values[chosen] = candidate[chosen]
            if with_gradient:
                gradient[chosen] = -2 * (
                    local[chosen] - parameter[chosen] * segment.segment_data[0][0]
                )
        return (values, gradient) if with_gradient else values

    def block_intervals(self, minima, maxima):
        low, high = np.full(len(minima), -np.inf), np.full(len(minima), -np.inf)
        for segment in self.segments:
            a, b = segment.block_intervals(minima, maxima)
            low, high = np.maximum(low, a), np.maximum(high, b)
        return low, high


@dataclass(frozen=True)
class CurvedFork:
    branches: tuple
    blend: float

    def __post_init__(self):
        if (
            not 1 <= len(self.branches) <= 16
            or not all(isinstance(b, CurveBranch) for b in self.branches)
            or not np.isfinite(self.blend)
            or self.blend < 0
        ):
            raise ValueError(
                "one through 16 branch plans and finite nonnegative blend required"
            )
        object.__setattr__(self, "branches", tuple(self.branches))
        if not np.isfinite(self.padding) or not np.isfinite(self.ray_extent):
            raise ValueError("finite squared field dimensions required")
        minimum, maximum = self.bounds()
        if (
            not np.isfinite(minimum).all()
            or not np.isfinite(maximum).all()
            or np.any(maximum <= minimum)
        ):
            raise ValueError("finite representable spatial field bounds required")

    @property
    def origin(self):
        return self.branches[0].points[0]

    @property
    def padding(self):
        return float(
            np.sqrt(max(max(b.radii) for b in self.branches) ** 2 + self.blend / 2)
        )

    @cached_property
    def nodes(self):
        nodes = np.vstack([b.points for b in self.branches])
        nodes.setflags(write=False)
        return nodes

    @property
    def ray_extent(self):
        return (
            float(
                np.max(np.linalg.norm(self.nodes - self.origin, axis=1)) + self.padding
            )
            * 1.001
        )

    def bounds(self):
        return np.min(self.nodes, axis=0) - self.padding, np.max(
            self.nodes, axis=0
        ) + self.padding

    def evaluate(self, points, with_gradient=False):
        points = np.asarray(points, dtype=float)
        if points.ndim != 2 or points.shape[1] != 3 or not np.isfinite(points).all():
            raise ValueError("finite spatial point batches required")
        results = [b.evaluate(points, with_gradient) for b in self.branches]
        scores = np.column_stack([r[0] for r in results] if with_gradient else results)
        if not np.isfinite(scores).all():
            raise ValueError("finite squared field values required")
        values, weights = smooth_maximum(scores, self.blend)
        if not with_gradient:
            return values
        gradient = sum(weights[:, i, None] * r[1] for i, r in enumerate(results))
        return values, gradient

    def normals(self, points):
        return unit(-self.evaluate(points, True)[1])

    def block_intervals(self, minima, maxima):
        intervals = [b.block_intervals(minima, maxima) for b in self.branches]
        low = np.max([pair[0] for pair in intervals], axis=0)
        high = np.max([pair[1] for pair in intervals], axis=0)
        high += self.blend / 2 * (1 - 1 / len(self.branches))
        guard = (
            4096
            * np.finfo(float).eps
            * len(self.branches) ** 2
            * (np.maximum(abs(low), abs(high)) + self.blend)
        )
        return np.nextafter(low - guard, -np.inf), np.nextafter(high + guard, np.inf)
