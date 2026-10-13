"""Closed diagnostic branch tubes; this does not solve welded fork junctions."""

import numpy as np
from form_plan import validate_parents


def branch_tubes(positions, parents, radii, sides=8):
    if not isinstance(sides, (int, np.integer)) or sides < 3:
        raise ValueError("at least three radial sides required")
    validate_parents(parents)
    positions = np.asarray(positions, dtype=np.float64)
    radii = np.asarray(radii, dtype=np.float64)
    if positions.shape != (len(parents), 3) or radii.shape != (len(parents),):
        raise ValueError("one position and radius per node required")
    if (
        not np.all(np.isfinite(positions))
        or not np.all(np.isfinite(radii))
        or np.any(radii <= 0)
    ):
        raise ValueError("finite positions and positive finite radii required")
    start, finish = positions[parents[1:]], positions[1:]
    directions = finish - start
    lengths = np.linalg.norm(directions, axis=1)
    if np.any(lengths <= 0) or not np.all(np.isfinite(lengths)):
        raise ValueError("finite positive branch lengths required")
    directions /= lengths[:, None]
    axes = np.eye(3)[np.argmin(np.abs(directions), axis=1)]
    first = np.cross(directions, axes)
    first /= np.linalg.norm(first, axis=1)[:, None]
    second = np.cross(directions, first)
    angles = np.arange(sides) * (2 * np.pi / sides)
    rings = first[:, None, :] * np.cos(angles)[None, :, None]
    rings += second[:, None, :] * np.sin(angles)[None, :, None]
    children = np.bincount(parents[1:], minlength=len(parents))
    start_radius = radii[1:]
    end_radius = start_radius * np.where(children[1:] == 0, 0.35, 1.0)
    vertices = np.concatenate(
        (
            start[:, None, :] + rings * start_radius[:, None, None],
            finish[:, None, :] + rings * end_radius[:, None, None],
            start[:, None, :],
            finish[:, None, :],
        ),
        axis=1,
    ).reshape(-1, 3)
    edge = np.arange(sides)
    following = (edge + 1) % sides
    local = np.concatenate(
        (
            np.stack((edge, following, sides + following), axis=1),
            np.stack((edge, sides + following, sides + edge), axis=1),
            np.stack((np.full(sides, 2 * sides), following, edge), axis=1),
            np.stack(
                (np.full(sides, 2 * sides + 1), sides + edge, sides + following), axis=1
            ),
        )
    )
    offsets = np.arange(len(start)) * (2 * sides + 2)
    faces = (local[None, :, :] + offsets[:, None, None]).reshape(-1, 3)
    slope = (start_radius - end_radius) / lengths
    side_normals = rings + directions[:, None, :] * slope[:, None, None]
    side_normals /= np.linalg.norm(side_normals, axis=2)[:, :, None]
    vertex_normals = np.concatenate(
        (side_normals, side_normals, -directions[:, None, :], directions[:, None, :]),
        axis=1,
    )
    normals = vertex_normals[:, local].copy()
    normals[:, 2 * sides : 3 * sides] = -directions[:, None, None, :]
    normals[:, 3 * sides :] = directions[:, None, None, :]
    return (
        vertices.astype(np.float32),
        faces.astype(np.int32),
        normals.reshape(-1, 3).astype(np.float32),
    )
