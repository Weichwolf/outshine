"""Batched metrical rectangles for the same flat frame/glazing/sill coverage."""

import numpy as np

from outshine_ref.buildings.rooflight_panels import triangle_surface


def template(width, height):
    quads, materials = [], []

    def rectangle(x0, y0, x1, y1, depth, material):
        quads.append(
            [[x0, depth, y0], [x1, depth, y0], [x1, depth, y1], [x0, depth, y1]]
        )
        materials.append(material)

    x0, x1, a, b = (
        -width / 2 - 0.055,
        width / 2 + 0.055,
        -width / 2 + 0.055,
        width / 2 - 0.055,
    )
    rectangle(x0, -0.055, x1, 0.055, 0.045, 10)
    rectangle(x0, height - 0.055, x1, height + 0.055, 0.045, 10)
    rectangle(x0, 0.055, a, height - 0.055, 0.045, 10)
    rectangle(b, 0.055, x1, height - 0.055, 0.045, 10)
    rectangle(-width / 2, 0, width / 2, height, 0, 3)
    if width > 2:
        rectangle(-0.0225, 0.055, 0.0225, height - 0.055, 0.046, 10)
    rectangle(
        -(width + 0.16) / 2,
        -0.09658333333333334,
        (width + 0.16) / 2,
        -0.06458333333333334,
        0.16,
        11,
    )
    return np.asarray(quads).reshape(-1, 3), np.repeat(materials, 2).astype(np.uint16)


def windows(facade):
    groups = {}
    for index, w in enumerate(facade.windows):
        groups.setdefault((w.width_m, w.height_m), []).append((index, w))
    vertices, faces, materials, owners = [], [], [], []
    offset = 0
    for dimensions, group in groups.items():
        points, roles = template(*dimensions)
        count, size = len(group), len(points)
        centres = np.asarray([w.centre for _i, w in group])
        tangent = np.asarray([w.tangent for _i, w in group])
        outward = tangent[:, [1, 0]] * [1, -1]
        xyz = np.empty((count, size, 3))
        xyz[:, :, :2] = (
            centres[:, None, :]
            + points[None, :, :1] * tangent[:, None, :]
            + points[None, :, 1:2] * outward[:, None, :]
        )
        xyz[:, :, 2] = (
            np.asarray([w.bottom_m for _i, w in group])[:, None] + points[None, :, 2]
        )
        index = np.arange(count * size).reshape(-1, 4)
        order = index[:, [[0, 1, 2], [0, 2, 3]]].reshape(-1, 3)
        vertices.append(xyz.reshape(-1, 3))
        faces.append(order + offset)
        materials.append(np.tile(roles, count))
        owners.append(np.repeat([i for i, _w in group], len(roles)))
        offset += count * size
    raw = {
        "vertices": np.concatenate(vertices) if vertices else np.empty((0, 3)),
        "faces": np.concatenate(faces) if faces else np.empty((0, 3), dtype=np.uint32),
    }
    mesh = triangle_surface(raw, 10)
    mesh["materials"] = (
        np.concatenate(materials) if materials else np.empty(0, dtype=np.uint16)
    )
    return mesh, np.concatenate(owners).astype(np.uint32) if owners else np.empty(
        0, dtype=np.uint32
    )
