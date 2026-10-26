"""Explicit exterior polygons of the deck/U-guard solid; shared contacts are absent."""

from itertools import pairwise

import numpy as np
import shapely


def boundary(plan):
    left, right = -plan.width_m / 2, plan.width_m / 2
    front, inner, back = -plan.depth_m, -plan.depth_m + plan.guard_m, plan.back_m
    li, ri = left + plan.guard_m, right - plan.guard_m
    bottom, top = -plan.slab_m, plan.guard_height_m
    vertices, faces, roles = [], [], []

    def face(points, normal, role):
        points, normal = np.asarray(points), np.asarray(normal)
        fixed = int(np.argmax(abs(normal)))
        axes = [i for i in range(3) if i != fixed]
        if len(points) == 4:
            triangles = points[[[0, 1, 2], [0, 2, 3]]]
        else:
            polygon = shapely.Polygon(points[:, axes])
            triangles = []
            for t in shapely.get_parts(shapely.constrained_delaunay_triangles(polygon)):
                xyz = np.tile(points[0], (3, 1))
                xyz[:, axes] = np.asarray(t.exterior.coords)[:3]
                triangles.append(xyz)
        for xyz in triangles:
            if np.cross(xyz[1] - xyz[0], xyz[2] - xyz[0]) @ normal < 0:
                xyz = xyz[::-1]
            first = len(vertices)
            vertices.extend(xyz)
            faces.append((first, first + 1, first + 2))
            roles.append(role)

    face(
        [
            (left, front, bottom),
            (right, front, bottom),
            (right, back, bottom),
            (left, back, bottom),
        ],
        (0, 0, -1),
        0,
    )
    free = [
        (left, back),
        (right, back),
        (right, 0),
        (ri, 0),
        (ri, inner),
        (li, inner),
        (li, 0),
        (left, 0),
    ]
    face([(*p, 0) for p in free], (0, 0, 1), 0)
    guard = [
        (left, front),
        (right, front),
        (right, 0),
        (ri, 0),
        (ri, inner),
        (li, inner),
        (li, 0),
        (left, 0),
    ]
    face([(*p, top) for p in guard], (0, 0, 1), 1)
    for x, normal in ((left, (-1, 0, 0)), (right, (1, 0, 0))):
        face(
            [
                (x, front, bottom),
                (x, back, bottom),
                (x, back, 0),
                (x, 0, 0),
                (x, front, 0),
            ],
            normal,
            0,
        )
    for y, normal in ((front, (0, -1, 0)), (back, (0, 1, 0))):
        face(
            [(left, y, bottom), (right, y, bottom), (right, y, 0), (left, y, 0)],
            normal,
            0,
        )
    polygon = shapely.orient_polygons(shapely.Polygon(guard))
    ring = np.asarray(polygon.exterior.coords)
    for a, b in pairwise(ring):
        tangent = b - a
        normal = [tangent[1], -tangent[0], 0]
        face([(*a, 0), (*b, 0), (*b, top), (*a, top)], normal, 1)
    return {
        "vertices": np.asarray(vertices),
        "faces": np.asarray(faces, dtype=np.uint32),
        "roles": np.asarray(roles, dtype=np.uint8),
    }
