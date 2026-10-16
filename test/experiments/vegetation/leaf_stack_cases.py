"""Controlled receiver/occluder geometry before interpreting grass transport."""

import numpy as np

CASES = (
    ("clear", ()),
    ("opaque-0.1mm", ((0.0001, "opaque", False),)),
    ("opaque-1mm", ((0.001, "opaque", False),)),
    ("opaque-10mm", ((0.01, "opaque", False),)),
    ("opaque-50mm", ((0.05, "opaque", False),)),
    ("sheet-50mm", ((0.05, "sheet", False),)),
    ("two-sheets", ((0.05, "sheet", False), (0.1, "sheet", False))),
    ("opaque-reversed", ((0.05, "opaque", True),)),
    ("sheet-reversed", ((0.05, "sheet", True),)),
)
VIEW = np.array((np.sqrt(3) / 2, 0, 0.5))
LIGHT = np.array((-np.sqrt(3) / 2, 0, 0.5))
REFLECTION = np.array((0.08, 0.22, 0.025))
TRANSMISSION = np.array((0.035, 0.12, 0.018))
IRRADIANCE = np.array((3.0, 2.7, 2.4))


def patch_geometry(x, y, reversed_winding=False):
    vertices = np.array(
        (
            (x, y - 0.06, -0.6),
            (x, y + 0.06, -0.6),
            (x, y + 0.06, 0.6),
            (x, y - 0.06, 0.6),
        )
    )
    faces = np.array(((0, 1, 2), (0, 2, 3)))
    return vertices, faces[:, ::-1] if reversed_winding else faces


def case_parts(index):
    if type(index) is not int or not 0 <= index < len(CASES):
        raise ValueError("existing controlled leaf case index required")
    y = (index - 4) * 0.14
    parts = [("receiver", *patch_geometry(0.0, y))]
    for distance, material, reverse in CASES[index][1]:
        parts.append((material, *patch_geometry(-distance, y, reverse)))
    return parts


def full_geometry():
    vertices, faces, offset = [], [], 0
    for i in range(len(CASES)):
        for _, v, f in case_parts(i):
            vertices.append(v)
            faces.append(f + offset)
            offset += len(v)
    return np.concatenate(vertices), np.concatenate(faces)


def receiver_points():
    return np.array([(0, (i - 4) * 0.14, 0) for i in range(len(CASES))], float)
