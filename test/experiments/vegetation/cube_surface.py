"""Closed cube parameter grids shared by procedural radial surface experiments."""

import numpy as np


def face_axes(axis, sign):
    normal = np.eye(3)[axis] * sign
    first = np.eye(3)[(axis + 1) % 3]
    return normal, first, np.cross(normal, first)


def cube_mesh(level):
    if not isinstance(level, (int, np.integer)) or not 0 <= level <= 7:
        raise ValueError("integer subdivision level between zero and seven required")
    divisions = 1 << level
    coordinates = np.linspace(-1, 1, divisions + 1)
    u, v = np.meshgrid(coordinates, coordinates, indexing="ij")
    parameters = []
    for axis in range(3):
        for sign in (-1, 1):
            normal, first, second = face_axes(axis, sign)
            parameters.append(normal + u[..., None] * first + v[..., None] * second)
    parameters, indices = np.unique(
        np.stack(parameters).reshape(-1, 3), axis=0, return_inverse=True
    )
    indices = indices.reshape(6, divisions + 1, divisions + 1)
    a, b = indices[:, :-1, :-1], indices[:, 1:, :-1]
    c, d = indices[:, 1:, 1:], indices[:, :-1, 1:]
    faces = np.stack((np.stack((a, b, c), -1), np.stack((a, c, d), -1)), -2)
    return parameters, faces.reshape(-1, 3).astype(np.int32)
