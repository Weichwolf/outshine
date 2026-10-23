"""Closed standard-box components with preserved source material and local frame."""

import numpy as np
import trimesh
from outshine_ref.buildings.rooflight_panels import triangle_surface
from recipe_hall_operators import sealed


def rotation(plan):
    c, s = np.cos(plan.angle), np.sin(plan.angle)
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])


def world_points(plan, points):
    return np.asarray(points) @ rotation(plan).T + [*plan.tower.source.centre_xy, 0]


def components(plan, *, clearance=False):
    unit = trimesh.creation.box()
    result = {}
    for part in plan.clearances if clearance else plan.parts:
        local = unit.vertices * part.extent + part.centre
        result[part.identifier] = sealed(
            triangle_surface(
                {"vertices": world_points(plan, local), "faces": unit.faces},
                plan.tower.source.materials[0],
            )
        )
    return result
