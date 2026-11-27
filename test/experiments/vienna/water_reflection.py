"""Select planar captures from water coverage in the reference camera."""

import numpy as np
import shapely

REFLECTION_RANGE_M = 650
LEVEL_TOLERANCE_M = 0.10
MAX_REFLECTIONS = 2


def camera_axes(look):
    right = np.cross(look, [0, 0, 1])
    right /= np.linalg.norm(right)
    return right, np.cross(right, look)


def reflection_levels(world):
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
    levels = []
    viewpoint = shapely.Point(world.eye[:2])
    for body in world.water_plan:
        if body.footprint.distance(viewpoint) >= REFLECTION_RANGE_M:
            continue
        level = float(body.heights(world.source, world.eye[None, :2])[0])
        distance = (level - world.eye[2]) / rays[:, 2]
        hit = (0 < distance) & (distance < distances)
        xy = world.eye[:2] + distance[hit, None] * rays[hit, :2]
        index = np.flatnonzero(hit)[shapely.contains_xy(body.footprint, *xy.T)]
        if len(index):
            owners[index] = len(levels)
            levels.append(level)
            distances[index] = distance[index]
    if not levels:
        return ()
    coverage = np.bincount(owners[owners >= 0], minlength=len(levels))
    selected = []
    for i in np.argsort(-coverage, kind="stable"):
        level = levels[i]
        if coverage[i] >= 3 and all(
            abs(other - level) > LEVEL_TOLERANCE_M for other in selected
        ):
            selected.append(level)
        if len(selected) == MAX_REFLECTIONS:
            break
    return tuple(selected)


def reflection_transform(level):
    transform = np.eye(4)
    transform[2, 2] = -1
    transform[2, 3] = 2 * level
    return transform
