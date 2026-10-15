"""Room-face jambs bridge the wall-to-leaf stand-off without narrowing the portal."""

import numpy as np
import shapely
from building_mesh import solid
from outshine_ref.buildings.interiors.doors import pose


def frame(leaf):
    pose(leaf, 0.0)
    half = leaf.width_m / 2
    edge = half + 0.075
    back, front = -leaf.wall_face_m, leaf.wall_face_m + leaf.stand_off_m
    storage = [], [], [], []
    for left, right in ((-edge, -half), (half, edge)):
        solid(
            storage,
            shapely.box(left, back, right, front),
            0.0,
            leaf.height_m + 0.075,
            40,
        )
    solid(
        storage,
        shapely.box(-half, back, half, front),
        leaf.height_m,
        leaf.height_m + 0.075,
        40,
    )
    vertices = np.asarray(storage[0], dtype=np.float64)
    vertices[:, :2] = (
        np.asarray(leaf.centre)
        + vertices[:, :1] * np.asarray(leaf.tangent)
        + vertices[:, 1:2] * np.asarray(leaf.inward)
    )
    vertices[:, 2] += leaf.floor_z_m
    faces = np.asarray(storage[1], dtype=np.int32)
    if np.linalg.det(np.column_stack([leaf.tangent, leaf.inward])) < 0:
        faces = faces[:, ::-1].copy()
    return {
        "vertices": vertices,
        "faces": faces,
        "materials": np.asarray(storage[2], dtype=np.uint8),
        "parts": np.asarray(storage[3], dtype=np.int32),
    }
