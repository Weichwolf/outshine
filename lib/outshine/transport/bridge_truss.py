"""Open steel side trusses for narrow, straight water crossings."""

import math

import numpy as np
import shapely

from outshine.geometry.mesh_members import rectangular_member


def steel_member(meshes, start, end, transverse, size):
    rectangular_member(meshes, start, end, transverse, size, size, "bridge metal")


def draw_open_truss(meshes, outline, span):
    width = outline.area / span.length
    if (
        not 35 <= span.length <= 100
        or not 6 <= width <= 18
        or span.length < 2.5 * width
    ):
        return False
    bays = math.ceil(span.length / 3.5)
    direction = span.direction
    lateral = np.array([-direction[1], direction[0]])
    stations = np.linspace(0.65, span.length - 0.65, bays + 1)
    centers = span.origin + stations[:, None] * direction
    cuts = shapely.linestrings(
        centers[:, None, :] + np.array([-3, 3])[None, :, None] * width * lateral
    )
    sections = shapely.intersection(outline, cuts)
    if np.any(shapely.get_type_id(sections) != 1) or np.any(
        shapely.length(sections) < 4
    ):
        return False
    left = shapely.get_coordinates(shapely.get_point(sections, 0))
    right = shapely.get_coordinates(shapely.get_point(sections, -1))
    endpoint_is_right = ((right - left) @ lateral) > 0
    sign = np.where(endpoint_is_right, 1, -1)
    offset = sign[:, None] * lateral * 0.23
    transverse = np.r_[lateral, 0]
    height = np.clip((span.length / bays) * 0.82, 2.7, 4.0)
    for side in (left + offset, right - offset):
        lower = np.c_[side, span.height(side) + 0.19]
        upper = lower + [0, 0, height]
        for a, b in zip(lower, upper, strict=True):
            steel_member(meshes, a - [0, 0, 0.28], b, transverse, 0.20)
        for i in range(bays):
            steel_member(meshes, lower[i], lower[i + 1], transverse, 0.28)
            steel_member(meshes, upper[i], upper[i + 1], transverse, 0.26)
            if i < (bays + 1) // 2:
                steel_member(meshes, lower[i], upper[i + 1], transverse, 0.15)
            if i >= bays // 2 or i == 0:
                steel_member(meshes, upper[i], lower[i + 1], transverse, 0.15)
    return True
