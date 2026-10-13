"""Conservative camera bounds for selecting products before their generation."""

from dataclasses import dataclass, field

import numpy as np


@dataclass(frozen=True)
class Camera:
    position: tuple
    target: tuple
    width: int = 1280
    height: int = 720
    fov_y: float = 60.0
    near: float = 0.1
    far: float = 30000.0
    axes: np.ndarray = field(init=False, repr=False, compare=False)
    planes: np.ndarray = field(init=False, repr=False, compare=False)
    limits: np.ndarray = field(init=False, repr=False, compare=False)

    def __post_init__(self):
        position, target = np.asarray(self.position), np.asarray(self.target)
        if position.shape != (3,) or target.shape != (3,):
            raise ValueError("three-dimensional camera position and target required")
        if not np.all(np.isfinite([position, target])):
            raise ValueError("finite camera required")
        if not 0 < self.fov_y < 179 or not 0 < self.near < self.far:
            raise ValueError("valid field of view and clipping planes required")
        if not all(np.isfinite(v) for v in (self.fov_y, self.near, self.far)):
            raise ValueError("finite camera projection required")
        if not all(
            isinstance(v, (int, np.integer)) and v > 0
            for v in (self.width, self.height)
        ):
            raise ValueError("positive image dimensions required")
        forward = target - position
        length = np.linalg.norm(forward)
        if length <= 0:
            raise ValueError("camera target cannot equal position")
        forward = forward / length
        reference = np.array([0.0, 0.0, 1.0])
        if abs(forward[2]) > 0.99:
            reference = np.array([0.0, 1.0, 0.0])
        right = np.cross(forward, reference)
        right /= np.linalg.norm(right)
        up = np.cross(right, forward)
        vertical = np.tan(np.deg2rad(self.fov_y) / 2)
        horizontal = vertical * self.width / self.height
        axes = np.array([right, up, forward])
        planes = np.array(
            [
                forward,
                -forward,
                forward * horizontal + right,
                forward * horizontal - right,
                forward * vertical + up,
                forward * vertical - up,
            ]
        )
        for name, value in (
            ("axes", axes),
            ("planes", planes),
            ("limits", np.array([self.near, -self.far, 0, 0, 0, 0])),
        ):
            value.setflags(write=False)
            object.__setattr__(self, name, value)
        object.__setattr__(self, "position", tuple(float(v) for v in position))
        object.__setattr__(self, "target", tuple(float(v) for v in target))

    def visible(self, lower, upper):
        lower, upper = np.asarray(lower), np.asarray(upper)
        centres, extents = (lower + upper) / 2, (upper - lower) / 2
        values = (centres - self.position) @ self.planes.T
        radii = extents @ np.abs(self.planes).T
        return np.all(values + radii >= self.limits - 1e-8, axis=-1)

    def projected_span(self, lower, upper):
        centre, extent = (lower + upper) / 2, (upper - lower) / 2
        local = self.axes @ (centre - self.position)
        minimum_depth = local[2] - np.abs(self.axes[2]) @ extent
        if minimum_depth <= self.near:
            return np.inf
        horizontal = np.linalg.norm(local[:2]) + np.linalg.norm(extent)
        focal = self.height / (2 * np.tan(np.deg2rad(self.fov_y) / 2))
        return (
            focal
            * np.linalg.norm(upper - lower)
            / minimum_depth
            * np.hypot(1, horizontal / minimum_depth)
        )


def box_distance(position, lower, upper):
    delta = np.maximum(np.maximum(lower - position, position - upper), 0)
    return np.linalg.norm(delta, axis=-1)
