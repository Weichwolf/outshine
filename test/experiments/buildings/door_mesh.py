"""Fixed local leaf template and rigid state expansion for rendering and collision."""

import math

import numpy as np
from door_geometry import pose


def template(leaf):
    """Eight local corners and twelve triangles; five millimetre head/foot gaps."""
    pose(leaf, 0.0)
    width, half = leaf.leaf_width_m, leaf.thickness_m / 2
    bottom, top = 0.005, leaf.height_m - 0.005
    ring = np.array([[0, -half], [width, -half], [width, half], [0, half]])
    vertices = np.concatenate(
        [np.column_stack([ring, np.full(4, z)]) for z in (bottom, top)]
    )
    faces = np.array(
        [
            [0, 2, 1],
            [0, 3, 2],
            [4, 5, 6],
            [4, 6, 7],
            [0, 1, 5],
            [0, 5, 4],
            [1, 2, 6],
            [1, 6, 5],
            [2, 3, 7],
            [2, 7, 6],
            [3, 0, 4],
            [3, 4, 7],
        ],
        dtype=np.int32,
    )
    vertices.flags.writeable = False
    faces.flags.writeable = False
    return {
        "vertices": vertices,
        "faces": faces,
        "dimensions_m": (leaf.leaf_width_m, leaf.thickness_m, leaf.height_m),
    }


def expand(leaf, fixed, angle_deg):
    """Transform the same cached local vertices for mesh and collision consumers."""
    pose(leaf, angle_deg)
    if fixed["dimensions_m"] != (leaf.leaf_width_m, leaf.thickness_m, leaf.height_m):
        raise ValueError("Cached door template does not match fixed leaf dimensions")
    angle = math.radians(angle_deg)
    tangent, inward = np.asarray(leaf.tangent), np.asarray(leaf.inward)
    along = -leaf.hinge_side * tangent * math.cos(angle) + inward * math.sin(angle)
    across = leaf.hinge_side * tangent * math.sin(angle) + inward * math.cos(angle)
    vertices = np.array(fixed["vertices"], copy=True)
    vertices[:, :2] = leaf.hinge + vertices[:, :1] * along + vertices[:, 1:2] * across
    vertices[:, 2] += leaf.floor_z_m
    faces = np.array(fixed["faces"], copy=True)
    if np.linalg.det(np.column_stack([along, across])) < 0:
        faces = faces[:, ::-1].copy()
    return {
        "vertices": vertices,
        "faces": faces,
        "materials": np.full(len(faces), 40, dtype=np.uint8),
        "parts": np.zeros(len(faces), dtype=np.int32),
    }
