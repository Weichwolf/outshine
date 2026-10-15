"""Two-triangle distant opening patches retain actual near opening and room IDs."""

import numpy as np
from opening_details import slots


def apply(plan, body, material):
    vertices = []
    uv = []
    records = []
    for opening in slots(plan):
        tangent = np.asarray(opening["tangent"])
        normal = np.array([tangent[1], -tangent[0]])
        centre = np.asarray(opening["centre"]) + normal * 0.001
        a = centre - tangent * opening["width_m"] / 2
        b = centre + tangent * opening["width_m"] / 2
        low = opening["bottom_z_m"]
        high = low + opening["height_m"]
        quad = np.array([(*a, low), (*b, low), (*b, high), (*a, high)])
        texture = np.array([[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]])
        for order in ((0, 1, 2), (0, 2, 3)):
            vertices.extend(quad[list(order)])
            uv.extend(texture[list(order)])
        records.append(
            opening
            | dict(
                first_triangle=len(body["faces"]) + len(vertices) // 3 - 2,
                outside_offset_m=0.001,
                scope="Closed far body with surface glazing estimate; actual opening is constructed in near product",
            )
        )
    points = np.asarray(vertices)
    faces = np.arange(len(points), dtype=np.uint32).reshape(-1, 3)
    xyz = points[faces]
    normals = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
    normals /= np.linalg.norm(normals, axis=1, keepdims=True)
    result = dict(
        vertices=np.r_[body["vertices"], points],
        faces=np.r_[body["faces"], faces + len(body["vertices"])],
        normals=np.r_[body["normals"], np.repeat(normals, 3, axis=0)],
        uv=np.r_[body["uv"], uv],
        materials=np.r_[
            body["materials"], np.full(len(faces), material, dtype=np.uint16)
        ],
        facade_extent=np.r_[
            np.zeros((len(body["vertices"]), 3)),
            np.tile([1.0, 1.0, 0.0], (len(points), 1)),
        ],
    )
    return result, records
