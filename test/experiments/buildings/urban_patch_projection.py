"""Projected front-face coverage before depth/alpha rejection, not GPU time."""

import numpy as np


def clip(polygon, normal, offset):
    if not len(polygon):
        return polygon
    result = []
    previous = polygon[-1]
    before = previous @ normal + offset
    for current in polygon:
        after = current @ normal + offset
        if (before >= 0) != (after >= 0):
            result.append(previous + (current - previous) * (before / (before - after)))
        if after >= 0:
            result.append(current)
        previous, before = current, after
    return np.array(result).reshape(-1, 3)


def projected_work(mesh, camera):
    positions, quads = mesh["positions"], mesh["quads"]
    eye, basis = np.array(camera["eye"]), np.array(camera["camera_basis"])
    focal = camera["focal_px"]
    horizontal, vertical = camera["width"] / (2 * focal), camera["height"] / (2 * focal)
    planes = [
        (np.array([0, 0, 1]), -0.1),
        (np.array([0, 0, -1]), 240000),
        (np.array([1, 0, horizontal]), 0),
        (np.array([-1, 0, horizontal]), 0),
        (np.array([0, 1, vertical]), 0),
        (np.array([0, -1, vertical]), 0),
    ]
    total, front, visible = 0.0, 0, 0
    for indices in quads:
        polygon = positions[indices]
        normal = np.cross(polygon[1] - polygon[0], polygon[2] - polygon[0])
        if normal @ (polygon.mean(axis=0) - eye) >= 0:
            continue
        front += 1
        polygon = (polygon - eye) @ basis.T
        for normal, offset in planes:
            polygon = clip(polygon, normal, offset)
            if not len(polygon):
                break
        if len(polygon) < 3:
            continue
        xy = polygon[:, :2] / polygon[:, 2, None] * focal
        area = (
            abs(
                np.sum(
                    xy[:, 0] * np.roll(xy[:, 1], -1) - xy[:, 1] * np.roll(xy[:, 0], -1)
                )
            )
            / 2
        )
        total += area
        visible += area > 0
    return {
        "geometry_quads": len(quads),
        "geometry_triangles": 2 * len(quads),
        "front_facing_quads": front,
        "frustum_visible_quads": int(visible),
        "projected_geometric_pixel_area": total,
        "scope": "Sum of clipped quad areas before depth, alpha and occlusion rejection. Not actual shader invocations, DRAM traffic or GPU time.",
    }
