"""Inherited physical bark charts across complete joined graph wood products."""

import numpy as np
from bark_coordinates import smooth_step
from scipy.spatial import cKDTree


def wood_material(axes, points, method="indexed"):
    points = np.asarray(points, dtype=float)
    if (
        method not in ("dense", "indexed")
        or points.ndim != 2
        or points.shape[1] != 3
        or not np.isfinite(points).all()
        or len(axes) > 128
        or len(axes) * len(points) > 64_000_000
    ):
        raise ValueError(
            "bounded finite complete bark request and declared method required"
        )
    if not axes:
        raise ValueError("complete wood axes required")
    tree = cKDTree(points) if method == "indexed" else None
    coordinates = np.zeros_like(points)
    radius = np.zeros(len(points))
    offsets = {}
    evaluations = 0
    lookup = {a.path: a for a in axes}
    assigned = np.zeros(len(points), dtype=bool)
    for axis in axes:
        parent = lookup.get(axis.path[:-1])
        if parent is None:
            indices = np.arange(len(points))
            offsets[axis.path] = np.zeros(3)
        else:
            inherited = parent.chart(axis.pose.anchor[None])[0][0]
            offsets[axis.path] = offsets[parent.path] + inherited
            if tree is None:
                indices = np.arange(len(points))
            else:
                centre, bound = axis.bounds()
                margin = 64 * np.finfo(float).eps * max(np.max(abs(centre)), bound, 1)
                indices = np.asarray(
                    tree.query_ball_point(centre, bound + margin), dtype=int
                )
        if not len(indices):
            continue
        chart, local_radius, distance = axis.chart(points[indices])
        evaluations += len(indices)
        if parent is None:
            weight = np.ones(len(indices))
        else:
            _origin_chart, origin_radius, _ = parent.chart(axis.pose.anchor[None])
            span = min(2.5 * origin_radius[0], axis.pose.length / 4)
            weight = smooth_step(chart[:, 2] / span) * smooth_step(
                (2 * local_radius - distance) / local_radius
            )
        chart += offsets[axis.path]
        coordinates[indices] += weight[:, None] * (chart - coordinates[indices])
        radius[indices] += weight * (local_radius - radius[indices])
        assigned[indices] |= weight > 0
    if not assigned.all() or not (radius > 0).all():
        raise ValueError("complete finite bark fields were not assigned")
    return {"bark_coordinates": coordinates, "bark_radius": radius}, {
        "point_axis_evaluations": evaluations,
        "vertices": len(points),
    }
