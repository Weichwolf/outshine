"""Compact symmetric blending of origin-connected capsule segments; values use m²."""

from dataclasses import dataclass
from functools import cached_property

import numpy as np
from scipy.optimize import elementwise
from spine_frames import unit


def smooth_maximum(scores, blend):
    maximum = np.max(scores, axis=1, keepdims=True)
    weights = np.zeros_like(scores)
    weights[np.arange(len(scores)), np.argmax(scores, axis=1)] = 1
    if blend == 0:
        return maximum[:, 0], weights
    mixed = np.sum(scores > maximum - blend, axis=1) > 1
    value = maximum[:, 0].copy()
    if not np.any(mixed):
        return value, weights
    selected = scores[mixed]
    shifted = np.maximum(selected - maximum[mixed], -blend) / blend
    ordered = np.sort(shifted, axis=1)[:, ::-1]
    count = np.arange(1, scores.shape[1] + 1)
    threshold = (np.cumsum(ordered, axis=1) - 1) / count
    active = np.sum(ordered > threshold, axis=1) - 1
    offset = threshold[np.arange(len(selected)), active]
    selected_weights = np.maximum(shifted - offset[:, None], 0)
    weights[mixed] = selected_weights
    value[mixed] = np.sum(selected_weights * selected, axis=1) + blend / 2 * (
        1 - np.sum(selected_weights**2, axis=1)
    )
    return value, weights


@dataclass(frozen=True)
class ForkField:
    endpoints: tuple
    radii: tuple
    blend: float
    origin: tuple = (0, 0, 0)

    def __post_init__(self):
        endpoints = np.asarray(self.endpoints, dtype=np.float64)
        radii = np.asarray(self.radii, dtype=np.float64)
        origin = np.asarray(self.origin, dtype=np.float64)
        if (
            endpoints.ndim != 2
            or endpoints.shape[1] != 3
            or not 1 <= len(endpoints) <= 16
            or radii.shape != (len(endpoints),)
            or origin.shape != (3,)
            or not np.isfinite(endpoints).all()
            or not np.isfinite(radii).all()
            or not np.isfinite(origin).all()
            or not np.isfinite(self.blend)
            or self.blend < 0
            or np.min(radii) <= 0
        ):
            raise ValueError(
                "one through 16 finite segments, positive radii and finite blend required"
            )
        squares = np.sum(endpoints**2, axis=1)
        padding_squared = np.max(radii) ** 2 + self.blend / 2
        if (
            not np.isfinite(squares).all()
            or not np.isfinite(padding_squared)
            or np.any(radii**2 == 0)
        ):
            raise ValueError(
                "finite squared dimensions and representable positive squared radii required"
            )
        object.__setattr__(self, "endpoints", tuple(tuple(p) for p in endpoints))
        object.__setattr__(self, "radii", tuple(float(r) for r in radii))
        object.__setattr__(self, "origin", tuple(float(p) for p in origin))
        object.__setattr__(self, "blend", float(self.blend))

    @cached_property
    def segment_data(self):
        endpoints = np.asarray(self.endpoints)
        squares = np.sum(endpoints**2, axis=1)
        endpoints.setflags(write=False)
        squares.setflags(write=False)
        return endpoints, squares

    @property
    def padding(self):
        return float(np.sqrt(max(self.radii) ** 2 + self.blend / 2))

    @property
    def ray_extent(self):
        return (float(np.sqrt(np.max(self.segment_data[1]))) + self.padding) * 1.001

    def bounds(self):
        endpoints = self.segment_data[0]
        return (
            np.minimum(np.min(endpoints, axis=0), 0) + self.origin - self.padding,
            np.maximum(np.max(endpoints, axis=0), 0) + self.origin + self.padding,
        )

    def evaluate(self, points, with_gradient=False):
        points = np.asarray(points, dtype=np.float64)
        if points.ndim != 2 or points.shape[1] != 3 or not np.isfinite(points).all():
            raise ValueError("finite spatial point batches required")
        local = points - self.origin
        scores = np.empty((len(points), len(self.radii)))
        endpoints, squares = self.segment_data
        for index, (endpoint, square, radius) in enumerate(
            zip(endpoints, squares, self.radii, strict=True)
        ):
            parameter = (
                np.clip(np.sum(local * endpoint, axis=1) / square, 0, 1)
                if square > 0
                else np.zeros(len(points))
            )
            delta = local - parameter[:, None] * endpoint
            scores[:, index] = radius**2 - np.sum(delta**2, axis=1)
        if not np.isfinite(scores).all():
            raise ValueError("finite squared field values required")
        value, weights = smooth_maximum(scores, self.blend)
        if not with_gradient:
            return value
        gradient = np.zeros_like(local)
        for index, (endpoint, square) in enumerate(
            zip(endpoints, squares, strict=True)
        ):
            parameter = (
                np.clip(np.sum(local * endpoint, axis=1) / square, 0, 1)
                if square > 0
                else np.zeros(len(points))
            )
            gradient -= (
                2 * weights[:, index, None] * (local - parameter[:, None] * endpoint)
            )
        return value, gradient

    def block_intervals(self, minima, maxima):
        from fork_intervals import block_intervals

        return block_intervals(self, minima, maxima)

    def normals(self, points):
        _value, gradient = self.evaluate(points, with_gradient=True)
        return unit(-gradient)

    def radial_radius(self, directions):
        directions = unit(directions)
        if directions.ndim != 2:
            raise ValueError("spatial ray batches required")

        def function(distance, x, y, z):
            points = distance[:, None] * np.column_stack((x, y, z)) + self.origin
            return self.evaluate(points)

        result = elementwise.find_root(
            function,
            (np.zeros(len(directions)), np.full(len(directions), self.ray_extent)),
            args=tuple(directions[:, axis] for axis in range(3)),
            tolerances={"xatol": self.ray_extent * 1e-10, "xrtol": 1e-12, "fatol": 0},
            maxiter=50,
        )
        if not np.all(result.success):
            raise ValueError("bounded radial surface solve did not converge")
        return result.x, {
            "maximum_iterations": int(np.max(result.nit)),
            "field_point_evaluations": int(np.sum(result.nfev)),
        }
