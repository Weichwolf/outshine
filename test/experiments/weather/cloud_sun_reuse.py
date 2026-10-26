"""Local full-column solar maps; inside-cloud and uncovered points stay explicit."""

from dataclasses import dataclass

import numpy as np
from cloud_field import Layer, altitude, optical_depth


def sun_frame(sun):
    direction = np.array(sun, dtype=float, copy=True)
    if direction.shape != (3,) or not np.isfinite(direction).all():
        raise ValueError("a finite solar direction is required")
    if abs(np.linalg.norm(direction) - 1) > 1e-6 or direction[1] <= 0:
        raise ValueError("this local experiment requires a unit upward Sun")
    horizontal = np.cross(direction, [0, 0, 1])
    horizontal /= np.linalg.norm(horizontal)
    vertical = np.cross(horizontal, direction)
    for array in (direction, horizontal, vertical):
        array.flags.writeable = False
    return direction, horizontal, vertical


def column_points(uv, sun, horizontal, vertical, layer):
    points = uv[..., 0, None] * horizontal + uv[..., 1, None] * vertical
    radius, height = layer.radius_m, layer.base_m
    b = radius * sun[1]
    c = (
        np.sum(points * points, axis=-1)
        + 2 * radius * points[..., 1]
        - height * (2 * radius + height)
    )
    discriminant = b * b - c
    root = np.sqrt(np.maximum(discriminant, 0))
    # Select the Sun-facing inner-shell intersection, one metre before the column.
    distance = np.where(discriminant >= 0, -c / (b + root), -b) - 1
    return points + distance[..., None] * sun


@dataclass(frozen=True)
class SolarLevel:
    origin_uv: np.ndarray
    step_uv: np.ndarray
    values: np.ndarray

    def sample(self, uv):
        coordinate = (uv - self.origin_uv) / self.step_uv
        inside = np.all(
            (coordinate >= 0) & (coordinate < np.array(self.values.shape[::-1]) - 1),
            axis=-1,
        )
        safe = np.where(inside[..., None], coordinate, 0)
        first = np.floor(safe).astype(int)
        fraction = safe - first
        result = np.zeros(coordinate.shape[:-1])
        for x in (0, 1):
            for y in (0, 1):
                result += (
                    self.values[first[..., 1] + y, first[..., 0] + x]
                    * (fraction[..., 0] if x else 1 - fraction[..., 0])
                    * (fraction[..., 1] if y else 1 - fraction[..., 1])
                )
        edge = np.minimum(safe, np.array(self.values.shape[::-1]) - 1 - safe).min(
            axis=-1
        )
        return result, np.where(inside, np.clip(edge / 16, 0, 1), 0)


@dataclass(frozen=True)
class SolarField:
    layer: Layer
    sun: np.ndarray
    horizontal: np.ndarray
    vertical: np.ndarray
    seconds: float
    levels: tuple[SolarLevel, ...]

    def transfer(self, positions, counters=None):
        shape = positions.shape[:-1]
        points = np.asarray(positions, float).reshape(-1, 3)
        uv = np.c_[points @ self.horizontal, points @ self.vertical]
        height = altitude(points, self.layer.radius_m)
        remaining = np.clip((self.layer.base_m - height) / 100, 0, 1)
        result = np.zeros(len(points))
        for level in self.levels:
            active = remaining > 0
            if not active.any():
                break
            value, weight = level.sample(uv[active])
            result[active] += remaining[active] * weight * value
            remaining[active] *= 1 - weight
            if counters is not None:
                counters["texture_samples"] += int(np.count_nonzero(weight))
                counters["level_tests"] = counters.get("level_tests", 0) + int(
                    active.sum()
                )
        # The height fade also requires the direct value in the complementary part.
        direct_weight = (
            1 - np.clip((self.layer.base_m - height) / 100, 0, 1) + remaining
        )
        active = direct_weight > 0
        if active.any():
            direct = np.exp(
                -optical_depth(
                    points[active], self.sun, self.layer, 6, self.seconds, maximum_m=1e8
                )
            )
            result[active] += direct_weight[active] * direct
        if counters is not None:
            counters["direct_points"] += int(active.sum())
            counters["points"] += len(points)
        return result.reshape(shape)


def build_field(layer, sun, seconds, anchor_m, spacing=(2, 16, 128), dimension=513):
    direction, horizontal, vertical = sun_frame(sun)
    if (
        dimension < 3
        or not np.isfinite(spacing).all()
        or np.any(np.asarray(spacing) <= 0)
    ):
        raise ValueError("positive spacing and at least three texels are required")
    anchor = np.asarray(anchor_m, float)
    center = np.array([anchor @ horizontal, anchor @ vertical])
    levels = []
    for scale in spacing:
        step = np.array([float(scale) * direction[1], float(scale)])
        origin = center - step * (dimension - 1) / 2
        axis = np.arange(dimension)
        coordinates = np.stack(np.meshgrid(axis, axis, indexing="xy"), axis=-1)
        uv = origin + coordinates.reshape(-1, 2) * step
        values = np.empty(len(uv), dtype="<f4")
        active_count = len(uv) if layer.cover > 0 and layer.extinction_per_m > 0 else 0
        if active_count == 0:
            values.fill(1)
        for first in range(0, active_count, 32768):
            batch = column_points(
                uv[first : first + 32768], direction, horizontal, vertical, layer
            )
            values[first : first + len(batch)] = np.exp(
                -optical_depth(
                    batch,
                    direction,
                    layer,
                    6,
                    seconds,
                    maximum_m=1e8,
                )
            )
        values = values.reshape(dimension, dimension)
        for array in (origin, step, values):
            array.flags.writeable = False
        levels.append(SolarLevel(origin, step, values))
    return SolarField(layer, direction, horizontal, vertical, seconds, tuple(levels))
