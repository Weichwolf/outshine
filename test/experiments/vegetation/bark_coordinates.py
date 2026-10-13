"""Continuous solid-texture charts along canonical circular branch axes."""

import numpy as np
from curve_frame import arc_frame
from scipy.spatial import cKDTree


def axis_chart(state, level, points):
    local = (points - state.origin) @ state.frame
    if state.bend == 0:
        parameter = np.clip(local[:, 1] / state.length, 0, 1)
    else:
        curvature = state.bend / state.length
        angle = np.arctan2(local[:, 1] * curvature, 1 - local[:, 2] * curvature)
        parameter = np.clip(angle / state.bend, 0, 1)
        candidates = np.column_stack(
            (parameter, np.zeros(len(points)), np.ones(len(points)))
        )
        centres = arc_frame(state.length, state.bend, 0, candidates)[0]
        distances = np.sum((local[:, None] - centres) ** 2, axis=2)
        parameter = candidates[np.arange(len(points)), np.argmin(distances, axis=1)]
    centre, _tangent, _lateral, normal = arc_frame(
        state.length, state.bend, 0, parameter
    )
    relative = local - centre
    coordinates = np.column_stack(
        (relative[:, 0], np.sum(relative * normal, axis=1), parameter * state.length)
    )
    radius = level.base_radius + parameter * (level.tip_radius - level.base_radius)
    return coordinates, radius, np.linalg.norm(relative, axis=1)


def smooth_step(value):
    value = np.clip(value, 0, 1)
    return value * value * (3 - 2 * value)


def material_coordinates(plant, points, method="indexed"):
    points = np.asarray(points)
    if points.ndim != 2 or points.shape[1] != 3 or np.iscomplexobj(points):
        raise ValueError("finite three-dimensional material points required")
    count = sum(plant.axis_counts)
    if (
        method not in ("dense", "indexed")
        or count > 128
        or count * len(points) > 64_000_000
    ):
        raise ValueError(
            "bounded complete material request and declared method required"
        )
    points = np.asarray(points, dtype=np.float64)
    if not np.isfinite(points).all():
        raise ValueError("finite three-dimensional material points required")
    if not len(points):
        return {"bark_coordinates": points.copy(), "bark_radius": np.zeros(0)}, {
            "point_axis_evaluations": 0
        }
    paths = tuple(plant.paths())
    root = plant.axis(())
    coordinates, radius, _distance = axis_chart(root, plant.levels[0], points)
    offsets = {(): np.zeros(3)}
    tree = cKDTree(points) if method == "indexed" else None
    evaluations = len(points)
    for path in paths[1:]:
        state, parent = plant.axis(path), plant.axis(path[:-1])
        level, parent_level = plant.levels[len(path)], plant.levels[len(path) - 1]
        inherited = axis_chart(parent, parent_level, state.origin[None])[0][0]
        offsets[path] = offsets[path[:-1]] + inherited
        if tree is None:
            indices = np.arange(len(points))
        else:
            centre = arc_frame(state.length, state.bend, 0, np.array([0.5]))[0][0]
            centre = centre @ state.frame.T + state.origin
            bound = state.length / 2 + 2 * level.base_radius
            margin = 64 * np.finfo(float).eps * max(np.max(abs(centre)), bound, 1)
            indices = np.asarray(
                tree.query_ball_point(centre, bound + margin), dtype=int
            )
        if not len(indices):
            continue
        chart, child_radius, distance = axis_chart(state, level, points[indices])
        evaluations += len(indices)
        parent_radius = parent_level.base_radius + state.parent_parameter * (
            parent_level.tip_radius - parent_level.base_radius
        )
        span = min(2.5 * parent_radius, state.length / 4)
        outer = 2 * child_radius
        if state.bend:
            outer = np.minimum(
                outer, (state.length / abs(state.bend) + child_radius) / 2
            )
        weight = smooth_step(chart[:, 2] / span) * smooth_step(
            (outer - distance) / (outer - child_radius)
        )
        chart += offsets[path]
        coordinates[indices] += weight[:, None] * (chart - coordinates[indices])
        radius[indices] += weight * (child_radius - radius[indices])
    return {"bark_coordinates": coordinates, "bark_radius": radius}, {
        "point_axis_evaluations": evaluations
    }
