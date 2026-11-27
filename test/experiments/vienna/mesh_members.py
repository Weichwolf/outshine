"""Closed rectangular members shared by stairs and bridge structures."""

import numpy as np


def rectangular_member(meshes, start, end, transverse, width, thickness, material):
    axis = end - start
    length = np.linalg.norm(axis)
    if length < 1e-6:
        return
    axis /= length
    right = transverse - (transverse @ axis) * axis
    right /= np.linalg.norm(right)
    up = np.cross(axis, right)
    dimensions = np.array([right * width, up * thickness])
    corners = np.array([[-1, -1], [1, -1], [1, 1], [-1, 1]]) @ dimensions / 2
    faces = [
        (0, 2, 1),
        (0, 3, 2),
        (4, 5, 6),
        (4, 6, 7),
        (0, 1, 5),
        (0, 5, 4),
        (1, 2, 6),
        (1, 6, 5),
        (2, 3, 7),
        (2, 7, 6),
        (3, 0, 4),
        (3, 4, 7),
    ]
    meshes.add(material, np.vstack((start + corners, end + corners)), faces)
