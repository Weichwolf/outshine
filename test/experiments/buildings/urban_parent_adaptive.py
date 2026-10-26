"""Geometry and ray-overlap sampling demand, never an empty-world certificate."""

import numpy as np
from urban_parent_conditional import grid
from urban_parent_light import light_at
from urban_parent_volume import centres_at


def rays_at(view, box, axes):
    eye, basis, focal, width, height = view
    u, v = np.meshgrid(*axes)
    x = box[0, 0] + u.ravel() * (box[1, 0] - box[0, 0])
    y = box[0, 1] + v.ravel() * (box[1, 1] - box[0, 1])
    directions = (
        np.column_stack(
            ((x - width / 2) / focal, (height / 2 - y) / focal, np.ones(len(x)))
        )
        @ basis
    )
    directions /= np.linalg.norm(directions, axis=1)[:, None]
    return np.broadcast_to(eye, directions.shape).copy(), directions


def adjacent_indices(mask, axis):
    changed = np.diff(mask.astype(np.int8), axis=axis) != 0
    indices = np.flatnonzero(changed.any(axis=1 - axis))
    return np.unique(np.r_[indices, indices + 1])


def adaptive_axes(parent, view, box, profile, spacing):
    width, height = box[1] - box[0]
    if np.any(np.asarray(spacing) < 1):
        raise ValueError("positive base spacing required")
    pixel_axes = ((np.arange(width) + 0.5) / width, (np.arange(height) + 0.5) / height)
    origins, directions = rays_at(view, box, pixel_axes)
    if np.any(directions[:, 2] >= 0):
        raise ValueError("down-looking support study required")
    product = grid(parent)
    pitch = product["header"]["pitch_m"]
    low = product["reference_origin_m"][:2]
    high = low + product["shape"] * pitch
    support = np.zeros(len(origins), bool)
    for z in np.r_[0, parent.heights]:
        time = (z - origins[:, 2]) / directions[:, 2]
        points = origins + time[:, None] * directions
        inside = np.all((points[:, :2] >= low) & (points[:, :2] <= high), axis=1)
        support |= inside & (centres_at(parent, points, "midpoint") > 0)
    ground = origins - (origins[:, 2] / directions[:, 2])[:, None] * directions
    light = light_at(ground, profile)[0]
    if np.any(light[:, 2] <= 0):
        raise ValueError("light above flat field required")
    slope = (
        directions[:, :2] / directions[:, 2, None] - light[:, :2] / light[:, 2, None]
    )
    footprint = pitch * parent.frame[5]
    overlap = np.any(
        np.all(
            np.abs(slope[:, None] * parent.heights[None, :, None]) < footprint, axis=2
        ),
        axis=1,
    ).reshape(height, width)
    slope = slope.reshape(height, width, 2)
    seed_uv = np.empty((0, 2))
    if not support.any() and parent.centre_fraction().max() > 0:
        centre = (low + high) / 2
        seeds = np.array(
            [
                [*centre, z]
                for z in [0, parent.heights @ parent.probabilities, parent.heights[-1]]
            ]
        )
        projected = (seeds - view[0]) @ view[1].T
        valid = projected[:, 2] > 0.1
        screen = np.array([view[3] / 2, view[4] / 2]) + (
            view[2] * projected[valid, :2] / projected[valid, 2, None] * [1, -1]
        )
        seed_uv = (screen - box[0]) / [width, height]
    axes, refinement = [], []
    for dimension, (size, step) in enumerate(
        zip([width, height], spacing, strict=True)
    ):
        image_axis = 1 - dimension
        edge = adjacent_indices(support.reshape(height, width), image_axis)
        change = np.abs(np.diff(slope, axis=image_axis)).max(axis=2)
        angular = change * parent.heights[-1] * step >= footprint / 2
        if dimension == 0:
            angular &= overlap[:, :-1] | overlap[:, 1:]
        else:
            angular &= overlap[:-1] | overlap[1:]
        at = np.flatnonzero(angular.any(axis=dimension))
        dense = np.unique(np.r_[edge, at, at + 1])
        count = int(np.ceil(size / step))
        base = (np.arange(count) + 0.5) / count
        # A refined pixel replaces its coarse node instead of duplicating it.
        base = base[~np.isin(np.floor(base * size).astype(int), dense)]
        seed = seed_uv[:, dimension]
        seed = seed[(seed > 0) & (seed < 1)]
        axes.append(np.unique(np.r_[base, (dense + 0.5) / size, seed]))
        refinement.append(
            {"support_edge_indices": len(edge), "angular_indices": len(at)}
        )
    return tuple(axes), {
        "support_ray_plane_tests": len(origins) * (len(parent.heights) + 1),
        "support_pixels": int(support.sum()),
        "refinement_xy": refinement,
        "rule": "Refine support transitions and overlapping view/light rays whose separation changes by half a footprint over the base step; heuristic, not an error bound.",
    }
