"""A closed tapered circular-axis tube with analytic corner normals."""

import numpy as np
from curve_frame import arc_frame


def axis_mesh(length, bend, base_radius, tip_radius, segments=24, sides=8):
    if not all(np.isfinite(v) for v in (length, bend, base_radius, tip_radius)):
        raise ValueError("finite axis parameters required")
    if (
        min(length, base_radius, tip_radius) <= 0
        or abs(bend) > np.pi
        or abs(bend) * max(base_radius, tip_radius) >= length
    ):
        raise ValueError("positive dimensions and regular curved tube required")
    if (
        not isinstance(segments, int)
        or not 2 <= segments <= 1024
        or not isinstance(sides, int)
        or not 3 <= sides <= 128
    ):
        raise ValueError("bounded integer segment counts required")
    t = np.linspace(0, 1, segments + 1)
    angle = np.arange(sides) * 2 * np.pi / sides
    centre, tangent, lateral, normal = arc_frame(length, bend, 0, t)
    radial = (
        np.cos(angle)[None, :, None] * lateral[:, None, :]
        + np.sin(angle)[None, :, None] * normal[:, None, :]
    )
    radius = base_radius + (tip_radius - base_radius) * t
    vertices = (centre[:, None, :] + radius[:, None, None] * radial).reshape(-1, 3)
    vertices = np.vstack((vertices, centre[0], centre[-1]))
    axial = length - bend * radius[:, None] * np.sin(angle)[None, :]
    normals = (
        axial[:, :, None] * radial - (tip_radius - base_radius) * tangent[:, None, :]
    )
    normals /= np.linalg.norm(normals, axis=-1, keepdims=True)
    normals = np.vstack((normals.reshape(-1, 3), -tangent[0], tangent[-1]))
    ring = np.arange((segments + 1) * sides).reshape(-1, sides)
    a, b = ring[:-1], np.roll(ring[:-1], -1, axis=1)
    d, c = ring[1:], np.roll(ring[1:], -1, axis=1)
    faces = np.stack(
        (np.stack((a, d, c), axis=-1), np.stack((a, c, b), axis=-1)), axis=2
    ).reshape(-1, 3)
    base = np.column_stack(
        (np.full(sides, len(vertices) - 2), ring[0], np.roll(ring[0], -1))
    )
    tip = np.column_stack(
        (np.full(sides, len(vertices) - 1), np.roll(ring[-1], -1), ring[-1])
    )
    faces = np.vstack((faces, base, tip)).astype(np.int32)
    corners = normals[faces].copy()
    corners[-2 * sides : -sides] = -tangent[0]
    corners[-sides:] = tangent[-1]
    return {"vertices": vertices, "faces": faces, "normals": corners.reshape(-1, 3)}
