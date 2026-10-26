"""Exact projected triangle mass, common normalisation, existing water objective."""

import numpy as np
from scipy import sparse


def area_mass(mesh):
    faces = mesh.triangles
    triangle = mesh.vertices[faces, :2]
    u, v = triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0]
    area = np.abs(u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0]) / 2
    local = area[:, None, None] / 12 * (np.ones((3, 3)) + np.eye(3))
    mass = sparse.coo_matrix(
        (
            local.ravel(),
            (np.repeat(faces, 3, axis=1).ravel(), np.tile(faces, (1, 3)).ravel()),
        ),
        shape=(len(mesh.vertices), len(mesh.vertices)),
    ).tocsr()
    return mass * (len(mesh.vertices) / mass.diagonal().sum())
