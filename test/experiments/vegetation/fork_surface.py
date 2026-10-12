"""Two bounded surface builders for exactly the same organic fork field."""

import numpy as np
from cube_surface import cube_mesh
from skimage.measure import marching_cubes
from spine_frames import unit


def mesh_product(vertices, faces, field):
    vertices = np.asarray(vertices, dtype=np.float64)
    faces = np.asarray(faces, dtype=np.int32)
    normals = field.normals(vertices)
    return {
        "vertices": vertices,
        "faces": faces,
        "normals": normals[faces].reshape(-1, 3),
    }


def radial_mesh(field, level):
    parameters, faces = cube_mesh(level)
    directions = unit(parameters)
    radius, cost = field.radial_radius(directions)
    vertices = directions * radius[:, None] + field.origin
    mesh = mesh_product(vertices, faces, field)
    return mesh, cost | {
        "method": "radial",
        "subdivision_level": level,
        "directions": len(directions),
        "volume_bytes": 0,
    }


def sampled_volume(field, cells):
    if not isinstance(cells, int) or not 16 <= cells <= 192:
        raise ValueError("16 through 192 grid cells on the longest axis required")
    lower, upper = field.bounds()
    step = float(np.max(upper - lower) / cells)
    lower = lower - 2 * step
    shape = np.ceil((upper + 2 * step - lower) / step).astype(int) + 1
    count = int(np.prod(shape))
    if count > 8_000_000:
        raise ValueError("bounded grid volume required")
    values = np.empty(count, dtype=np.float32)
    point_evaluations = 0
    for start in range(0, count, 16384):
        indices = np.arange(start, min(start + 16384, count))
        coordinates = np.column_stack(np.unravel_index(indices, tuple(shape)))
        points = lower + coordinates * step
        values[start : start + len(indices)] = field.evaluate(points)
        point_evaluations += len(indices)
    volume = values.reshape(tuple(shape))
    for axis in range(3):
        assert np.max(np.take(volume, (0, shape[axis] - 1), axis=axis)) < 0
    return volume, lower, step, point_evaluations


def voxel_mesh(field, cells):
    volume, lower, step, evaluations = sampled_volume(field, cells)
    vertices, faces, _normals, _values = marching_cubes(
        volume,
        0,
        spacing=(step, step, step),
        gradient_direction="descent",
        method="lewiner",
        allow_degenerate=False,
    )
    vertices = vertices.astype(np.float64) + lower
    triangles = vertices[faces] - field.origin
    signed = np.sum(
        np.einsum(
            "ij,ij->i", triangles[:, 0], np.cross(triangles[:, 1], triangles[:, 2])
        )
    )
    if signed < 0:
        faces = faces[:, [0, 2, 1]]
    mesh = mesh_product(vertices, faces, field)
    return mesh, {
        "method": "voxel",
        "longest_axis_cells": cells,
        "grid_shape": list(volume.shape),
        "field_point_evaluations": evaluations,
        "volume_bytes": volume.nbytes,
    }
