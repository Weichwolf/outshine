"""Two bounded surface builders for exactly the same organic fork field."""

import numpy as np
from cube_surface import cube_mesh
from fork_volume import blocked_volume, dense_volume
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


def voxel_mesh(field, cells, sampler=dense_volume):
    volume, lower, step, work = sampler(field, cells)
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
    return mesh, work | {
        "method": "voxel",
        "longest_axis_cells": cells,
        "grid_shape": list(volume.shape),
        "volume_bytes": volume.nbytes,
    }


def blocked_voxel_mesh(field, cells, block_size=4):
    from functools import partial

    mesh, work = voxel_mesh(
        field, cells, partial(blocked_volume, block_size=block_size)
    )
    return mesh, work | {"method": "blocked"}
