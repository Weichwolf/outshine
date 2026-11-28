"""Keep woody geometry that can contribute to a reference camera or its shadows."""

from math import cos, sin

import numpy as np

from outshine.rendering.water_reflection import camera_axes

FRUSTUM_MARGIN_PX = 4


def _sweep_reaches_view(distances, rates):
    # A common distance along the shadow ray must satisfy every frustum plane.
    entry, exit = 0.0, np.inf
    for distance, rate in zip(distances, rates, strict=True):
        if rate > 0:
            entry = max(entry, -distance / rate)
        elif rate < 0:
            exit = min(exit, -distance / rate)
        elif distance < 0:
            return False
    return entry <= exit


def reference_visibility_test(world, reflectors, width, height, sun_angle):
    right, up = camera_axes(world.look)
    horizontal = world.look * ((width / 2 + FRUSTUM_MARGIN_PX) / world.focal)
    vertical = world.look * ((height / 2 + FRUSTUM_MARGIN_PX) / world.focal)
    planes = np.array(
        (horizontal - right, horizontal + right, vertical - up, vertical + up)
    )
    views = [(world.eye, planes)]
    for reflector in reflectors:
        mirror = reflector.mirror()
        linear, translation = mirror[:3, :3], mirror[:3, 3]
        views.append((linear @ world.eye + translation, planes @ linear.T))

    # Expand each plane by the solar cone. The full diameter also leaves margin for jitter.
    spread = 2 * sin(sun_angle / 2)
    sweeps = [
        (eye, normals, normals @ -world.sun + np.linalg.norm(normals, axis=1) * spread)
        for eye, normals in views
    ]

    def may_contribute(centre, extent, position, angle, scale):
        c, s = cos(angle), sin(angle)
        rotation = np.array(((c, -s, 0), (s, c, 0), (0, 0, 1)))
        centre = position + scale * (rotation @ centre)
        for eye, normals, rates in sweeps:
            support = np.abs(normals @ rotation) @ (extent * scale)
            distances = normals @ (centre - eye) + support + 1e-6
            if _sweep_reaches_view(distances, rates):
                return True
        return False

    return may_contribute


def visible_woody_placements(meshes, placements, may_contribute):
    vertices = [np.asarray(points) for points, _ in meshes.values() if len(points)]
    if not vertices:
        return placements[:0]
    lo = np.min([points.min(axis=0) for points in vertices], axis=0)
    hi = np.max([points.max(axis=0) for points in vertices], axis=0)
    centre, extent = (lo + hi) / 2, (hi - lo) / 2
    visible = [
        may_contribute(centre, extent, position, angle, scale)
        for position, angle, scale in zip(
            placements[:, :3], placements[:, 3], placements[:, 4], strict=True
        )
    ]
    return placements[visible]
