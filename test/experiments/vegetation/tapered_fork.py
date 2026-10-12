"""Linear-radius sphere sweeps with analytic envelope maxima and shared fork blending."""

from dataclasses import dataclass
from functools import cached_property

import numpy as np
from fork_field import ForkField, smooth_maximum


@dataclass(frozen=True)
class TaperedFork(ForkField):
    tip_radii: tuple = ()

    def __post_init__(self):
        super().__post_init__()
        tips = np.asarray(self.tip_radii, dtype=float)
        if (
            tips.shape != (len(self.radii),)
            or not np.isfinite(tips).all()
            or np.any(tips <= 0)
            or np.any(tips**2 == 0)
            or not np.isfinite(tips**2 + self.blend / 2).all()
        ):
            raise ValueError(
                "one finite positive representable tip radius per segment required"
            )
        object.__setattr__(self, "tip_radii", tuple(float(r) for r in tips))

    @property
    def padding(self):
        return float(np.sqrt(max(*self.radii, *self.tip_radii) ** 2 + self.blend / 2))

    @cached_property
    def enclosing_fields(self):
        return tuple(
            ForkField(
                self.endpoints,
                operation(self.radii, self.tip_radii),
                self.blend,
                self.origin,
            )
            for operation in (np.minimum, np.maximum)
        )

    def block_intervals(self, minima, maxima):
        inner, outer = self.enclosing_fields
        low, _unused = inner.block_intervals(minima, maxima)
        _unused, high = outer.block_intervals(minima, maxima)
        return low, high

    def segment_scores(self, local):
        scores = np.empty((len(local), len(self.radii)))
        parameters = np.empty_like(scores)
        for index, (endpoint, square, base, tip) in enumerate(
            zip(
                self.segment_data[0],
                self.segment_data[1],
                self.radii,
                self.tip_radii,
                strict=True,
            )
        ):
            change = tip - base
            denominator = square - change**2
            if denominator > 0:
                parameter = np.clip(
                    (np.sum(local * endpoint, axis=1) + base * change) / denominator,
                    0,
                    1,
                )
            else:
                first = base**2 - np.sum(local**2, axis=1)
                last = tip**2 - np.sum((local - endpoint) ** 2, axis=1)
                parameter = (last > first).astype(float)
            delta = local - parameter[:, None] * endpoint
            scores[:, index] = ((1 - parameter) * base + parameter * tip) ** 2 - np.sum(
                delta**2, axis=1
            )
            parameters[:, index] = parameter
        return scores, parameters

    def evaluate(self, points, with_gradient=False):
        points = np.asarray(points, dtype=float)
        if points.ndim != 2 or points.shape[1] != 3 or not np.isfinite(points).all():
            raise ValueError("finite spatial point batches required")
        local = points - self.origin
        scores, parameters = self.segment_scores(local)
        if not np.isfinite(scores).all():
            raise ValueError("finite squared field values required")
        values, weights = smooth_maximum(scores, self.blend)
        if not with_gradient:
            return values
        gradient = np.zeros_like(local)
        for index, endpoint in enumerate(self.segment_data[0]):
            gradient -= (
                2
                * weights[:, index, None]
                * (local - parameters[:, index, None] * endpoint)
            )
        return values, gradient
