"""Select planar captures from water coverage in the reference camera."""

from dataclasses import dataclass

import numpy as np
import shapely

REFLECTION_RANGE_M = 650
LEVEL_TOLERANCE_M = 0.10
MAX_REFLECTIONS = 2


@dataclass(frozen=True)
class ReflectionPlane:
    height: float
    normal: np.ndarray
    offset: float

    @classmethod
    def through(cls, eye_xy, height, gradient):
        normal = np.array([-gradient[0], -gradient[1], 1.0])
        normal /= np.linalg.norm(normal)
        return cls(height, normal, -float(normal @ [*eye_xy, height]))

    def coefficients(self):
        return (*self.normal, self.offset)

    def mirror(self):
        transform = np.eye(4)
        transform[:3, :3] -= 2 * np.outer(self.normal, self.normal)
        transform[:3, 3] = -2 * self.offset * self.normal
        return transform


def camera_axes(look):
    right = np.cross(look, [0, 0, 1])
    right /= np.linalg.norm(right)
    return right, np.cross(right, look)


def fit_plane(world, body, xy, level):
    gradient = np.zeros(2)
    if getattr(body, "profile", None) is not None and len(xy) >= 3:
        design = np.column_stack((xy - world.eye[:2], np.ones(len(xy))))
        heights = body.heights(world.source, xy)
        fit, _, rank, _ = np.linalg.lstsq(design, heights, rcond=None)
        if rank == 3 and np.linalg.norm(fit[:2]) < 0.002:
            gradient, level = fit[:2], float(fit[2])
        else:
            level = float(np.median(heights))
    return ReflectionPlane.through(world.eye[:2], level, gradient)


def reflection_planes(world):
    right, up = camera_axes(world.look)
    x, y = np.meshgrid(np.arange(-20, 20) + 0.5, np.arange(-11, 11) + 0.5)
    rays = world.look + (x.ravel()[:, None] * right + y.ravel()[:, None] * up) * (
        360 / world.focal / 11
    )
    rays /= np.linalg.norm(rays, axis=1, keepdims=True)
    rays = rays[rays[:, 2] < -1e-6]
    if not len(rays):
        return ()
    distances = np.full(len(rays), REFLECTION_RANGE_M)
    owners = np.full(len(rays), -1, dtype=int)
    planes = []
    viewpoint = shapely.Point(world.eye[:2])
    for body in world.water_plan:
        if body.footprint.distance(viewpoint) >= REFLECTION_RANGE_M:
            continue
        level = float(body.heights(world.source, world.eye[None, :2])[0])
        distance = (level - world.eye[2]) / rays[:, 2]
        hit = (0 < distance) & (distance < distances)
        xy = world.eye[:2] + distance[hit, None] * rays[hit, :2]
        inside = shapely.contains_xy(body.footprint, *xy.T)
        index = np.flatnonzero(hit)[inside]
        if len(index):
            owners[index] = len(planes)
            planes.append(fit_plane(world, body, xy[inside], level))
            distances[index] = distance[index]
    if not planes:
        return ()
    coverage = np.bincount(owners[owners >= 0], minlength=len(planes))
    selected = []
    for i in np.argsort(-coverage, kind="stable"):
        plane = planes[i]
        if coverage[i] >= 3 and all(
            abs(other.height - plane.height) > LEVEL_TOLERANCE_M
            or np.linalg.norm(other.normal - plane.normal) > 0.0001
            for other in selected
        ):
            selected.append(plane)
        if len(selected) == MAX_REFLECTIONS:
            break
    return tuple(selected)
